#pragma once

#include <cstdint>

// App state machine from PLAN.md §5.3. Pure logic, unit-tested on the host.
// Only AppController calls apply(); everything else sees copies of State.

namespace app {

enum class Phase : uint8_t { Boot, SetupAP, WifiConnecting, NeedSpotifyLink, Ready, Offline };

// What Ready shows when no overlay is open.
enum class View : uint8_t { Idle, NowPlaying };

// Screens that open on top of Ready (and Offline). WriteCard matters to policy:
// while it is open, card taps never trigger playback.
enum class Overlay : uint8_t { None, Menu, Speakers, WriteCard, Wifi, Spotify, Settings, About };

enum class ScreenId : uint8_t {
  Boot,
  Setup,
  Connecting,
  LinkSpotify,
  Idle,
  NowPlaying,
  Menu,
  Speakers,
  WriteCard,
  Wifi,
  Spotify,
  Settings,
  About,
};

enum class Trigger : uint8_t {
  BootDone,       // arg: 1 if Wi-Fi networks are saved
  WifiSubmitted,  // credentials entered in the setup portal
  WifiFailed,
  SetUpNetwork,   // user chose "Set up network"
  WifiConnected,  // arg: 1 if a refresh token is stored
  SpotifyLinked,
  LoginExpired,   // invalid_grant
  WifiLost,
  WifiRestored,
  PlaybackStarted,
  IdleTimeout,    // nothing playing for 10 min
  OpenOverlay,    // arg: Overlay
  Back,
  Home,
};

struct State {
  Phase phase = Phase::Boot;
  View view = View::Idle;
  Overlay overlay = Overlay::None;
  Overlay backTo = Overlay::None;  // Where Back goes: Menu if the overlay was opened from it
  uint8_t wifiFailures = 0;
};

// Applies a trigger. Returns true if the state changed; triggers that don't
// apply in the current phase are ignored.
bool apply(State &state, Trigger trigger, int32_t arg = 0);

ScreenId screenFor(const State &state);

const char *name(Phase phase);
const char *name(Overlay overlay);
const char *name(ScreenId screen);
const char *name(Trigger trigger);

// Reverse lookups for the dev console. Return false if the name is unknown.
bool parseTrigger(const char *text, Trigger &out);
bool parseOverlay(const char *text, Overlay &out);

}  // namespace app
