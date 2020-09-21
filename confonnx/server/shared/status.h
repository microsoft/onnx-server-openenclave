// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

namespace onnxruntime {
namespace server {

enum EnclaveCallStatus {
  SUCCESS = 0,
  UNKNOWN_ERROR = 1,
  CRYPTO_ERROR = 2,
  MODEL_LOADING_ERROR = 3,
  SESSION_ALREADY_INITIALIZED_ERROR = 4,
  SESSION_INITIALIZATION_ERROR = 5,
  PAYLOAD_PARSE_ERROR = 6,
  INFERENCE_ERROR = 7,
  OUTPUT_BUFFER_TOO_SMALL_ERROR = 8,
  OUTPUT_SERIALIZATION_ERROR = 9,
  ATTESTATION_ERROR = 10,
  KEY_REFRESH_ERROR = 11,
  UNKNOWN_REQUEST_TYPE_ERROR = 12,
  MODEL_ALREADY_INITIALIZED_ERROR = 13
};

}  // namespace server
}  // namespace onnxruntime
