// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stdexcept>
#include <openenclave/host.h>

#include "server/shared/status.h"

namespace onnxruntime {
namespace server {

class EnclaveSDKError : public std::runtime_error {
 public:
  explicit EnclaveSDKError(oe_result_t result);

  static void Check(oe_result_t result);
};

class EnclaveCallError : public std::runtime_error {
 public:
  explicit EnclaveCallError(int status);

  static void Check(int status);

  const EnclaveCallStatus status;
};

}  // namespace server
}  // namespace onnxruntime
