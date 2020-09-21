// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "google/protobuf/message.h"

namespace pb = google::protobuf;

namespace onnxruntime {
namespace server {
namespace test {

bool ProtobufCompare(const pb::Message& a, const pb::Message& b);

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
