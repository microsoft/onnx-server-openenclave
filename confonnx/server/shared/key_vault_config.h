// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>

namespace onnxruntime {
namespace server {

class KeyVaultConfig {
 public:
  std::string app_id;
  std::string app_pwd;
  std::string url;
  std::string key_name;
  std::string attestation_url;

  KeyVaultConfig() = default;

  KeyVaultConfig(const std::string& app_id,
                 const std::string& app_pwd,
                 const std::string& url,
                 const std::string& key_name,
                 const std::string& attestation_url = "");

  KeyVaultConfig(const KeyVaultConfig& other) = default;
  KeyVaultConfig(KeyVaultConfig&& other) = default;
  KeyVaultConfig& operator=(const KeyVaultConfig& p) = default;
};

}  // namespace server
}  // namespace onnxruntime
