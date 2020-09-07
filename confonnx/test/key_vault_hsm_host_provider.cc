// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include <confmsg/shared/crypto.h>
#include "server/shared/util.h"
#include <server/host/enclave_error.h>
#include "test/helpers/helpers.h"
#include "test/test_config.h"
#include "key_vault_hsm_host_provider.h"

#include "test_u.h"

namespace onnxruntime {
namespace server {
namespace test {

KeyVaultHsmHostProvider::KeyVaultHsmHostProvider(KeyVaultConfig&& config)
    : confmsg::KeyProvider(SYMMETRIC_KEY_SIZE, confmsg::KeyType::Curve25519),
      config(config),
      verbose(GetVerbose()) {
  uint32_t enclave_flags = OE_ENCLAVE_FLAG_DEBUG;
  EnclaveSDKError::Check(oe_create_test_enclave(
      TEST_ENCLAVE_PATH.c_str(), OE_ENCLAVE_TYPE_SGX, enclave_flags, nullptr, 0, &enclave));
}

KeyVaultHsmHostProvider::~KeyVaultHsmHostProvider() {
  EnclaveSDKError::Check(oe_terminate_enclave(enclave));
}

bool KeyVaultHsmHostProvider::DoRefreshKey(bool sync_only) {
  (void)sync_only;
  EnclaveSDKError::Check(TestEnclaveCreateKeyVaultHsmKey(enclave,
                                                         config.app_id.c_str(),
                                                         config.app_pwd.c_str(),
                                                         config.url.c_str(),
                                                         config.key_name.c_str(),
                                                         config.attestation_url.c_str(),
                                                         verbose));

  std::vector<uint8_t> output(SYMMETRIC_KEY_SIZE);
  size_t output_size;
  uint32_t version;

  EnclaveSDKError::Check(TestEnclaveExportKeyVaultHsmKey(enclave,
                                                         config.app_id.c_str(),
                                                         config.app_pwd.c_str(),
                                                         config.url.c_str(),
                                                         config.key_name.c_str(),
                                                         config.attestation_url.c_str(),
                                                         verbose,
                                                         output.data(), &output_size, output.size(),
                                                         &version));

  current_key = output;
  current_key_version = version;
  return true;
}

void KeyVaultHsmHostProvider::DeleteKey() {
  KeyProvider::DeleteKey();
  EnclaveSDKError::Check(TestEnclaveDeleteKeyVaultHsmKey(enclave,
                                                         config.app_id.c_str(),
                                                         config.app_pwd.c_str(),
                                                         config.url.c_str(),
                                                         config.key_name.c_str(),
                                                         config.attestation_url.c_str(),
                                                         verbose));
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime