#include <unity.h>

#include "util/Backoff.h"

namespace {

void doublesUpToTheCapWithoutJitter() {
  Backoff b(1000, 60000, 0);
  const uint32_t expected[] = {1000, 2000, 4000, 8000, 16000, 32000, 60000, 60000};
  for (uint32_t e : expected) TEST_ASSERT_EQUAL_UINT32(e, b.next(12345));
  TEST_ASSERT_EQUAL_UINT32(8, b.attempts());
}

void jitterSpansPlusMinusPercent() {
  Backoff low(1000, 60000, 20), mid(1000, 60000, 20), high(1000, 60000, 20);
  TEST_ASSERT_EQUAL_UINT32(800, low.next(0));
  TEST_ASSERT_EQUAL_UINT32(1000, mid.next(200));
  TEST_ASSERT_EQUAL_UINT32(1200, high.next(400));
}

void jitterNeverExceedsTheCap() {
  Backoff b(60000, 60000, 20);
  for (uint32_t r = 0; r < 30000; r += 997) TEST_ASSERT_LESS_OR_EQUAL_UINT32(60000, b.next(r));
}

void resetStartsOver() {
  Backoff b(1000, 60000, 0);
  b.next(0);
  b.next(0);
  b.reset();
  TEST_ASSERT_EQUAL_UINT32(0, b.attempts());
  TEST_ASSERT_EQUAL_UINT32(1000, b.next(0));
}

void initialAboveCapIsClamped() {
  Backoff b(90000, 60000, 0);
  TEST_ASSERT_EQUAL_UINT32(60000, b.next(0));
}

}  // namespace

void runBackoffTests() {
  RUN_TEST(doublesUpToTheCapWithoutJitter);
  RUN_TEST(jitterSpansPlusMinusPercent);
  RUN_TEST(jitterNeverExceedsTheCap);
  RUN_TEST(resetStartsOver);
  RUN_TEST(initialAboveCapIsClamped);
}
