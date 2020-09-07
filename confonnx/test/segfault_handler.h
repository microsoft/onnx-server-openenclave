// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

namespace {
void PrintStacktrace(int sig) {
  size_t max_depth = 50;
  void* addresses[max_depth];
  size_t size;

  size = backtrace(addresses, max_depth);
  backtrace_symbols_fd(addresses, size, STDERR_FILENO);

  signal(sig, SIG_DFL);
  kill(getpid(), sig);
}
}  // namespace

namespace onnxruntime {
namespace server {
namespace test {

void InstallSegFaultHandler() {
  signal(SIGSEGV, PrintStacktrace);
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
