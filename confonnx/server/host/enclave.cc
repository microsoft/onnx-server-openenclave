// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <cstring>
#include <cerrno>
#include <iostream>
#include <fstream>
#include <vector>

#include "server_u.h"
#include "server/shared/constants.h"
#include "server/host/enclave_error.h"
#include "server/host/enclave.h"

namespace {
void CheckError(const std::ifstream& stream) {
  if (!stream.good()) {
    throw std::runtime_error(std::strerror(errno));
  }
}

std::vector<char> ReadFile(const std::string& path) {
  std::ifstream file(path);
  CheckError(file);
  file.seekg(0, std::ios::end);
  CheckError(file);
  size_t size = file.tellg();
  CheckError(file);
  std::vector<char> data(size);
  file.seekg(0, std::ios::beg);
  CheckError(file);
  file.read(data.data(), size);
  CheckError(file);
  file.close();
  return data;
}
}  // namespace

namespace onnxruntime {
namespace server {

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
Enclave::Enclave(const std::string& enclave_path, bool debug, bool simulate,
                 const std::shared_ptr<ServerEnvironment>& env,
                 KeyVaultConfig&& service_kvc,
                 KeyVaultConfig&& model_kvc,
                 bool use_model_key_provisioning,
                 std::chrono::seconds key_rollover_interval,
                 std::chrono::seconds key_sync_interval,
                 std::chrono::seconds key_error_retry_interval)
    : key_rollover_interval(key_rollover_interval),
      key_sync_interval(key_sync_interval),
      key_error_retry_interval(key_error_retry_interval),
      service_kvc(service_kvc),
      model_kvc(model_kvc),
      use_model_key_provisioning(use_model_key_provisioning) {
  auto logger = env->GetAppLogger();

  uint32_t enclave_flags = 0;

  if (debug) {
    enclave_flags |= OE_ENCLAVE_FLAG_DEBUG;
    logger->info("Enabling enclave debug mode");
  }
  if (simulate) {
    enclave_flags |= OE_ENCLAVE_FLAG_SIMULATE;
    logger->info("Enabling enclave simulation mode");
  }

  logger->info("Creating enclave");
  EnclaveSDKError::Check(oe_create_server_enclave(
      enclave_path.c_str(), OE_ENCLAVE_TYPE_SGX, enclave_flags, nullptr, 0, &enclave));
  logger->info("Enclave created");
}

void Enclave::Initialize(const std::string& model_path, const std::shared_ptr<ServerEnvironment>& env) {
  auto logger = env->GetAppLogger();

  logger->debug("Loading model file");
  std::vector<char> model = ReadFile(model_path);

  logger->debug("Initializing enclave");
  int status;
  uint32_t key_rollover_interval_seconds = key_rollover_interval.count();
  EnclaveSDKError::Check(EnclaveInitialize(enclave, &status,
                                           (uint8_t*)model.data(), model.size(),
                                           key_rollover_interval_seconds,
                                           use_model_key_provisioning,
                                           !service_kvc.url.empty(),
                                           service_kvc.app_id.c_str(), service_kvc.app_pwd.c_str(), service_kvc.url.c_str(),
                                           service_kvc.key_name.c_str(),
                                           model_kvc.key_name.c_str(),
                                           service_kvc.attestation_url.c_str()));
  EnclaveCallError::Check(status);
  logger->info("Enclave initialized");

  logger->info("Key rollover interval: {}s", key_rollover_interval_seconds);
  logger->info("Key sync interval: {}s", key_sync_interval.count());
  logger->info("Key rollover/sync error retry interval: {}s", key_error_retry_interval.count());
  StartPeriodicKeyRefreshBackgroundThread(logger);
}

void Enclave::HandleRequest(const std::string& request_id,
                            RequestType request_type,
                            const uint8_t* input_buf, size_t input_size,
                            uint8_t* output_buf, size_t* output_size, const std::shared_ptr<ServerEnvironment>& env) const {
  (void)env;
  int status;
  EnclaveSDKError::Check(EnclaveHandleRequest(enclave, &status, request_id.c_str(), static_cast<uint8_t>(request_type),
                                              input_buf, input_size, output_buf, output_size, MAX_OUTPUT_SIZE));
  EnclaveCallError::Check(status);
}

void Enclave::StartPeriodicKeyRefreshBackgroundThread(std::shared_ptr<spdlog::logger> logger) {
  auto fn = [=]() {
    key_refresh_timer.wait_for(key_sync_interval);
    while (!key_refresh_timer.cancelled()) {
      try {
        int status;
        EnclaveSDKError::Check(EnclaveMaybeRefreshKey(enclave, &status));
        EnclaveCallError::Check(status);
        key_refresh_timer.wait_for(key_sync_interval);
      } catch (EnclaveCallError& e) {
        if (e.status == KEY_REFRESH_ERROR) {
          logger->info("Key refresh failed, will retry shortly");
        } else {
          logger->error("{}: Unexpected error occurred during key refresh, will retry shortly", __func__);
        }
        key_refresh_timer.wait_for(key_error_retry_interval);
      } catch (EnclaveSDKError& e) {
        logger->critical("Unknown OE error occurred during key refresh, will retry shortly -- {}", e.what());
        key_refresh_timer.wait_for(key_error_retry_interval);
      } catch (std::exception& e) {
        logger->critical("{}: Unexpected exception occurred on host during key refresh, will retry shortly -- {}", __func__, e.what());
        key_refresh_timer.wait_for(key_error_retry_interval);
      } catch (...) {
        logger->critical("{}: Unexpected non-std exception occurred on host during key refresh, will retry shortly", __func__);
        key_refresh_timer.wait_for(key_error_retry_interval);
      }
    }
    logger->info("key refresh background thread stopping");
  };
  key_refresh_thread = std::make_unique<std::thread>(fn);
}

Enclave::~Enclave() {
  key_refresh_timer.cancel();
  if (key_refresh_thread) key_refresh_thread->join();
  int status;
  EnclaveSDKError::Check(EnclaveDestroy(enclave, &status));
  EnclaveSDKError::Check(oe_terminate_enclave(enclave));
}

}  // namespace server
}  // namespace onnxruntime
