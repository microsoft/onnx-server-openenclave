// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>
#include <vector>

#include "core/session/onnxruntime_cxx_api.h"
#include <spdlog/spdlog.h>

#include "confmsg/shared/keyprovider.h"

namespace onnxruntime {
namespace server {

class ServerEnvironment {
 public:
  explicit ServerEnvironment(OrtLoggingLevel severity, spdlog::sinks_init_list sink, std::unique_ptr<confmsg::KeyProvider>&& model_key_provider);
  ~ServerEnvironment() = default;
  ServerEnvironment(const ServerEnvironment&) = delete;

  OrtLoggingLevel GetLogSeverity() const;

  const Ort::Session& GetSession() const;
  void InitializeModel(const uint8_t* model_data, size_t model_data_length);
  void InitializeModel(std::unique_ptr<confmsg::KeyProvider>&& model_key_provider);
  void SetEncryptedModel(const uint8_t* model_data, size_t model_data_length);
  const std::vector<std::string>& GetModelOutputNames() const;
  std::shared_ptr<spdlog::logger> GetLogger(const std::string& request_id) const;
  std::shared_ptr<spdlog::logger> GetAppLogger() const;

 private:
  const OrtLoggingLevel severity_;
  const std::string logger_id_;
  const std::vector<spdlog::sink_ptr> sink_;
  const std::shared_ptr<spdlog::logger> default_logger_;

  Ort::Env runtime_environment_;
  Ort::SessionOptions options_;
  Ort::Session session;
  std::vector<std::string> model_output_names_;
  std::vector<uint8_t> encrypted_model_;  // only kept while model key not provisioned yet

  std::unique_ptr<confmsg::KeyProvider> model_key_provider_;
};

}  // namespace server
}  // namespace onnxruntime
