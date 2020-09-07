// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include <confmsg/shared/crypto.h>

#include "test/helpers/crypto_helpers.h"

namespace onnxruntime {
namespace server {
namespace test {

void Hex2Bytes(const std::string& str, std::vector<uint8_t>& out, size_t size) {
  if (str.size() != size * 2)
    throw std::runtime_error("incompatible string and buffer sizes");

  for (size_t i = 0; i < size; i++) {
    out[i] = std::stoi(str.substr(i * 2, 2), nullptr, 16);
  }
}

std::string Bytes2Hex(const std::vector<uint8_t>& bytes) {
  std::stringstream ss;
  for (size_t i = 0; i < bytes.size(); i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
  }
  return ss.str();
}

void CheckSecret(std::unique_ptr<confmsg::KeyProvider>& kp, const std::string& secret) {
  size_t keysize = kp->GetCurrentKey().size();

  if (keysize != secret.size() / 2) {
    throw std::runtime_error("Key length mismatch");
  }

  std::vector<uint8_t> buf(keysize);
  Hex2Bytes(secret, buf, keysize);
  for (size_t i = 0; i < keysize; i++) {
    if (kp->GetCurrentKey()[i] != buf[i]) {
      throw std::runtime_error("Secrets do not match.");
    }
  }
}

void CheckSecret(std::unique_ptr<confmsg::KeyProvider>& kp1, std::unique_ptr<confmsg::KeyProvider>& kp2) {
  size_t keysize = kp1->GetCurrentKey().size();

  if (keysize != kp2->GetCurrentKey().size()) {
    throw std::runtime_error("Key length mismatch");
  }

  for (size_t i = 0; i < keysize; i++) {
    if (kp1->GetCurrentKey()[i] != kp2->GetCurrentKey()[i]) {
      throw std::runtime_error("Secrets do not match.");
    }
  }
}

std::vector<uint8_t> HashModelFile(const std::string& in_filename) {
  std::ifstream fin(in_filename, std::ios::in | std::ios::binary);
  if (!fin) throw std::runtime_error("Can't open file: " + in_filename);
  fin.seekg(0, std::ios_base::end);
  size_t buf_sz = fin.tellg();
  fin.seekg(0, std::ios_base::beg);
  uint8_t buffer[buf_sz];
  fin.read((char*)buffer, buf_sz);
  fin.close();

  std::vector<uint8_t> model_hash;
  confmsg::internal::SHA256(confmsg::CBuffer(buffer, buf_sz), model_hash);
  return model_hash;
}

std::vector<uint8_t> EncryptModelFile(const std::vector<uint8_t>& key, const std::string& in_filename, const std::string& out_filename) {
  std::ifstream fin(in_filename, std::ios::in | std::ios::binary);
  if (!fin) throw std::runtime_error("Can't open file: " + in_filename);
  fin.seekg(0, std::ios_base::end);
  size_t buf_sz = fin.tellg();
  fin.seekg(0, std::ios_base::beg);
  uint8_t buffer[buf_sz];
  fin.read((char*)buffer, buf_sz);
  fin.close();

  confmsg::CBuffer plain(buffer, buf_sz);
  std::vector<uint8_t> iv(IV_SIZE, 0);
  std::vector<uint8_t> cipher, tag;

  confmsg::InitCrypto();
  confmsg::internal::Encrypt(key, iv, plain, confmsg::CBuffer(), cipher, tag);

  std::ofstream fout(out_filename, std::ios::out | std::ios::binary);
  if (!fout) throw std::runtime_error("Can't open file: " + in_filename);
  fout.write((char*)cipher.data(), cipher.size());
  fout.write((char*)tag.data(), tag.size());
  fout.close();

  std::vector<uint8_t> model_hash;
  confmsg::internal::SHA256({cipher, tag}, model_hash);
  return model_hash;
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
