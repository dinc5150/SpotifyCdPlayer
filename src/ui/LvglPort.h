#pragma once

#include <lvgl.h>

#include "Display.h"
#include "Touch.h"

// LVGL glue: display flush through Arduino_GFX, AXS5106L touch input, LVGL logs.
// Call begin() once from setup(); after ui::startTask() only the UI task touches LVGL.
namespace ui::port {

void begin(board::Display &display, board::Touch &touch);
lv_display_t *display();

// Draws everything that's pending, now (used for the first frame at boot).
void renderNow();

// Buttons call notePress() on LV_EVENT_PRESSED. With logging on, the next
// completed frame logs the time from the touch interrupt to pixels on the panel.
void notePress();
void setPressLatencyLog(bool on);

void setPerfOverlay(bool on);

}  // namespace ui::port
