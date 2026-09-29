#include "app/AppController.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "app/StateMachine.h"
#include "config.h"
#include "ui/ConfirmDialog.h"
#include "ui/Model.h"
#include "ui/ScreenManager.h"
#include "ui/Toast.h"
#include "ui/UiBridge.h"
#include "util/ButtonGesture.h"
#include "util/Log.h"

namespace app {

namespace {

constexpr const char *kTag = "app";

// Until WifiSupervisor (Phase 2) and SpotifyAuth (Phase 3) exist, boot straight
// to Ready so the Ready screens can be built and tested. Remove in Phase 2.
constexpr bool kStubConnectivity = true;

Settings *settings = nullptr;
board::Backlight *backlight = nullptr;
QueueHandle_t queue = nullptr;
State state;

void toast(const String &text, bool error = false) {
  ui::post([text, error] { ui::showToast(text, error); });
}

ui::Model buildModel() {
  ui::Model model;
  model.state = state;
  model.deviceName = settings->device().name;
  model.speakerName = settings->spotify().targetName;
  if (state.phase == Phase::Offline) {
    model.statusLine = "No connection";
  } else if (kStubConnectivity) {
    model.statusLine = "Phase 1 build: Wi-Fi and Spotify not connected yet";
  }
  return model;
}

void publish() {
  ui::Model model = buildModel();
  ui::post([model] { ui::screens::apply(model); });
}

void fire(Trigger trigger, int32_t arg = 0) {
  const Phase before = state.phase;
  if (!apply(state, trigger, arg)) {
    LOG_D(kTag, "Trigger %s ignored in %s", name(trigger), name(state.phase));
    return;
  }
  if (state.phase != before) LOG_I(kTag, "State %s -> %s", name(before), name(state.phase));
  publish();
}

void applySettings() { backlight->set(settings->ui().brightness); }

void onButton(ButtonEvent event) {
  switch (event) {
    case ButtonEvent::ShortPress:
      // Play/pause arrives with the Spotify client in Phase 4.
      toast("Play/pause needs Spotify (Phase 4)");
      break;
    case ButtonEvent::LongPress:
      fire(Trigger::Home);
      break;
    case ButtonEvent::FactoryHold:
      ui::post([] {
        ui::showConfirm(DialogId::FactoryReset, "Factory reset?",
                        "Erases Wi-Fi, the Spotify link and all settings, then restarts.", "Erase", true);
      });
      break;
    case ButtonEvent::None:
      break;
  }
}

void onConfirm(DialogId id, bool accepted) {
  LOG_I(kTag, "Dialog %u %s", static_cast<unsigned>(id), accepted ? "accepted" : "cancelled");
  if (id == DialogId::FactoryReset && accepted) settings->factoryReset();
  if (id == DialogId::Test) toast(accepted ? "Confirmed" : "Cancelled");
}

void logState() {
  LOG_I(kTag, "phase=%s view=%s overlay=%s backTo=%s screen=%s wifiFailures=%u", name(state.phase),
        state.view == View::Idle ? "idle" : "now_playing", name(state.overlay), name(state.backTo),
        name(screenFor(state)), state.wifiFailures);
}

void logHeap() {
  LOG_I(kTag, "heap internal %u (min %u), psram %u (min %u)", heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}

void handle(const AppEvent &event) {
  switch (event.type) {
    case EventType::Start:
      applySettings();
      fire(Trigger::BootDone, kStubConnectivity || settings->wifiCount() > 0);
      if (kStubConnectivity) fire(Trigger::WifiConnected, 1);
      break;
    case EventType::Trigger:
      fire(static_cast<Trigger>(event.code), event.value);
      break;
    case EventType::OpenOverlay:
      fire(Trigger::OpenOverlay, event.code);
      break;
    case EventType::Back:
      fire(Trigger::Back);
      break;
    case EventType::Button:
      onButton(static_cast<ButtonEvent>(event.code));
      break;
    case EventType::ConfirmResult:
      onConfirm(static_cast<DialogId>(event.code), event.value != 0);
      break;
    case EventType::SettingsChanged:
      applySettings();
      publish();
      break;
    case EventType::DumpState:
      logState();
      logHeap();
      break;
  }
}

void run(void *) {
  esp_task_wdt_add(nullptr);
  uint32_t lastHeapLog = millis();
  AppEvent event;
  for (;;) {
    if (xQueueReceive(queue, &event, pdMS_TO_TICKS(1000)) == pdTRUE) handle(event);
    esp_task_wdt_reset();
    if (millis() - lastHeapLog >= config::kHeapLogPeriodMs) {
      lastHeapLog = millis();
      logHeap();
    }
  }
}

}  // namespace

bool post(const AppEvent &event) {
  if (queue && xQueueSend(queue, &event, 0) == pdTRUE) return true;
  LOG_W(kTag, "App queue full; event %u dropped", static_cast<unsigned>(event.type));
  return false;
}

void begin(Settings &s, board::Backlight &b) {
  settings = &s;
  backlight = &b;
  queue = xQueueCreate(config::kAppQueueLength, sizeof(AppEvent));
  post(EventType::Start);
}

void startTask() {
  const config::TaskSpec &spec = config::kAppTask;
  xTaskCreatePinnedToCore(run, spec.name, spec.stackBytes, nullptr, spec.priority, nullptr, spec.core);
}

}  // namespace app
