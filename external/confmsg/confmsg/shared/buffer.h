// https://github.com/microsoft/CCF/blob/master/src/ds/buffer.h

// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the Apache 2.0 License.
#pragma once
#include <atomic>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace confmsg {

template <typename T>
struct Array {
  // pointer to the buffer
  T* p;
  // number of elements
  size_t n;
  size_t rawSize() const {
    return n * sizeof(T);
  }

  constexpr Array() : p(nullptr), n(0) {}
  constexpr Array(T* p, size_t n) : p(p), n(n) {}

  Array(const std::string& s) : p(reinterpret_cast<decltype(p)>(s.data())),
                                n(s.size()) {}

  using T_NON_CONST = typename std::remove_const<T>::type;
  Array(std::vector<T_NON_CONST>& v) : p(v.data()), n(v.size()) {}
  Array(const std::vector<T_NON_CONST>& v) : p(v.data()), n(v.size()) {}

  template <typename U, typename V = void>
  using ENABLE_CTOR = typename std::enable_if<std::is_convertible<U*, T*>::value, V>::type;
  template <typename U, typename = ENABLE_CTOR<U>>
  Array(const Array<U>& b) : p(b.p), n(b.n) {}

  bool operator==(const Array<T>& that) {
    return (that.n == n) && (that.p == p);
  }

  bool operator!=(const Array<T>& that) {
    return !(*this == that);
  }

  explicit operator std::vector<T_NON_CONST>() const {
    return {p, p + n};
  }
};

template <typename T>
using CArray = Array<const T>;
using Buffer = Array<uint8_t>;
using CBuffer = Array<const uint8_t>;
constexpr CBuffer nullb;

template <typename T>
struct UntrustedArray : public Array<T> {
  using Array<T>::Array;
  Array<T> load() {
    return *this;
  }

  template <typename U, typename V = void>
  using ENABLE_CTOR = typename std::enable_if<std::is_convertible<U*, T*>::value, V>::type;
  template <typename U, typename = ENABLE_CTOR<U>>
  UntrustedArray(const Array<U>& b) : Array<T>(b) {}
};

template <typename T>
CBuffer asCb(const T& o) {
  return {reinterpret_cast<const uint8_t*>(&o), sizeof(T)};
}

}  // namespace confmsg
