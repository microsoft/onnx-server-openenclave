// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "server/shared/constants.h"
#include "server/host/core/http_server.h"
#include "server/host/environment.h"
#include "server/host/json_handling.h"
#include "server/host/enclave_error.h"
#include "server/host/request_handler.h"

namespace onnxruntime {
namespace server {

#define GenerateErrorResponse(logger, http_error_code, app_error_code, message, context) \
  {                                                                                      \
    (context).response.insert("x-ms-request-id", ((context).request_id));                \
    if (!(context).client_request_id.empty()) {                                          \
      (context).response.insert("x-ms-client-request-id", (context).client_request_id);  \
    }                                                                                    \
    auto json_error_message = CreateJsonError((app_error_code), (message));              \
    logger->debug(json_error_message);                                                   \
    (context).response.result((http_error_code));                                        \
    (context).response.body() = json_error_message;                                      \
    (context).response.set(http::field::content_type, "application/json");               \
  }

void HandleRequest(/* in, out */ HttpContext& context,
                   RequestType request_type,
                   Enclave& enclave,
                   const std::shared_ptr<ServerEnvironment>& env) {
  auto logger = env->GetLogger(context.request_id);

  if (env->IsAuthEnabled()) {
    bool auth_ok = false;
    if (context.request.find(http::field::authorization) != context.request.end()) {
      auth_ok = context.request[http::field::authorization] == "Bearer " + env->GetAuthKey();
    }
    if (!auth_ok) {
      auto msg = "Invalid authorization key";
      GenerateErrorResponse(logger, http::status::unauthorized, -1, msg, context);
      return;
    }
  }

  if (!context.client_request_id.empty()) {
    logger->info("x-ms-client-request-id: [{}]", context.client_request_id);
  }

  // Forward request to enclave.
  auto body = context.request.body();
  const uint8_t* input_buf = (uint8_t*)body.c_str();
  size_t input_size = body.size();
  // TODO keep thread local static buffers around
  std::vector<uint8_t> output_vec(MAX_OUTPUT_SIZE);
  uint8_t* output_buf = output_vec.data();
  size_t output_size;
  try {
    enclave.HandleRequest(context.request_id, request_type, input_buf, input_size, output_buf, &output_size, env);
  } catch (EnclaveSDKError& exc) {
    auto message = exc.what();
    GenerateErrorResponse(logger, http::status::internal_server_error, -1, message, context);
    return;
  } catch (EnclaveCallError& exc) {
    auto message = exc.what();
    auto status = exc.status;
    GenerateErrorResponse(logger, http::status::bad_request, status, message, context);
    return;
  }

  // Build HTTP response
  std::string response_body((char*)output_buf, output_size);
  context.response.set(http::field::content_type, "application/octet-stream");
  context.response.insert("x-ms-request-id", context.request_id);
  if (!context.client_request_id.empty()) {
    context.response.insert("x-ms-client-request-id", context.client_request_id);
  }
  context.response.body() = response_body;
  context.response.result(http::status::ok);
};

}  // namespace server
}  // namespace onnxruntime
