#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Wi-Fi decisions that don't need the radio (PLAN.md §8). Pure logic, unit-tested on the host.
namespace wifi_policy {

struct Network {
  std::string ssid;
  std::string password;
};

struct ScanResult {
  std::string ssid;
  int32_t rssi;
};

// Order to try saved networks in: those seen in the scan first, strongest
// first; then the rest in saved order (they may be hidden or out of range).
// Returns indexes into `saved`.
std::vector<size_t> connectOrder(const std::vector<Network> &saved, const std::vector<ScanResult> &scan);

// Adds or updates a network at the front (most recently used first),
// dropping duplicates of the SSID and anything beyond `max`.
void remember(std::vector<Network> &saved, const Network &network, size_t max);

// Removes a saved network by SSID. Returns true if it was there.
bool forget(std::vector<Network> &saved, const std::string &ssid);

// A valid mDNS/DHCP hostname from whatever was typed as the web name:
// lowercase letters, digits and single hyphens, at most 32 characters;
// "spotify-cd" (the default) if nothing usable is left.
std::string hostname(const std::string &typed);

// A disconnect reason from the Wi-Fi driver, in words for the screen and portal.
const char *describeFailure(uint8_t reason);

// Signal strength in words (same bands as the portal): Excellent, Good, Fair, Weak.
const char *signalWords(int32_t rssi);

// Wi-Fi QR code payload (WIFI:T:WPA;S:...;P:...;;), escaping \ ; , : " as the format requires.
std::string wifiQr(const std::string &ssid, const std::string &password);

}  // namespace wifi_policy
