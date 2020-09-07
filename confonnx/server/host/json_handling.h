// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

namespace onnxruntime {
namespace server {

// Constructs JSON error message from error code and error message
std::string CreateJsonError(int error_code, const std::string& error_message);

// Escapes a string following the JSON standard
// Mostly taken from here: https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c/33799784#33799784
std::string escape_string(const std::string& message);

}  // namespace server
}  // namespace onnxruntime
