// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <cassert>
#include <string>
#include "server/host/enclave_error.h"

namespace onnxruntime {
namespace server {

EnclaveSDKError::EnclaveSDKError(oe_result_t result)
    : std::runtime_error(oe_result_str(result)) {
  assert(result != OE_OK);
}

void EnclaveSDKError::Check(oe_result_t result) {
  if (result != OE_OK) {
    throw EnclaveSDKError(result);
  }
}

EnclaveCallError::EnclaveCallError(int status)
    : std::runtime_error("Code: " + std::to_string(status)), status(EnclaveCallStatus(status)) {
  assert(status != 0);
}

void EnclaveCallError::Check(int status) {
  if (status != 0) {
    throw EnclaveCallError(status);
  }
}

}  // namespace server
}  // namespace onnxruntime
