// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "test/helpers/prettyprint.h"

namespace onnxruntime {
namespace server {
namespace test {

std::ostream& operator<<(std::ostream& os, const std::vector<int64_t>& vec) {
  os << "(";
  for (size_t i = 0; i < vec.size(); i++) {
    if (i > 0) {
      os << ", ";
    }
    os << vec[i];
  }
  os << ")";
  return os;
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
