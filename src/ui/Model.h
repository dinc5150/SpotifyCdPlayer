#pragma once

#include <Arduino.h>

#include "app/StateMachine.h"
#include "net/WifiSupervisor.h"

// What the app wants on screen. The app task builds it and posts a copy to the
// UI task; screens read it when built and in refresh(). Fields grow per phase.
namespace ui {

struct Model {
  app::State state;
  String deviceName;
  String speakerName;  // Empty = no target chosen yet
  String statusLine;   // Short status under the Idle prompt
  net::Status net;
};

}  // namespace ui
