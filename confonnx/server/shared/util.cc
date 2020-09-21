// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <cctype>
#include <sstream>
#include <iomanip>

#include <base64.h>

#include "util.h"

namespace onnxruntime {
namespace server {

std::string Trim(const std::string& source) {
  return Trim(source, [](char c) { return std::isspace(static_cast<uint8_t>(c)); });
}

std::string Trim(const std::string& source, const char character) {
  return Trim(source, [character](char c) { return c == character; });
}

std::string Trim(const std::string& source, std::function<bool(char)> fn) {
  auto begin = source.c_str();
  auto end = begin + source.size() - 1;

  while (begin <= end && fn(*begin)) {
    begin++;
  }
  while (end >= begin && fn(*end)) {
    end--;
  }

  if (end > begin) {
    return source.substr(begin - source.c_str(), end - begin + 1);
  } else {
    return std::string();
  }
}

std::vector<std::string> Split(const std::string& source, const char delimiter) {
  size_t offset = 0;
  size_t position = std::string::npos;
  std::vector<std::string> result;

  while ((position = source.find(delimiter, offset)) != std::string::npos) {
    result.push_back(source.substr(offset, position - offset));

    offset = position + 1;
  }

  if (offset < source.size()) {
    result.push_back(source.substr(offset, source.size() - offset));
  }

  return result;
}

std::vector<uint8_t> FromHex(const std::string& hex) {
  if (hex.size() % 2 != 0) {
    throw std::runtime_error("invalid size of hex string");
  }

  std::vector<uint8_t> vec(hex.size() / 2);
  for (size_t i = 0; i < vec.size(); i++) {
    vec[i] = std::stoi(hex.substr(i * 2, 2), nullptr, 16);
  }
  return vec;
}

std::string ToHex(confmsg::CBuffer vec) {
  std::stringstream ss;
  for (size_t i = 0; i < vec.n; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(vec.p[i]);
  }
  return ss.str();
}

std::string Base64Url(confmsg::CBuffer data) {
  std::string b64 = base64_encode(data.p, data.n);
  std::replace(b64.begin(), b64.end(), '+', '-');
  std::replace(b64.begin(), b64.end(), '/', '_');
  return b64;
}

}  // namespace server
}  // namespace onnxruntime
