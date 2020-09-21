// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <iostream>
#include <vector>

namespace onnxruntime {
namespace server {
namespace test {

std::ostream& operator<<(std::ostream& os, const std::vector<int64_t>& vec);

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
