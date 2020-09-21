// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <fstream>

#include "gtest/gtest.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/fmt/ostr.h>

#include <confmsg/client/api.h>
#include <confmsg/shared/crypto.h>
#include <confmsg/test/openenclave_debug_key.h>

#include "server/host/core/context.h"
#include "server/host/environment.h"
#include "server/host/enclave.h"
#include "server/enclave/key_vault_provider.h"
#include "server/host/request_handler.h"
#include "server/shared/util.h"
#include "server/shared/request_type.h"
#include "test/test_config.h"
#include "test/helpers/helpers.h"
#include "test_key_vault_config.h"
#include "key_vault_hsm_host_provider.h"

namespace onnxruntime {
namespace server {
namespace test {

// Parameters: enable_auth; encrypt_model; use_akv; use_akv_hsm
class InferenceRequest : public testing::TestWithParam<std::tuple<bool, bool, bool, bool>> {};

INSTANTIATE_TEST_SUITE_P(WithoutAuthWithoutModelEncryptionWithoutAkv,
                         InferenceRequest,
                         testing::Values(std::tuple(false, false, false, false)));

INSTANTIATE_TEST_SUITE_P(WithoutAuthWithoutModelEncryptionWithAkv,
                         InferenceRequest,
                         testing::Values(std::tuple(false, false, true, false)));

INSTANTIATE_TEST_SUITE_P(WithAuthWithModelEncryptionWithAkv,
                         InferenceRequest,
                         testing::Values(std::tuple(true, true, true, false)));

#ifdef HAVE_LIBSKR
INSTANTIATE_TEST_SUITE_P(WithAuthWithModelEncryptionWithAkvHsm,
                         InferenceRequest,
                         testing::Values(std::tuple(true, true, true, true)));
#endif

INSTANTIATE_TEST_SUITE_P(WithAuthWithModelEncryptionWithModelKeyProvisioning,
                         InferenceRequest,
                         testing::Values(std::tuple(true, true, false, false)));

TEST_P(InferenceRequest, SqueezeNet) {
  const bool& enable_auth = std::get<0>(GetParam());
  const bool& encrypt_model = std::get<1>(GetParam());
  const bool& use_akv = std::get<2>(GetParam());
  const bool& use_akv_hsm = std::get<3>(GetParam());
  const bool& use_model_key_provisioning = encrypt_model && !use_akv;

  TestKeyVaultConfig kvc = GetAKVConfigOrExit(use_akv, use_akv_hsm);

  std::string model_dir = TEST_DATA_PATH + "/squeezenet/";
  std::string model_path = model_dir + "model.onnx";
  std::string input_path = model_dir + "test_data_set_0/test_data_0_input.pb";
  std::string expected_output_path = model_dir + "test_data_set_0/test_data_0_output.pb";

  auto model = LoadProtobufFromFile<ONNX_NAMESPACE::ModelProto>(model_path);

  PredictRequest request = TensorProtoToRequest(model, {input_path});
  PredictResponse expected_response = TensorProtoToResponse(model, {expected_output_path});

  std::string auth_key = enable_auth ? "foo" : "";

  const auto env = std::make_shared<server::ServerEnvironment>(spdlog::level::level_enum::info,
                                                               spdlog::sinks_init_list{std::make_shared<spdlog::sinks::stdout_sink_mt>()},
                                                               auth_key);

  KeyVaultConfig service_kvc(kvc);
  KeyVaultConfig model_kvc;
  TestKeyDeleter skvcd(service_kvc);
  TestKeyDeleter mkvcd(model_kvc);

  std::vector<uint8_t> expected_service_id;
  std::string tmp_filename;

  std::unique_ptr<confmsg::KeyProvider> model_key_provider;
  if (encrypt_model) {
    tmp_filename = std::tmpnam(nullptr);
    model_kvc = kvc;
    model_kvc.key_name += "-model";
    if (use_model_key_provisioning) {
      model_key_provider = confmsg::RandomEd25519KeyProvider::Create();
    } else if (use_akv_hsm) {
#ifdef HAVE_LIBSKR
      model_key_provider = KeyVaultHsmHostProvider::Create(KeyVaultConfig(model_kvc));
#else
      abort();
#endif
    } else {
      model_key_provider = KeyVaultProvider::Create(KeyVaultConfig(model_kvc));
    }
    expected_service_id = EncryptModelFile(model_key_provider->GetCurrentKey(), model_path, tmp_filename);
    model_path = tmp_filename;
  } else {
    expected_service_id = HashModelFile(model_path);
  }

  std::vector<uint8_t> expected_enclave_hash;  // empty = don't check

  bool debug = true;
  bool simulate = false;
  server::Enclave enclave(SERVER_ENCLAVE_PATH, debug, simulate, env,
                          KeyVaultConfig(service_kvc), KeyVaultConfig(model_kvc),
                          use_model_key_provisioning);
  enclave.Initialize(model_path, env);

  if (!tmp_filename.empty()) {
    std::remove(tmp_filename.c_str());
  }

  HttpContext context;

  size_t proto_size = request.ByteSizeLong();
  std::vector<uint8_t> predict_request_buf(proto_size);
  if (!request.SerializeToArray(predict_request_buf.data(), proto_size)) {
    throw std::runtime_error("protobuf serialization error");
  }

  // Unsigned enclaves are signed by the OE debug key (OE_DEBUG_SIGN_PUBLIC_KEY) upon creation.
  auto key_provider = confmsg::RandomKeyProvider::Create(KEY_SIZE);
  confmsg::Client client(std::move(key_provider),
                         OE_DEBUG_SIGN_PUBLIC_KEY,
                         expected_enclave_hash,
                         expected_service_id,
                         true);

  // Create key request message
  size_t extra = 1024;
  std::vector<uint8_t> key_request_buf(proto_size + extra);
  size_t key_request_size;
  client.MakeKeyRequest(key_request_buf.data(), &key_request_size, key_request_buf.size());

  // Check if wrong auth key results in error
  if (!auth_key.empty()) {
    context.request.body() = "";
    context.request.set(http::field::authorization, "Bearer invalidkey");
    server::HandleRequest(context, RequestType::Score, enclave, env);
    EXPECT_EQ(context.response.result_int(), 401);
  }

  // Send key request
  std::string key_request_body((char*)key_request_buf.data(), key_request_size);
  context.request.body() = key_request_body;
  if (!auth_key.empty()) {
    context.request.set(http::field::authorization, "Bearer " + auth_key);
  }
  server::HandleRequest(context, RequestType::Score, enclave, env);
  if (context.response.result_int() != 200) {
    std::cerr << context.response.body() << std::endl;
  }
  EXPECT_EQ(context.response.result_int(), 200);
  confmsg::Client::Result key_result = client.HandleMessage((const uint8_t*)context.response.body().c_str(), context.response.body().size());
  EXPECT_TRUE(key_result.IsKeyResponse());

  // Provision model key
  if (use_model_key_provisioning) {
    const std::vector<uint8_t>& model_key = model_key_provider->GetCurrentKey();
    // Create model key provisioning request message
    std::vector<uint8_t> request_buf(model_key.size() + extra);
    size_t request_size;
    client.MakeRequest(model_key, request_buf.data(), &request_size, request_buf.size());

    // Send model key provisioning request
    std::string request_body((char*)request_buf.data(), request_size);
    context.request.body() = request_body;
    if (!auth_key.empty()) {
      context.request.set(http::field::authorization, "Bearer " + auth_key);
    }
    server::HandleRequest(context, RequestType::ProvisionModelKey, enclave, env);
    if (context.response.result_int() != 200) {
      std::cerr << context.response.body() << std::endl;
    }
    EXPECT_EQ(context.response.result_int(), 200);
    confmsg::Client::Result r = client.HandleMessage((const uint8_t*)context.response.body().c_str(), context.response.body().size());
    EXPECT_TRUE(r.IsResponse());
  }

  // Create inference request message
  std::vector<uint8_t> request_buf(proto_size + extra);
  size_t request_size;
  client.MakeRequest(predict_request_buf, request_buf.data(), &request_size, request_buf.size());

  // Send inference request
  std::string request_body((char*)request_buf.data(), request_size);
  context.request.body() = request_body;
  if (!auth_key.empty()) {
    context.request.set(http::field::authorization, "Bearer " + auth_key);
  }
  server::HandleRequest(context, RequestType::Score, enclave, env);
  if (context.response.result_int() != 200) {
    std::cerr << context.response.body() << std::endl;
  }
  EXPECT_EQ(context.response.result_int(), 200);
  confmsg::Client::Result r = client.HandleMessage((const uint8_t*)context.response.body().c_str(), context.response.body().size());
  EXPECT_TRUE(r.IsResponse());
  PredictResponse actual_response;
  actual_response.ParseFromArray(r.GetPayload().data(), r.GetPayload().size());
  EXPECT_TRUE(ProtobufCompare(expected_response, actual_response));
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
