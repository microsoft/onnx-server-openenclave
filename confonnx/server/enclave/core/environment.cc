// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <memory>
#include "environment.h"
#include "core/session/onnxruntime_cxx_api.h"
#include "confmsg/shared/util.h"
#include "confmsg/shared/crypto.h"
#include "server/enclave/exceptions.h"

namespace onnxruntime {
namespace server {

static spdlog::level::level_enum Convert(OrtLoggingLevel in) {
  switch (in) {
    case OrtLoggingLevel::ORT_LOGGING_LEVEL_VERBOSE:
      return spdlog::level::level_enum::debug;
    case OrtLoggingLevel::ORT_LOGGING_LEVEL_INFO:
      return spdlog::level::level_enum::info;
    case OrtLoggingLevel::ORT_LOGGING_LEVEL_WARNING:
      return spdlog::level::level_enum::warn;
    case OrtLoggingLevel::ORT_LOGGING_LEVEL_ERROR:
      return spdlog::level::level_enum::err;
    case OrtLoggingLevel::ORT_LOGGING_LEVEL_FATAL:
      return spdlog::level::level_enum::critical;
    default:
      return spdlog::level::level_enum::off;
  }
}

void ORT_API_CALL Log(void* param, OrtLoggingLevel severity, const char* category, const char* logid, const char* code_location,
                      const char* message) {
  spdlog::logger* logger = static_cast<spdlog::logger*>(param);
  logger->log(Convert(severity), "[{} {} {}]: {}", logid, category, code_location, message);
  return;
}

ServerEnvironment::ServerEnvironment(OrtLoggingLevel severity, spdlog::sinks_init_list sink,
                                     std::unique_ptr<confmsg::KeyProvider>&& model_key_provider) : severity_(severity),
                                                                                                   logger_id_("ServerApp"),
                                                                                                   sink_(sink),
                                                                                                   default_logger_(std::make_shared<spdlog::logger>(logger_id_, sink)),
                                                                                                   runtime_environment_(severity, logger_id_.c_str(), Log, default_logger_.get()),
                                                                                                   session(nullptr),
                                                                                                   model_key_provider_(std::move(model_key_provider)) {
  spdlog::set_automatic_registration(false);
  spdlog::set_level(Convert(severity_));
  spdlog::initialize_logger(default_logger_);
}

void ServerEnvironment::SetEncryptedModel(const uint8_t* model_data, size_t model_data_length) {
  encrypted_model_.assign(model_data, model_data + model_data_length);
}

void ServerEnvironment::InitializeModel(std::unique_ptr<confmsg::KeyProvider>&& model_key_provider) {
  if (!model_output_names_.empty()) {
    throw ModelAlreadyInitializedError();
  }
  model_key_provider_ = std::move(model_key_provider);
  InitializeModel(encrypted_model_.data(), encrypted_model_.size());
  encrypted_model_.clear();
}

void ServerEnvironment::InitializeModel(const uint8_t* model_data, size_t model_data_length) {
  Ort::SessionOptions sess_opts;
  // TODO Allow customization of threading options.
  //      For now, every inference request runs sequentially, while multiple
  //      requests are handled in parallel.
  // sess_opts.SetIntraOpNumThreads(intra_op_num_threads);
  // sess_opts.SetInterOpNumThreads(inter_op_num_threads);
  // sess_opts.SetExecutionMode(ORT_PARALLEL);

  if (model_key_provider_ == nullptr) {
    session = Ort::Session(runtime_environment_, model_data, model_data_length, std::move(sess_opts));
  } else {
    if (model_data_length <= TAG_SIZE) {
      throw std::runtime_error("Not enough encrypted model data");
    }

    std::vector<uint8_t> iv(IV_SIZE, 0);
    confmsg::CBuffer cipher((const uint8_t*)model_data, model_data_length - TAG_SIZE);
    confmsg::CBuffer tag((const uint8_t*)(model_data) + model_data_length - TAG_SIZE, TAG_SIZE);
    std::vector<uint8_t> plain(cipher.n, 0);
    confmsg::internal::Decrypt(model_key_provider_->GetCurrentKey(), iv, tag, cipher, confmsg::CBuffer(), plain);
    session = Ort::Session(runtime_environment_, plain.data(), plain.size(), std::move(sess_opts));
  }

  auto output_count = session.GetOutputCount();

  Ort::AllocatorWithDefaultOptions allocator;
  for (size_t i = 0; i < output_count; i++) {
    auto name = session.GetOutputName(i, allocator);
    model_output_names_.push_back(name);
    allocator.Free(name);
  }
}

const std::vector<std::string>& ServerEnvironment::GetModelOutputNames() const {
  return model_output_names_;
}

OrtLoggingLevel ServerEnvironment::GetLogSeverity() const {
  return severity_;
}

const Ort::Session& ServerEnvironment::GetSession() const {
  return session;
}

std::shared_ptr<spdlog::logger> ServerEnvironment::GetLogger(const std::string& request_id) const {
  auto logger = std::make_shared<spdlog::logger>(request_id, sink_.begin(), sink_.end());
  spdlog::initialize_logger(logger);
  return logger;
}

std::shared_ptr<spdlog::logger> ServerEnvironment::GetAppLogger() const {
  return default_logger_;
}

}  // namespace server
}  // namespace onnxruntime
