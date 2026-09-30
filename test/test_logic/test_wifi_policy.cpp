#include <unity.h>

#include "net/WifiPolicy.h"

using namespace wifi_policy;

namespace {

void strongestSeenFirstThenSavedOrder() {
  const std::vector<Network> saved = {{"home", ""}, {"hidden", ""}, {"office", ""}, {"away", ""}};
  const std::vector<ScanResult> scan = {{"office", -50}, {"home", -70}, {"cafe", -40}, {"home", -60}};
  const std::vector<size_t> order = connectOrder(saved, scan);
  const std::vector<size_t> expected = {2, 0, 1, 3};  // office -50, home -60, then hidden, away
  TEST_ASSERT_EQUAL_size_t(expected.size(), order.size());
  for (size_t i = 0; i < expected.size(); i++) TEST_ASSERT_EQUAL_size_t(expected[i], order[i]);
}

void emptyScanKeepsSavedOrder() {
  const std::vector<Network> saved = {{"a", ""}, {"b", ""}};
  const std::vector<size_t> order = connectOrder(saved, {});
  TEST_ASSERT_EQUAL_size_t(0, order[0]);
  TEST_ASSERT_EQUAL_size_t(1, order[1]);
}

void rememberPutsNewestFirstAndCaps() {
  std::vector<Network> saved = {{"a", "1"}, {"b", "2"}, {"c", "3"}};
  remember(saved, {"b", "new"}, 3);
  TEST_ASSERT_EQUAL_STRING("b", saved[0].ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("new", saved[0].password.c_str());
  TEST_ASSERT_EQUAL_size_t(3, saved.size());
  remember(saved, {"d", "4"}, 3);
  TEST_ASSERT_EQUAL_STRING("d", saved[0].ssid.c_str());
  TEST_ASSERT_EQUAL_size_t(3, saved.size());
  TEST_ASSERT_EQUAL_STRING("a", saved[2].ssid.c_str());  // "c" dropped
}

void forgetRemovesBySsid() {
  std::vector<Network> saved = {{"a", ""}, {"b", ""}};
  TEST_ASSERT_TRUE(forget(saved, "a"));
  TEST_ASSERT_FALSE(forget(saved, "zzz"));
  TEST_ASSERT_EQUAL_size_t(1, saved.size());
}

void hostnameIsSlugged() {
  TEST_ASSERT_EQUAL_STRING("spotify-cd", hostname("").c_str());
  TEST_ASSERT_EQUAL_STRING("spotify-cd", hostname("!!!").c_str());
  TEST_ASSERT_EQUAL_STRING("spotify-cd", hostname("spotify-cd").c_str());
  TEST_ASSERT_EQUAL_STRING("kitchen", hostname("Kitchen.LOCAL").c_str());  // Typed suffix dropped
  TEST_ASSERT_EQUAL_STRING("spotify-cd-player", hostname("Spotify CD Player").c_str());
  TEST_ASSERT_EQUAL_STRING("kids-room-2", hostname("  Kid's -- Room 2!").c_str());
  TEST_ASSERT_EQUAL_size_t(32, hostname(std::string(40, 'x')).size());
}

void failuresAreDescribed() {
  TEST_ASSERT_EQUAL_STRING("Wrong password?", describeFailure(15));
  TEST_ASSERT_EQUAL_STRING("Network not found", describeFailure(201));
  TEST_ASSERT_EQUAL_STRING("Couldn't connect", describeFailure(99));
}

void signalBands() {
  TEST_ASSERT_EQUAL_STRING("Excellent", signalWords(-40));
  TEST_ASSERT_EQUAL_STRING("Good", signalWords(-60));
  TEST_ASSERT_EQUAL_STRING("Fair", signalWords(-70));
  TEST_ASSERT_EQUAL_STRING("Weak", signalWords(-80));
}

void wifiQrEscapes() {
  TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:SpotifyCD-AB12;P:abc23xyz;;", wifiQr("SpotifyCD-AB12", "abc23xyz").c_str());
  TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:a\\;b;P:p\\:q\\\\;;", wifiQr("a;b", "p:q\\").c_str());
}

}  // namespace

void runWifiPolicyTests() {
  RUN_TEST(signalBands);
  RUN_TEST(wifiQrEscapes);
  RUN_TEST(strongestSeenFirstThenSavedOrder);
  RUN_TEST(emptyScanKeepsSavedOrder);
  RUN_TEST(rememberPutsNewestFirstAndCaps);
  RUN_TEST(forgetRemovesBySsid);
  RUN_TEST(hostnameIsSlugged);
  RUN_TEST(failuresAreDescribed);
}
