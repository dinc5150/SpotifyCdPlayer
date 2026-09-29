#pragma once

#include <functional>

// The only way into LVGL from outside the UI task (PLAN.md §5.1). Other tasks
// post closures; the UI task runs them before each LVGL timer pass. Capture by
// value: the closure runs later, on another task.
namespace ui {

using Update = std::function<void()>;

// Creates the queue. Call once before any post().
void bridgeBegin();

// Any task. Returns false (and drops the update) if the queue is full.
bool post(Update update);

// Starts the UI task (LVGL timer handler, bridge drain, watchdog).
void startTask();

}  // namespace ui
