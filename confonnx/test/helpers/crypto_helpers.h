// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>

#include <confmsg/shared/keyprovider.h>

namespace onnxruntime {
namespace server {
namespace test {

void Hex2Bytes(const std::string& str, std::vector<uint8_t>& out, size_t size);

std::string Bytes2Hex(const std::vector<uint8_t>& bytes);

void CheckSecret(std::unique_ptr<confmsg::KeyProvider>& kp, const std::string& secret);

void CheckSecret(std::unique_ptr<confmsg::KeyProvider>& kp1, std::unique_ptr<confmsg::KeyProvider>& kp2);

std::vector<uint8_t> HashModelFile(const std::string& in_filename);

std::vector<uint8_t> EncryptModelFile(const std::vector<uint8_t>& key, const std::string& in_filename, const std::string& out_filename);

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
