// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>
#include "onnx_protobuf.h"

namespace onnxruntime {
namespace server {
namespace test {

std::vector<int64_t> GetTensorShapeFromTensorProto(const ONNX_NAMESPACE::TensorProto& tensor_proto);

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
