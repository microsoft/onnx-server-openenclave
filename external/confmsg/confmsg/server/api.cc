// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <stdexcept>
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>

#include "confmsg/server/api.h"
#include "confmsg/shared/exceptions.h"
#include "confmsg/shared/util.h"

#ifdef OE_BUILD_ENCLAVE
#include <openenclave/enclave.h>
#endif

#include "protocol_generated.h"

using namespace confmsg::protocol;

namespace confmsg {

Server::Server(const std::vector<uint8_t>& service_identifier, Server::Callback f, std::unique_ptr<KeyProvider>&& kp)
    : key_provider(std::move(kp)),
      service_identifier(service_identifier),
      request_callback(f) {
  InitCrypto();

  Randomize(nonce, NONCE_SIZE);
  MakePublicKeys();
  UpdateEvidence();
}

Server::~Server() {
  Wipe(nonce);
  Wipe(public_key);
  Wipe(public_signing_key);
}

bool Server::RefreshKey(bool sync_only) {
  bool refreshed = key_provider->RefreshKey(sync_only);
  if (refreshed) {
    MakePublicKeys();
    UpdateEvidence();
  }
  return refreshed;
}

void Server::UpdateEvidence() {
  // TODO skip if in simulation mode, otherwise we'll crash
  //      https://github.com/openenclave/openenclave/issues/3173
#ifdef OE_BUILD_ENCLAVE
  std::vector<uint8_t> quote;
  std::vector<uint8_t> collateral;
  GenerateQuote(quote, collateral);
  evidence.clear();
  evidence.emplace_back(EvidenceType::Quote, std::move(quote));
  // TODO enable again after GenerateQuote is fixed
  //evidence.emplace_back(EvidenceType::Collateral, std::move(collateral));
#endif
}

std::chrono::time_point<std::chrono::system_clock> Server::GetLastKeyRefresh() const {
  return key_provider->GetLastRefreshed();
}

void Server::MakePublicKeys() {
  if (key_provider->GetKeyType() == KeyType::Curve25519) {
    internal::MakePublicKeysCurve25519(key_provider->GetCurrentKey(), public_key, public_signing_key);
  } else {
    throw CryptoError("unsupported key type");
  }
}

void Server::HandleKeyRequest(const KeyRequest* r, uint8_t* out_msg, size_t* out_msg_size, size_t max_out_msg_size) {
  const flatbuffers::Vector<uint8_t>* client_nonce = r->nonce();
  *out_msg_size = 0;

  if (client_nonce == nullptr || client_nonce->size() != NONCE_SIZE) {
    throw CryptoError("invalid client nonce");
  }

  flatbuffers::FlatBufferBuilder builder;

  std::vector<uint8_t> msg;
  msg.insert(msg.end(), service_identifier.begin(), service_identifier.end());
  msg.insert(msg.end(), client_nonce->cbegin(), client_nonce->cend());
  std::vector<uint8_t> signature;
  internal::SignCurve25519(msg, key_provider->GetCurrentKey(), signature);

  auto nonce_fb = builder.CreateVector(nonce);
  auto service_identifier_fb = builder.CreateVector(service_identifier);
  auto public_key_fb = builder.CreateVector(public_key);
  auto server_ecpoint_fb = CreateECPoint(builder, PointFormat_Compressed, public_key_fb);
  auto server_signing_key_fb = builder.CreateVector(public_signing_key);
  auto server_signing_ecpoint_fb = CreateECPoint(builder, PointFormat_Compressed, server_signing_key_fb);
  auto signature_fb = builder.CreateVector(signature);
  auto service_identity_fb = CreateSignedServiceIdentity(builder, nonce_fb, service_identifier_fb, server_ecpoint_fb, server_signing_ecpoint_fb, signature_fb);
  uint32_t lifetime_hint = 0;
  uint32_t key_version = key_provider->GetCurrentKeyVersion();

  std::vector<flatbuffers::Offset<Evidence>> evidence_fbs;
  for (auto e : evidence) {
    auto ec_fb = builder.CreateVector(e.second);
    confmsg::protocol::EvidenceType et_fb;
    switch (e.first) {
      case Quote:
        et_fb = EvidenceType_Quote;
        break;
      case Collateral:
        et_fb = EvidenceType_Collateral;
        break;
      default:
        throw CryptoError("Unknown evidence type");
    }
    auto e_fb = CreateEvidence(builder, et_fb, ec_fb);
    evidence_fbs.push_back(e_fb);
  }

  auto authenticator_fb = builder.CreateVector(evidence_fbs);
  auto key_response_fb = CreateKeyResponse(builder, service_identity_fb, lifetime_hint, key_version, authenticator_fb);
  auto message_fb = CreateMessage(builder, Version_v1, Body_KeyResponse, key_response_fb.Union());
  builder.Finish(message_fb);

  WriteMessage(builder, out_msg, out_msg_size, max_out_msg_size);

#ifdef _DEBUG
  auto verifier = flatbuffers::Verifier(out_msg, *out_msg_size);
  if (!VerifyMessageBuffer(verifier)) {
    throw SerializationError("constructed flatbuffer invalid");
  }
#endif
}

void Server::HandleRequest(const Request* r, uint8_t* out_msg, size_t* out_msg_size, size_t max_out_msg_size) {
  uint32_t key_version = r->key_version();

  // If the client has previously talked to a backend with a newer key than
  // what we have ourselves, then refresh the key first. This will incur
  // a slight delay for this particular request.
  if (key_version > key_provider->GetCurrentKeyVersion()) {
    bool sync_only = true;
    RefreshKey(sync_only);
    if (key_version > key_provider->GetCurrentKeyVersion()) {
      throw CryptoError("key still older than client key version after refresh");
    }
  }

  CBuffer in_iv(r->iv()->Data(), r->iv()->size());
  CBuffer in_tag(r->tag()->Data(), r->tag()->size());
  CBuffer in_additional_data;
  CBuffer in_ciphertext(r->ciphertext()->data(), r->ciphertext()->size());
  const ECPoint* client_share = r->client_share();

  if (r->additional_data() != nullptr) {
    in_additional_data = CBuffer(r->additional_data()->Data(), r->additional_data()->size());
  }

  if (in_iv.n != IV_SIZE) {
    throw CryptoError("invalid iv size");
  }
  if (in_tag.n != TAG_SIZE) {
    throw CryptoError("invalid tag size");
  }
  if (client_share->xy()->size() != KEY_SIZE) {
    throw CryptoError("invalid client share");
  }

  thread_local static std::vector<uint8_t> shared_secret;
  thread_local static std::vector<uint8_t> symmetric_key;
  thread_local static std::vector<uint8_t> static_iv;
  static_iv.resize(IV_SIZE);
  CBuffer public_key(r->client_share()->xy()->Data(), client_share->xy()->size());
  internal::ComputeSharedSecretCurve25519(key_provider->GetKey(key_version), public_key, shared_secret);
  internal::DeriveSymmetricKey(shared_secret, false, symmetric_key, static_iv);

  std::vector<uint8_t> xor_iv(IV_SIZE);
  for (size_t i = 0; i < IV_SIZE; i++) {
    xor_iv[i] = static_iv[i] ^ in_iv.p[i];
  }

  thread_local static std::vector<uint8_t> application_data;
  application_data.resize(in_ciphertext.n);
  internal::Decrypt(symmetric_key, xor_iv, in_tag, in_ciphertext, in_additional_data, application_data);

  request_callback(application_data);

  std::vector<uint8_t> out_tag(TAG_SIZE);
  std::vector<uint8_t> out_ciphertext(application_data.size());

  internal::DeriveSymmetricKey(shared_secret, true, symmetric_key, static_iv);
  internal::Encrypt(symmetric_key, static_iv, application_data, nonce, out_ciphertext, out_tag);

  flatbuffers::FlatBufferBuilder builder(application_data.size() + 1024);

  bool key_outdated = key_provider->IsKeyOutdated(key_version);
  auto static_iv_fb = builder.CreateVector(static_iv);
  auto out_tag_fb = builder.CreateVector(out_tag);
  auto out_ciphertext_fb = builder.CreateVector(out_ciphertext);
  auto nonce_fb = builder.CreateVector(nonce);
  auto response_fb = CreateResponse(builder, key_outdated, static_iv_fb, out_tag_fb, nonce_fb, out_ciphertext_fb);
  auto message_fb = CreateMessage(builder, Version_v1, Body_Response, response_fb.Union());
  builder.Finish(message_fb);

  WriteMessage(builder, out_msg, out_msg_size, max_out_msg_size);

#ifdef _DEBUG
  auto verifier = flatbuffers::Verifier(out_msg, *out_msg_size);
  if (!VerifyMessageBuffer(verifier)) {
    throw SerializationError("constructed flatbuffer invalid");
  }
#endif
}

void Server::RespondToMessage(const uint8_t* in_msg, size_t in_msg_size, uint8_t* out_msg, size_t* out_msg_size, size_t max_out_msg_size) {
  auto verifier = flatbuffers::Verifier(in_msg, in_msg_size);
  if (!VerifyMessageBuffer(verifier)) {
    throw PayloadParseError("flatbuffer not valid");
  }

  const Message* in_msg_fb = GetMessage(in_msg);

  if (in_msg_fb->version() != Version_v1) {
    throw PayloadParseError("unsupported protocol version");
  }

  switch (in_msg_fb->body_type()) {
    case Body_KeyRequest:
      HandleKeyRequest(in_msg_fb->body_as_KeyRequest(), out_msg, out_msg_size, max_out_msg_size);
      break;
    case Body_Request:
      HandleRequest(in_msg_fb->body_as_Request(), out_msg, out_msg_size, max_out_msg_size);
      break;
    case Body_KeyResponse:
    case Body_Response:
      throw PayloadParseError("message not supposed to be handled by confmsg server");
      break;
    default:
      throw PayloadParseError("unhandled message type");
  }
}

#ifdef OE_BUILD_ENCLAVE
void Server::GenerateQuote(std::vector<uint8_t>& quote, std::vector<uint8_t>& collateral) {
  std::vector<uint8_t> hash;
  confmsg::internal::SHA256({public_key, service_identifier}, hash);

  uint8_t* report;
  size_t report_len = 0;
  oe_result_t res = oe_get_report(OE_REPORT_FLAGS_REMOTE_ATTESTATION,  // (use 0 for local attestation)
                                  hash.data(), hash.size(),            // Store hash in report_data field
                                  nullptr, 0,                          // opt_params is empty for remote attestation
                                  &report, &report_len);
  if (res != OE_OK) {
    throw std::runtime_error("oe_get_report failed: " + std::string(oe_result_str(res)));
  }

  //uint8_t* endorsements;
  //size_t endorsements_len = 0;
  //// oe_get_sgx_endorsements is not in public API
  //res = oe_get_sgx_endorsements(report, report_len,
  //                              &endorsements, &endorsements_len);
  //if (res != OE_OK) {
  //  oe_free_report(report);
  //  throw std::runtime_error("oe_get_sgx_endorsements failed: " + std::string(oe_result_str(res)));
  //}

  quote.assign(report, report + report_len);
  //collateral.assign(endorsements, endorsements + endorsements_len);
  oe_free_report(report);
  //oe_free_sgx_endorsements(endorsements);
  (void)collateral;
}
#endif

}  // namespace confmsg
