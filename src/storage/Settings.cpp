#include "storage/Settings.h"

#include <Preferences.h>
#include <esp_random.h>
#include <nvs_flash.h>

#include "util/Log.h"

namespace {

constexpr const char *kTag = "settings";

// Preferences logs an error for every missing key, so check first.
String getString(Preferences &p, const char *key, const String &fallback) {
  return p.isKey(key) ? p.getString(key, fallback) : fallback;
}

void putString(Preferences &p, const char *key, const String &before, const String &after) {
  if (before == after) return;
  if (after.isEmpty()) {
    if (p.isKey(key)) p.remove(key);
  } else {
    p.putString(key, after);
  }
}

// 8 characters without look-alikes (0/O, 1/l/I), for the setup AP (§8.1).
String randomPassword() {
  static const char kAlphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
  String out;
  for (int i = 0; i < 8; i++) out += kAlphabet[esp_random() % (sizeof(kAlphabet) - 1)];
  return out;
}

class Namespace {
 public:
  explicit Namespace(const char *name) { prefs_.begin(name, false); }
  ~Namespace() { prefs_.end(); }
  Preferences *operator->() { return &prefs_; }
  Preferences &operator*() { return prefs_; }

 private:
  Preferences prefs_;
};

}  // namespace

bool Settings::begin() {
  std::lock_guard<std::mutex> lock(mutex_);
  {
    Namespace dev("dev");
    const uint8_t stored = dev->getUChar("schema", 0);
    if (stored == 0) {
      LOG_I(kTag, "First boot: schema %u", kSchemaVersion);
    } else if (stored > kSchemaVersion) {
      LOG_W(kTag, "Stored schema %u is newer than this firmware's %u (downgrade?)", stored, kSchemaVersion);
    }
    // Migrations from older schemas go here.
    if (stored != kSchemaVersion) dev->putUChar("schema", kSchemaVersion);
  }
  loadUi();
  loadDevice();
  loadWifi();
  loadSpotify();

  if (device_.apPassword.isEmpty()) {
    device_.apPassword = randomPassword();
    Namespace dev("dev");
    dev->putString("ap_pw", device_.apPassword);
  }
  LOG_I(kTag, "Loaded: %u Wi-Fi network(s), Spotify %s, brightness %u%%", static_cast<unsigned>(wifi_.size()),
        spotify_.refreshToken.isEmpty() ? "not linked" : "linked", ui_.brightness);
  return true;
}

void Settings::loadUi() {
  Namespace p("ui");
  const UiSettings d;
  ui_.brightness = p->getUChar("bright", d.brightness);
  ui_.dimSeconds = p->getUShort("dim_s", d.dimSeconds);
  ui_.offSeconds = p->getUShort("off_s", d.offSeconds);
  ui_.sameCard = static_cast<SameCard>(p->getUChar("same_card", static_cast<uint8_t>(d.sameCard)));
  ui_.pauseOnRemove = p->getBool("rm_pause", d.pauseOnRemove);
  ui_.albumsInOrder = p->getBool("album_order", d.albumsInOrder);
  if (settings_rules::sanitize(ui_)) LOG_W(kTag, "Out-of-range UI settings corrected");
}

void Settings::loadDevice() {
  Namespace p("dev");
  const DeviceSettings d;
  device_.name = getString(*p, "name", d.name);
  device_.apPassword = getString(*p, "ap_pw", "");
  device_.adminHash = getString(*p, "admin", "");
}

void Settings::loadWifi() {
  Namespace p("wifi");
  wifi_.clear();
  const uint8_t count = min<uint8_t>(p->getUChar("n", 0), kMaxWifiNetworks);
  for (uint8_t i = 0; i < count; i++) {
    const String s = "s" + String(i);
    const String pw = "p" + String(i);
    wifi_.push_back({getString(*p, s.c_str(), ""), getString(*p, pw.c_str(), "")});
  }
}

void Settings::loadSpotify() {
  Namespace p("sp");
  spotify_.clientId = getString(*p, "client_id", "");
  spotify_.relayUrl = getString(*p, "relay_url", "");
  spotify_.loginMode = p->getUChar("login_mode", 0);
  spotify_.refreshToken = getString(*p, "rt", "");
  spotify_.linkedAt = p->getULong("linked_at", 0);
  spotify_.userId = getString(*p, "uid", "");
  spotify_.userName = getString(*p, "uname", "");
  spotify_.targetId = getString(*p, "tgt_id", "");
  spotify_.targetName = getString(*p, "tgt_name", "");
  spotify_.targetType = getString(*p, "tgt_type", "");
}

