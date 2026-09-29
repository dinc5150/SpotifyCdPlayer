// Spike S1 — display, touch, LVGL, backlight, panel sleep, CPU clock, Wi-Fi power save.
// See docs/spikes.md for the procedure and what to record.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <lvgl.h>

#include "Backlight.h"
#include "Display.h"
#include "Touch.h"
#include "pins.h"
#include "Console.h"

namespace {

constexpr int16_t kWidth = 320;  // Landscape
constexpr int16_t kHeight = 172;
constexpr int32_t kDefaultLcdHz = 80000000;  // Verified stable in 1.6
constexpr size_t kBufferLines = 40;

board::Display display;
board::Backlight backlight;
board::Touch touch;

lv_display_t *lvDisplay = nullptr;
lv_obj_t *touchLabel = nullptr;
bool lvglPaused = false;
bool printTouches = false;
bool swallowUntilRelease = false;
uint8_t rotation = 1;
uint8_t activeBrightness = 80;
enum class Profile { Active, Dim, Off } profile = Profile::Active;

uint32_t tick() { return millis(); }

void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *pixels) {
  const uint32_t w = lv_area_get_width(area);
  const uint32_t h = lv_area_get_height(area);
  display.gfx()->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t *>(pixels), w, h);
  lv_display_flush_ready(disp);
}

