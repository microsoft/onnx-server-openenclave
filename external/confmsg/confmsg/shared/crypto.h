// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stddef.h>

#include <confmsg/shared/buffer.h>

#define KEY_SIZE 32
#define SYMMETRIC_KEY_SIZE 32
#define TAG_SIZE 16
#define IV_SIZE 12
#define SIGNATURE_SIZE 64
#define NONCE_SIZE 16
#define SHA256_SIZE 32

namespace confmsg {

void InitCrypto();

namespace internal {

void MakePublicKeyCurve25519(const std::vector<uint8_t>& secret_key, std::vector<uint8_t>& public_key);

void MakePublicKeysCurve25519(const std::vector<uint8_t>& secret_key, std::vector<uint8_t>& public_key, std::vector<uint8_t>& public_signing_key);

void DeriveSymmetricKey(CBuffer shared_secret, bool server, std::vector<uint8_t>& symmetric_key, std::vector<uint8_t>& static_iv);

void IncrementIV(std::vector<uint8_t>& iv);

void ComputeSharedSecretCurve25519(CBuffer our_secret, CBuffer their_public, std::vector<uint8_t>& shared_secret);

void Encrypt(CBuffer key, CBuffer iv, CBuffer plain, CBuffer additional_data, std::vector<uint8_t>& cipher, std::vector<uint8_t>& tag);

void Decrypt(CBuffer key, CBuffer iv, CBuffer tag, CBuffer cipher, CBuffer additional_data, std::vector<uint8_t>& plain);

void SignCurve25519(CBuffer msg, CBuffer key, std::vector<uint8_t>& signature);

bool VerifyCurve25519(CBuffer msg, CBuffer public_key, CBuffer signature);

void SHA256(CBuffer data, std::vector<uint8_t>& hash);
void SHA256(std::initializer_list<CBuffer> data, std::vector<uint8_t>& hash);

void PEM2MRSigner(const std::string& public_key_pem, std::vector<uint8_t>& mrsigner);

}  // namespace internal
}  // namespace confmsg