// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <cstdlib>
#include <stdexcept>

#include <openenclave/host.h>

#include "gtest/gtest.h"
#include "test/test_config.h"
#include "test/helpers/helpers.h"

#include "server/shared/curl_helper.h"
#include "server/host/enclave_error.h"

#include "test_u.h"

namespace onnxruntime {
namespace server {
namespace test {

TEST(Curl, BasicHTTPSNoEnclave) {
  bool verbose = GetVerbose();
  CurlInit(verbose);
  std::string response = Curl("https://www.microsoft.com/en-us/", {}, {});
  if (response.empty()) {
    throw std::logic_error("Curl response empty");
  }
  CurlCleanup();
}

TEST(Curl, BasicHTTPSEnclave) {
  bool verbose = GetVerbose();
  uint32_t enclave_flags = OE_ENCLAVE_FLAG_DEBUG;

  oe_enclave_t* enclave;
  EnclaveSDKError::Check(oe_create_test_enclave(
      TEST_ENCLAVE_PATH.c_str(), OE_ENCLAVE_TYPE_SGX, enclave_flags, nullptr, 0, &enclave));

  std::string url = "https://www.microsoft.com/en-us/";
  EnclaveSDKError::Check(TestEnclaveCallCurl(enclave, url.c_str(), verbose));

  EnclaveSDKError::Check(oe_terminate_enclave(enclave));
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
