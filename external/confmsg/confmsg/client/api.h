// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stddef.h>

#include <functional>

#include <confmsg/shared/buffer.h>
#include <confmsg/shared/crypto.h>
#include <confmsg/shared/keyprovider.h>

namespace confmsg {

namespace protocol {
struct KeyResponse;
struct Response;
}  // namespace protocol

class Client {
 public:
  class Result {
   public:
    static Result CreateKeyResponse() {
      Result r;
      r.type = Type::KeyResponse;
      return r;
    }

    static Result CreateResponse(std::vector<uint8_t>&& payload, bool key_outdated) {
      Result r;
      r.type = Type::Response;
      r.payload = std::move(payload);
      r.key_outdated = key_outdated;
      return r;
    }

    bool IsKeyResponse() const { return type == Type::KeyResponse; }
    bool IsResponse() const { return type == Type::Response; }

    bool IsKeyOutdated() const {
      if (!IsResponse()) {
        throw std::logic_error("IsKeyOutdated() can only be called on non-key responses");
      }
      return key_outdated;
    }

    const std::vector<uint8_t>& GetPayload() {
      if (!IsResponse()) {
        throw std::logic_error("GetPayload() can only be called on non-key responses");
      }
      return payload;
    }

   private:
    enum class Type { KeyResponse,
                      Response };

    Type type;
    std::vector<uint8_t> payload;
    bool key_outdated;
  };

  Client(std::unique_ptr<KeyProvider>&& kp,
         const std::string& expected_enclave_signing_key_pem,
         const std::vector<uint8_t>& expected_enclave_hash,
         const std::vector<uint8_t>& expected_service_identifier,
         bool verbose = false);
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  Client(Client&&) = default;
  ~Client();

  Result HandleMessage(const uint8_t* msg, size_t msg_size);
  void MakeKeyRequest(uint8_t* msg, size_t* msg_size, size_t max_msg_size);
  void MakeRequest(CBuffer plaintext, uint8_t* msg, size_t* msg_size, size_t max_msg_size);

 private:
  std::unique_ptr<KeyProvider> key_provider;

  uint32_t key_version;
  std::vector<uint8_t> nonce;
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> in_symmetric_key;
  std::vector<uint8_t> out_symmetric_key;
  std::vector<uint8_t> static_iv;
  std::vector<uint8_t> dynamic_iv;
  std::vector<uint8_t> server_nonce;
  std::string expected_enclave_signing_key_pem;
  std::vector<uint8_t> expected_enclave_hash;
  std::vector<uint8_t> expected_service_identifier;

  bool verbose;

  Client::Result HandleKeyResponse(const protocol::KeyResponse* r);
  Client::Result HandleResponse(const protocol::Response* r);
  void VerifyQuote(CBuffer quote, CBuffer collateral, CBuffer service_public_key, CBuffer service_identifier);
};

}  // namespace confmsg