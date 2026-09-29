#pragma once

#include "storage/Settings.h"

// Serial console for the dev build (ENABLE_SERIAL_CONSOLE): inspect and drive the
// app without the features that normally would (PLAN.md §12). 'help' lists commands.
namespace dev {

void begin(Settings &settings);
void poll();

}  // namespace dev
