// Spotify CD Player — NFC Spotify controller (PLAN.md).
// Boot order: log -> settings -> display -> first frame -> tasks -> touch.
// After setup() the Arduino loop task only polls the BOOT button and the dev console.

#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>

#include "Backlight.h"
#include "Display.h"
#include "Touch.h"
#include "app/AppController.h"
#include "config.h"
#include "dev/DevConsole.h"
#include "hal/BootButton.h"
#include "net/Ota.h"
#include "net/Portal.h"
#include "net/WifiSupervisor.h"
#include "storage/Settings.h"
#include "ui/LvglPort.h"
#include "ui/ScreenManager.h"
#include "ui/Theme.h"
#include "ui/UiBridge.h"
#include "util/Diagnostics.h"
#include "util/Log.h"

namespace {

constexpr const char *kTag = "boot";

board::Display display;
board::Backlight backlight;
board::Touch touch;
hal::BootButton bootButton;
Settings settings;

// Task watchdog over our own tasks (PLAN.md §5.2, §11), which subscribe
// themselves. A missed deadline panics, which reboots and leaves a coredump.
// Idle tasks aren't watched: long TLS handshakes on core 0 would trip it.
void configureWatchdog() {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = config::kWatchdogMs;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&cfg) == ESP_ERR_INVALID_STATE) esp_task_wdt_init(&cfg);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // Don't stall boot when no USB host is listening
  logging::begin();
  LOG_I(kTag, "Spotify CD Player %s", config::kFirmwareVersion);
  diag::logBootDiagnostics();

  settings.begin();
  ota::begin();

  backlight.begin(0);  // Dark until the first frame is drawn
  if (!display.begin(config::kRotation, config::kLcdSpiHz)) LOG_E(kTag, "Display init failed");
  ui::port::begin(display, touch);
  ui::theme::begin(ui::port::display());
  ui::bridgeBegin();

  ui::Model bootModel;
  bootModel.deviceName = settings.device().name;
  ui::screens::apply(bootModel);
  ui::port::renderNow();
  backlight.set(settings.ui().brightness);
  LOG_I(kTag, "First frame at %lu ms", millis());

  configureWatchdog();
  ui::startTask();
  app::begin(settings, backlight);
  app::startTask();
  net::begin(settings);
  portal::begin(settings);
  net::startTask();
  LOG_I(kTag, "Tasks started at %lu ms", millis());

  // Touch reset takes ~0.5 s. It runs while the UI task shows Idle; touch reads
  // are ignored until it finishes.
  if (touch.begin(Wire, config::kScreenWidth, config::kScreenHeight, board::Touch::kLandscape)) {
    LOG_I(kTag, "Touch ready at %lu ms", millis());
  } else {
    LOG_E(kTag, "Touch controller not responding");
  }

  bootButton.begin();
  dev::begin(settings);
}

void loop() {
  const ButtonEvent event = bootButton.poll();
  if (event != ButtonEvent::None) app::post(app::EventType::Button, static_cast<uint8_t>(event));
  dev::poll();

#if OTA_CRASH_TEST
  // Deliberately bad image for the rollback test (env ota_crash_test): installed
  // over the portal, it crashes before it can be marked valid, so the
  // bootloader returns to the previous image. Harmless if flashed over USB.
  static const bool pending = ota::pendingVerify();
  if (pending && millis() > 10000) {
    LOG_E(kTag, "OTA crash test: crashing on purpose");
    Serial.flush();
    abort();
  }
#endif

  delay(config::kButtonPollMs);
}