void readTouch(lv_indev_t *, lv_indev_data_t *data) {
  board::TouchPoint point, raw;
  bool pressed = touch.read(point, &raw);
  // The touch that woke the screen is swallowed until the finger lifts (PLAN.md §9.1).
  if (swallowUntilRelease) {
    if (!pressed) swallowUntilRelease = false;
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (pressed) {
    data->point.x = point.x;
    data->point.y = point.y;
    data->state = LV_INDEV_STATE_PRESSED;
    if (touchLabel) lv_label_set_text_fmt(touchLabel, "x %u  y %u\nraw %u, %u", point.x, point.y, raw.x, raw.y);
    if (printTouches) Serial.printf("touch x=%u y=%u raw=%u,%u\n", point.x, point.y, raw.x, raw.y);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void onCornerClicked(lv_event_t *e) {
  const char *name = static_cast<const char *>(lv_event_get_user_data(e));
  Serial.printf("Button %s clicked\n", name);
}

void addCornerButton(lv_obj_t *parent, lv_align_t align, const char *name) {
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_set_size(button, 72, 54);  // Primary target size (PLAN.md §9.1)
  lv_obj_align(button, align, 0, 0);
  lv_obj_add_event_cb(button, onCornerClicked, LV_EVENT_CLICKED, const_cast<char *>(name));
  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, name);
  lv_obj_center(label);
}

void buildTestScreen() {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_pad_all(screen, 4, 0);
  addCornerButton(screen, LV_ALIGN_TOP_LEFT, "TL");
  addCornerButton(screen, LV_ALIGN_TOP_RIGHT, "TR");
  addCornerButton(screen, LV_ALIGN_BOTTOM_LEFT, "BL");
  addCornerButton(screen, LV_ALIGN_BOTTOM_RIGHT, "BR");

  lv_obj_t *qr = lv_qrcode_create(screen);
  lv_qrcode_set_size(qr, 96);
  lv_qrcode_set_dark_color(qr, lv_color_black());
  lv_qrcode_set_light_color(qr, lv_color_white());
  const char *wifiQr = "WIFI:T:WPA;S:CardPlayer-TEST;P:12345678;;";
  lv_qrcode_update(qr, wifiQr, strlen(wifiQr));
  lv_obj_align(qr, LV_ALIGN_CENTER, 0, -14);

  touchLabel = lv_label_create(screen);
  lv_label_set_text(touchLabel, "Touch anywhere");
  lv_obj_set_style_text_align(touchLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(touchLabel, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void startLvgl() {
  lv_init();
  lv_tick_set_cb(tick);
  lvDisplay = lv_display_create(kWidth, kHeight);
  lv_display_set_flush_cb(lvDisplay, flush);
  const size_t bytes = kWidth * kBufferLines * sizeof(uint16_t);
  void *buf1 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void *buf2 = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_display_set_buffers(lvDisplay, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readTouch);
  buildTestScreen();
}

// Raw Arduino_GFX pattern to check the landscape column/row offsets:
// the red 1 px border must be fully visible on all four edges.
void drawOffsetPattern() {
  Arduino_GFX *gfx = display.gfx();
  gfx->fillScreen(RGB565_BLACK);
  gfx->drawRect(0, 0, gfx->width(), gfx->height(), RGB565_RED);
  gfx->fillRect(0, 0, 10, 10, RGB565_GREEN);                                  // top-left
  gfx->fillRect(gfx->width() - 10, 0, 10, 10, RGB565_BLUE);                   // top-right
  gfx->fillRect(0, gfx->height() - 10, 10, 10, RGB565_YELLOW);                // bottom-left
  gfx->fillRect(gfx->width() - 10, gfx->height() - 10, 10, 10, RGB565_WHITE); // bottom-right
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(20, 20);
  gfx->printf("S1 offsets  %dx%d  rot %u", gfx->width(), gfx->height(), rotation);
  gfx->setCursor(20, 36);
  gfx->print("Red border visible on all 4 edges?");
  gfx->setCursor(20, 52);
  gfx->print("TL green, TR blue, BL yellow, BR white");
}

void printHeap() {
  Serial.printf("heap internal free %u (min %u), psram free %u (min %u)\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}

// Wake sequence from PLAN.md §9.5: CPU and Wi-Fi first, panel, redraw, then backlight.
void wakeToActive(const char *reason) {
  const uint32_t start = millis();
  setCpuFrequencyMhz(240);
  if (WiFi.isConnected()) WiFi.setSleep(WIFI_PS_MIN_MODEM);
  display.wake();
  lvglPaused = false;
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(lvDisplay);
  backlight.set(activeBrightness);
  profile = Profile::Active;
  Serial.printf("Woke (%s) in %lu ms\n", reason, millis() - start);
}

void applyProfile(Profile target) {
  switch (target) {
    case Profile::Active:
      wakeToActive("command");
      break;
    case Profile::Dim:
      if (profile == Profile::Off) wakeToActive("command");
      backlight.set(20);
      profile = Profile::Dim;
      Serial.println("Profile: Dim");
      break;
    case Profile::Off:
      backlight.set(0);
      lvglPaused = true;
      display.sleep();
      if (WiFi.isConnected()) WiFi.setSleep(WIFI_PS_MAX_MODEM);
      setCpuFrequencyMhz(80);
      profile = Profile::Off;
      Serial.printf("Profile: Screen off (CPU %lu MHz). Touch or BOOT wakes.\n", getCpuFrequencyMhz());
      break;
  }
}

void registerCommands() {
  console::add("bl", "<0-100>  backlight percent", [](String args) {
    activeBrightness = args.toInt();
    backlight.set(activeBrightness);
    Serial.printf("Backlight %u%%\n", backlight.get());
  });
  console::add("rot", "<1|3>  landscape rotation (3 = flipped 180)", [](String args) {
    rotation = args.toInt() == 3 ? 3 : 1;
    display.setRotation(rotation);
    touch.setTransform(rotation == 1 ? board::Touch::kLandscape : board::Touch::Transform{true, true, true},
                       kWidth, kHeight);
    lv_obj_invalidate(lv_screen_active());
    Serial.printf("Rotation %u\n", rotation);
  });
  console::add("txf", "<swap> <mirrorX> <mirrorY>  touch transform, e.g. txf 1 0 0", [](String args) {
    board::Touch::Transform t{};
    t.swapXY = console::nextWord(args).toInt();
    t.mirrorX = console::nextWord(args).toInt();
    t.mirrorY = console::nextWord(args).toInt();
    touch.setTransform(t, kWidth, kHeight);
    Serial.printf("Touch transform swap=%d mirrorX=%d mirrorY=%d\n", t.swapXY, t.mirrorX, t.mirrorY);
  });
  console::add("touchlog", "<on|off>  print every touch sample", [](String args) {
    printTouches = args == "on";
  });
  console::add("pattern", "raw offset pattern (pauses LVGL; 'resume' to return)", [](String) {
    lvglPaused = true;
    drawOffsetPattern();
  });
  console::add("resume", "resume LVGL after 'pattern' or 'bench'", [](String) {
    lvglPaused = false;
    lv_obj_invalidate(lv_screen_active());
  });
  console::add("bench", "time 10 full-screen fills and an LVGL full redraw", [](String) {
    lvglPaused = true;
    uint32_t start = micros();
    for (int i = 0; i < 10; i++) display.gfx()->fillScreen(i % 2 ? RGB565_BLUE : RGB565_BLACK);
    Serial.printf("Raw full-screen fill: %.1f ms each\n", (micros() - start) / 10000.0f);
    lvglPaused = false;
    start = micros();
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(lvDisplay);
    Serial.printf("LVGL full redraw: %.1f ms\n", (micros() - start) / 1000.0f);
  });
  // The SPI bus can't be re-clocked cleanly once started, so save and reboot.
  console::add("lcdhz", "<MHz>  save the LCD SPI clock (e.g. 40, 80) and reboot", [](String args) {
    int32_t hz = args.toInt() * 1000000;
    if (hz <= 0) hz = kDefaultLcdHz;
    Preferences prefs;
    prefs.begin("s1", false);
    prefs.putInt("lcdhz", hz);
    prefs.end();
    Serial.printf("LCD clock %ld Hz saved; rebooting...\n", hz);
    Serial.flush();
    delay(100);
    ESP.restart();
  });
  console::add("sleep", "panel sleep test: sleep 3 s, then time the wake sequence", [](String) {
    applyProfile(Profile::Off);
    delay(3000);
    wakeToActive("sleep test");
  });
  console::add("profile", "<active|dim|off>  apply a power profile (measure current in each)", [](String args) {
    if (args == "active") applyProfile(Profile::Active);
    else if (args == "dim") applyProfile(Profile::Dim);
    else if (args == "off") applyProfile(Profile::Off);
    else Serial.println("Use: profile active|dim|off");
  });
  console::add("cpu", "<80|160|240>  set CPU clock", [](String args) {
    setCpuFrequencyMhz(args.toInt());
    Serial.printf("CPU %lu MHz\n", getCpuFrequencyMhz());
  });
  console::add("wifi", "<ssid> <password> | off  join Wi-Fi (for power measurements)", [](String args) {
    if (args == "off") {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      Serial.println("Wi-Fi off");
      return;
    }
    String ssid = console::nextWord(args);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), args.c_str());
    Serial.printf("Joining %s", ssid.c_str());
    for (int i = 0; i < 40 && !WiFi.isConnected(); i++) {
      delay(250);
      Serial.print('.');
    }
    Serial.printf("\n%s  IP %s  RSSI %d\n", WiFi.isConnected() ? "Connected" : "FAILED",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  });
  console::add("ps", "<none|min|max>  Wi-Fi power save mode", [](String args) {
    wifi_ps_type_t mode = args == "none" ? WIFI_PS_NONE : args == "max" ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM;
    Serial.printf("Wi-Fi power save %s: %s\n", args.c_str(), WiFi.setSleep(mode) ? "ok" : "FAILED");
  });
  console::add("perf", "<on|off>  LVGL FPS/CPU overlay", [](String args) {
    if (args == "off") lv_sysmon_hide_performance(lvDisplay);
    else lv_sysmon_show_performance(lvDisplay);
  });
  console::add("heap", "print free internal RAM and PSRAM", [](String) { printHeap(); });
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);  // Give USB CDC time to enumerate
  Serial.println("\n=== Spike S1: display, touch, LVGL, power ===");

  pinMode(pins::BOOT_BUTTON, INPUT_PULLUP);
  backlight.begin(activeBrightness);

  Preferences prefs;
  prefs.begin("s1", true);
  const int32_t lcdHz = prefs.getInt("lcdhz", kDefaultLcdHz);
  prefs.end();
  if (!display.begin(rotation, lcdHz)) Serial.println("Display begin FAILED");
  Serial.printf("LCD SPI clock %ld Hz\n", lcdHz);
  Serial.printf("Display %dx%d, rotation %u\n", display.width(), display.height(), rotation);
  drawOffsetPattern();

  bool touchOk = touch.begin(Wire, kWidth, kHeight, board::Touch::kLandscape);
  const uint8_t *id = touch.chipId();
  Serial.printf("Touch %s, chip id %02X %02X %02X\n", touchOk ? "ok" : "NOT RESPONDING", id[0], id[1], id[2]);

  printHeap();
  delay(4000);  // Leave the offset pattern up long enough to check

  startLvgl();
  registerCommands();
  Serial.println("LVGL running. Type 'help' for commands.");
}

void loop() {
  console::poll();

  if (profile == Profile::Off) {
    // Wake sources while dark: touch interrupt or BOOT button. The touch that wakes is swallowed.
    if (board::Touch::interruptPending() || digitalRead(pins::BOOT_BUTTON) == LOW) {
      swallowUntilRelease = board::Touch::interruptPending();
      wakeToActive(digitalRead(pins::BOOT_BUTTON) == LOW ? "BOOT" : "touch");
      while (digitalRead(pins::BOOT_BUTTON) == LOW) delay(10);
    }
    delay(20);
    return;
  }

  if (!lvglPaused) lv_timer_handler();
  delay(5);
}
