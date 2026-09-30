#include "app/StateMachine.h"

#include <cstddef>
#include <cstring>

namespace app {

namespace {

bool overlaysAllowed(Phase phase) { return phase == Phase::Ready || phase == Phase::Offline; }

bool setPhase(State &s, Phase phase) {
  if (s.phase == phase) return false;
  s.phase = phase;
  if (!overlaysAllowed(phase)) {
    s.overlay = Overlay::None;
    s.backTo = Overlay::None;
  }
  return true;
}

template <typename T, size_t N>
bool lookup(const char *const (&names)[N], const char *text, T &out) {
  for (size_t i = 0; i < N; i++) {
    if (strcmp(names[i], text) == 0) {
      out = static_cast<T>(i);
      return true;
    }
  }
  return false;
}

const char *const kPhaseNames[] = {"boot", "setup_ap", "wifi_connecting", "need_spotify_link", "ready", "offline"};
const char *const kOverlayNames[] = {"none", "menu", "speakers", "write_card", "wifi", "spotify", "settings", "about"};
const char *const kScreenNames[] = {"boot",       "setup",    "connecting", "link_spotify", "idle",
                                    "now_playing", "menu",     "speakers",   "write_card",   "wifi",
                                    "spotify",    "settings", "about"};
const char *const kTriggerNames[] = {"boot_done",      "wifi_submitted",   "wifi_failed", "set_up_network",
                                     "wifi_connected", "spotify_linked",   "login_expired", "wifi_lost",
                                     "wifi_restored",  "playback_started", "idle_timeout", "open_overlay",
                                     "back",           "home"};

}  // namespace

bool apply(State &s, Trigger trigger, int32_t arg) {
  switch (trigger) {
    case Trigger::BootDone:
      if (s.phase != Phase::Boot) return false;
      return setPhase(s, arg ? Phase::WifiConnecting : Phase::SetupAP);

    case Trigger::WifiSubmitted:
      if (s.phase != Phase::SetupAP) return false;
      return setPhase(s, Phase::WifiConnecting);

    // The Wi-Fi supervisor decides when to give up (§8.2): a network just
    // entered in the portal fails once; saved networks only after 10 min.
    case Trigger::WifiFailed:
      if (s.phase != Phase::WifiConnecting && s.phase != Phase::Offline) return false;
      return setPhase(s, Phase::SetupAP);

    case Trigger::SetUpNetwork:
      if (s.phase != Phase::WifiConnecting && s.phase != Phase::Offline) return false;
      return setPhase(s, Phase::SetupAP);

    // SetupAP keeps retrying saved networks in the background, so it can connect too.
    case Trigger::WifiConnected:
      if (s.phase != Phase::WifiConnecting && s.phase != Phase::SetupAP) return false;
      s.view = View::Idle;
      return setPhase(s, arg ? Phase::Ready : Phase::NeedSpotifyLink);

    case Trigger::SpotifyLinked:
      if (s.phase != Phase::NeedSpotifyLink) return false;
      s.view = View::Idle;
      return setPhase(s, Phase::Ready);

    case Trigger::LoginExpired:
      if (s.phase != Phase::Ready && s.phase != Phase::Offline) return false;
      return setPhase(s, Phase::NeedSpotifyLink);

    case Trigger::WifiLost:
      if (s.phase != Phase::Ready) return false;
      return setPhase(s, Phase::Offline);

    case Trigger::WifiRestored:
      if (s.phase != Phase::Offline) return false;
      return setPhase(s, Phase::Ready);

    case Trigger::PlaybackStarted:
      if (s.phase != Phase::Ready || s.view == View::NowPlaying) return false;
      s.view = View::NowPlaying;
      return true;

    case Trigger::IdleTimeout:
      if (s.view == View::Idle) return false;
      s.view = View::Idle;
      return true;

    case Trigger::OpenOverlay: {
      const auto target = static_cast<Overlay>(arg);
      if (!overlaysAllowed(s.phase) || target == Overlay::None || target > Overlay::About) return false;
      if (s.overlay == target) return false;
      s.backTo = s.overlay == Overlay::Menu ? Overlay::Menu : Overlay::None;
      s.overlay = target;
      return true;
    }

    case Trigger::Back:
      if (s.overlay == Overlay::None) return false;
      s.overlay = s.backTo;
      s.backTo = Overlay::None;
      return true;

    case Trigger::Home:
      if (s.overlay == Overlay::None) return false;
      s.overlay = Overlay::None;
      s.backTo = Overlay::None;
      return true;
  }
  return false;
}

ScreenId screenFor(const State &s) {
  switch (s.phase) {
    case Phase::Boot:
      return ScreenId::Boot;
    case Phase::SetupAP:
      return ScreenId::Setup;
    case Phase::WifiConnecting:
      return ScreenId::Connecting;
    case Phase::NeedSpotifyLink:
      return ScreenId::LinkSpotify;
    case Phase::Ready:
    case Phase::Offline:
      break;
  }
  switch (s.overlay) {
    case Overlay::None:
      return s.view == View::NowPlaying ? ScreenId::NowPlaying : ScreenId::Idle;
    case Overlay::Menu:
      return ScreenId::Menu;
    case Overlay::Speakers:
      return ScreenId::Speakers;
    case Overlay::WriteCard:
      return ScreenId::WriteCard;
    case Overlay::Wifi:
      return ScreenId::Wifi;
    case Overlay::Spotify:
      return ScreenId::Spotify;
    case Overlay::Settings:
      return ScreenId::Settings;
    case Overlay::About:
      return ScreenId::About;
  }
  return ScreenId::Idle;
}

const char *name(Phase phase) { return kPhaseNames[static_cast<size_t>(phase)]; }
const char *name(Overlay overlay) { return kOverlayNames[static_cast<size_t>(overlay)]; }
const char *name(ScreenId screen) { return kScreenNames[static_cast<size_t>(screen)]; }
const char *name(Trigger trigger) { return kTriggerNames[static_cast<size_t>(trigger)]; }

bool parseTrigger(const char *text, Trigger &out) { return lookup(kTriggerNames, text, out); }
bool parseOverlay(const char *text, Overlay &out) { return lookup(kOverlayNames, text, out); }

}  // namespace app
