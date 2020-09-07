// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <vector>
#include <memory>
#include <streambuf>
#include <istream>
#include <chrono>

#ifdef HAVE_LIBSKR
#include <skr/skr.h>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/fmt/ostr.h>

// ONNX RT
#include "core/session/onnxruntime_cxx_api.h"

#include <confmsg/server/api.h>
#include <confmsg/shared/crypto.h>

#include "server/shared/constants.h"
#include "server/shared/request_type.h"
#include "server/shared/status.h"
#include "server/shared/curl_helper.h"
#include "server/enclave/core/predict_protobuf.h"
#include "server/enclave/core/environment.h"
#include "server/enclave/core/executor.h"
#include "server/enclave/threading.h"
#include "server/enclave/key_vault_provider.h"
#include "server/enclave/key_vault_hsm_provider.h"
#include "server/enclave/exceptions.h"
#include "server_t.h"

namespace onnxruntime {
namespace server {

namespace protobufutil = google::protobuf::util;

confmsg::Server* confmsg_server = nullptr;
server::ServerEnvironment* env = nullptr;
std::chrono::seconds key_rollover_interval;

// Value of x-ms-request-id header field, generated and forwarded from the host.
// Used for correlating log messages to requests.
thread_local static const char* current_request_id;

thread_local static RequestType current_request_type;

void HandleRequest(std::vector<uint8_t>& data) {
  auto logger = env->GetLogger(current_request_id);

  if (current_request_type == RequestType::Score) {
    // Parse protobuf
    PredictRequest predict_request;
    if (!predict_request.ParseFromArray(data.data(), data.size())) {
      throw PayloadParseError("Protobuf parsing error");
    }

    // Run inference
    protobufutil::Status status;
    Executor executor(env, current_request_id);
    PredictResponse predict_response{};
    status = executor.Predict(predict_request, predict_response);
    if (!status.ok()) {
      throw InferenceError(status.error_message());
    }

    // Serialize output
    size_t proto_size = predict_response.ByteSizeLong();
    data.resize(proto_size);
    if (!predict_response.SerializeToArray(data.data(), proto_size)) {
      throw SerializationError("Protobuf serialization error");
    }
  } else if (current_request_type == RequestType::ProvisionModelKey) {
    env->InitializeModel(confmsg::StaticKeyProvider::Create(data, confmsg::KeyType::Curve25519));
  } else {
    throw UnknownRequestTypeError(std::to_string(static_cast<uint8_t>(current_request_type)));
  }
}

// Each entrypoint is wrapped in a separate function to allow
// easy setting of breakpoints, otherwise we break on the host side.
int _EnclaveInitialize(
    const uint8_t* model_buf, size_t model_len, bool use_model_key_provisioning,
    bool use_akv,
    const std::string& akv_app_id, const std::string& akv_app_pwd,
    const std::string& akv_vault_url, const std::string& akv_service_key_name, const std::string& akv_model_key_name,
    const std::string& akv_attestation_url) {
  if (env) {
    return SESSION_ALREADY_INITIALIZED_ERROR;
  }

  oe_load_module_host_socket_interface();
  oe_load_module_host_resolver();
  initialize_oe_pthreads();
#ifdef _DEBUG
  bool verbose_curl = true;
#else
  bool verbose_curl = false;
#endif
  CurlInit(verbose_curl);

#ifdef HAVE_LIBSKR
  if (use_akv) {
    skr_initialize();
  }
#endif

  std::vector<uint8_t> service_id;
  confmsg::internal::SHA256(confmsg::CBuffer(model_buf, model_len), service_id);

  std::unique_ptr<confmsg::KeyProvider> model_key_provider = nullptr;

  if (use_akv && !akv_model_key_name.empty()) {
    if (akv_attestation_url.empty()) {
      KeyVaultConfig kvc(akv_app_id, akv_app_pwd, akv_vault_url, akv_model_key_name);
      model_key_provider = KeyVaultProvider::Create(std::move(kvc));
    } else {
#ifdef HAVE_LIBSKR
      KeyVaultConfig kvc(akv_app_id, akv_app_pwd, akv_vault_url, akv_model_key_name, akv_attestation_url);
      model_key_provider = KeyVaultHsmProvider::Create(std::move(kvc));
#else
      abort();
#endif
    }
  }

#ifdef _DEBUG
  OrtLoggingLevel log_level = ORT_LOGGING_LEVEL_VERBOSE;
#else
  // FATAL is mapped to 'critical' for spdlog.
  // That means, ORT itself will only log fatal errors and
  // the server itself (via spdlog) will only log logger->critical() messages.
  OrtLoggingLevel log_level = ORT_LOGGING_LEVEL_FATAL;
#endif

  env = new server::ServerEnvironment(log_level,
                                      spdlog::sinks_init_list{
                                          std::make_shared<spdlog::sinks::stdout_sink_mt>()},
                                      std::move(model_key_provider));

  auto logger = env->GetAppLogger();

  std::unique_ptr<confmsg::KeyProvider> key_provider;
  if (use_akv) {
    logger->info("Using Azure Key Vault for inference key management");
    try {
      KeyVaultConfig kvc(akv_app_id, akv_app_pwd, akv_vault_url, akv_service_key_name, akv_attestation_url);
      if (akv_attestation_url.empty()) {
        key_provider = KeyVaultProvider::Create(std::move(kvc));
      } else {
#ifdef HAVE_LIBSKR
        key_provider = KeyVaultHsmProvider::Create(std::move(kvc));
#else
        logger->critical("attestation url given, but libskr not available");
        abort();
#endif
      }

    } catch (std::exception& exc) {
      logger->critical("Error initializing AKV key management: {}: {}", typeid(exc).name(), exc.what());
      return KEY_REFRESH_ERROR;
    }
  } else {
    logger->info("Using local inference key management");
    key_provider = confmsg::RandomEd25519KeyProvider::Create();
  }
  confmsg_server = new confmsg::Server(service_id, HandleRequest, std::move(key_provider));

  logger->info("Service identifier: {}", confmsg::Buffer2Hex(service_id));

  if (use_model_key_provisioning) {
    env->SetEncryptedModel(model_buf, model_len);
  } else {
    try {
      env->InitializeModel(model_buf, model_len);
      logger->debug("Model initialized successfully!");
    } catch (const Ort::Exception& ex) {
      logger->critical("Model initialization failed: {} ---- Error: [{}]", ex.GetOrtErrorCode(), ex.what());
      return MODEL_LOADING_ERROR;
    }
  }

  return SUCCESS;
}

extern "C" int EnclaveInitialize(
    const uint8_t* model_buf, size_t model_len,
    uint32_t key_rollover_interval_seconds,
    bool use_model_key_provisioning,
    bool use_akv, const char* akv_app_id, const char* akv_app_pwd,
    const char* akv_vault_url, const char* akv_service_key_name, const char* akv_model_key_name,
    const char* akv_attestation_url) {
  key_rollover_interval = std::chrono::seconds(key_rollover_interval_seconds);
  try {
    return _EnclaveInitialize(model_buf, model_len, use_model_key_provisioning,
                              use_akv, std::string(akv_app_id), std::string(akv_app_pwd),
                              std::string(akv_vault_url), std::string(akv_service_key_name), std::string(akv_model_key_name),
                              std::string(akv_attestation_url));
  } catch (std::exception& exc) {
    std::cerr << __func__ << ": Unexpected exception " << typeid(exc).name() << ": " << exc.what() << std::endl;
    return UNKNOWN_ERROR;
  } catch (...) {
    std::cerr << __func__ << ": Unexpected non-std exception" << std::endl;
    return UNKNOWN_ERROR;
  }
}

extern "C" int EnclaveHandleRequest(
    const char* request_id,
    uint8_t request_type,
    const uint8_t* input_buf, size_t input_size,
    uint8_t* output_buf, size_t* output_size, size_t output_max_size) {
  auto logger = env->GetLogger(request_id);
  // Currently, all errors are reported to the host as simple error codes
  // and then sent as plaintext JSON to the client.
  // This makes sense for encryption-related errors, since then a secure
  // connection is not possible.
  // However, any other errors (e.g. ONNX RT inference errors) should be
  // sent via the secure messaging protocol including error messages.
  // Having to handle both cases complicates clients but improves security
  // and debuggability.
  // TODO return non-encryption errors via messaging protocol
  try {
    current_request_id = request_id;
    current_request_type = static_cast<RequestType>(request_type);
    confmsg_server->RespondToMessage(input_buf, input_size, output_buf, output_size, output_max_size);
  } catch (confmsg::CryptoError& exc) {
    logger->error(exc.what());
    return CRYPTO_ERROR;
  } catch (confmsg::KeyRefreshError& exc) {
    logger->error(exc.what());
    return KEY_REFRESH_ERROR;
  } catch (confmsg::PayloadParseError& exc) {
    logger->error(exc.what());
    return PAYLOAD_PARSE_ERROR;
  } catch (confmsg::OutputBufferTooSmallError& exc) {
    logger->error(exc.what());
    return OUTPUT_BUFFER_TOO_SMALL_ERROR;
  } catch (confmsg::SerializationError& exc) {
    logger->error(exc.what());
    return OUTPUT_SERIALIZATION_ERROR;
  } catch (confmsg::AttestationError& exc) {
    logger->error(exc.what());
    return ATTESTATION_ERROR;
  } catch (server::PayloadParseError& exc) {
    logger->error(exc.what());
    return PAYLOAD_PARSE_ERROR;
  } catch (server::ModelAlreadyInitializedError& exc) {
    logger->error(exc.what());
    return MODEL_ALREADY_INITIALIZED_ERROR;
  } catch (server::SerializationError& exc) {
    logger->error(exc.what());
    return OUTPUT_SERIALIZATION_ERROR;
  } catch (server::InferenceError& exc) {
    logger->error(exc.what());
    // TODO forward error message, see notes above
    return INFERENCE_ERROR;
  } catch (server::UnknownRequestTypeError& exc) {
    logger->error(exc.what());
    return UNKNOWN_REQUEST_TYPE_ERROR;
  } catch (std::exception& exc) {
    logger->error("{}: Unexpected exception {}: {}", __func__, typeid(exc).name(), exc.what());
    return UNKNOWN_ERROR;
  } catch (...) {
    logger->error("{}: Unexpected non-std exception", __func__);
    return UNKNOWN_ERROR;
  }
  return SUCCESS;
}

extern "C" int EnclaveMaybeRefreshKey() {
  auto logger = env->GetAppLogger();
  auto now = std::chrono::system_clock::now();
  bool sync_only = now - confmsg_server->GetLastKeyRefresh() < key_rollover_interval;

  try {
    bool refreshed = confmsg_server->RefreshKey(sync_only);
    if (refreshed) {
      logger->info("Key refreshed");
    } else {
      logger->info("Key up to date, not refreshed");
    }
  } catch (confmsg::KeyRefreshError& e) {
    logger->error("Key refresh failed, will retry shortly -- Error: {}", e.what());
    return KEY_REFRESH_ERROR;
  } catch (std::exception& e) {
    logger->error("{}: Unexpected exception {}: {}", __func__, typeid(e).name(), e.what());
    return UNKNOWN_ERROR;
  } catch (...) {
    logger->critical("{}: Unexpected non-std exception", __func__);
    return UNKNOWN_ERROR;
  }

  return SUCCESS;
}

extern "C" int EnclaveDestroy() {
  delete confmsg_server;
  delete env;
  confmsg_server = nullptr;
  env = nullptr;
  CurlCleanup();
#ifdef HAVE_LIBSKR
  skr_terminate();
#endif
  return SUCCESS;
}

}  // namespace server
}  // namespace onnxruntime