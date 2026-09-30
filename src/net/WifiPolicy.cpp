#include "net/WifiPolicy.h"

#include <algorithm>
#include <cctype>

namespace wifi_policy {

std::vector<size_t> connectOrder(const std::vector<Network> &saved, const std::vector<ScanResult> &scan) {
  struct Seen {
    size_t index;
    int32_t rssi;
  };
  std::vector<Seen> seen;
  std::vector<size_t> unseen;
  for (size_t i = 0; i < saved.size(); i++) {
    int32_t best = INT32_MIN;
    for (const ScanResult &r : scan) {
      if (r.ssid == saved[i].ssid && r.rssi > best) best = r.rssi;
    }
    if (best == INT32_MIN) {
      unseen.push_back(i);
    } else {
      seen.push_back({i, best});
    }
  }
  std::stable_sort(seen.begin(), seen.end(), [](const Seen &a, const Seen &b) { return a.rssi > b.rssi; });

  std::vector<size_t> order;
  for (const Seen &s : seen) order.push_back(s.index);
  order.insert(order.end(), unseen.begin(), unseen.end());
  return order;
}

void remember(std::vector<Network> &saved, const Network &network, size_t max) {
  forget(saved, network.ssid);
  saved.insert(saved.begin(), network);
  if (saved.size() > max) saved.resize(max);
}

bool forget(std::vector<Network> &saved, const std::string &ssid) {
  const auto it = std::remove_if(saved.begin(), saved.end(), [&](const Network &n) { return n.ssid == ssid; });
  const bool found = it != saved.end();
  saved.erase(it, saved.end());
  return found;
}

std::string hostname(const std::string &typed) {
  std::string in = typed;
  // People type the whole address; ".local" isn't part of the name.
  constexpr char kSuffix[] = ".local";
  constexpr size_t kSuffixLength = sizeof(kSuffix) - 1;
  if (in.size() >= kSuffixLength) {
    std::string tail = in.substr(in.size() - kSuffixLength);
    for (char &c : tail) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (tail == kSuffix) in.resize(in.size() - kSuffixLength);
  }

  std::string out;
  for (char c : in) {
    if (c == '\'') continue;  // "Kid's Room" -> "kids-room"
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (alnum) {
      out += c;
    } else if (!out.empty() && out.back() != '-') {
      out += '-';
    }
    if (out.size() == 32) break;
  }
  while (!out.empty() && out.back() == '-') out.pop_back();
  return out.empty() ? "spotify-cd" : out;
}

const char *describeFailure(uint8_t reason) {
  // wifi_err_reason_t values (ESP-IDF 5.5).
  switch (reason) {
    case 2:    // AUTH_EXPIRE
    case 15:   // 4WAY_HANDSHAKE_TIMEOUT
    case 202:  // AUTH_FAIL
    case 204:  // HANDSHAKE_TIMEOUT
      return "Wrong password?";
    case 201:  // NO_AP_FOUND
    case 210:  // NO_AP_FOUND_W_COMPATIBLE_SECURITY
    case 211:  // NO_AP_FOUND_IN_AUTHMODE_THRESHOLD
    case 212:  // NO_AP_FOUND_IN_RSSI_THRESHOLD
      return "Network not found";
    case 200:  // BEACON_TIMEOUT
      return "Signal lost";
    case 0:
      return "Timed out";
    default:
      return "Couldn't connect";
  }
}

const char *signalWords(int32_t rssi) {
  if (rssi > -55) return "Excellent";
  if (rssi > -65) return "Good";
  if (rssi > -75) return "Fair";
  return "Weak";
}

std::string wifiQr(const std::string &ssid, const std::string &password) {
  const auto escape = [](const std::string &in) {
    std::string out;
    for (char c : in) {
      if (c == '\\' || c == ';' || c == ',' || c == ':' || c == '"') out += '\\';
      out += c;
    }
    return out;
  };
  return "WIFI:T:WPA;S:" + escape(ssid) + ";P:" + escape(password) + ";;";
}

}  // namespace wifi_policy
