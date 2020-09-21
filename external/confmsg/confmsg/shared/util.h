// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stddef.h>

#include "buffer.h"

namespace flatbuffers {
class FlatBufferBuilder;
}

namespace confmsg {

void Randomize(std::vector<uint8_t>& vec, size_t sz);

void Wipe(std::vector<uint8_t>& vec);

void WriteMessage(flatbuffers::FlatBufferBuilder& builder, uint8_t* msg, size_t* msg_size, size_t max_msg_size);

std::string Buffer2Hex(CBuffer b);

std::vector<uint8_t> Hex2Buffer(const std::string& s);

}  // namespace confmsg