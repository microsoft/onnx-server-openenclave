// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

#include <openenclave/host.h>

#include "gtest/gtest.h"
#include "test/test_config.h"
#include "test/helpers/helpers.h"

#include "server/shared/curl_helper.h"
#include "server/enclave/key_vault_provider.h"
#include "server/enclave/key_vault_hsm_provider.h"
#include "server/host/enclave_error.h"
#include "test_key_vault_config.h"

#include "test_u.h"

namespace onnxruntime {
namespace server {
namespace test {

TEST(KeyVaultHsmProvider, Enclave) {
  TestKeyVaultConfig cfg = GetAKVConfigOrExit(true, true);

  bool verbose = GetVerbose();
  uint32_t enclave_flags = OE_ENCLAVE_FLAG_DEBUG;

  oe_enclave_t* enclave;
  EnclaveSDKError::Check(oe_create_test_enclave(
      TEST_ENCLAVE_PATH.c_str(), OE_ENCLAVE_TYPE_SGX, enclave_flags, nullptr, 0, &enclave));

  EnclaveSDKError::Check(TestEnclaveKeyVaultHsm(enclave,
                                                cfg.app_id.c_str(),
                                                cfg.app_pwd.c_str(),
                                                cfg.url.c_str(),
                                                cfg.attestation_url.c_str(),
                                                cfg.key_name.c_str(),
                                                verbose,
                                                false));

  EnclaveSDKError::Check(oe_terminate_enclave(enclave));
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
