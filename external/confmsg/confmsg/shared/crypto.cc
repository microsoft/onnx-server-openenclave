// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <stdexcept>
#include <algorithm>

extern "C" {
#include <EverCrypt_AutoConfig2.h>
#include <EverCrypt_AEAD.h>
#include <EverCrypt_Curve25519.h>
#include <Hacl_Ed25519.h>
#include <EverCrypt_Hash.h>
#include <EverCrypt_HKDF.h>
}

#include <mbedtls/x509.h>
#include <mbedtls/error.h>

#include "shared/crypto.h"
#include "shared/util.h"
#include "shared/exceptions.h"

namespace confmsg {

void InitCrypto() {
  EverCrypt_AutoConfig2_init();
}

namespace internal {

void MakePublicKeyCurve25519(const std::vector<uint8_t>& secret_key, std::vector<uint8_t>& public_key) {
  public_key.resize(secret_key.size());
  EverCrypt_Curve25519_secret_to_public(public_key.data(), const_cast<uint8_t*>(secret_key.data()));
}

void MakePublicKeysCurve25519(const std::vector<uint8_t>& secret_key, std::vector<uint8_t>& public_key, std::vector<uint8_t>& public_signing_key) {
  public_key.resize(secret_key.size());
  public_signing_key.resize(secret_key.size());
  EverCrypt_Curve25519_secret_to_public(public_key.data(), const_cast<uint8_t*>(secret_key.data()));
  Hacl_Ed25519_secret_to_public(public_signing_key.data(), const_cast<uint8_t*>(secret_key.data()));
}

void ComputeSharedSecretCurve25519(CBuffer our_secret, CBuffer their_public, std::vector<uint8_t>& shared_secret) {
  if (our_secret.n != KEY_SIZE || their_public.n != KEY_SIZE) {
    throw CryptoError("Invalid key sizes");
  }

  shared_secret.resize(KEY_SIZE);
  EverCrypt_Curve25519_ecdh(
      shared_secret.data(),
      const_cast<uint8_t*>(our_secret.p),
      const_cast<uint8_t*>(their_public.p));
}

void DeriveSymmetricKey(CBuffer shared_secret, bool server, std::vector<uint8_t>& symmetric_key, std::vector<uint8_t>& static_iv) {
  if (shared_secret.n != KEY_SIZE) {
    throw CryptoError("Invalid shared secret (wrong size)");
  }

  symmetric_key.resize(SYMMETRIC_KEY_SIZE);
  static_iv.resize(IV_SIZE);

  std::string label_key = server ? "server key" : "client key";
  std::string label_iv = server ? "server iv" : "client iv";

  EverCrypt_HKDF_expand_sha2_256(
      symmetric_key.data(),
      const_cast<uint8_t*>(shared_secret.p),
      shared_secret.n,
      reinterpret_cast<uint8_t*>(const_cast<char*>(label_key.data())),
      label_key.size(),
      SYMMETRIC_KEY_SIZE);

  EverCrypt_HKDF_expand_sha2_256(
      static_iv.data(),
      const_cast<uint8_t*>(shared_secret.p),
      shared_secret.n,
      reinterpret_cast<uint8_t*>(const_cast<char*>(label_iv.data())),
      label_iv.size(),
      IV_SIZE);
}

void IncrementIV(std::vector<uint8_t>& iv) {
  if (iv.size() != IV_SIZE) {
    throw CryptoError("Invalid IV");
  }

  for (size_t i = IV_SIZE; i > 0; i--) {
    if (++iv[i - 1] != 0) {
      // no overflow happened, we're done
      break;
    }
  }
}

void Encrypt(CBuffer key, CBuffer iv, CBuffer plain, CBuffer additional_data, std::vector<uint8_t>& cipher, std::vector<uint8_t>& tag) {
  if (key.n != SYMMETRIC_KEY_SIZE) {
    throw CryptoError("Invalid AEAD key size: " + std::to_string(key.n));
  }
  if (iv.n != IV_SIZE) {
    throw CryptoError("Invalid AEAD IV size: " + std::to_string(iv.n));
  }

  tag.resize(TAG_SIZE);
  cipher.resize(plain.n);

  EverCrypt_AEAD_state_s* aead_state = nullptr;
  if (EverCrypt_AEAD_create_in(Spec_Agile_AEAD_AES256_GCM, &aead_state, (uint8_t*)key.p) != EverCrypt_Error_Success) {
    throw CryptoError("AEAD context creation failed");
  }

  auto status = EverCrypt_AEAD_encrypt(
      aead_state,
      const_cast<uint8_t*>(iv.p), iv.n,
      const_cast<uint8_t*>(additional_data.p), additional_data.n,
      const_cast<uint8_t*>(plain.p), plain.n,
      cipher.data(),
      tag.data());

  EverCrypt_AEAD_free(aead_state);

  if (status != EverCrypt_Error_Success) {
    throw CryptoError("encryption failed [code=" + std::to_string(status) + "]");
  }
}

void Decrypt(CBuffer key, CBuffer iv, CBuffer tag, CBuffer cipher, CBuffer additional_data, std::vector<uint8_t>& plain) {
  if (key.n != SYMMETRIC_KEY_SIZE) {
    throw CryptoError("Invalid AEAD key size: " + std::to_string(key.n));
  }
  if (iv.n != IV_SIZE) {
    throw CryptoError("Invalid AEAD IV size: " + std::to_string(iv.n));
  }
  if (tag.n != TAG_SIZE) {
    throw CryptoError("Invalid AEAD tag size: " + std::to_string(tag.n));
  }

  plain.resize(cipher.n);

  EverCrypt_AEAD_state_s* aead_state = nullptr;
  if (EverCrypt_AEAD_create_in(Spec_Agile_AEAD_AES256_GCM, &aead_state, const_cast<uint8_t*>(key.p)) != EverCrypt_Error_Success) {
    throw CryptoError("AEAD context creation failed");
  }

  auto status = EverCrypt_AEAD_decrypt(
      aead_state,
      const_cast<uint8_t*>(iv.p), iv.n,
      const_cast<uint8_t*>(additional_data.p), additional_data.n,
      const_cast<uint8_t*>(cipher.p), cipher.n,
      const_cast<uint8_t*>(tag.p),
      plain.data());

  EverCrypt_AEAD_free(aead_state);

  if (status != EverCrypt_Error_Success) {
    throw CryptoError("decryption failed [code=" + std::to_string(status) + "]");
  }
}

void SignCurve25519(CBuffer msg, CBuffer key, std::vector<uint8_t>& signature) {
  if (key.n != KEY_SIZE) {
    throw CryptoError("Invalid key size");
  }

  signature.resize(SIGNATURE_SIZE);
  Hacl_Ed25519_sign(
      signature.data(),
      const_cast<uint8_t*>(key.p),
      msg.n,
      const_cast<uint8_t*>(msg.p));
}

bool VerifyCurve25519(CBuffer msg, CBuffer public_key, CBuffer signature) {
  if (public_key.n != KEY_SIZE) {
    throw CryptoError("Invalid public key size");
  }

  if (signature.n != SIGNATURE_SIZE) {
    throw CryptoError("Invalid signature size");
  }

  return Hacl_Ed25519_verify(
      const_cast<uint8_t*>(public_key.p),
      msg.n,
      const_cast<uint8_t*>(msg.p),
      const_cast<uint8_t*>(signature.p));
}

void SHA256(CBuffer data, std::vector<uint8_t>& hash) {
  hash.resize(SHA256_SIZE);
  EverCrypt_Hash_hash_256(const_cast<uint8_t*>(data.p), data.n, hash.data());
}

void SHA256(std::initializer_list<CBuffer> data, std::vector<uint8_t>& hash) {
  hash.resize(SHA256_SIZE);

  Spec_Hash_Definitions_hash_alg a = Spec_Hash_Definitions_SHA2_256;
  EverCrypt_Hash_Incremental_state_s* s = EverCrypt_Hash_Incremental_create_in(a);
  for (auto b : data) {
    EverCrypt_Hash_Incremental_update(s, const_cast<uint8_t*>(b.p), b.n);
  }
  EverCrypt_Hash_Incremental_finish(s, hash.data());
  EverCrypt_Hash_Incremental_free(s);
}

void PEM2MRSigner(const std::string& public_key_pem, std::vector<uint8_t>& mrsigner) {
  // TODO cwinter: would be nice to do this without mbedTLS...

  const char* public_key_pem_c = public_key_pem.c_str();
  size_t size_with_null = public_key_pem.size() + 1;

  mbedtls_pk_context ctx;

  mbedtls_pk_init(&ctx);
  auto error_code = mbedtls_pk_parse_public_key(
      &ctx,
      reinterpret_cast<const unsigned char*>(public_key_pem_c),
      size_with_null);
  if (error_code != 0) {
    char error[256];
    mbedtls_strerror(error_code, error, sizeof(error));
    throw CryptoError("mbedtls_pk_parse_public_key failed: " + std::string(error));
  }

  if (mbedtls_pk_get_type(&ctx) != MBEDTLS_PK_RSA) {
    throw CryptoError("mbedtls_pk_get_type had incorrect type");
  }

  mbedtls_rsa_context* rsa_ctx = mbedtls_pk_rsa(ctx);
  size_t n = mbedtls_rsa_get_len(rsa_ctx);
  std::vector<uint8_t> modulus(n);

  error_code = mbedtls_rsa_export_raw(
      rsa_ctx,
      modulus.data(),
      modulus.size(),
      nullptr, 0,
      nullptr, 0,
      nullptr, 0,
      nullptr, 0);
  if (error_code != 0) {
    char error[256];
    mbedtls_strerror(error_code, error, sizeof(error));
    throw CryptoError("mbedtls_rsa_export failed: " + std::string(error));
  }

  std::reverse(std::begin(modulus), std::end(modulus));
  internal::SHA256(CBuffer(modulus), mrsigner);
}

}  // namespace internal
}  // namespace confmsg
