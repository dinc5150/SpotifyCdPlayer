#include <unity.h>

#include "storage/SettingsRules.h"

namespace {

void defaultsAreValid() {
  UiSettings s;
  TEST_ASSERT_FALSE(settings_rules::sanitize(s));
}

void brightnessIsClamped() {
  UiSettings s;
  s.brightness = 0;
  TEST_ASSERT_TRUE(settings_rules::sanitize(s));
  TEST_ASSERT_EQUAL_UINT8(settings_rules::kMinBrightness, s.brightness);
  s.brightness = 250;
  settings_rules::sanitize(s);
  TEST_ASSERT_EQUAL_UINT8(100, s.brightness);
}

void unknownTimersFallBackToDefaults() {
  UiSettings s;
  s.dimSeconds = 45;
  s.offSeconds = 1;
  TEST_ASSERT_TRUE(settings_rules::sanitize(s));
  TEST_ASSERT_EQUAL_UINT16(UiSettings().dimSeconds, s.dimSeconds);
  TEST_ASSERT_EQUAL_UINT16(UiSettings().offSeconds, s.offSeconds);
}

void neverIsAValidOffTime() {
  UiSettings s;
  s.offSeconds = 0;
  TEST_ASSERT_FALSE(settings_rules::sanitize(s));
  TEST_ASSERT_EQUAL_UINT16(0, s.offSeconds);
}

void unknownSameCardFallsBack() {
  UiSettings s;
  s.sameCard = static_cast<SameCard>(7);
  TEST_ASSERT_TRUE(settings_rules::sanitize(s));
  TEST_ASSERT_EQUAL(SameCard::IgnoreOrResume, s.sameCard);
}

}  // namespace

void runSettingsRulesTests() {
  RUN_TEST(defaultsAreValid);
  RUN_TEST(brightnessIsClamped);
  RUN_TEST(unknownTimersFallBackToDefaults);
  RUN_TEST(neverIsAValidOffTime);
  RUN_TEST(unknownSameCardFallsBack);
}
