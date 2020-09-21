// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <condition_variable>
#include <mutex>
#include <chrono>
#include <iostream>

namespace onnxruntime {
namespace server {

class CancellableTimer {
 public:
  template <class R, class P>
  void wait_for(const std::chrono::duration<R, P>& duration) {
    if (cancelled_) {
      throw std::logic_error("timer already cancelled");
    }
    start_time = std::chrono::system_clock::now();
    std::unique_lock<std::mutex> lock(m);
    cv.wait_for(lock, duration, [&] {
      if (cancelled_) {
        return true;
      }
      auto now = std::chrono::system_clock::now();
      auto actual_duration = now - start_time;
      bool spurious_wakeup = actual_duration < duration;
      return !spurious_wakeup;
    });
  }

  void cancel() {
    std::unique_lock<std::mutex> lock(m);
    cancelled_ = true;
    cv.notify_all();
  }

  bool cancelled() {
    return cancelled_;
  }

 private:
  std::condition_variable cv;
  std::mutex m;
  std::chrono::time_point<std::chrono::system_clock> start_time;
  bool cancelled_ = false;
};

}  // namespace server
}  // namespace onnxruntime
