// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <fstream>
#include <exception>

#include <confmsg/shared/util.h>
#include <confmsg/shared/crypto.h>

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

int main(int argc, const char** argv) {
  try {
    if (argc != 4) {
      std::cout << "Usage: " << argv[0] << " <key|key-file> <in-file> <out-file>" << std::endl;
      return 1;
    }

    std::string key_hex = argv[1];
    std::string in_filename = argv[2];
    std::string out_filename = argv[3];

    std::ifstream fkey(key_hex, std::ios::in | std::ios::binary);
    if (fkey) fkey >> key_hex;

    if (key_hex.size() != 2 * SYMMETRIC_KEY_SIZE) {
      std::cout << "Error: expected key size of " << SYMMETRIC_KEY_SIZE << " bytes (" << 2 * SYMMETRIC_KEY_SIZE << " characters)" << std::endl;
      return 4;
    }

    std::vector<uint8_t> key = confmsg::Hex2Buffer(key_hex);

    std::vector<uint8_t> model_hash = EncryptModelFile(key, in_filename, out_filename);
    std::cout << "model hash: " << confmsg::Buffer2Hex(model_hash) << std::endl;

    return 0;
  } catch (std::exception& ex) {
    std::cout << "Caught exception: " << ex.what() << std::endl;
    return 2;
  } catch (...) {
    std::cout << "Caught unknown exception" << std::endl;
    return 2;
  }
}