// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <chrono>
#include <openenclave/host.h>

#include <confmsg/shared/keyprovider.h>
#include <server/shared/key_vault_config.h>

namespace onnxruntime {
namespace server {
namespace test {

class KeyVaultHsmHostProvider : public confmsg::KeyProvider {
 public:
  static std::unique_ptr<confmsg::KeyProvider> Create(KeyVaultConfig&& config) {
    std::unique_ptr<KeyVaultHsmHostProvider> kp(new KeyVaultHsmHostProvider(std::move(config)));
    kp->Initialize();
    return kp;
  }

  void DeleteKey() override;

  ~KeyVaultHsmHostProvider();

 protected:
  bool DoRefreshKey(bool sync_only) override;

 private:
  explicit KeyVaultHsmHostProvider(KeyVaultConfig&& config);

  oe_enclave_t* enclave;
  KeyVaultConfig config;
  bool verbose;
};

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
