#pragma once

#include <cstdint>

// Exponential backoff with jitter (Wi-Fi reconnect 1 s -> 60 s, API retries).
// Pure logic, unit-tested on the host.
class Backoff {
 public:
  Backoff(uint32_t initialMs, uint32_t maxMs, uint8_t jitterPercent = 20);

  // Delay before the next attempt: initialMs, then doubling up to maxMs, each
  // spread by +/- jitterPercent. `random` is any uniformly distributed 32-bit
  // value (esp_random() on the device). The result never exceeds maxMs.
  uint32_t next(uint32_t random);

  void reset();
  uint32_t attempts() const { return attempts_; }

 private:
  uint32_t initialMs_;
  uint32_t maxMs_;
  uint8_t jitterPercent_;
  uint32_t currentMs_ = 0;
  uint32_t attempts_ = 0;
};
