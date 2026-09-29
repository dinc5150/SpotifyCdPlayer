#pragma once

#include "util/ButtonGesture.h"

namespace hal {

// BOOT button on GPIO0 (active low), polled every config::kButtonPollMs.
// Holding it during reset still enters download mode; after boot it's ours.
class BootButton {
 public:
  BootButton();
  void begin();
  ButtonEvent poll();
  bool pressed() const { return gesture_.pressed(); }

 private:
  ButtonGesture gesture_;
};

}  // namespace hal
