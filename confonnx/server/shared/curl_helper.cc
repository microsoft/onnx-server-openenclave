// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>
#include <map>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#ifdef OE_BUILD_ENCLAVE
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_crl.h>
#include <mbedtls/ssl.h>
#endif

#include "ca_certificates.h"

#include "server/shared/util.h"
#include "server/shared/http_challenge.h"
#include "server/shared/curl_helper.h"

namespace onnxruntime {
namespace server {

static bool verbose_curl;

void CurlInit(bool verbose) {
  verbose_curl = verbose;
  if (verbose_curl) {
    std::cout << "Global init curl..." << std::endl;
  }
#ifdef OE_BUILD_ENCLAVE
  curl_global_sslset(CURLSSLBACKEND_MBEDTLS, nullptr, nullptr);
#endif
  curl_global_init(CURL_GLOBAL_ALL);
  if (verbose_curl) {
    std::cout << "Global init curl done" << std::endl;
  }
}

void CurlCleanup() {
  if (verbose_curl) {
    std::cout << "Global cleanup curl..." << std::endl;
  }
  curl_global_cleanup();
  if (verbose_curl) {
    std::cout << "Global cleanup curl done" << std::endl;
  }
}

static size_t CurlWriteCallback(void* ptr, size_t size, size_t nmemb, std::string* s) {
  s->append(reinterpret_cast<char*>(ptr), size * nmemb);
  return size * nmemb;
}

static size_t CurlHeaderCallback(char* ptr, size_t size, size_t nmemb, std::map<std::string, std::string>* s) {
  // Each header field is written to CURLOPT_HEADERDATA (see Curl() below).
  std::string header(ptr, size * nmemb);
  auto loc = header.find(':');
  if (loc != std::string::npos) {
    std::string name = Trim(header.substr(0, loc));
    std::string value = Trim(header.substr(loc + 1));
    s->emplace(name, value);
  }
  return size * nmemb;
}

#ifdef OE_BUILD_ENCLAVE
static CURLcode sslctxfunc(CURL* curl, void* sslctx, void* parm) {
  (void)parm;
  (void)curl;
  mbedtls_ssl_config* cfg = reinterpret_cast<mbedtls_ssl_config*>(sslctx);

  int r = mbedtls_x509_crt_parse(cfg->ca_chain,
                                 _etc_ssl_certs_ca_certificates_crt,
                                 _etc_ssl_certs_ca_certificates_crt_len);

  return r != 0 ? CURLE_ABORTED_BY_CALLBACK : CURLE_OK;
}
#endif

std::string Curl(const std::string& url,
                 const std::map<std::string, std::string>& req_fields,
                 const std::map<std::string, std::string>& headers,
                 HttpMethod method) {
  std::string response;
  std::map<std::string, std::string> response_headers;

  CURL* curl = curl_easy_init();
  if (curl == nullptr) {
    throw std::runtime_error("Error initializing curl");
  }

  std::stringstream req_s;
  if (req_fields.count("")) {
    req_s << req_fields.at("");
  } else {
    bool first = true;
    for (auto item : req_fields) {
      if (!first) {
        req_s << "&";
      }
      char* escaped = curl_easy_escape(curl, item.second.c_str(), item.second.size());
      if (escaped) {
        req_s << item.first << "=" << escaped;
        curl_free(escaped);
      } else {
        throw std::runtime_error("curl escape error for: " + item.second);
      }
      first = false;
    }
  }

  std::string req = req_s.str();

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  if (!req.empty()) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.size());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.c_str());
  }
  if (method == HttpMethod::GET && !req.empty()) {
    method = HttpMethod::POST;
  }
  if (method == HttpMethod::GET) {
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  } else if (method == HttpMethod::POST) {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
  } else if (method == HttpMethod::PUT) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  } else if (method == HttpMethod::DELETE) {
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  }
  curl_easy_setopt(curl, CURLOPT_VERBOSE, verbose_curl ? 1L : 0L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

#ifdef OE_BUILD_ENCLAVE
  curl_easy_setopt(curl, CURLOPT_CAINFO, NULL);
  curl_easy_setopt(curl, CURLOPT_CAPATH, NULL);
  curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
  curl_easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, sslctxfunc);
