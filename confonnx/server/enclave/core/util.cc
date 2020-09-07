// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <sstream>
#include <google/protobuf/stubs/status.h>

#include "core/common/status.h"
#include "util.h"

namespace onnxruntime {
namespace server {

namespace protobufutil = google::protobuf::util;

protobufutil::Status GenerateProtobufStatus(const int& onnx_status, const std::string& message) {
  protobufutil::error::Code code = protobufutil::error::Code::UNKNOWN;
  switch (onnx_status) {
    case onnxruntime::common::StatusCode::OK:
    case onnxruntime::common::StatusCode::MODEL_LOADED:
      code = protobufutil::error::Code::OK;
      break;
    case onnxruntime::common::StatusCode::FAIL:
    case onnxruntime::common::StatusCode::INVALID_ARGUMENT:
    case onnxruntime::common::StatusCode::INVALID_PROTOBUF:
    case onnxruntime::common::StatusCode::INVALID_GRAPH:
    case onnxruntime::common::StatusCode::NO_SUCHFILE:
    case onnxruntime::common::StatusCode::NO_MODEL:
      code = protobufutil::error::Code::INVALID_ARGUMENT;
      break;
    case onnxruntime::common::StatusCode::NOT_IMPLEMENTED:
      code = protobufutil::error::Code::UNIMPLEMENTED;
      break;
    case onnxruntime::common::StatusCode::RUNTIME_EXCEPTION:
    case onnxruntime::common::StatusCode::EP_FAIL:
      code = protobufutil::error::Code::INTERNAL;
      break;
    default:
      code = protobufutil::error::Code::UNKNOWN;
  }

  std::ostringstream oss;
  oss << "ONNX Runtime Status Code: " << onnx_status << ". " << message;
  return protobufutil::Status(code, oss.str());
}

}  // namespace server
}  // namespace onnxruntime
