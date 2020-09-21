// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <chrono>
#include <memory>

#include <confmsg/shared/util.h>
#include <confmsg/shared/exceptions.h>

namespace confmsg {

enum class KeyType { Generic,
                     Curve25519 };

class KeyProvider {
 public:
  KeyProvider(const KeyProvider&) = delete;
  KeyProvider& operator=(const KeyProvider&) = delete;
  KeyProvider(KeyProvider&&) = default;

  virtual ~KeyProvider() {
    Wipe(previous_key);
    Wipe(current_key);
  }

  bool RefreshKey(bool sync_only = false) {
    bool refreshed = DoRefreshKey(sync_only);
    if (refreshed) {
      last_refreshed = std::chrono::system_clock::now();
    }
    return refreshed;
  }

  std::chrono::time_point<std::chrono::system_clock> GetLastRefreshed() const {
    return last_refreshed;
  }

  KeyType GetKeyType() {
    return key_type;
  }

  const std::vector<uint8_t>& GetCurrentKey() {
    return current_key;
  }

  uint32_t GetCurrentKeyVersion() {
    return current_key_version;
  }

  const std::vector<uint8_t>& GetKey(uint32_t key_version) {
    if (current_key_version == key_version) {
      return current_key;
    } else if (previous_key_version == key_version) {
      return previous_key;
    } else {
      throw CryptoError("key with specified version not found");
    }
  }

  bool IsKeyOutdated(uint32_t key_version) {
    if (current_key_version == key_version) {
      return false;
    } else if (previous_key_version == key_version) {
      return true;
    } else {
      throw CryptoError("key with specified version not found");
    }
  }

  virtual void DeleteKey() {
    Wipe(previous_key);
    Wipe(current_key);
    current_key_version = previous_key_version = 0;
    initialized = false;
  }

 protected:
  std::vector<uint8_t> previous_key;
  std::vector<uint8_t> current_key;
  uint32_t previous_key_version;
  uint32_t current_key_version;
  bool initialized = false;

  KeyProvider(size_t key_size, KeyType key_type) : key_type(key_type) {
    previous_key.resize(key_size);
    current_key.resize(key_size);
  }

  virtual bool DoRefreshKey(bool sync_only) = 0;

  void Initialize() {
    bool sync_only = false;
    RefreshKey(sync_only);
    initialized = true;
  }

 private:
  KeyType key_type;
  std::chrono::time_point<std::chrono::system_clock> last_refreshed;
};

class StaticKeyProvider : public KeyProvider {
 public:
  static std::unique_ptr<KeyProvider> Create(const std::vector<uint8_t>& key, KeyType key_type) {
    std::unique_ptr<StaticKeyProvider> kp(new StaticKeyProvider(key, key_type));
    kp->Initialize();
    return kp;
  }

 protected:
  bool DoRefreshKey(bool sync_only) override {
    (void)sync_only;
    return false;
  }

 private:
  StaticKeyProvider(const std::vector<uint8_t>& key, KeyType key_type) : KeyProvider(key.size(), key_type) {
    std::copy(key.begin(), key.end(), this->current_key.begin());
  }
};

class RandomKeyProvider : public KeyProvider {
 public:
  static std::unique_ptr<KeyProvider> Create(size_t key_size) {
    std::unique_ptr<RandomKeyProvider> kp(new RandomKeyProvider(key_size));
    kp->Initialize();
    return kp;
  }

 protected:
  bool DoRefreshKey(bool sync_only) override {
    if (sync_only) {
      return false;
    }
    previous_key_version = current_key_version;
    previous_key = current_key;
    current_key_version++;
    Randomize(current_key, current_key.size());
    return true;
  }

 private:
  RandomKeyProvider(size_t key_size) : KeyProvider(key_size, KeyType::Generic) {}
};

class RandomEd25519KeyProvider : public KeyProvider {
 public:
  static std::unique_ptr<KeyProvider> Create() {
    std::unique_ptr<RandomEd25519KeyProvider> kp(new RandomEd25519KeyProvider());
    kp->Initialize();
    return kp;
  }

 protected:
  virtual bool DoRefreshKey(bool sync_only) override {
    if (sync_only) {
      return false;
    }
    previous_key_version = current_key_version;
    previous_key = current_key;
    current_key_version++;
    Randomize(current_key, current_key.size());

    // See https://tools.ietf.org/html/rfc8032#section-5.1.5
    current_key[0] &= 248;
    current_key[31] &= 127;
    current_key[31] |= 64;
    return true;
  }

 private:
  RandomEd25519KeyProvider() : KeyProvider(32, KeyType::Curve25519) {}
};

}  // namespace confmsg