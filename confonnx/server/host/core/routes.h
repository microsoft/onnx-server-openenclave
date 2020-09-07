// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <boost/beast/http.hpp>

#include "context.h"

namespace onnxruntime {
namespace server {

namespace http = boost::beast::http;  // from <boost/beast/http.hpp>

using HandlerFn = std::function<void(HttpContext&)>;
using ErrorFn = std::function<void(HttpContext&)>;

// This class maintains two lists of regex -> function lists. One for POST requests and one for GET requests
// If the incoming URL could match more than one regex, the first one will win.
class Routes {
 public:
  Routes() = default;
  ErrorFn on_error;
  bool RegisterController(http::verb method, const std::string& url_pattern, const HandlerFn& controller);
  bool RegisterErrorCallback(const ErrorFn& controller);

  http::status ParseUrl(http::verb method,
                        const std::string& url,
                        /* out */ HandlerFn& func) const;

 private:
  std::vector<std::pair<std::string, HandlerFn>> post_fn_table;
  std::vector<std::pair<std::string, HandlerFn>> get_fn_table;
};

}  //namespace server
}  // namespace onnxruntime
