// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <vector>
#include <map>
#include <exception>

namespace onnxruntime {
namespace server {

class CurlError : public std::runtime_error {
 public:
  explicit CurlError(const std::string& msg, const std::string& url)
      : std::runtime_error("Curl error: " + msg + " (" + url + ")"), url(url) {}

  const std::string url;
};

class CurlHTTPError : public CurlError {
 public:
  explicit CurlHTTPError(const std::string& url, int status_code, std::map<std::string, std::string>&& headers)
      : CurlError("HTTP code " + std::to_string(status_code), url), status_code(status_code), headers(headers) {}

  const int status_code;
  const std::map<std::string, std::string> headers;
};

class CurlOtherError : public CurlError {
 public:
  explicit CurlOtherError(const std::string& msg, const std::string& url, int error_code)
      : CurlError(msg, url), error_code(error_code) {}

  const int error_code;
};

enum class HttpMethod { GET,
                        POST,
                        PUT,
                        DELETE };

void CurlInit(bool verbose = false);

void CurlCleanup();

std::string Curl(const std::string& url,
                 const std::map<std::string, std::string>& req_fields,
                 const std::map<std::string, std::string>& headers,
                 HttpMethod method = HttpMethod::GET);

std::string FetchOAuthToken(const std::string& authority_url,
                            const std::string& resource,
                            const std::string& app_id,
                            const std::string& app_password);

class HTTPClient {
 public:
  explicit HTTPClient(const std::string& app_id = "", const std::string& app_password = "");

  std::string Request(const std::string& url,
                      const std::string& body = "",
                      const std::map<std::string, std::string>& headers = {},
                      HttpMethod method = HttpMethod::GET);

  std::string Request(const std::string& url,
                      const std::map<std::string, std::string>& req_fields,
                      const std::map<std::string, std::string>& headers = {},
                      HttpMethod method = HttpMethod::POST);

 private:
  std::string app_id_;
  std::string app_password_;
  std::string token_;
};

}  // namespace server
}  // namespace onnxruntime