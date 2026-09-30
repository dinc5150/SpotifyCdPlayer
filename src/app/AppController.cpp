#include "app/AppController.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

#include <atomic>

#include "config.h"
#include "net/Ota.h"
#include "net/WifiSupervisor.h"
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
constexpr uint32_t kLoopMs = 250;
constexpr uint32_t kWifiScreenRefreshMs = 5000;

// Until SpotifyAuth exists (Phase 3), act as if Spotify is linked once Wi-Fi
// is up, so Ready and its screens are reachable. Remove in Phase 3.
constexpr bool kStubSpotifyLinked = true;

Settings *settings = nullptr;
board::Backlight *backlight = nullptr;
QueueHandle_t queue = nullptr;
State state;
std::atomic<Phase> sharedPhase{Phase::Boot};

bool portalForSetup = false;  // The AP is up because of SetupAP (not "Set up network")
uint32_t healthySinceMs = 0;  // For marking a new OTA image valid; 0 = not healthy yet
bool otaChecked = false;
uint32_t restartAtMs = 0;
bool factoryResetPending = false;
uint32_t wifiScreenRefreshedMs = 0;

bool hasToken() { return kStubSpotifyLinked || !settings->spotify().refreshToken.isEmpty(); }

void toast(const String &text, bool error = false) {
  ui::post([text, error] { ui::showToast(text, error); });
}

ui::Model buildModel() {
  ui::Model model;
  model.state = state;
  model.deviceName = settings->device().name;
  model.speakerName = settings->spotify().targetName;
  model.net = net::status();
  if (state.phase == Phase::Offline) {
    model.statusLine = "No connection";
  } else if (kStubSpotifyLinked && model.net.staConnected) {
    model.statusLine = "Wi-Fi: " + model.net.ssid + " · Spotify comes in Phase 3";
  }
  return model;
}

void publish() {
  ui::Model model = buildModel();
  ui::post([model] { ui::screens::apply(model); });
}

// Side effects of entering a phase: the setup AP, and OTA health tracking.
void onPhaseChanged(Phase before, Phase after) {
  LOG_I(kTag, "State %s -> %s", name(before), name(after));
  sharedPhase = after;

  if (after == Phase::SetupAP && before != Phase::SetupAP) {
    net::startPortal(false);
    portalForSetup = true;
  }
  // First setup done: leave the AP up a little so the phone can show the
  // result, then stop it. (Phase 3 keeps it through NeedSpotifyLink.)
  if (after == Phase::Ready && portalForSetup) {
    net::stopPortal(config::kPortalGraceMs);
    portalForSetup = false;
  }

  const bool healthy = after == Phase::Ready || after == Phase::NeedSpotifyLink || after == Phase::SetupAP;
  if (!healthy) {
    healthySinceMs = 0;
  } else if (!healthySinceMs) {
    healthySinceMs = millis() | 1;
  }
}

void fire(Trigger trigger, int32_t arg = 0) {
  const Phase before = state.phase;
  if (!apply(state, trigger, arg)) {
    LOG_D(kTag, "Trigger %s ignored in %s", name(trigger), name(state.phase));
    return;
  }
  if (state.phase != before) onPhaseChanged(before, state.phase);
  publish();
}

void applySettings() { backlight->set(settings->ui().brightness); }

void onNet(NetEvent event, int32_t value) {
  switch (event) {
    case NetEvent::Connected:
      fire(Trigger::WifiConnected, hasToken());  // From WifiConnecting or SetupAP
      fire(Trigger::WifiRestored);               // From Offline
      break;
    case NetEvent::Disconnected:
      fire(Trigger::WifiLost);
      break;
    case NetEvent::GaveUp:
      fire(Trigger::WifiFailed);
      break;
    case NetEvent::SubmitStarted:
      fire(Trigger::WifiSubmitted);
      break;
    case NetEvent::SubmitFailed:
      // Only a first-setup attempt returns to setup; a failed switch made from
      // the LAN portal just falls back to the saved networks.
      if (state.phase == Phase::WifiConnecting) fire(Trigger::WifiFailed);
      LOG_I(kTag, "Portal network failed (reason %ld)", static_cast<long>(value));
      break;
    case NetEvent::Changed:
      break;
  }
  publish();
}

