// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>

#include "gtest/gtest.h"
#include "test/test_config.h"
#include "test/segfault_handler.h"

using namespace confmsg::test;

GTEST_API_ int main(int argc, char** argv) {
  int status = 0;

  InstallSegFaultHandler();

  if (!(argc == 2 && std::string(argv[1]) == "--gtest_list_tests")) {
    std::cout << "Command line: ";
    for (int i = 0; i < argc; i++) {
      std::cout << argv[i] << " ";
    }
    std::cout << std::endl;
  }

  try {
    // Parses CLI args and removes recognized ones from argc/argv.
    ::testing::InitGoogleTest(&argc, argv);

    if (argc > 1) {
      ENCLAVE_PATH = argv[1];
    } else {
      // Allows CMake to query tests. Running tests would fail here.
    }

    status = RUN_ALL_TESTS();
  } catch (const std::exception& ex) {
    std::cerr << ex.what();
    status = -1;
  }

  return status;
}
