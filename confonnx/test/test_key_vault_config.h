// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>

#include <server/shared/key_vault_config.h>
namespace onnxruntime {
namespace server {
namespace test {

class TestKeyVaultConfig : public KeyVaultConfig {
 public:
  explicit TestKeyVaultConfig(bool use_hsm);
};

TestKeyVaultConfig GetAKVConfigOrExit(bool use_akv, bool use_hsm);

class TestKeyDeleter {
 public:
  TestKeyDeleter(const KeyVaultConfig& kvc) : kvc(kvc) {}
  ~TestKeyDeleter();

 protected:
  const KeyVaultConfig& kvc;
};

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
