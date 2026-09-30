#pragma once

#include <Arduino.h>

// Why the device last restarted, for the log and the status page (PLAN.md §11).
namespace diag {

// Call early in setup(), after logging::begin(). Logs the reset reason and, if
// the last run crashed, a summary of the crash dump saved in flash (task, PC,
// backtrace for addr2line); the dump is then erased so it's reported once.
void logBootDiagnostics();

const String &resetReason();  // "power on", "crash", "task watchdog", ...
const String &lastCrash();    // Empty unless the last run crashed and left a dump

}  // namespace diag
