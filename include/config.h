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
constexpr uint32_t kHeapLogPeriodMs = 60000;  // §5.5

// Wi-Fi and portal (§8)
constexpr TaskSpec kNetTask{"net", 0, 2, 6 * 1024};
constexpr uint32_t kNetLoopMs = 100;
constexpr uint32_t kWifiAttemptMs = 15000;         // One network, one try
constexpr uint32_t kWifiBackoffStartMs = 1000;     // Between rounds of saved networks
constexpr uint32_t kWifiBackoffMaxMs = 60000;
constexpr uint32_t kWifiGiveUpMs = 10 * 60 * 1000;  // Saved networks failing this long -> setup AP (§8.2)
constexpr uint32_t kPortalGraceMs = 2 * 60 * 1000;  // AP stays up after setup succeeds, so the phone sees it
constexpr uint32_t kPortalIdleMs = 15 * 60 * 1000;  // "Set up network" AP stops after this long without a client
constexpr uint32_t kScanStaleMs = 10000;
constexpr const char *kApPrefix = "SpotifyCD-";
constexpr const char *kPortalIp = "192.168.4.1";
constexpr const char *kAdminUser = "admin";
constexpr const char *kAdminRealm = "Spotify CD Player";
constexpr const char *kNtpServer1 = "pool.ntp.org";
constexpr const char *kNtpServer2 = "time.google.com";

// OTA (§4.8, §11): a new image is marked valid once it has run this long
// with the network up (or the setup portal running). A reboot before that
// rolls back to the previous image.
constexpr uint32_t kOtaHealthyAfterMs = 30000;

}  // namespace config
