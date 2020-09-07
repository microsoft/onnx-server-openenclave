// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include <openenclave/host.h>

#include "gtest/gtest.h"
#include "test/test_config.h"
#include "test/helpers/helpers.h"

#include "server/shared/curl_helper.h"
#include "server/shared/util.h"
#include "server/enclave/key_vault_provider.h"
#include "server/host/enclave_error.h"
#include "test_key_vault_config.h"
#include "test_u.h"

namespace onnxruntime {
namespace server {
namespace test {

TEST(KeyVaultProvider, HostCheckKeyContent) {
  auto kvc = GetAKVConfigOrExit(true, false);
  bool verbose = GetVerbose();
  CurlInit(verbose);
  TestKeyDeleter kvcd(kvc);

  TestKeyVaultConfig new_kvc(kvc);
  auto new_kp = KeyVaultProvider::Create(std::move(new_kvc));

  new_kp->RefreshKey();

  auto kp = KeyVaultProvider::Create(std::move(kvc));

  EXPECT_EQ(kp->GetCurrentKey().size(), new_kp->GetCurrentKey().size());
  EXPECT_EQ(kp->GetCurrentKeyVersion(), 2);
  CheckSecret(kp, new_kp);

  CurlCleanup();
}

TEST(KeyVaultProvider, HostRollover) {
  auto kvc = GetAKVConfigOrExit(true, false);
  bool verbose = GetVerbose();
  CurlInit(verbose);
  TestKeyDeleter kvcd(kvc);

  auto kp = KeyVaultProvider::Create(std::move(kvc));

  auto initial_key = kp->GetCurrentKey();
  auto initial_key_version = kp->GetCurrentKeyVersion();
  auto initial_last_refreshed = kp->GetLastRefreshed();

  // check if newer key available, no rollover
  bool sync_only = true;
  bool refreshed = kp->RefreshKey(sync_only);
  EXPECT_FALSE(refreshed);
  EXPECT_EQ(kp->GetLastRefreshed(), initial_last_refreshed);
  EXPECT_EQ(kp->GetCurrentKeyVersion(), initial_key_version);
  EXPECT_FALSE(kp->IsKeyOutdated(initial_key_version));
  EXPECT_EQ(kp->GetCurrentKey(), initial_key);
  EXPECT_EQ(kp->GetKey(initial_key_version), kp->GetCurrentKey());

  // rollover key
  sync_only = false;
  refreshed = kp->RefreshKey(sync_only);
  EXPECT_TRUE(refreshed);
  EXPECT_GT(kp->GetLastRefreshed(), initial_last_refreshed);
  EXPECT_EQ(kp->GetCurrentKeyVersion(), initial_key_version + 1);
  EXPECT_TRUE(kp->IsKeyOutdated(initial_key_version));
  EXPECT_NE(kp->GetCurrentKey(), initial_key);
  EXPECT_EQ(kp->GetKey(initial_key_version + 1), kp->GetCurrentKey());

  CurlCleanup();
}

TEST(KeyVaultProvider, EnclaveCheckKeyContent) {
  auto kvc = GetAKVConfigOrExit(true, false);
  bool verbose = GetVerbose();
  CurlInit(verbose);
  uint32_t enclave_flags = OE_ENCLAVE_FLAG_DEBUG;
  TestKeyDeleter kvcd(kvc);

  oe_enclave_t* enclave;
  EnclaveSDKError::Check(oe_create_test_enclave(
      TEST_ENCLAVE_PATH.c_str(), OE_ENCLAVE_TYPE_SGX, enclave_flags, nullptr, 0, &enclave));

  TestKeyVaultConfig new_kvc(kvc);
  auto new_kp = KeyVaultProvider::Create(std::move(new_kvc));

  std::string key_hex = ToHex(new_kp->GetCurrentKey());

  EnclaveSDKError::Check(TestEnclaveKeyVault(enclave,
                                             kvc.app_id.c_str(),
                                             kvc.app_pwd.c_str(),
                                             kvc.url.c_str(),
                                             kvc.key_name.c_str(),
                                             key_hex.c_str(),
                                             verbose));

  EnclaveSDKError::Check(oe_terminate_enclave(enclave));

  CurlCleanup();
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
