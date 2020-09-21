// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include "onnx_protobuf.h"
#include "predict_protobuf.h"

namespace onnxruntime {
namespace server {
namespace test {

PredictRequest TensorProtoToRequest(const ONNX_NAMESPACE::ModelProto& model,
                                    const std::vector<std::string>& paths);

PredictResponse TensorProtoToResponse(const ONNX_NAMESPACE::ModelProto& model,
                                      const std::vector<std::string>& paths);

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
