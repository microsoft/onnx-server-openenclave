// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <memory>

#include <spdlog/spdlog.h>

namespace onnxruntime {
namespace server {

class ServerEnvironment {
 public:
  explicit ServerEnvironment(spdlog::level::level_enum severity, spdlog::sinks_init_list sink,
                             const std::string& auth_key = "");
  ~ServerEnvironment() = default;
  ServerEnvironment(const ServerEnvironment&) = delete;

  std::shared_ptr<spdlog::logger> GetLogger(const std::string& request_id) const;
  std::shared_ptr<spdlog::logger> GetAppLogger() const;
  bool IsAuthEnabled() const;
  const std::string& GetAuthKey() const;

 private:
  const std::string logger_id_;
  const std::vector<spdlog::sink_ptr> sink_;
  const std::shared_ptr<spdlog::logger> default_logger_;
  const std::string auth_key_;
};

}  // namespace server
}  // namespace onnxruntime
