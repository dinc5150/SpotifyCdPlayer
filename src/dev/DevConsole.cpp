#include "dev/DevConsole.h"

#if ENABLE_SERIAL_CONSOLE

#include <memory>

#include "Console.h"
#include "app/Events.h"
#include "app/StateMachine.h"
#include "ui/ConfirmDialog.h"
#include "ui/LvglPort.h"
#include "ui/Toast.h"
#include "ui/UiBridge.h"
#include "util/ButtonGesture.h"
#include "util/Log.h"

namespace dev {

namespace {

Settings *settings = nullptr;

void printSettings() {
  const UiSettings ui = settings->ui();
  const DeviceSettings device = settings->device();
  const SpotifySettings sp = settings->spotify();
  Serial.printf("ui: bright=%u dim=%u off=%u same=%u rmpause=%d albumorder=%d\n", ui.brightness, ui.dimSeconds,
                ui.offSeconds, static_cast<unsigned>(ui.sameCard), ui.pauseOnRemove, ui.albumsInOrder);
  Serial.printf("dev: name=\"%s\" ap_pw=%s admin=%s\n", device.name.c_str(), device.apPassword.c_str(),
                device.adminHash.isEmpty() ? "(none)" : "(set)");
  const std::vector<WifiNetwork> wifi = settings->wifiNetworks();
  Serial.printf("wifi: %u network(s)", static_cast<unsigned>(wifi.size()));
  for (const WifiNetwork &n : wifi) Serial.printf(" \"%s\"", n.ssid.c_str());
  Serial.println();
  Serial.printf("sp: client_id=%s linked=%s user=\"%s\" target=\"%s\"\n",
                sp.clientId.isEmpty() ? "(none)" : sp.clientId.c_str(), sp.refreshToken.isEmpty() ? "no" : "yes",
                sp.userName.c_str(), sp.targetName.c_str());
}

bool parseBool(const String &value) { return value == "1" || value == "on" || value == "true" || value == "yes"; }

void set(String args) {
  const String key = console::nextWord(args);
  const String value = args;
  UiSettings ui = settings->ui();
  if (key == "bright") {
    ui.brightness = value.toInt();
  } else if (key == "dim") {
    ui.dimSeconds = value.toInt();
  } else if (key == "off") {
    ui.offSeconds = value.toInt();
  } else if (key == "same") {
    ui.sameCard = static_cast<SameCard>(value.toInt());
  } else if (key == "rmpause") {
    ui.pauseOnRemove = parseBool(value);
  } else if (key == "albumorder") {
    ui.albumsInOrder = parseBool(value);
  } else if (key == "name") {
    DeviceSettings device = settings->device();
    device.name = value;
    settings->setDevice(device);
    app::post(app::EventType::SettingsChanged);
    printSettings();
    return;
  } else if (key == "speaker") {  // Stand-in until the Speakers screen (Phase 6)
    SpotifySettings sp = settings->spotify();
    sp.targetName = value;
    settings->setSpotify(sp);
    app::post(app::EventType::SettingsChanged);
    printSettings();
    return;
  } else {
    Serial.println("Keys: bright dim off same rmpause albumorder name speaker");
    return;
  }
  settings->setUi(ui);  // Sanitised: out-of-range values fall back
  app::post(app::EventType::SettingsChanged);
  printSettings();
}

void event(String args) {
  const String word = console::nextWord(args);
  app::Trigger trigger;
  if (!app::parseTrigger(word.c_str(), trigger)) {
    Serial.println("Triggers: boot_done wifi_submitted wifi_failed set_up_network wifi_connected spotify_linked");
    Serial.println("          login_expired wifi_lost wifi_restored playback_started idle_timeout");
    Serial.println("          open_overlay <menu|speakers|write_card|wifi|spotify|settings|about> back home");
    return;
  }
  int32_t arg = args.toInt();
  app::Overlay overlay;
  if (trigger == app::Trigger::OpenOverlay && app::parseOverlay(args.c_str(), overlay)) {
    arg = static_cast<int32_t>(overlay);
  }
  app::post(app::EventType::Trigger, static_cast<uint8_t>(trigger), arg);
  app::post(app::EventType::DumpState);
}

void printLog(String) {
  constexpr size_t kBytes = 4096;
  std::unique_ptr<char[]> buffer(new char[kBytes]);
  const size_t length = logging::copyRecent(buffer.get(), kBytes);
  logging::writeSerial(buffer.get(), length, 200);
}

}  // namespace

void begin(Settings &s) {
  settings = &s;
  console::add("state", "log app state and heap", [](String) { app::post(app::EventType::DumpState); });
  console::add("ev", "<trigger> [arg]  fire a state machine trigger, e.g. 'ev wifi_lost', 'ev open_overlay about'",
               event);
  console::add("settings", "print stored settings", [](String) { printSettings(); });
  console::add("set", "<key> <value>  change a setting (saved to NVS; survives reboot)", set);
  console::add("toast", "<text>  show a toast", [](String args) {
    ui::post([args] { ui::showToast(args); });
  });
  console::add("confirm", "show a test confirmation dialog", [](String) {
    ui::post([] { ui::showConfirm(app::DialogId::Test, "Test dialog", "Tap a button.", "OK", false); });
  });
  console::add("factory", "open the factory reset dialog (tap Erase on screen)", [](String) {
    app::post(app::EventType::Button, static_cast<uint8_t>(ButtonEvent::FactoryHold));
  });
  console::add("presslog", "<on|off>  log touch-to-pixels time for each button press", [](String args) {
    const bool on = parseBool(args);
    ui::post([on] { ui::port::setPressLatencyLog(on); });
  });
  console::add("perf", "<on|off>  LVGL FPS/CPU overlay", [](String args) {
    const bool on = parseBool(args);
    ui::post([on] { ui::port::setPerfOverlay(on); });
  });
  console::add("level", "<e|w|i|d>  log level", [](String args) {
    logging::setLevel(args == "e"   ? logging::Level::Error
                      : args == "w" ? logging::Level::Warn
                      : args == "d" ? logging::Level::Debug
                                    : logging::Level::Info);
  });
  console::add("log", "print the recent log ring buffer", printLog);
  console::add("heap", "print free internal RAM and PSRAM", [](String) {
    Serial.printf("internal %u (min %u, largest %u), psram %u (min %u)\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                  heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
  });
  console::add("reboot", "restart the device", [](String) {
    Serial.flush();
    esp_restart();
  });
}

void poll() {
  if (!Serial.available()) return;
  // Someone just typed, so someone is reading: let command output (help, log,
  // settings) wait for the USB buffer instead of being cut off. Back to 0 after,
  // so an unread port can never stall the device.
  Serial.setTxTimeoutMs(200);
  console::poll();
  Serial.setTxTimeoutMs(0);
}

}  // namespace dev

#else

namespace dev {
void begin(Settings &) {}
void poll() {}
}  // namespace dev

#endif
