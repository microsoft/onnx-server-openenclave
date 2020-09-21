// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <memory>

#include "server/host/environment.h"

namespace onnxruntime {
namespace server {

ServerEnvironment::ServerEnvironment(spdlog::level::level_enum severity, spdlog::sinks_init_list sink,
                                     const std::string& auth_key) : logger_id_("ServerApp"),
                                                                    sink_(sink),
                                                                    default_logger_(std::make_shared<spdlog::logger>(logger_id_, sink)),
                                                                    auth_key_(auth_key) {
  spdlog::set_automatic_registration(false);
  spdlog::set_level(severity);
  spdlog::initialize_logger(default_logger_);
}

std::shared_ptr<spdlog::logger> ServerEnvironment::GetLogger(const std::string& request_id) const {
  auto logger = std::make_shared<spdlog::logger>(request_id, sink_.begin(), sink_.end());
  spdlog::initialize_logger(logger);
  return logger;
}

std::shared_ptr<spdlog::logger> ServerEnvironment::GetAppLogger() const {
  return default_logger_;
}

bool ServerEnvironment::IsAuthEnabled() const {
  return !auth_key_.empty();
}

const std::string& ServerEnvironment::GetAuthKey() const {
  return auth_key_;
}

}  // namespace server
}  // namespace onnxruntime
