// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iomanip>

#include "server/host/json_handling.h"

namespace onnxruntime {
namespace server {

std::string CreateJsonError(const int error_code, const std::string& error_message) {
  auto escaped_message = escape_string(error_message);
  return R"({"error_code": )" + std::to_string(error_code) + R"(, "error_message": ")" + escaped_message + R"("})" + "\n";
}

std::string escape_string(const std::string& message) {
  std::ostringstream o;
  for (char c : message) {
    switch (c) {
      case '"':
        o << "\\\"";
        break;
      case '\\':
        o << "\\\\";
        break;
      case '\b':
        o << "\\b";
        break;
      case '\f':
        o << "\\f";
        break;
      case '\n':
        o << "\\n";
        break;
      case '\r':
        o << "\\r";
        break;
      case '\t':
        o << "\\t";
        break;
      default:
        if ('\x00' <= c && c <= '\x1f') {
          o << "\\u"
            << std::hex << std::setw(4) << std::setfill('0') << (int)c;
        } else {
          o << c;
        }
    }
  }
  return o.str();
}

}  // namespace server
}  // namespace onnxruntime