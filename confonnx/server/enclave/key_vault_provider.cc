// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include <confmsg/shared/exceptions.h>
#include <confmsg/shared/crypto.h>

#include "server/shared/util.h"
#include "server/shared/curl_helper.h"
#include "server/enclave/key_vault_provider.h"

namespace onnxruntime {
namespace server {

static constexpr auto AKV_API_VERSION = "7.0";

class KeyVaultKey {
 public:
  KeyVaultKey(std::vector<uint8_t>&& key, uint32_t version) : key(std::move(key)), version(version), not_found(false) {}
  KeyVaultKey() = default;
  std::vector<uint8_t> key;
  uint32_t version = 0;  // stored in tags as custom metadata
  bool not_found = true;
};

KeyVaultProvider::KeyVaultProvider(
    KeyVaultConfig&& config,
    std::unique_ptr<confmsg::KeyProvider>&& random_key_provider)
    : confmsg::KeyProvider(SYMMETRIC_KEY_SIZE, random_key_provider->GetKeyType()),
      config(config),
      http_client(config.app_id, config.app_pwd),
      random_key_provider(std::move(random_key_provider)) {}

bool KeyVaultProvider::DoRefreshKey(bool sync_only) {
  // TODO load previous key from AKV as well if we're starting fresh
  //      -> Currently not easily possible as old versions are not returned in order. Will be fixed in AKV.

  try {
    KeyVaultKey current_key_akv = FetchKey();

    // Key not found in vault, store initial version.
    if (current_key_akv.not_found) {
      uint32_t new_version = 1;
      KeyVaultKey new_key_akv = UpdateKey(new_version);
      current_key_version = new_key_akv.version;
      current_key = new_key_akv.key;
      return true;
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
      KeyVaultKey new_key_akv = UpdateKey(new_version);
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

KeyVaultKey KeyVaultProvider::FetchKey(const std::string& key_version) {
  std::string response_str;
  try {
    response_str = http_client.Request(
        config.url + "secrets/" + config.key_name + "/" + key_version + "?api-version=" + AKV_API_VERSION);
  } catch (CurlHTTPError& err) {
    if (err.status_code == 404) {
      KeyVaultKey not_found;
      return not_found;
    } else {
      throw;
    }
  }

  auto response = nlohmann::json::parse(response_str);

  std::string key_str = response["value"];
  uint32_t version = 0;
  if (response.find("tags") != response.end()) {
    if (response["tags"].find("version") != response["tags"].end()) {
      std::string version_str = response["tags"]["version"];
      version = std::stoi(version_str);
    }
  }

  KeyVaultKey result(FromHex(key_str), version);
  return result;
}

KeyVaultKey KeyVaultProvider::UpdateKey(uint32_t new_version) {
  random_key_provider->RefreshKey();
  std::vector<uint8_t> new_key = random_key_provider->GetCurrentKey();
  std::string new_key_str = ToHex(new_key);

  std::map<std::string, std::string> headers{
      {"Content-Type", "application/json"}};

  std::stringstream body_s;
  body_s << "{"
         << R"("value": ")" << new_key_str << R"(",)"
         << R"("tags": { "version": ")" << new_version << R"(" })"
         << "}";
  std::string body = body_s.str();

  std::string response_str = http_client.Request(
      config.url + "secrets/" + config.key_name + "?api-version=" + AKV_API_VERSION,
      body, headers, HttpMethod::PUT);

  auto response = nlohmann::json::parse(response_str);

  std::string key_str = response["value"];
  std::string version_str = response["tags"]["version"];
  uint32_t version = std::stoi(version_str);

  if (version != new_version) {
    throw std::runtime_error("unexpected version found in tags of key");
  }

  KeyVaultKey result(FromHex(key_str), new_version);
  return result;
}

void KeyVaultProvider::DeleteKey() {
  KeyProvider::DeleteKey();

  std::map<std::string, std::string> headers{
      {"Content-Type", "application/json"}};

  std::string response_str = http_client.Request(
      config.url + "secrets/" + config.key_name + "?api-version=" + AKV_API_VERSION,
      "", headers, HttpMethod::DELETE);
}

}  // namespace server
}  // namespace onnxruntime