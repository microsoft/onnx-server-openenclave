// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <openenclave/host.h>

#include "server/host/environment.h"
#include "server/host/cancellable_timer.h"
#include "server/shared/key_vault_config.h"
#include "server/shared/request_type.h"

namespace onnxruntime {
namespace server {

class KeyVaultConfig;

class Enclave {
 public:
  Enclave(const std::string& enclave_path, bool debug, bool simulate,
          const std::shared_ptr<ServerEnvironment>& env,
          KeyVaultConfig&& service_kvc,
          KeyVaultConfig&& model_kvc,
          bool use_model_key_provisioning = false,
          std::chrono::seconds key_rollover_interval = std::chrono::hours(24),
          std::chrono::seconds key_sync_interval = std::chrono::hours(1),
          std::chrono::seconds key_error_retry_interval = std::chrono::minutes(5));
  ~Enclave();

  Enclave(const Enclave&) = delete;
  void operator=(const Enclave&) = delete;

  void Initialize(const std::string& model_path, const std::shared_ptr<ServerEnvironment>& env);

  void HandleRequest(const std::string& request_id,
                     RequestType request_type,
                     const uint8_t* input_buf, size_t input_size,
                     uint8_t* output_buf, size_t* output_size,
                     const std::shared_ptr<ServerEnvironment>& env) const;

 private:
  void StartPeriodicKeyRefreshBackgroundThread(std::shared_ptr<spdlog::logger> logger);

  oe_enclave_t* enclave;
  std::unique_ptr<std::thread> key_refresh_thread;
  CancellableTimer key_refresh_timer;
  std::chrono::seconds key_rollover_interval;
  std::chrono::seconds key_sync_interval;
  std::chrono::seconds key_error_retry_interval;
  KeyVaultConfig service_kvc;
  KeyVaultConfig model_kvc;
  bool use_model_key_provisioning;
};

}  // namespace server
}  // namespace onnxruntime
