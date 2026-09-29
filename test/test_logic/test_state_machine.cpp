#include <unity.h>

#include "app/StateMachine.h"

using namespace app;

namespace {

State ready() {
  State s;
  apply(s, Trigger::BootDone, 1);
  apply(s, Trigger::WifiConnected, 1);
  return s;
}

void bootWithoutWifiGoesToSetup() {
  State s;
  TEST_ASSERT_TRUE(apply(s, Trigger::BootDone, 0));
  TEST_ASSERT_EQUAL(Phase::SetupAP, s.phase);
  TEST_ASSERT_EQUAL(ScreenId::Setup, screenFor(s));
}

void bootWithWifiConnectsThenNeedsLinkWithoutToken() {
  State s;
  apply(s, Trigger::BootDone, 1);
  TEST_ASSERT_EQUAL(Phase::WifiConnecting, s.phase);
  apply(s, Trigger::WifiConnected, 0);
  TEST_ASSERT_EQUAL(Phase::NeedSpotifyLink, s.phase);
  apply(s, Trigger::SpotifyLinked);
  TEST_ASSERT_EQUAL(Phase::Ready, s.phase);
  TEST_ASSERT_EQUAL(ScreenId::Idle, screenFor(s));
}

void threeWifiFailuresReturnToSetup() {
  State s;
  apply(s, Trigger::BootDone, 1);
  apply(s, Trigger::WifiFailed);
  apply(s, Trigger::WifiFailed);
  TEST_ASSERT_EQUAL(Phase::WifiConnecting, s.phase);
  apply(s, Trigger::WifiFailed);
  TEST_ASSERT_EQUAL(Phase::SetupAP, s.phase);
  TEST_ASSERT_EQUAL_UINT8(0, s.wifiFailures);
}

void wifiLossKeepsTheOverlay() {
  State s = ready();
  apply(s, Trigger::OpenOverlay, static_cast<int32_t>(Overlay::Menu));
  apply(s, Trigger::WifiLost);
  TEST_ASSERT_EQUAL(Phase::Offline, s.phase);
  TEST_ASSERT_EQUAL(ScreenId::Menu, screenFor(s));
  apply(s, Trigger::WifiRestored);
  TEST_ASSERT_EQUAL(Phase::Ready, s.phase);
}

void loginExpiryClosesOverlays() {
  State s = ready();
  apply(s, Trigger::OpenOverlay, static_cast<int32_t>(Overlay::Settings));
  apply(s, Trigger::LoginExpired);
  TEST_ASSERT_EQUAL(Phase::NeedSpotifyLink, s.phase);
  TEST_ASSERT_EQUAL(Overlay::None, s.overlay);
  TEST_ASSERT_EQUAL(ScreenId::LinkSpotify, screenFor(s));
}

void backFromMenuChildReturnsToMenu() {
  State s = ready();
  apply(s, Trigger::OpenOverlay, static_cast<int32_t>(Overlay::Menu));
  apply(s, Trigger::OpenOverlay, static_cast<int32_t>(Overlay::About));
  TEST_ASSERT_EQUAL(ScreenId::About, screenFor(s));
  apply(s, Trigger::Back);
  TEST_ASSERT_EQUAL(ScreenId::Menu, screenFor(s));
  apply(s, Trigger::Back);
  TEST_ASSERT_EQUAL(ScreenId::Idle, screenFor(s));
  TEST_ASSERT_FALSE(apply(s, Trigger::Back));
}

void backFromDirectOverlayReturnsHome() {
  State s = ready();
  apply(s, Trigger::OpenOverlay, static_cast<int32_t>(Overlay::Speakers));
  apply(s, Trigger::Back);
  TEST_ASSERT_EQUAL(ScreenId::Idle, screenFor(s));
}

void homeClosesEverything() {
  State s = ready();
  apply(s, Trigger::OpenOverlay, static_cast<int32_t>(Overlay::Menu));
  apply(s, Trigger::OpenOverlay, static_cast<int32_t>(Overlay::WriteCard));
  TEST_ASSERT_TRUE(apply(s, Trigger::Home));
  TEST_ASSERT_EQUAL(Overlay::None, s.overlay);
  TEST_ASSERT_EQUAL(Overlay::None, s.backTo);
}

void overlaysNeedReadyOrOffline() {
  State s;
  apply(s, Trigger::BootDone, 0);
  TEST_ASSERT_FALSE(apply(s, Trigger::OpenOverlay, static_cast<int32_t>(Overlay::Menu)));
  TEST_ASSERT_FALSE(apply(s, Trigger::OpenOverlay, 99));
}

void playbackSwitchesTheView() {
  State s = ready();
  apply(s, Trigger::PlaybackStarted);
  TEST_ASSERT_EQUAL(ScreenId::NowPlaying, screenFor(s));
  apply(s, Trigger::IdleTimeout);
  TEST_ASSERT_EQUAL(ScreenId::Idle, screenFor(s));
}

void triggersOutOfPhaseAreIgnored() {
  State s;
  TEST_ASSERT_FALSE(apply(s, Trigger::WifiConnected, 1));
  TEST_ASSERT_FALSE(apply(s, Trigger::WifiLost));
  TEST_ASSERT_EQUAL(Phase::Boot, s.phase);
}

void namesRoundTrip() {
  Trigger t;
  TEST_ASSERT_TRUE(parseTrigger("wifi_lost", t));
  TEST_ASSERT_EQUAL(Trigger::WifiLost, t);
  TEST_ASSERT_EQUAL_STRING("home", name(Trigger::Home));
  Overlay o;
  TEST_ASSERT_TRUE(parseOverlay("write_card", o));
  TEST_ASSERT_EQUAL(Overlay::WriteCard, o);
  TEST_ASSERT_FALSE(parseTrigger("nope", t));
  TEST_ASSERT_EQUAL_STRING("about", name(ScreenId::About));
}

}  // namespace

void runStateMachineTests() {
  RUN_TEST(bootWithoutWifiGoesToSetup);
  RUN_TEST(bootWithWifiConnectsThenNeedsLinkWithoutToken);
  RUN_TEST(threeWifiFailuresReturnToSetup);
  RUN_TEST(wifiLossKeepsTheOverlay);
  RUN_TEST(loginExpiryClosesOverlays);
  RUN_TEST(backFromMenuChildReturnsToMenu);
  RUN_TEST(backFromDirectOverlayReturnsHome);
  RUN_TEST(homeClosesEverything);
  RUN_TEST(overlaysNeedReadyOrOffline);
  RUN_TEST(playbackSwitchesTheView);
  RUN_TEST(triggersOutOfPhaseAreIgnored);
  RUN_TEST(namesRoundTrip);
}
