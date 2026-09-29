#pragma once

#include "Backlight.h"
#include "app/Events.h"
#include "storage/Settings.h"

// The app task: single owner of the app state (PLAN.md §5.1). It reacts to
// AppEvents from the UI, the BOOT button and (from Phase 2) the workers, and
// tells the UI what to show by posting ui::Model copies.
namespace app {

void begin(Settings &settings, board::Backlight &backlight);
void startTask();

}  // namespace app