#endif

  struct curl_slist* curl_headers = nullptr;
  for (auto& item : headers) {
    std::string header = item.first + ": " + item.second;
    curl_headers = curl_slist_append(curl_headers, header.c_str());
  }
  if (curl_headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
  }
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, CurlHeaderCallback);

  auto result = curl_easy_perform(curl);
  curl_slist_free_all(curl_headers);

  int64_t response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  curl_easy_cleanup(curl);

  if (verbose_curl) {
    std::cout << "CURL: URL=" << url << std::endl;
    std::cout << "CURL: Request body=" << req << std::endl;
    std::cout << "CURL: Response body=" << response << std::endl;
    std::cout << "CURL: Response code=" << response_code << std::endl;
  }

  if (result != CURLE_OK) {
    const char* err_s = curl_easy_strerror(result);
    throw CurlOtherError(url, err_s, result);
  }
  if (response_code >= 400) {
    throw CurlHTTPError(url, response_code, std::move(response_headers));
  }

  return response;
}

std::string FetchOAuthToken(const std::string& authority_url,
                            const std::string& resource,
                            const std::string& app_id,
                            const std::string& app_password) {
  // clang-format off
  std::string response_str = Curl(authority_url + "/oauth2/token", {
    {"grant_type", "client_credentials"},
    {"client_id", app_id},
    {"client_secret", app_password},
    {"resource", resource}
  }, {
    {"Content-Type", "application/x-www-form-urlencoded"},
    {"Accept", "application/json"}
  });
  // clang-format on

  auto response = nlohmann::json::parse(response_str);

  if (response["resource"] != resource || response["token_type"] != "Bearer") {
    throw std::runtime_error("Unexpected token type");
  }

  std::string access_token = response["access_token"];

  return access_token;
}

HTTPClient::HTTPClient(const std::string& app_id, const std::string& app_password)
    : app_id_(app_id), app_password_(app_password) {}

std::string HTTPClient::Request(const std::string& url,
                                const std::string& body,
                                const std::map<std::string, std::string>& headers,
                                HttpMethod method) {
  return Request(url, {{"", body}}, headers, method);
}

static std::string to_lower(const std::string& s) {
  std::string r = s;
  std::transform(r.begin(), r.end(), r.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return r;
}

static const std::string* find_header(const std::string& name, const std::map<std::string, std::string>& headers) {
  std::string ln = to_lower(name);
  for (auto& e : headers) {
    if (to_lower(e.first) == ln) {
      return &e.second;
    }
  }
  return nullptr;
}

std::string HTTPClient::Request(const std::string& url,
                                const std::map<std::string, std::string>& req_fields,
                                const std::map<std::string, std::string>& headers,
                                HttpMethod method) {
  try {
    if (!token_.empty()) {
      auto headers_with_token = headers;
      headers_with_token["Authorization"] = "Bearer " + token_;
      return Curl(url, req_fields, headers_with_token, method);
    } else {
      return Curl(url, req_fields, headers, method);
    }
  } catch (CurlHTTPError& err) {
    const std::string* challenge = find_header("WWW-Authenticate", err.headers);
    if (!challenge || !HttpChallenge::isBearerChallenge(*challenge)) {
      throw;
    }
    HttpChallenge ch(*challenge);
    token_ = FetchOAuthToken(ch.authority(), ch.resource(), app_id_, app_password_);
    auto headers_with_token = headers;
    headers_with_token["Authorization"] = "Bearer " + token_;
    return Curl(url, req_fields, headers_with_token, method);
  }
}

}  // namespace server
}  // namespace onnxruntime