UiSettings Settings::ui() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return ui_;
}

void Settings::setUi(UiSettings ui) {
  settings_rules::sanitize(ui);
  std::lock_guard<std::mutex> lock(mutex_);
  if (ui == ui_) return;
  Namespace p("ui");
  if (ui.brightness != ui_.brightness) p->putUChar("bright", ui.brightness);
  if (ui.dimSeconds != ui_.dimSeconds) p->putUShort("dim_s", ui.dimSeconds);
  if (ui.offSeconds != ui_.offSeconds) p->putUShort("off_s", ui.offSeconds);
  if (ui.sameCard != ui_.sameCard) p->putUChar("same_card", static_cast<uint8_t>(ui.sameCard));
  if (ui.pauseOnRemove != ui_.pauseOnRemove) p->putBool("rm_pause", ui.pauseOnRemove);
  if (ui.albumsInOrder != ui_.albumsInOrder) p->putBool("album_order", ui.albumsInOrder);
  ui_ = ui;
}

DeviceSettings Settings::device() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return device_;
}

void Settings::setDevice(const DeviceSettings &device) {
  std::lock_guard<std::mutex> lock(mutex_);
  Namespace p("dev");
  putString(*p, "name", device_.name, device.name);
  putString(*p, "ap_pw", device_.apPassword, device.apPassword);
  putString(*p, "admin", device_.adminHash, device.adminHash);
  device_ = device;
}

std::vector<WifiNetwork> Settings::wifiNetworks() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return wifi_;
}

size_t Settings::wifiCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return wifi_.size();
}

void Settings::setWifiNetworks(std::vector<WifiNetwork> networks) {
  if (networks.size() > kMaxWifiNetworks) networks.resize(kMaxWifiNetworks);
  std::lock_guard<std::mutex> lock(mutex_);
  Namespace p("wifi");
  for (size_t i = 0; i < kMaxWifiNetworks; i++) {
    const String s = "s" + String(i);
    const String pw = "p" + String(i);
    const WifiNetwork before = i < wifi_.size() ? wifi_[i] : WifiNetwork{};
    const WifiNetwork after = i < networks.size() ? networks[i] : WifiNetwork{};
    putString(*p, s.c_str(), before.ssid, after.ssid);
    putString(*p, pw.c_str(), before.password, after.password);
  }
  if (networks.size() != wifi_.size()) p->putUChar("n", networks.size());
  wifi_ = std::move(networks);
}

SpotifySettings Settings::spotify() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return spotify_;
}

void Settings::setSpotify(const SpotifySettings &sp) {
  std::lock_guard<std::mutex> lock(mutex_);
  Namespace p("sp");
  putString(*p, "client_id", spotify_.clientId, sp.clientId);
  putString(*p, "relay_url", spotify_.relayUrl, sp.relayUrl);
  if (sp.loginMode != spotify_.loginMode) p->putUChar("login_mode", sp.loginMode);
  // The refresh token is written only when it actually changes (§11: NVS write rate).
  putString(*p, "rt", spotify_.refreshToken, sp.refreshToken);
  if (sp.linkedAt != spotify_.linkedAt) p->putULong("linked_at", sp.linkedAt);
  putString(*p, "uid", spotify_.userId, sp.userId);
  putString(*p, "uname", spotify_.userName, sp.userName);
  putString(*p, "tgt_id", spotify_.targetId, sp.targetId);
  putString(*p, "tgt_name", spotify_.targetName, sp.targetName);
  putString(*p, "tgt_type", spotify_.targetType, sp.targetType);
  spotify_ = sp;
}

void Settings::factoryReset() {
  LOG_W(kTag, "Factory reset: erasing NVS and restarting");
  Serial.flush();
  mutex_.lock();  // Held until the restart, so nothing writes mid-erase
  const esp_err_t err = nvs_flash_erase();
  if (err != ESP_OK) LOG_E(kTag, "nvs_flash_erase failed: %s", esp_err_to_name(err));
  delay(100);
  esp_restart();
}