void onSetUpNetwork(bool start) {
  if (!start) {
    if (state.phase != Phase::SetupAP) net::stopPortal(0);
    return;
  }
  if (state.phase == Phase::WifiConnecting || state.phase == Phase::Offline) {
    fire(Trigger::SetUpNetwork);  // -> SetupAP, which starts the AP
  } else if (state.phase == Phase::Ready) {
    net::startPortal(true);  // Keeps the current connection (§8.2)
  }
}

void onOta(OtaEvent event) {
  switch (event) {
    case OtaEvent::Started:
      toast("Installing update...");
      break;
    case OtaEvent::Failed:
      toast("Update failed: " + ota::lastError(), true);
      break;
    case OtaEvent::Succeeded:
      toast("Update installed. Restarting...");
      restartAtMs = millis() + 1500;  // Lets the portal's reply go out first
      break;
  }
}

void onMaintenance(Maintenance action) {
  if (action == Maintenance::Reboot) {
    toast("Restarting...");
  } else {
    toast("Erasing settings...");
    factoryResetPending = true;
  }
  restartAtMs = millis() + 800;
}

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
  const net::Status n = net::status();
  LOG_I(kTag, "phase=%s view=%s overlay=%s backTo=%s screen=%s", name(state.phase),
        state.view == View::Idle ? "idle" : "now_playing", name(state.overlay), name(state.backTo),
        name(screenFor(state)));
  LOG_I(kTag, "wifi: connected=%d ssid=\"%s\" ip=%s rssi=%ld connecting=%d error=\"%s\" ap=%d clients=%u time=%d",
        n.staConnected, n.ssid.c_str(), n.ip.c_str(), static_cast<long>(n.rssi), n.connecting, n.lastError.c_str(),
        n.apActive, n.apClients, n.timeSynced);
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
      fire(Trigger::BootDone, settings->wifiCount() > 0);
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
    case EventType::Net:
      onNet(static_cast<NetEvent>(event.code), event.value);
      break;
    case EventType::SetUpNetwork:
      onSetUpNetwork(true);
      break;
    case EventType::StopSetUpNetwork:
      onSetUpNetwork(false);
      break;
    case EventType::Ota:
      onOta(static_cast<OtaEvent>(event.code));
      break;
    case EventType::Maintenance:
      onMaintenance(static_cast<Maintenance>(event.code));
      break;
  }
}

// Timed work, checked every kLoopMs.
void tick() {
  const uint32_t now = millis();

  if (restartAtMs && static_cast<int32_t>(now - restartAtMs) >= 0) {
    if (factoryResetPending) settings->factoryReset();
    LOG_I(kTag, "Restarting");
    Serial.flush();
    esp_restart();
  }

  if (!otaChecked && healthySinceMs && now - healthySinceMs >= config::kOtaHealthyAfterMs) {
    otaChecked = true;
    ota::markValid();  // No-op unless this is a new image waiting to be verified
  }

  // The Wi-Fi screen shows live signal strength.
  if (state.overlay == Overlay::Wifi && now - wifiScreenRefreshedMs >= kWifiScreenRefreshMs) {
    wifiScreenRefreshedMs = now;
    publish();
  }
}

void run(void *) {
  esp_task_wdt_add(nullptr);
  uint32_t lastHeapLog = millis();
  AppEvent event;
  for (;;) {
    if (xQueueReceive(queue, &event, pdMS_TO_TICKS(kLoopMs)) == pdTRUE) handle(event);
    tick();
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

Phase currentPhase() { return sharedPhase.load(); }

}  // namespace app
