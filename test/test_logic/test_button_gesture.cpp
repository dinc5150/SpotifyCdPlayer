#include <unity.h>

#include "util/ButtonGesture.h"

namespace {

constexpr uint32_t kDebounce = 30;
constexpr uint32_t kLong = 1500;
constexpr uint32_t kFactory = 10000;

// Holds `pressed` from `from` to `to` (exclusive) in 10 ms steps; returns the
// events seen, in order, packed one per byte (low byte first).
uint32_t drive(ButtonGesture &g, bool pressed, uint32_t from, uint32_t to) {
  uint32_t events = 0;
  int shift = 0;
  for (uint32_t t = from; t < to; t += 10) {
    const ButtonEvent e = g.update(pressed, t);
    if (e != ButtonEvent::None && shift < 32) {
      events |= static_cast<uint32_t>(e) << shift;
      shift += 8;
    }
  }
  return events;
}

constexpr uint32_t ev(ButtonEvent e) { return static_cast<uint32_t>(e); }

void shortPressOnRelease() {
  ButtonGesture g(kDebounce, kLong, kFactory);
  TEST_ASSERT_EQUAL_UINT32(0, drive(g, true, 0, 300));
  TEST_ASSERT_TRUE(g.pressed());
  TEST_ASSERT_EQUAL_UINT32(ev(ButtonEvent::ShortPress), drive(g, false, 300, 400));
}

void bounceIsIgnored() {
  ButtonGesture g(kDebounce, kLong, kFactory);
  // 20 ms blips never last the debounce time.
  for (uint32_t t = 0; t < 200; t += 40) {
    TEST_ASSERT_EQUAL(ButtonEvent::None, g.update(true, t));
    TEST_ASSERT_EQUAL(ButtonEvent::None, g.update(false, t + 20));
  }
  TEST_ASSERT_FALSE(g.pressed());
}

void longPressFiresOnceAndSuppressesShort() {
  ButtonGesture g(kDebounce, kLong, kFactory);
  TEST_ASSERT_EQUAL_UINT32(ev(ButtonEvent::LongPress), drive(g, true, 0, 3000));
  TEST_ASSERT_EQUAL_UINT32(0, drive(g, false, 3000, 3100));
}

void factoryHoldFollowsLongPress() {
  ButtonGesture g(kDebounce, kLong, kFactory);
  const uint32_t expected = ev(ButtonEvent::LongPress) | ev(ButtonEvent::FactoryHold) << 8;
  TEST_ASSERT_EQUAL_UINT32(expected, drive(g, true, 0, 12000));
  TEST_ASSERT_EQUAL_UINT32(0, drive(g, false, 12000, 12100));
}

void secondPressWorksAfterLongPress() {
  ButtonGesture g(kDebounce, kLong, kFactory);
  drive(g, true, 0, 2000);
  drive(g, false, 2000, 2100);
  drive(g, true, 2100, 2300);
  TEST_ASSERT_EQUAL_UINT32(ev(ButtonEvent::ShortPress), drive(g, false, 2300, 2400));
}

}  // namespace

void runButtonGestureTests() {
  RUN_TEST(shortPressOnRelease);
  RUN_TEST(bounceIsIgnored);
  RUN_TEST(longPressFiresOnceAndSuppressesShort);
  RUN_TEST(factoryHoldFollowsLongPress);
  RUN_TEST(secondPressWorksAfterLongPress);
}
