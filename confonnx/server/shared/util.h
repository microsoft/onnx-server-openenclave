// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>
#include <functional>

#include <confmsg/shared/buffer.h>

namespace onnxruntime {
namespace server {

std::string Trim(const std::string& source);

std::string Trim(const std::string& source, char character);

std::string Trim(const std::string& source, std::function<bool(char)> fn);

std::vector<std::string> Split(const std::string& source, char delimiter);

std::vector<uint8_t> FromHex(const std::string& hex);

std::string ToHex(confmsg::CBuffer data);

std::string Base64Url(confmsg::CBuffer data);

}  // namespace server
}  // namespace onnxruntime
