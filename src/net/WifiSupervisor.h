#pragma once

#include <Arduino.h>

#include <vector>

#include "storage/Settings.h"

// The net task: owns the Wi-Fi radio (PLAN.md §8). It connects to the saved
// networks (strongest first, backoff 1 s -> 60 s), tries networks entered in the
// portal, runs the setup AP with captive DNS, scans, and starts mDNS and NTP.
// It reports to the app as EventType::Net events. Every function here is safe
// to call from any task: they queue a command or return a snapshot.
namespace net {

struct Status {
  bool staConnected = false;
  String ssid;  // Connected network
  int32_t rssi = 0;
  String ip;
  bool connecting = false;
  String connectingSsid;
  String lastError;  // Last failure in words ("Wrong password?"); cleared on success
  bool apActive = false;
  String apSsid;
  String apPassword;
  uint8_t apClients = 0;
  bool timeSynced = false;
  String hostname;
  uint8_t savedNetworks = 0;
};

struct ScanEntry {
  String ssid;
  int32_t rssi;
  bool secure;
};

void begin(Settings &settings);
void startTask();

Status status();

// Cached scan results, strongest first. Starts a new scan if they're stale.
std::vector<ScanEntry> scanResults(bool &scanning);

// Try a network entered in the portal; saved only if it connects.
void submit(const String &ssid, const String &password);
void forget(const String &ssid);

// Setup AP + captive portal DNS. With autoStop, it stops by itself after
// config::kPortalIdleMs without a client ("Set up network" from the menu).
void startPortal(bool autoStop);
void stopPortal(uint32_t afterMs = 0);

// The web name (dev/host) sets the mDNS and DHCP hostname; call after changing it.
void refreshHostname();

}  // namespace net
