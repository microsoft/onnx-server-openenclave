// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <stdexcept>
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>

#ifdef HAVE_OE_HOST_VERIFY
#include <openenclave/host_verify.h>
#endif

#include "confmsg/client/api.h"
#include "confmsg/shared/util.h"
#include "confmsg/shared/exceptions.h"
#include "protocol_generated.h"

using namespace confmsg::protocol;

namespace {
std::string COK = "\033[92m";
std::string CWARN = "\033[93m";
std::string CEND = "\033[0m";
}  // namespace

namespace confmsg {

Client::Client(std::unique_ptr<KeyProvider>&& kp,
               const std::string& expected_enclave_signing_key_pem,
               const std::vector<uint8_t>& expected_enclave_hash,
               const std::vector<uint8_t>& expected_service_identifier,
               bool verbose) : key_provider(std::move(kp)), key_version(-1), expected_enclave_signing_key_pem(expected_enclave_signing_key_pem), expected_enclave_hash(expected_enclave_hash), expected_service_identifier(expected_service_identifier), verbose(verbose) {
  if (key_provider == nullptr) {
    throw std::invalid_argument("key_provider == null");
  }
  InitCrypto();

  Randomize(nonce, NONCE_SIZE);
  Randomize(dynamic_iv, IV_SIZE);

  internal::MakePublicKeyCurve25519(key_provider->GetCurrentKey(), public_key);
}

Client::~Client() {
  Wipe(nonce);
  Wipe(public_key);
  Wipe(in_symmetric_key);
  Wipe(out_symmetric_key);
  Wipe(static_iv);
  Wipe(dynamic_iv);
  Wipe(server_nonce);
}

Client::Result Client::HandleKeyResponse(const protocol::KeyResponse* r) {
  const SignedServiceIdentity* sid = r->id();
  // TODO use lifetime hint
  //uint32_t lth = r->lifetime_hint();
  const flatbuffers::Vector<flatbuffers::Offset<Evidence>>* auth = r->authenticator();

  CBuffer service_identifier(sid->service_identifier()->data(), sid->service_identifier()->size());
  CBuffer spublic(sid->server_share()->xy()->data(), sid->server_share()->xy()->size());
  CBuffer sspublic(sid->server_signature_share()->xy()->data(), sid->server_signature_share()->xy()->size());
  CBuffer ssignature(sid->signature()->data(), sid->signature()->size());

  std::vector<uint8_t> msg(service_identifier);
  msg.insert(msg.end(), nonce.begin(), nonce.end());
  if (!internal::VerifyCurve25519(msg, sspublic, ssignature)) {
    throw std::runtime_error("Invalid service signature");
  }

  CBuffer quote;
  CBuffer collateral;

  for (const Evidence* e : *auth) {
    CBuffer contents(e->contents()->data(), e->contents()->size());
    switch (e->type()) {
      case EvidenceType_Quote: {
        quote = contents;
        break;
      }
      case EvidenceType_Collateral:
        collateral = contents;
        break;
      default:
        throw std::runtime_error("Unknown evidence type");
    }
  }

  if (quote.p) {
    if (std::getenv("CONFONNX_DUMP_QUOTE")) {
      {
        std::ofstream outfile("confonnx_sgx_quote.bin", std::ofstream::binary);
        outfile.write((const char*)quote.p, quote.n);
        outfile.close();
      }
      {
        std::ofstream outfile("confonnx_sgx_ehd.bin", std::ofstream::binary);
        outfile.write((const char*)spublic.p, spublic.n);
        outfile.write((const char*)service_identifier.p, service_identifier.n);
        outfile.close();
      }
    }

#ifdef HAVE_OE_HOST_VERIFY
    if (expected_enclave_signing_key_pem.empty() && expected_enclave_hash.empty()) {
      std::cout << CWARN << "WARNING: Expected enclave signer / hash not provided, skipping identity verification" << CEND << std::endl;
    }
    VerifyQuote(quote, collateral, spublic, service_identifier);
#else
    if (!expected_enclave_signing_key_pem.empty() || !expected_enclave_hash.empty()) {
      throw std::runtime_error("cannot verify enclave identity without quote verification support");
    }
    std::cout << CWARN << "WARNING: no support for quote verification" << CEND << std::endl;
#endif
  } else {
    if (!expected_enclave_signing_key_pem.empty() || !expected_enclave_hash.empty()) {
      throw std::runtime_error("no quote received from server, cannot verify identity");
    }
  }

  if (!expected_service_identifier.empty()) {
    bool mismatch = false;
    mismatch = expected_service_identifier.size() != service_identifier.n;
    for (size_t i = 0; !mismatch && i < expected_service_identifier.size(); i++) {
      mismatch = expected_service_identifier[i] != service_identifier.p[i];
    }
    if (mismatch) {
      std::string expected_s = Buffer2Hex(expected_service_identifier);
      std::string actual_s = Buffer2Hex(service_identifier);
      throw std::runtime_error("Enclave service identifier mismatch: expected=" + expected_s + " actual=" + actual_s);
    } else {
      if (verbose) {
        std::string expected_s = Buffer2Hex(expected_service_identifier);
        std::cout << COK << "Enclave service identifier verified: " << expected_s << CEND << std::endl;
      }
    }
  }

  server_nonce.clear();
  server_nonce.insert(server_nonce.end(), sid->nonce()->begin(), sid->nonce()->end());

  std::vector<uint8_t> shared_secret;
  internal::ComputeSharedSecretCurve25519(key_provider->GetCurrentKey(), spublic, shared_secret);
  internal::DeriveSymmetricKey(shared_secret, true, in_symmetric_key, static_iv);
  internal::DeriveSymmetricKey(shared_secret, false, out_symmetric_key, static_iv);

  key_version = r->key_version();

  return Result::CreateKeyResponse();
}

Client::Result Client::HandleResponse(const protocol::Response* r) {
  bool key_outdated = r->key_outdated();
  CBuffer siv(r->iv()->Data(), r->iv()->size());
  CBuffer tag(r->tag()->Data(), r->tag()->size());
  CBuffer additional_data;
  CBuffer ciphertext(r->ciphertext()->data(), r->ciphertext()->size());

  if (r->additional_data() != nullptr) {
    additional_data = CBuffer(r->additional_data()->Data(), r->additional_data()->size());
  }

  if (siv.n != IV_SIZE) {
    throw std::runtime_error("invalid iv size");
  }
  if (tag.n != TAG_SIZE) {
    throw std::runtime_error("invalid tag size");
  }

  std::vector<uint8_t> payload(ciphertext.n);
  internal::Decrypt(in_symmetric_key, siv, tag, ciphertext, additional_data, payload);
  Client::Result result = Client::Result::CreateResponse(std::move(payload), key_outdated);

  return result;
}

Client::Result Client::HandleMessage(const uint8_t* msg, size_t msg_size) {
  auto verifier = flatbuffers::Verifier(msg, msg_size);
  if (!VerifyMessageBuffer(verifier)) {
    throw std::runtime_error("flatbuffer not valid");
  }

  const Message* msg_fb = GetMessage(msg);

  if (msg_fb->version() != Version_v1) {
    throw std::runtime_error("unsupported protocol version");
  }

  switch (msg_fb->body_type()) {
    case Body_KeyResponse:
      return HandleKeyResponse(msg_fb->body_as_KeyResponse());
    case Body_Response:
      return HandleResponse(msg_fb->body_as_Response());
    case Body_KeyRequest:
    case Body_Request:
      throw std::runtime_error("message not supposed to be handled by confmsg client");
    default:
      throw std::runtime_error("unhandled message type");
  }
}

void Client::MakeKeyRequest(uint8_t* msg, size_t* msg_size, size_t max_msg_size) {
  flatbuffers::FlatBufferBuilder builder;
  auto nonce_fb = builder.CreateVector(nonce);
  auto request_fb = CreateKeyRequest(builder, nonce_fb);
  auto msg_fb = CreateMessage(builder, Version_v1, Body_KeyRequest, request_fb.Union());
  builder.Finish(msg_fb);

  WriteMessage(builder, msg, msg_size, max_msg_size);

#ifdef _DEBUG
  auto verifier = flatbuffers::Verifier(msg, *msg_size);
  if (!VerifyMessageBuffer(verifier)) {
    throw std::runtime_error("constructed flatbuffer invalid");
  }
#endif
}

void Client::MakeRequest(CBuffer plaintext, uint8_t* msg, size_t* msg_size, size_t max_msg_size) {
  if (public_key.size() != KEY_SIZE || out_symmetric_key.size() != SYMMETRIC_KEY_SIZE) {
    throw std::runtime_error("No or invalid keys; issue a key request first");
  }

  std::vector<uint8_t> tag(TAG_SIZE);
  std::vector<uint8_t> ciphertext(plaintext.n);
  const std::vector<uint8_t>& additional_data = server_nonce;

  std::vector<uint8_t> xor_iv(IV_SIZE);
  for (size_t i = 0; i < IV_SIZE; i++) {
    xor_iv[i] = static_iv[i] ^ dynamic_iv[i];
  }

  internal::Encrypt(out_symmetric_key, xor_iv, plaintext, additional_data, ciphertext, tag);

  flatbuffers::FlatBufferBuilder builder(ciphertext.size() + 1024);

  auto dynamic_iv_fb = builder.CreateVector(dynamic_iv);
  auto tag_fb = builder.CreateVector(tag);
  auto public_key_fb = builder.CreateVector(public_key);
  auto public_ecpoint_fb = CreateECPoint(builder, PointFormat_Compressed, public_key_fb);
  auto additional_data_fb = builder.CreateVector(additional_data);
  auto ciphertext_fb = builder.CreateVector(ciphertext);
  auto request_fb = CreateRequest(builder, key_version, dynamic_iv_fb, tag_fb, public_ecpoint_fb, additional_data_fb, ciphertext_fb);
  auto msg_fb = CreateMessage(builder, Version_v1, Body_Request, request_fb.Union());
  builder.Finish(msg_fb);

  WriteMessage(builder, msg, msg_size, max_msg_size);

  internal::IncrementIV(dynamic_iv);

#ifdef _DEBUG
  auto verifier = flatbuffers::Verifier(msg, *msg_size);
  if (!VerifyMessageBuffer(verifier)) {
    throw std::runtime_error("constructed flatbuffer invalid");
  }
#endif
}

void Client::VerifyQuote(CBuffer quote, CBuffer collateral, CBuffer service_public_key, CBuffer service_identifier) {
  if (quote.n == 0) {
    throw AttestationError("no quote to verify");
  }

// FIXME should also be enabled if building for enclaves; are the APIs the same?
#ifdef HAVE_OE_HOST_VERIFY
  // 1) Validate the report's trustworthiness
  // Verify the remote report to ensure its authenticity.
  oe_report_t parsed_report;
  if (oe_verify_remote_report(quote.p, quote.n, collateral.p, collateral.n, &parsed_report) != OE_OK) {
    throw AttestationError("Enclave quote invalid");
  } else {
    if (verbose) {
      std::cout << COK << "Enclave quote verified: authentic Intel SGX platform" << CEND << std::endl;
    }
  }

  // 2) Validate the enclave's identity
  // Check that the enclave was signed by a trusted entity and/or
  // check that the enclave's code hash matches a provided hash.

  if (parsed_report.identity.id_version != 0) {
    throw AttestationError("unsupported report format version");
  }

  // Check the enclave hash
  if (!expected_enclave_hash.empty()) {
    if (expected_enclave_hash.size() != OE_UNIQUE_ID_SIZE ||
        memcmp(parsed_report.identity.unique_id, expected_enclave_hash.data(), expected_enclave_hash.size()) != 0) {
      std::string expected_s = Buffer2Hex(expected_enclave_hash);
      std::string actual_s = Buffer2Hex(CBuffer(parsed_report.identity.unique_id, OE_UNIQUE_ID_SIZE));
      throw AttestationError("Enclave hash mismatch: expected=" + expected_s + " actual=" + actual_s);
    } else {
      if (verbose) {
        std::string expected_s = Buffer2Hex(expected_enclave_hash);
        std::cout << COK << "Enclave hash verified: " << expected_s << CEND << std::endl;
      }
    }
  }

  // Check the enclave signing key
  if (!expected_enclave_signing_key_pem.empty()) {
    std::vector<uint8_t> expected_mrsigner;
    internal::PEM2MRSigner(expected_enclave_signing_key_pem, expected_mrsigner);
    if (expected_mrsigner.size() != OE_SIGNER_ID_SIZE ||
        memcmp(parsed_report.identity.signer_id, expected_mrsigner.data(), OE_SIGNER_ID_SIZE) != 0) {
      std::string expected_s = Buffer2Hex(expected_mrsigner);
      std::string actual_s = Buffer2Hex(CBuffer(parsed_report.identity.signer_id, OE_UNIQUE_ID_SIZE));
      throw AttestationError("Enclave signer mismatch: expected=" + expected_s + " actual=" + actual_s);
    } else {
      if (verbose) {
        std::string expected_s = Buffer2Hex(expected_mrsigner);
        std::cout << COK << "Enclave signer verified: " << expected_s << CEND << std::endl;
      }
    }
  }

  // Check the enclave's product id and security version
  if (parsed_report.identity.product_id[0] != 1) {
    throw AttestationError("product id check failed");
  }

  if (parsed_report.identity.security_version < 1) {
    throw AttestationError("security version check failed");
  }

  // 3) Validate the report data
  // The quote's report_data is a hash of the actual report data.
  std::vector<uint8_t> data_hash;
  internal::SHA256({service_public_key, service_identifier}, data_hash);

  // TODO check why parsed_report.report_data_size is 64 when it should be 32
  bool ok = memcmp(parsed_report.report_data, data_hash.data(), data_hash.size()) == 0;

  if (!ok) {
    std::string expected_s = Buffer2Hex(data_hash);
    std::string actual_s = Buffer2Hex(CBuffer(parsed_report.report_data, data_hash.size()));
    throw AttestationError("Enclave quote data mismatch: expected=" + expected_s + " actual=" + actual_s);
  } else {
    if (verbose) {
      std::string expected_s = Buffer2Hex(data_hash);
      std::cout << COK << "Enclave quote data verified: " << expected_s << CEND << std::endl;
    }
  }

#else
  (void)(quote);
  (void)(collateral);
  (void)(service_public_key);
  (void)(service_identifier);
  throw AttestationError("quote verification requires OE host verify library");
#endif
}
}  // namespace confmsg
