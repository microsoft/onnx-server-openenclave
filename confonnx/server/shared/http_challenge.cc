// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <vector>
#include <memory>

#include "server/shared/util.h"
#include "server/shared/http_challenge.h"

using namespace std;

namespace onnxruntime {
namespace server {

static const std::string BearerPrefix("Bearer ");

static const std::string Authorization("authorization");
static const std::string AuthorizationUri("authorization_uri");
static const std::string Resource("resource");

bool HttpChallenge::isBearerChallenge(const std::string& challenge) {
  return challenge.find(BearerPrefix) != std::string::npos;
}

HttpChallenge::HttpChallenge(const std::string& challenge) {
  if (challenge.empty()) throw invalid_argument("challenge");
  parseChallenge(challenge);
}

std::string HttpChallenge::scheme() const {
  return _scheme;
}

std::string HttpChallenge::authority() const {
  auto result = _parameters.find(Authorization);

  if (result != _parameters.end()) {
    return result->second;
  }

  result = _parameters.find(AuthorizationUri);

  if (result != _parameters.end()) {
    return result->second;
  }

  throw std::runtime_error("authority not found");
}

std::string HttpChallenge::resource() const {
  auto result = _parameters.find(Resource);

  if (result != _parameters.end()) {
    return result->second;
  }

  throw std::runtime_error("resource not found");
}

void HttpChallenge::parseChallenge(const std::string& challenge) {
  if (challenge.empty()) {
    throw invalid_argument("challenge");
  }

  auto TrimmedChallenge = Trim(challenge);

  // A correct challenge now consists of "scheme [param=value, param=value]
  size_t offset = 0;
  size_t position = std::string::npos;

  if ((position = TrimmedChallenge.find(' ', offset)) != std::string::npos) {
    // found a space character, so offset is now the length of the scheme
    _scheme = TrimmedChallenge.substr(0, position);

    // now parse the parameters
    TrimmedChallenge = TrimmedChallenge.substr(position + 1, TrimmedChallenge.size() - position - 1);

    // Split the Trimmed challenge into a set of name=value strings that
    // are comma separated. The value fields are expected to be within
    // quotation characters that are stripped here.
    vector<std::string> pairs = Split(TrimmedChallenge, ',');

    if (!pairs.empty()) {
      // Process the name=value strings
      for (std::string key_val_pair : pairs) {
        vector<std::string> pair = Split(key_val_pair, '=');

        if (pair.size() == 2) {
          // We have a key and a value, now need to Trim and decode
          auto key = Trim(Trim(pair[0]), '\"');
          auto value = Trim(Trim(pair[1]), '\"');

          if (!key.empty()) {
            _parameters[key] = value;
          }
        }
      }
    }

    // Minimum set of parameters
    if (_parameters.empty()) {
      throw invalid_argument("Invalid challenge parameters");
    }
  } else {
    throw invalid_argument("challenge");
  }
}

}  // namespace server
}  // namespace onnxruntime
