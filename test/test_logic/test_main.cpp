// Unit tests for the pure-logic modules (PLAN.md §12).
// Runs on the host (pio test -e native) or on the board (pio test -e test_device).

#include <unity.h>

void runBackoffTests();
void runButtonGestureTests();
void runSettingsRulesTests();
void runStateMachineTests();
void runWifiPolicyTests();

void setUp() {}
void tearDown() {}

static int runAll() {
  UNITY_BEGIN();
  runBackoffTests();
  runButtonGestureTests();
  runSettingsRulesTests();
  runStateMachineTests();
  runWifiPolicyTests();
  return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
void setup() {
  delay(2000);  // Let USB CDC enumerate before the runner starts reading
  runAll();
}
void loop() {}
#else
int main() { return runAll(); }
#endif
