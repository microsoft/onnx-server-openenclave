// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "server/shared/key_vault_config.h"

namespace onnxruntime {
namespace server {

KeyVaultConfig::KeyVaultConfig(
    const std::string& app_id,
    const std::string& app_pwd,
    const std::string& url,
    const std::string& key_name,
    const std::string& attestation_url)
    : app_id(app_id),
      app_pwd(app_pwd),
      url(url),
      key_name(key_name),
      attestation_url(attestation_url) {
}

}  // namespace server
}  // namespace onnxruntime
