#pragma once

#include <Arduino.h>

#include <mutex>
#include <vector>

#include "storage/SettingsRules.h"

// NVS-backed settings (PLAN.md §10). Loaded once at begin() and cached; setters
// write only the keys whose values changed. Thread-safe: getters return copies.

struct WifiNetwork {
  String ssid;
  String password;
};

struct DeviceSettings {
  String name = "Card Player";
  String apPassword;  // Setup AP (WPA2), generated on first boot
  String adminHash;   // "salt:sha256", empty = no admin password
};

struct SpotifySettings {
  String clientId;
  String relayUrl;
  uint8_t loginMode = 0;  // 0 relay, 1 loopback
  String refreshToken;
  uint32_t linkedAt = 0;  // epoch seconds
  String userId;
  String userName;
  String targetId;
  String targetName;
  String targetType;
};

class Settings {
 public:
  static constexpr uint8_t kSchemaVersion = 1;
  static constexpr size_t kMaxWifiNetworks = 3;

  bool begin();

  UiSettings ui() const;
  void setUi(UiSettings ui);  // Sanitised before saving

  DeviceSettings device() const;
  void setDevice(const DeviceSettings &device);

  std::vector<WifiNetwork> wifiNetworks() const;
  size_t wifiCount() const;
  void setWifiNetworks(std::vector<WifiNetwork> networks);  // Keeps the first kMaxWifiNetworks

  SpotifySettings spotify() const;
  void setSpotify(const SpotifySettings &spotify);

  // Erases every namespace (and the Wi-Fi driver's own NVS data), then restarts.
  [[noreturn]] void factoryReset();

 private:
  void loadUi();
  void loadDevice();
  void loadWifi();
  void loadSpotify();

  mutable std::mutex mutex_;
  UiSettings ui_;
  DeviceSettings device_;
  std::vector<WifiNetwork> wifi_;
  SpotifySettings spotify_;
};
