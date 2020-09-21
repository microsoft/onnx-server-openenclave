// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <cstdlib>
#include <string>
#include <cstring>
#include <iostream>

#include <confmsg/shared/util.h>

#include <server/shared/util.h>
#include <server/enclave/key_vault_provider.h>

#include "key_vault_hsm_host_provider.h"
#include "test_key_vault_config.h"

namespace onnxruntime {
namespace server {
namespace test {

static std::string get_env_var(const char* name) {
  const char* s = std::getenv(name);
  return s != nullptr ? s : "";
}

TestKeyVaultConfig::TestKeyVaultConfig(bool use_hsm)
    : KeyVaultConfig(get_env_var("CONFONNX_TEST_APP_ID"),
                     get_env_var("CONFONNX_TEST_APP_PWD"),
                     use_hsm ? get_env_var("CONFONNX_TEST_VAULT_HSM_URL") : get_env_var("CONFONNX_TEST_VAULT_URL"),
                     "",
                     use_hsm ? get_env_var("CONFONNX_TEST_ATTESTATION_URL") : "") {
  std::vector<uint8_t> tmp(8);
  confmsg::Randomize(tmp, tmp.size());
  key_name = "test-" + onnxruntime::server::ToHex(tmp);
}

const int CTEST_SKIP_RETURN_CODE = 42;

TestKeyVaultConfig GetAKVConfigOrExit(bool use_akv, bool use_hsm) {
  TestKeyVaultConfig kvc(use_hsm);

  if (use_akv && (kvc.app_id.empty() || kvc.app_pwd.empty() || kvc.url.empty())) {
    std::cerr << "Missing key vault secrets; skipping test" << std::endl;
    // This only works as each test case is run separately via CTest.
    std::exit(CTEST_SKIP_RETURN_CODE);
  }

  return kvc;
}

TestKeyDeleter::~TestKeyDeleter() {
  if (kvc.url.empty()) {
    return;
  }
  if (kvc.attestation_url.empty()) {
    KeyVaultProvider::Create(KeyVaultConfig(kvc))->DeleteKey();
  } else {
#ifdef HAVE_LIBSKR
    KeyVaultHsmHostProvider::Create(KeyVaultConfig(kvc))->DeleteKey();
#else
    abort();
#endif
  }
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
