// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include "test/helpers/tensorproto_util.h"

namespace onnxruntime {
namespace server {
namespace test {

// from onnxruntime/core/framework/tensorprotoutils.cc
std::vector<int64_t> GetTensorShapeFromTensorProto(const ONNX_NAMESPACE::TensorProto& tensor_proto) {
  const auto& dims = tensor_proto.dims();
  std::vector<int64_t> tensor_shape_vec(dims.size());
  for (int i = 0; i < dims.size(); ++i) {
    tensor_shape_vec[i] = dims[i];
  }
  return tensor_shape_vec;
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
