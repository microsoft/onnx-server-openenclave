// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stddef.h>

#include <functional>
#include <chrono>

#include <confmsg/shared/buffer.h>
#include <confmsg/shared/crypto.h>
#include <confmsg/shared/util.h>
#include <confmsg/shared/keyprovider.h>
#include <confmsg/shared/exceptions.h>

namespace flatbuffers {
template <typename T>
class Vector;
}

namespace confmsg {
namespace protocol {
struct KeyRequest;
struct Request;
}  // namespace protocol

class Server {
 public:
  typedef std::function<void(std::vector<uint8_t>&)> Callback;

  Server(const std::vector<uint8_t>& service_identifier, Callback f, std::unique_ptr<KeyProvider>&& kp);
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;
  Server(Server&&) = default;
  ~Server();

  bool RefreshKey(bool sync_only);

  std::chrono::time_point<std::chrono::system_clock> GetLastKeyRefresh() const;

  void RespondToMessage(const uint8_t* in_msg, size_t in_msg_size, uint8_t* out_msg, size_t* out_msg_size, size_t max_out_msg_size);

  const std::vector<uint8_t>& PublicKey(void) { return public_key; }

  enum EvidenceType { Quote,
                      Collateral };

 private:
  std::unique_ptr<KeyProvider> key_provider;

  std::vector<uint8_t> nonce;
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> public_signing_key;
  std::vector<uint8_t> service_identifier;
  std::vector<std::pair<EvidenceType, std::vector<uint8_t>>> evidence;

  Callback request_callback;

  void UpdateEvidence();
  void MakePublicKeys();
  void HandleKeyRequest(const protocol::KeyRequest* r, uint8_t* out_msg, size_t* out_msg_size, size_t max_out_msg_size);
  void HandleRequest(const protocol::Request* r, uint8_t* out_msg, size_t* out_msg_size, size_t max_out_msg_size);
#ifdef OE_BUILD_ENCLAVE
  void GenerateQuote(std::vector<uint8_t>& quote, std::vector<uint8_t>& collateral);
#endif
};

}  // namespace confmsg