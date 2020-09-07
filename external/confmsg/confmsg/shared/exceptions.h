// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <stdexcept>

namespace confmsg {

class Error : public std::exception {
 private:
  std::string msg;

 public:
  Error(const std::string& msg) : msg(msg) {}

  const char* what() const noexcept override {
    return msg.c_str();
  }
};

class CryptoError : public Error {
 public:
  CryptoError(const std::string& msg) : Error(msg) {}
};

class PayloadParseError : public Error {
 public:
  PayloadParseError(const std::string& msg) : Error(msg) {}
};

class SerializationError : public Error {
 public:
  SerializationError(const std::string& msg) : Error(msg) {}
};

class OutputBufferTooSmallError : public SerializationError {
 public:
  OutputBufferTooSmallError(const std::string& msg) : SerializationError(msg) {}
};

class AttestationError : public Error {
 public:
  AttestationError(const std::string& msg) : Error(msg) {}
};

class KeyRefreshError : public Error {
 public:
  KeyRefreshError(const std::string& msg) : Error(msg) {}
};

}  // namespace confmsg