#pragma once

#include <Arduino.h>

// Minimal line-based serial console shared by the Phase 0 spikes.
// Commands are registered with a name, a one-line help text and a handler
// that receives the rest of the line (arguments, already trimmed).

namespace console {

using Handler = void (*)(String args);

void add(const char *name, const char *help, Handler handler);
void printHelp();

// Call from loop(): reads available serial bytes and dispatches complete lines.
void poll();

// Splits off the first whitespace-separated word of `args` and returns it;
// `args` keeps the remainder, trimmed.
String nextWord(String &args);

}  // namespace console
