// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <stdexcept>

namespace onnxruntime {
namespace server {

class Error : public std::exception {
 private:
  std::string msg;

 public:
  explicit Error(const std::string& msg) : msg(msg) {}

  const char* what() const noexcept override {
    return msg.c_str();
  }
};

class UnknownRequestTypeError : public Error {
 public:
  explicit UnknownRequestTypeError(const std::string& msg) : Error(msg) {}
};

class ModelAlreadyInitializedError : public Error {
 public:
  explicit ModelAlreadyInitializedError() : Error("") {}
};

class PayloadParseError : public Error {
 public:
  explicit PayloadParseError(const std::string& msg) : Error(msg) {}
};

class SerializationError : public Error {
 public:
  explicit SerializationError(const std::string& msg) : Error(msg) {}
};

class InferenceError : public Error {
 public:
  explicit InferenceError(const std::string& msg) : Error(msg) {}
};

}  // namespace server
}  // namespace onnxruntime