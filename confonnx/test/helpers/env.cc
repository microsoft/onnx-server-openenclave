// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <cstdlib>

namespace onnxruntime {
namespace server {
namespace test {

bool GetVerbose() {
  const char* verbose = std::getenv("VERBOSE");
  return verbose && std::string(verbose) == "1";
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime