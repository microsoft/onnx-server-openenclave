// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>

#include <nlohmann/json.hpp>
#include <openenclave/enclave.h>
#include <skr/skr.h>

#include <confmsg/shared/exceptions.h>
#include <confmsg/shared/crypto.h>

#include "server/shared/util.h"
#include "server/shared/curl_helper.h"
#include "server/enclave/key_vault_hsm_provider.h"

namespace skr = microsoft::skr;

namespace onnxruntime {
namespace server {

static constexpr auto AKV_API_VERSION = "7.0-preview";
static constexpr auto AAS_API_VERSION = "2018-09-01-preview";

class KeyVaultHsmKey {
 public:
  enum class Status { OK,
                      NOT_FOUND,
                      DENIED };

  KeyVaultHsmKey(std::vector<uint8_t>&& key, uint32_t version) : key(std::move(key)), version(version), status(Status::OK) {}
  explicit KeyVaultHsmKey(Status status) : status(status) {}
  std::vector<uint8_t> key;
  uint32_t version = 0;  // stored in tags as custom metadata
  Status status;
};

KeyVaultHsmProvider::KeyVaultHsmProvider(KeyVaultConfig&& config)
    : confmsg::KeyProvider(SYMMETRIC_KEY_SIZE, confmsg::KeyType::Curve25519),
      config(config),
      http_client_akv(config.app_id, config.app_pwd),
      http_client_aas(config.app_id, config.app_pwd) {
  if (config.url.find("https://") == std::string::npos) {
    throw std::invalid_argument("vault url invalid");
  }
  if (config.url.back() != '/') {
    throw std::invalid_argument("vault url invalid");
  }
  if (config.attestation_url.find("https://") == std::string::npos) {
    throw std::invalid_argument("attestation url invalid");
  }
  if (config.attestation_url.back() != '/') {
    throw std::invalid_argument("attestation url invalid");
  }
}

bool KeyVaultHsmProvider::DoRefreshKey(bool sync_only) {
  // TODO load previous key from AKV as well if we're starting fresh
  //      -> Currently not easily possible as old versions are not returned in order. Will be fixed in AKV.

  try {
    KeyVaultHsmKey current_key_akv = FetchKey();

    if (current_key_akv.status == KeyVaultHsmKey::Status::NOT_FOUND) {
      std::cout << "Key not found in Vault, storing initial version" << std::endl;
      uint32_t new_version = 1;
      KeyVaultHsmKey new_key_akv = UpdateKey(new_version);
      current_key_version = new_key_akv.version;
      current_key = new_key_akv.key;
      return true;
    } else if (current_key_akv.status == KeyVaultHsmKey::Status::DENIED) {
      if (sync_only) {
        throw std::logic_error("AKV key export denied");
      } else {
        // Likely "Target environment attestation does not meet key release policy requirements".
        // Can happen if we changed the policy.
        // TODO check if version (from tags) can be retrieved without SKR; if yes, use it and +1
        std::cout << "Key export denied, storing new key with version 1" << std::endl;
        uint32_t new_version = 1;
        KeyVaultHsmKey new_key_akv = UpdateKey(new_version);
        current_key_version = new_key_akv.version;
        current_key = new_key_akv.key;
        return true;
      }
    }

    // Existing key found in vault, use if either:
    // - we're in initialization phase, or
    // - it's newer than what we have already.
    if (!initialized || current_key_akv.version > current_key_version) {
      previous_key_version = current_key_version;
      previous_key = current_key;
      current_key_version = current_key_akv.version;
      current_key = current_key_akv.key;
      return true;
    }

    // Existing key in vault is not newer, now we can either return or do a key rollover.
    if (sync_only) {
      return false;
    } else {
      uint32_t new_version = current_key_version + 1;
      KeyVaultHsmKey new_key_akv = UpdateKey(new_version);
      previous_key_version = current_key_version;
      previous_key = current_key;
      current_key_version = new_key_akv.version;
      current_key = new_key_akv.key;
      return true;
    }
  } catch (CurlError& err) {
    throw confmsg::KeyRefreshError(err.what());
  }
}

KeyVaultHsmKey KeyVaultHsmProvider::FetchKey(const std::string& key_version) {
  // Generate a quote for the enclave containing the hash of the enclave-held data
  // and returns the report and enclave-held data for attestation.
  uint8_t* report_buffer = nullptr;
  size_t report_buffer_size = 0;
  uint8_t* report_data_buffer = nullptr;
  size_t report_data_buffer_size = 0;
  auto skr_result = skr_get_report(OE_REPORT_FLAGS_REMOTE_ATTESTATION,
                                   &report_buffer, &report_buffer_size,
                                   &report_data_buffer, &report_data_buffer_size);
  if (skr_result != OE_OK) {
    throw std::runtime_error("skr_get_report failed");
  }

  // Encode as Base64Url for AAS.
  std::string quote_b64 = Base64Url(confmsg::CBuffer(report_buffer, report_buffer_size));
  std::string enclave_held_data_b64 = Base64Url(confmsg::CBuffer(report_data_buffer, report_data_buffer_size));

  skr_free_report(report_buffer, report_data_buffer);

  // Retrieve attestation from Azure Attestation Service (AAS) using the data above.
  // See azure-keyvault-hsm/client_enclave_host/src/Attestation.cpp
  std::map<std::string, std::string> aas_headers{
      {"Content-Type", "application/json"},
      {"Accept", "application/json"}};

  std::stringstream aas_body_s;
  aas_body_s << "{"
             << " \"Quote\": \"" << quote_b64 << "\","
             << " \"EnclaveHeldData\": \"" << enclave_held_data_b64 << "\""
             << "}";
  std::string aas_body = aas_body_s.str();

  // Result is a JWT wrapped in a JSON string: "<attestation token>".
  // This is used as-is for AKV below.
  std::string aas_response_str = http_client_aas.Request(
      config.attestation_url + "attest/Tee/OpenEnclave?api-version=" + AAS_API_VERSION,
      aas_body, aas_headers, HttpMethod::POST);

  // Carry out secure key release in AKV with attestation from AAS.
  std::map<std::string, std::string> akv_headers{
      {"Content-Type", "application/json"},
      {"Accept", "application/json"}};

  std::stringstream akv_body_s;
  akv_body_s << "{"
             << " \"env\": " << aas_response_str
             << "}";
  std::string akv_body = akv_body_s.str();

  std::string response_str;
  try {
    response_str = http_client_akv.Request(
        MakeKeyIdentifier(key_version) + "/export" + "?api-version=" + AKV_API_VERSION,
        akv_body, akv_headers, HttpMethod::POST);
  } catch (CurlHTTPError& err) {
    if (err.status_code == 404) {
      return KeyVaultHsmKey(KeyVaultHsmKey::Status::NOT_FOUND);
    } else if (err.status_code == 403) {
      // Likely "Target environment attestation does not meet key release policy requirements".
      // Can happen if we changed the policy.
      return KeyVaultHsmKey(KeyVaultHsmKey::Status::DENIED);
    } else {
      throw;
    }
  }

  auto response = nlohmann::json::parse(response_str);

  std::string key_str = response["value"];

  // Unpack key
  // See azure-keyvault-hsm/client_enclave/enclave/src/enclave_callin.cpp.

  // TODO is this still needed with libskr?
  size_t pl = key_str.size() % 4;
  if (pl != 0) key_str.append(4 - pl, '=');

  skr::KeyBundle key_bundle;
  auto result = skr_import_key((const uint8_t*)key_str.c_str(), key_str.size(), key_bundle);
  if (result != OE_OK) {
    throw std::runtime_error("skr_import_key failed");
  }

  skr::WebKey web_key = key_bundle.key;
  std::vector<uint8_t> key = web_key.k;

  // Convert into Curve25517 key
  // See https://tools.ietf.org/html/rfc8032#section-5.1.5
  key[0] &= 248;
  key[31] &= 127;
  key[31] |= 64;

  std::string version_str = key_bundle.tags.at("version");
  uint32_t version = std::stoi(version_str);

  KeyVaultHsmKey k(std::move(key), version);
  return k;
}

KeyVaultHsmKey KeyVaultHsmProvider::UpdateKey(uint32_t new_version) {
  // Get enclave signer
  uint8_t* report;
  size_t report_len = 0;
  // TODO is this the best way to get mrsigner?
  oe_result_t res = oe_get_report(OE_REPORT_FLAGS_REMOTE_ATTESTATION,  // (use 0 for local attestation)
                                  nullptr, 0,                          // no extra data
                                  nullptr, 0,                          // opt_params is empty for remote attestation
                                  &report, &report_len);
  if (res != OE_OK) {
    throw std::runtime_error("oe_get_report failed: " + std::string(oe_result_str(res)));
  }

  oe_report_t parsed_report;
  res = oe_parse_report(report, report_len, &parsed_report);
  if (res != OE_OK) {
    throw std::runtime_error("oe_parse_report failed: " + std::string(oe_result_str(res)));
  }

  std::string mrsigner = ToHex(confmsg::CBuffer(parsed_report.identity.signer_id, OE_SIGNER_ID_SIZE));

  oe_free_report(report);

  // Store key with release policy
  // Note: We create an AES-256 key that we patch to an EC Curve25519 key on key release.
  //       This is because AKV HSM does not support Curve25519.
  std::map<std::string, std::string> headers{
      {"Content-Type", "application/json"},
      {"Accept", "application/json"}};

  std::stringstream body_s;
  body_s << "{"
         << " \"kty\": \"AES-HSM\","
         << " \"key_size\": " << KEY_SIZE * 8 << ","
         << " \"key_ops\": [],"
         << " \"attributes\": {"
         << "  \"exportable\": true"
         << " },"
         << " \"release_policy\": {"
         << "  \"" << config.attestation_url << "\": {"
         << "   \"sgx-mrsigner\": \"" << mrsigner << "\""
         << "  }"
         << " },"
         << " \"tags\": { \"version\": \"" << new_version << "\" }"
         << "}";
  std::string body = body_s.str();

  std::string response_str = http_client_akv.Request(
      MakeKeyIdentifier() + "/create?api-version=" + AKV_API_VERSION,
      body, headers, HttpMethod::POST);

  auto response = nlohmann::json::parse(response_str);

  std::string version_str = response["tags"]["version"];
  uint32_t version = std::stoi(version_str);
  std::string key_id_with_version = response["key"]["kid"];
  std::string key_id = MakeKeyIdentifier();
  std::string key_version = key_id_with_version.substr(key_id.size() + 1);

  if (version != new_version) {
    throw std::runtime_error("unexpected version found in tags of key");
  }

  // Exact version is used for fetching, otherwise AKV may return an old cached
  // key if we end up at a different node in the pool and replication hasn't finished.
  KeyVaultHsmKey exported_key = FetchKey(key_version);
  if (exported_key.status != KeyVaultHsmKey::Status::OK) {
    throw std::logic_error("AKV key export failed after creation");
  }
  if (exported_key.version < new_version) {
    throw std::logic_error("unexpected version found in exported key after rollover: expected=" +
                           std::to_string(new_version) + " actual=" + std::to_string(exported_key.version));
  }

  return exported_key;
}

void KeyVaultHsmProvider::DeleteKey() {
  KeyProvider::DeleteKey();

  std::map<std::string, std::string> headers{
      {"Content-Type", "application/json"}};

  std::string response_str = http_client_akv.Request(
      MakeKeyIdentifier() + "?api-version=" + AKV_API_VERSION,
      "", headers, HttpMethod::DELETE);
}

std::string KeyVaultHsmProvider::MakeKeyIdentifier(const std::string& key_version) const {
  std::string id = config.url + "keys/" + config.key_name;
  if (!key_version.empty()) {
    id = id + "/" + key_version;
  }
  return id;
}

}  // namespace server
}  // namespace onnxruntime