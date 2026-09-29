#include "util/Backoff.h"

#include <algorithm>

Backoff::Backoff(uint32_t initialMs, uint32_t maxMs, uint8_t jitterPercent)
    : initialMs_(std::min(initialMs, maxMs)), maxMs_(maxMs), jitterPercent_(std::min<uint8_t>(jitterPercent, 100)) {}

uint32_t Backoff::next(uint32_t random) {
  currentMs_ = attempts_ == 0 ? initialMs_
                              : static_cast<uint32_t>(std::min<uint64_t>(static_cast<uint64_t>(currentMs_) * 2, maxMs_));
  attempts_++;

  const uint32_t span = static_cast<uint64_t>(currentMs_) * jitterPercent_ / 100;
  if (span == 0) return currentMs_;
  const uint64_t delay = static_cast<uint64_t>(currentMs_) - span + random % (2 * static_cast<uint64_t>(span) + 1);
  return static_cast<uint32_t>(std::min<uint64_t>(delay, maxMs_));
}

void Backoff::reset() {
  currentMs_ = 0;
  attempts_ = 0;
}
