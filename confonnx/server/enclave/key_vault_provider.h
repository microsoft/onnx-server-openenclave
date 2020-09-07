// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <chrono>
#include <confmsg/shared/keyprovider.h>
#include <server/shared/curl_helper.h>
#include <server/shared/key_vault_config.h>

namespace onnxruntime {
namespace server {

class KeyVaultKey;
class KeyVaultProvider : public confmsg::KeyProvider {
 public:
  static std::unique_ptr<confmsg::KeyProvider> Create(KeyVaultConfig&& config) {
    // Only temporarily used until new AKV can create keys for us.
    std::unique_ptr<confmsg::KeyProvider> random_key_provider =
        confmsg::RandomEd25519KeyProvider::Create();

    std::unique_ptr<KeyVaultProvider> kp(new KeyVaultProvider(std::move(config), std::move(random_key_provider)));
    kp->Initialize();
    return kp;
  }

  void DeleteKey() override;

 protected:
  bool DoRefreshKey(bool sync_only) override;

 private:
  KeyVaultProvider(KeyVaultConfig&& config, std::unique_ptr<confmsg::KeyProvider>&& random_key_provider);

  KeyVaultKey FetchKey(const std::string& key_version = "");
  KeyVaultKey UpdateKey(uint32_t new_version);

  KeyVaultConfig config;

  HTTPClient http_client;

  std::unique_ptr<confmsg::KeyProvider> random_key_provider;
};

}  // namespace server
}  // namespace onnxruntime
