#include "ui/UiBridge.h"

#include <Arduino.h>
#include <esp_task_wdt.h>
#include <lvgl.h>

#include "config.h"
#include "util/Log.h"

namespace ui {

namespace {

constexpr const char *kTag = "ui";

QueueHandle_t queue = nullptr;
TaskHandle_t task = nullptr;

void drain() {
  Update *update = nullptr;
  while (xQueueReceive(queue, &update, 0) == pdTRUE) {
    (*update)();
    delete update;
  }
}

void run(void *) {
  esp_task_wdt_add(nullptr);
  for (;;) {
    drain();
    uint32_t sleepMs = lv_timer_handler();
    esp_task_wdt_reset();
    if (sleepMs > config::kUiMaxSleepMs) sleepMs = config::kUiMaxSleepMs;
    // post() wakes us early, so updates from other tasks show without waiting.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(sleepMs ? sleepMs : 1));
  }
}

}  // namespace

void bridgeBegin() {
  if (!queue) queue = xQueueCreate(config::kUiQueueLength, sizeof(Update *));
}

bool post(Update update) {
  auto *boxed = new Update(std::move(update));
  if (xQueueSend(queue, &boxed, 0) != pdTRUE) {
    delete boxed;
    LOG_W(kTag, "UI queue full; update dropped");
    return false;
  }
  if (task) xTaskNotifyGive(task);
  return true;
}

void startTask() {
  const config::TaskSpec &spec = config::kUiTask;
  xTaskCreatePinnedToCore(run, spec.name, spec.stackBytes, nullptr, spec.priority, &task, spec.core);
}

}  // namespace ui
