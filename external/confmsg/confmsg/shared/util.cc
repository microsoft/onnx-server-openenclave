// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

#include "flatbuffers/flatbuffers.h"
#include "shared/exceptions.h"

#include "util.h"

namespace confmsg {

std::default_random_engine rng(std::chrono::system_clock::now().time_since_epoch().count());
std::uniform_int_distribution<int> rng_dist(0, 255);

void Randomize(std::vector<uint8_t>& vec, size_t sz) {
  vec.resize(sz);
  for (size_t i = 0; i < sz; i++) {
    vec[i] = static_cast<uint8_t>(rng_dist(rng));
  }
}

void Wipe(std::vector<uint8_t>& vec) {
  std::fill(vec.begin(), vec.end(), 0);
  vec.clear();
}

void WriteMessage(flatbuffers::FlatBufferBuilder& builder, uint8_t* msg, size_t* msg_size, size_t max_msg_size) {
  uint8_t* fb = builder.GetBufferPointer();
  auto fb_size = builder.GetSize();

  if (fb_size > max_msg_size) {
    throw OutputBufferTooSmallError("message too large (" +
                                    std::to_string(fb_size) + " > " + std::to_string(max_msg_size) + ")");
  }

  *msg_size = fb_size;
  std::memcpy(msg, fb, fb_size);
}

std::string Buffer2Hex(CBuffer b) {
  std::stringstream rss;
  for (size_t i = 0; i < b.n; i++) {
    rss << std::hex << std::setw(2) << std::setfill('0') << (unsigned)b.p[i];
  }
  return rss.str();
}

std::vector<uint8_t> Hex2Buffer(const std::string& s) {
  if (s.length() % 2 != 0) {
    throw std::runtime_error("number of characters must be even");
  }

  size_t n = s.length() / 2;
  std::vector<uint8_t> r(n);

  for (size_t i = 0; i < s.length(); i += 2) {
    std::string ss = s.substr(i, 2);
    r[i / 2] = std::strtol(ss.c_str(), nullptr, 16);
  }

  return r;
}

}  // namespace confmsg