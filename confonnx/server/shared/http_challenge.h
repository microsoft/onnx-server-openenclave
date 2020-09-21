// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <map>

namespace onnxruntime {
namespace server {

class HttpChallenge {
 public:
  static bool isBearerChallenge(const std::string& challenge);

  explicit HttpChallenge(const std::string& challenge);

  std::string scheme() const;
  std::string authority() const;
  std::string resource() const;

 private:
  std::string _scheme;
  std::map<std::string, std::string> _parameters;

  void parseChallenge(const std::string& challenge);
};

}  // namespace server
}  // namespace onnxruntime
