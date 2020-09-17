// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <random>
#include "gtest/gtest.h"
#include <openenclave/host.h>

#include "client/api.h"
#include "server/api.h"
#include "shared/util.h"
#include "test/openenclave_debug_key.h"

#include "test_u.h"  // generated
#include "test/test_config.h"

#define OE_THROW_ON_ERROR(expr)                           \
  do {                                                    \
    oe_result_t oe_result = (expr);                       \
    if (oe_result != OE_OK) {                             \
      throw std::runtime_error(oe_result_str(oe_result)); \
    }                                                     \
  } while (0);

namespace confmsg {
namespace test {

static void check_same(const std::vector<uint8_t>& x, const std::vector<uint8_t>& y) {
  ASSERT_EQ(x.size(), y.size());
  ASSERT_EQ(x, y);
}

TEST(Integration, HostSimple) {
  std::vector<uint8_t> plaintext(1024);
  Randomize(plaintext, 1024);

  // TODO define service identifier so that validation is made
  std::vector<uint8_t> service_identifier;
  std::vector<uint8_t> expected_service_identifier = service_identifier;
  std::string expected_enclave_signing_key_pem;  // empty = don't check
  std::vector<uint8_t> expected_enclave_hash;    // empty = don't check
  auto server_key_provider = confmsg::RandomEd25519KeyProvider::Create();
  auto client_key_provider = confmsg::RandomKeyProvider::Create(KEY_SIZE);
  confmsg::Server server(service_identifier, [](auto) { return 0; }, std::move(server_key_provider));
  confmsg::Client client(std::move(client_key_provider),
                         expected_enclave_signing_key_pem,
                         expected_enclave_hash,
                         expected_service_identifier,
                         true);

  // Client
  uint8_t key_request_msg[1024];
  size_t key_request_msg_size = 0;
  client.MakeKeyRequest(key_request_msg, &key_request_msg_size, sizeof(key_request_msg));

  // Server
  uint8_t key_response_msg[1024];
  size_t key_response_msg_size = 0;
  server.RespondToMessage(key_request_msg, key_request_msg_size, key_response_msg, &key_response_msg_size, sizeof(key_response_msg));

  // Client
  client.HandleMessage(key_response_msg, key_response_msg_size);
  uint8_t request_msg[2048];
  size_t request_msg_size = 0;
  client.MakeRequest(CBuffer(plaintext), request_msg, &request_msg_size, sizeof(request_msg));

  // Server
  uint8_t response_msg[2048];
  size_t response_msg_size;
  server.RespondToMessage(request_msg, request_msg_size, response_msg, &response_msg_size, sizeof(response_msg));

  // Client
  Client::Result r = client.HandleMessage(response_msg, response_msg_size);
  EXPECT_TRUE(r.IsResponse());
  check_same(plaintext, r.GetPayload());
}

TEST(Integration, HostTwoClients) {
  std::vector<uint8_t> plaintext(1024);
  Randomize(plaintext, 1024);

  // TODO define service identifier so that validation is made
  std::vector<uint8_t> service_identifier;
  std::vector<uint8_t> expected_service_identifier = service_identifier;
  std::string expected_enclave_signing_key_pem;  // empty = don't check
  std::vector<uint8_t> expected_enclave_hash;    // empty = don't check
  auto server_key_provider = confmsg::RandomEd25519KeyProvider::Create();
  auto client_key_provider1 = confmsg::RandomKeyProvider::Create(KEY_SIZE);
  auto client_key_provider2 = confmsg::RandomKeyProvider::Create(KEY_SIZE);
  confmsg::Server server(service_identifier, [](auto) { return 0; }, std::move(server_key_provider));
  confmsg::Client client1(std::move(client_key_provider1),
                          expected_enclave_signing_key_pem,
                          expected_enclave_hash,
                          expected_service_identifier,
                          true);
  confmsg::Client client2(std::move(client_key_provider2),
                          expected_enclave_signing_key_pem,
                          expected_enclave_hash,
                          expected_service_identifier,
                          true);

  // Client 1
  uint8_t key_request_msg_1[1024];
  size_t key_request_msg_size_1 = 0;
  client1.MakeKeyRequest(key_request_msg_1, &key_request_msg_size_1, sizeof(key_request_msg_1));

  // Client 2
  uint8_t key_request_msg_2[1024];
  size_t key_request_msg_size_2 = 0;
  client2.MakeKeyRequest(key_request_msg_2, &key_request_msg_size_2, sizeof(key_request_msg_2));

  // Server
  uint8_t key_response_msg_2[1024];
  size_t key_response_msg_size_2 = 0;
  server.RespondToMessage(key_request_msg_2, key_request_msg_size_2, key_response_msg_2, &key_response_msg_size_2, sizeof(key_response_msg_2));
  uint8_t key_response_msg_1[1024];
  size_t key_response_msg_size_1 = 0;
  server.RespondToMessage(key_request_msg_1, key_request_msg_size_1, key_response_msg_1, &key_response_msg_size_1, sizeof(key_response_msg_1));

  // Client 1
  client1.HandleMessage(key_response_msg_1, key_response_msg_size_1);
  uint8_t request_msg_1[2048];
  size_t request_msg_size_1 = 0;
  client1.MakeRequest(CBuffer(plaintext), request_msg_1, &request_msg_size_1, sizeof(request_msg_1));

  // Client 2
  client2.HandleMessage(key_response_msg_2, key_response_msg_size_2);
  uint8_t request_msg_2[2048];
  size_t request_msg_size_2 = 0;
  client2.MakeRequest(CBuffer(plaintext), request_msg_2, &request_msg_size_2, sizeof(request_msg_2));

  // Server
  uint8_t response_msg_2[2048];
  size_t response_msg_size_2;
  server.RespondToMessage(request_msg_2, request_msg_size_2, response_msg_2, &response_msg_size_2, sizeof(response_msg_2));
  uint8_t response_msg_1[2048];
  size_t response_msg_size_1;
  server.RespondToMessage(request_msg_1, request_msg_size_1, response_msg_1, &response_msg_size_1, sizeof(response_msg_1));

  // Client 1
  Client::Result r1 = client1.HandleMessage(response_msg_1, response_msg_size_1);
  EXPECT_TRUE(r1.IsResponse());
  check_same(plaintext, r1.GetPayload());

  // Client 2
  Client::Result r2 = client2.HandleMessage(response_msg_2, response_msg_size_2);
  EXPECT_TRUE(r2.IsResponse());
  check_same(plaintext, r2.GetPayload());
}

TEST(Integration, EnclaveSimple) {
  bool debug = true;
  bool simulate = false;

  uint32_t enclave_flags = 0;

  if (debug) {
    enclave_flags |= OE_ENCLAVE_FLAG_DEBUG;
  }
  if (simulate) {
    enclave_flags |= OE_ENCLAVE_FLAG_SIMULATE;
  }

  std::vector<uint8_t> plaintext(1024);
  Randomize(plaintext, 1024);
  // TODO define service identifier so that validation is made
  //std::vector<uint8_t> service_id;
  std::vector<uint8_t> expected_service_id;    // = service_id;
  std::vector<uint8_t> expected_enclave_hash;  // empty = don't check

  oe_enclave_t* enclave;
  OE_THROW_ON_ERROR(oe_create_test_enclave(ENCLAVE_PATH.c_str(), OE_ENCLAVE_TYPE_SGX, enclave_flags, nullptr, 0, &enclave));
  OE_THROW_ON_ERROR(EnclaveInitialize(enclave));

  // Unsigned enclaves are signed by this OE debug key upon creation.
  auto client_key_provider = confmsg::RandomKeyProvider::Create(KEY_SIZE);
  confmsg::Client client(std::move(client_key_provider),
                         OE_DEBUG_SIGN_PUBLIC_KEY,
                         expected_enclave_hash,
                         expected_service_id,
                         true);

  // Client
  uint8_t key_request_msg[1024];
  size_t key_request_msg_size = 0;
  client.MakeKeyRequest(key_request_msg, &key_request_msg_size, sizeof(key_request_msg));

  // Enclave Server
  uint8_t key_response_msg[10 * 1024];  // cwinter: evidence can be large
  size_t key_response_msg_size = 0;
  EnclaveRespondToMessage(enclave, key_request_msg, key_request_msg_size, key_response_msg, &key_response_msg_size, sizeof(key_response_msg));

  // Client
  client.HandleMessage(key_response_msg, key_response_msg_size);
  uint8_t request_msg[2048];
  size_t request_msg_size = 0;
  client.MakeRequest(CBuffer(plaintext), request_msg, &request_msg_size, sizeof(request_msg));

  // Enclave Server
  uint8_t response_msg[2048];
  size_t response_msg_size;
  EnclaveRespondToMessage(enclave, request_msg, request_msg_size, response_msg, &response_msg_size, sizeof(response_msg));

  // Client
  Client::Result r = client.HandleMessage(response_msg, response_msg_size);
  EXPECT_TRUE(r.IsResponse());
  check_same(plaintext, r.GetPayload());
}

TEST(Integration, EnclaveMultipleRequests) {
  bool debug = true;
  bool simulate = false;

  uint32_t enclave_flags = 0;

  if (debug) {
    enclave_flags |= OE_ENCLAVE_FLAG_DEBUG;
  }
  if (simulate) {
    enclave_flags |= OE_ENCLAVE_FLAG_SIMULATE;
  }

  std::vector<uint8_t> plaintext(1024);
  Randomize(plaintext, 1024);
  // TODO define service identifier so that validation is made
  //std::vector<uint8_t> service_id;
  std::vector<uint8_t> expected_service_id;    // = service_id;
  std::vector<uint8_t> expected_enclave_hash;  // empty = don't check

  oe_enclave_t* enclave;
  OE_THROW_ON_ERROR(oe_create_test_enclave(ENCLAVE_PATH.c_str(), OE_ENCLAVE_TYPE_SGX, enclave_flags, nullptr, 0, &enclave));
  OE_THROW_ON_ERROR(EnclaveInitialize(enclave));

  // Unsigned enclaves are signed by this OE debug key upon creation.
  auto client_key_provider = confmsg::RandomKeyProvider::Create(KEY_SIZE);
  confmsg::Client client(std::move(client_key_provider),
                         OE_DEBUG_SIGN_PUBLIC_KEY,
                         expected_enclave_hash,
                         expected_service_id,
                         debug);

  // Client
  uint8_t key_request_msg[1024];
  size_t key_request_msg_size = 0;
  client.MakeKeyRequest(key_request_msg, &key_request_msg_size, sizeof(key_request_msg));

  // Enclave Server
  uint8_t key_response_msg[10 * 1024];  // cwinter: evidence can be large
  size_t key_response_msg_size = 0;
  EnclaveRespondToMessage(enclave, key_request_msg, key_request_msg_size, key_response_msg, &key_response_msg_size, sizeof(key_response_msg));

  // Client
  client.HandleMessage(key_response_msg, key_response_msg_size);

  for (size_t i = 0; i < 32; i++) {
    // Client
    uint8_t request_msg[2048];
    size_t request_msg_size = 0;
    client.MakeRequest(CBuffer(plaintext), request_msg, &request_msg_size, sizeof(request_msg));

    // Enclave Server
    uint8_t response_msg[2048];
    size_t response_msg_size;
    EnclaveRespondToMessage(enclave, request_msg, request_msg_size, response_msg, &response_msg_size, sizeof(response_msg));

    // Client
    Client::Result r = client.HandleMessage(response_msg, response_msg_size);
    EXPECT_TRUE(r.IsResponse());
    check_same(plaintext, r.GetPayload());
  }
}

}  // namespace test
}  // namespace confmsg
