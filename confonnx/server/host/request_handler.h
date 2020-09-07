// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "server/host/core/http_server.h"
#include "server/host/json_handling.h"
#include "server/host/enclave.h"
#include "server/shared/request_type.h"

namespace onnxruntime {
namespace server {

namespace beast = boost::beast;
namespace http = beast::http;

void BadRequest(HttpContext& context, const std::string& error_message);

void HandleRequest(/* in, out */ HttpContext& context,
                   RequestType request_type,
                   Enclave& enclave,
                   const std::shared_ptr<ServerEnvironment>& env);

}  // namespace server
}  // namespace onnxruntime
