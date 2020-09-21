// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#ifdef HAVE_LIBSKR

#include <chrono>
#include <confmsg/shared/keyprovider.h>
#include <server/shared/key_vault_config.h>

namespace onnxruntime {
namespace server {

class KeyVaultHsmKey;

class KeyVaultHsmProvider : public confmsg::KeyProvider {
 public:
  static std::unique_ptr<confmsg::KeyProvider> Create(KeyVaultConfig&& config) {
    std::unique_ptr<KeyVaultHsmProvider> kp(new KeyVaultHsmProvider(std::move(config)));
    kp->Initialize();
    return kp;
  }

  void DeleteKey() override;

 protected:
  bool DoRefreshKey(bool sync_only) override;

 private:
  explicit KeyVaultHsmProvider(KeyVaultConfig&& config);

  KeyVaultHsmKey FetchKey(const std::string& key_version = "");
  KeyVaultHsmKey UpdateKey(uint32_t new_version);
  std::string MakeKeyIdentifier(const std::string& key_version = "") const;

  KeyVaultConfig config;

  HTTPClient http_client_akv;
  HTTPClient http_client_aas;
};

}  // namespace server
}  // namespace onnxruntime

#endif
