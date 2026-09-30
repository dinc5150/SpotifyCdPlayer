#pragma once

#include <cstdint>

// Everything the app task reacts to arrives as an AppEvent on its queue
// (PLAN.md §5.1). Events are copied by value, so they stay trivially copyable.
// Workers add their event types here in later phases (Spotify, NFC, Wi-Fi).

namespace app {

enum class EventType : uint8_t {
  Start,            // Posted once when the app task starts
  Trigger,          // State machine trigger: code = Trigger, value = argument
  OpenOverlay,      // UI intent: code = Overlay
  Back,             // UI intent: the back button
  Button,           // BOOT button: code = ButtonEvent
  ConfirmResult,    // code = DialogId, value = 1 accepted / 0 cancelled
  SettingsChanged,  // Re-read settings and apply them
  DumpState,        // Dev console: log the app state
  Net,              // Wi-Fi supervisor: code = NetEvent, value = detail
  SetUpNetwork,     // UI intent: start the setup AP ("Set up network")
  StopSetUpNetwork, // UI intent: stop it
  Ota,              // Firmware upload: code = OtaEvent
  Maintenance,      // From the portal: code = Maintenance
};

enum class Maintenance : uint8_t { Reboot, FactoryReset };

enum class NetEvent : uint8_t {
  Connected,      // Station got an IP
  Disconnected,   // Lost the connection it had
  GaveUp,         // Saved networks have failed for 10 min (§8.2)
  SubmitStarted,  // Trying a network entered in the portal
  SubmitFailed,   // ...which failed; value = driver reason
  Changed,        // Anything else on the Wi-Fi screen changed (AP clients, portal on/off, time sync)
};

enum class OtaEvent : uint8_t { Started, Succeeded, Failed };

// Confirmation dialogs the app can ask for; the answer comes back as ConfirmResult.
enum class DialogId : uint8_t { FactoryReset, Test };

struct AppEvent {
  EventType type;
  uint8_t code = 0;
  int32_t value = 0;
};

// Any task, including the UI task. Returns false if the queue is full.
bool post(const AppEvent &event);

inline bool post(EventType type, uint8_t code = 0, int32_t value = 0) { return post(AppEvent{type, code, value}); }

}  // namespace app
