#pragma once

#include <cstdint>

// Timings, limits and task layout (PLAN.md §5.2, §9). Pins live in pins.h.
// Pure C++: included by host unit tests too.

namespace config {

constexpr const char *kFirmwareVersion = "0.1.0";

// Display (spike S1)
constexpr int16_t kScreenWidth = 320;  // Landscape (D5)
constexpr int16_t kScreenHeight = 172;
constexpr uint8_t kRotation = 1;
constexpr int32_t kLcdSpiHz = 80000000;
constexpr uint16_t kDrawBufferLines = 40;  // 2 x 320x40x2 B = 51 KB internal DMA RAM (§5.5)

// Tasks (§5.2)
struct TaskSpec {
  const char *name;
  uint8_t core;
  uint8_t priority;
  uint32_t stackBytes;
};
constexpr TaskSpec kUiTask{"ui", 1, 3, 8 * 1024};
constexpr TaskSpec kAppTask{"app", 1, 4, 6 * 1024};
constexpr uint32_t kWatchdogMs = 10000;
constexpr uint8_t kAppQueueLength = 32;
constexpr uint8_t kUiQueueLength = 32;
constexpr uint32_t kUiMaxSleepMs = 50;  // UI task wakes at least this often

// UI (§9)
constexpr uint32_t kToastMs = 2500;
constexpr int32_t kExtClickArea = 6;  // Extra hit zone around small targets (§9.1)

// BOOT button (§9.4)
constexpr uint32_t kButtonPollMs = 10;
constexpr uint32_t kButtonDebounceMs = 30;
constexpr uint32_t kButtonLongPressMs = 1500;
constexpr uint32_t kButtonFactoryHoldMs = 10000;

// App
constexpr uint8_t kWifiFailuresBeforeSetup = 3;  // §5.3: WifiConnecting -> SetupAP after 3 failures
constexpr uint32_t kHeapLogPeriodMs = 60000;     // §5.5

}  // namespace config
