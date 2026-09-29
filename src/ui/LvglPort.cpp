#include "ui/LvglPort.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include "config.h"
#include "util/Log.h"

namespace ui::port {

namespace {

constexpr const char *kTag = "ui";

board::Display *panel = nullptr;
board::Touch *touchpad = nullptr;
lv_display_t *lvDisplay = nullptr;

bool logPressLatency = false;
bool pressPending = false;
bool wasPressed = false;
int64_t touchDownUs = 0;

uint32_t tick() { return millis(); }

void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *pixels) {
  panel->gfx()->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pixels),
                                   lv_area_get_width(area), lv_area_get_height(area));
  if (pressPending && lv_display_flush_is_last(disp)) {
    pressPending = false;
    LOG_I(kTag, "Press to pixels: %.1f ms", (esp_timer_get_time() - touchDownUs) / 1000.0f);
  }
  lv_display_flush_ready(disp);
}

void readTouch(lv_indev_t *, lv_indev_data_t *data) {
  board::TouchPoint point;
  const bool pressed = touchpad->read(point);
  if (pressed && !wasPressed) {
    // The interrupt marks the moment the finger landed; fall back to now if it's stale.
    const int64_t now = esp_timer_get_time();
    const int64_t irq = board::Touch::lastInterruptUs();
    touchDownUs = now - irq < 100000 ? irq : now;
  }
  wasPressed = pressed;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (pressed) {
    data->point.x = point.x;
    data->point.y = point.y;
  }
}

void logFromLvgl(lv_log_level_t level, const char *text) {
  // LVGL lines already end with a newline and carry their own level prefix.
  const size_t length = strcspn(text, "\n");
  const logging::Level ours = level >= LV_LOG_LEVEL_ERROR  ? logging::Level::Error
                              : level >= LV_LOG_LEVEL_WARN ? logging::Level::Warn
                                                           : logging::Level::Debug;
  logging::write(ours, "lvgl", "%.*s", static_cast<int>(length), text);
}

}  // namespace

void begin(board::Display &display, board::Touch &touch) {
  panel = &display;
  touchpad = &touch;

  lv_init();
  lv_tick_set_cb(tick);
  lv_log_register_print_cb(logFromLvgl);

  lvDisplay = lv_display_create(config::kScreenWidth, config::kScreenHeight);
  lv_display_set_flush_cb(lvDisplay, flush);
  const size_t bytes = config::kScreenWidth * config::kDrawBufferLines * sizeof(uint16_t);
  void *buf1 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void *buf2 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!buf1 || !buf2) LOG_E(kTag, "Draw buffer allocation failed");
  lv_display_set_buffers(lvDisplay, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  setPerfOverlay(false);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readTouch);
}

lv_display_t *display() { return lvDisplay; }

void renderNow() { lv_refr_now(lvDisplay); }

void notePress() {
  if (logPressLatency) pressPending = true;
}

void setPressLatencyLog(bool on) { logPressLatency = on; }

void setPerfOverlay(bool on) {
#if LV_USE_PERF_MONITOR
  if (on) {
    lv_sysmon_show_performance(lvDisplay);
  } else {
    lv_sysmon_hide_performance(lvDisplay);
  }
#else
  LV_UNUSED(on);
#endif
}

}  // namespace ui::port
