// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <fcntl.h>

#include "test/helpers/protobuf_parsing_utils.h"

namespace onnxruntime {
namespace server {
namespace test {

template <typename T>
T LoadProtobufFromFile(const std::string& path) {
  std::cout << "Loading " << path << std::endl;
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    throw std::invalid_argument("unable to open protobuf file");
  }

  google::protobuf::io::FileInputStream f(fd);
  f.SetCloseOnDelete(true);
  T pb;
  if (!pb.ParseFromZeroCopyStream(&f)) {
    throw std::invalid_argument("Failed to load protobuf because parsing failed.");
  }
  return pb;
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
