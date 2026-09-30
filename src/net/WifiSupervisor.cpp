#include "net/WifiSupervisor.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_task_wdt.h>

#include <algorithm>
#include <deque>
#include <mutex>

#include "app/Events.h"
#include "config.h"
#include "net/WifiPolicy.h"
#include "util/Backoff.h"
#include "util/Log.h"

namespace net {

namespace {

constexpr const char *kTag = "wifi";
constexpr uint8_t kReasonAssocLeave = 8;  // What our own WiFi.disconnect() reports
constexpr time_t kValidEpoch = 1700000000;  // Any time after 2023 means NTP has answered

enum class CmdType : uint8_t {
  Submit,
  Forget,
  StartPortal,
  StopPortal,
  Scan,
  RefreshHostname,
  EvGotIp,
  EvDisconnected,
  EvApClients,
  EvScanDone,
};

struct Command {
  CmdType type;
  String a;
  String b;
  uint32_t value = 0;
};

Settings *settings = nullptr;
TaskHandle_t task = nullptr;

std::mutex queueMutex;
std::deque<Command> commands;

// Shared with other tasks, under statusMutex.
std::mutex statusMutex;
Status snapshot;
std::vector<ScanEntry> scanCache;
uint32_t scanAtMs = 0;
bool scanning = false;

// Net task only.
enum class Sta : uint8_t { Idle, Attempting, Connected, Waiting };
Sta sta = Sta::Idle;
std::vector<wifi_policy::Network> saved;
std::vector<size_t> order;
size_t orderPos = 0;
wifi_policy::Network attempt;
bool submitting = false;
uint32_t attemptStartedMs = 0;
uint32_t nextRoundAtMs = 0;
uint32_t failingSinceMs = 0;  // 0 = not failing
bool gaveUpReported = false;
Backoff backoff(config::kWifiBackoffStartMs, config::kWifiBackoffMaxMs);
String connectedSsid;
String lastError;
uint32_t rssiAtMs = 0;

DNSServer dns;
String apSsid;
bool apActive = false;
bool apAutoStop = false;
uint32_t apStopAtMs = 0;  // 0 = no stop scheduled
uint32_t apLastClientMs = 0;
uint8_t apClients = 0;

String host;
bool ntpStarted = false;
bool timeSynced = false;
bool mdnsStarted = false;

bool reached(uint32_t now, uint32_t at) { return static_cast<int32_t>(now - at) >= 0; }

void enqueue(Command command) {
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    commands.push_back(std::move(command));
  }
  if (task) xTaskNotifyGive(task);
}

void notifyApp(app::NetEvent event, int32_t value = 0) {
  app::post(app::EventType::Net, static_cast<uint8_t>(event), value);
}

void publish() {
  Status s;
  s.staConnected = sta == Sta::Connected;
  s.ssid = connectedSsid;
  s.rssi = s.staConnected ? WiFi.RSSI() : 0;
  s.ip = s.staConnected ? WiFi.localIP().toString() : String();
  s.connecting = sta == Sta::Attempting;
  s.connectingSsid = attempt.ssid.c_str();
  s.lastError = lastError;
  s.apActive = apActive;
  s.apSsid = apSsid;
  s.apPassword = settings->device().apPassword;
  s.apClients = apClients;
  s.timeSynced = timeSynced;
  s.hostname = host;
  s.savedNetworks = saved.size();
  std::lock_guard<std::mutex> lock(statusMutex);
  snapshot = s;
}

void loadSaved() {
  saved.clear();
  for (const WifiNetwork &n : settings->wifiNetworks()) saved.push_back({n.ssid.c_str(), n.password.c_str()});
}

void storeSaved() {
  std::vector<WifiNetwork> out;
  for (const wifi_policy::Network &n : saved) out.push_back({String(n.ssid.c_str()), String(n.password.c_str())});
  settings->setWifiNetworks(out);
}

void readScanResults(int count) {
  std::vector<ScanEntry> entries;
  for (int i = 0; i < count; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;  // Hidden
    const int32_t rssi = WiFi.RSSI(i);
    auto it = std::find_if(entries.begin(), entries.end(), [&](const ScanEntry &e) { return e.ssid == ssid; });
    if (it == entries.end()) {
      entries.push_back({ssid, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
    } else if (rssi > it->rssi) {
      it->rssi = rssi;
    }
  }
  WiFi.scanDelete();
  std::sort(entries.begin(), entries.end(), [](const ScanEntry &a, const ScanEntry &b) { return a.rssi > b.rssi; });
  std::lock_guard<std::mutex> lock(statusMutex);
  scanCache = std::move(entries);
  scanAtMs = millis();
  scanning = false;
}

// Blocking scan (~2-3 s) to pick the strongest saved network. Skipped while an
// async scan for the portal is running; the cache is used instead.
std::vector<wifi_policy::ScanResult> scanForRound() {
  bool busy;
  {
    std::lock_guard<std::mutex> lock(statusMutex);
    busy = scanning;
    if (!busy) scanning = true;
  }
  if (!busy) {
    const int count = WiFi.scanNetworks(false, false);
    if (count >= 0) {
      readScanResults(count);
    } else {
      std::lock_guard<std::mutex> lock(statusMutex);
      scanning = false;
    }
  }
  std::vector<wifi_policy::ScanResult> results;
  std::lock_guard<std::mutex> lock(statusMutex);
  for (const ScanEntry &e : scanCache) results.push_back({e.ssid.c_str(), e.rssi});
  return results;
}

void beginAttempt(const wifi_policy::Network &network) {
  if (sta == Sta::Connected) {
    notifyApp(app::NetEvent::Disconnected, kReasonAssocLeave);
    WiFi.disconnect(false);  // Its event (reason 8) is ignored while attempting
  } else if (sta == Sta::Attempting) {
    WiFi.disconnect(false);
  }
  attempt = network;
  sta = Sta::Attempting;
  attemptStartedMs = millis();
  LOG_I(kTag, "Connecting to \"%s\"%s", network.ssid.c_str(), submitting ? " (entered in the portal)" : "");
  WiFi.begin(network.ssid.c_str(), network.password.empty() ? nullptr : network.password.c_str());
  publish();
}

void startRound() {
  loadSaved();
  if (saved.empty()) {
    sta = Sta::Idle;
    publish();
    return;
  }
  if (!failingSinceMs) failingSinceMs = millis();
  std::vector<wifi_policy::ScanResult> scan;
  if (saved.size() > 1 && apClients == 0) scan = scanForRound();
  order = wifi_policy::connectOrder(saved, scan);
  orderPos = 0;
  beginAttempt(saved[order[0]]);
}

void attemptFailed(uint8_t reason) {
  const char *why = wifi_policy::describeFailure(reason);
  LOG_W(kTag, "\"%s\": %s (reason %u)", attempt.ssid.c_str(), why, reason);
  lastError = String(attempt.ssid.c_str()) + ": " + why;

  if (submitting) {
    submitting = false;
    sta = Sta::Waiting;
    nextRoundAtMs = millis();  // Back to the saved networks, if any
    notifyApp(app::NetEvent::SubmitFailed, reason);
    publish();
    return;
  }
  if (++orderPos < order.size()) {
    beginAttempt(saved[order[orderPos]]);
    return;
  }
  const uint32_t delayMs = backoff.next(esp_random());
  LOG_I(kTag, "All saved networks failed; next try in %lu ms", delayMs);
  sta = Sta::Waiting;
  nextRoundAtMs = millis() + delayMs;
  publish();
}

void startServices() {
  if (!ntpStarted) {
    configTime(0, 0, config::kNtpServer1, config::kNtpServer2);  // UTC: only needed for TLS and token expiry
    ntpStarted = true;
  }
  if (!mdnsStarted && MDNS.begin(host.c_str())) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
    LOG_I(kTag, "mDNS: http://%s.local/", host.c_str());
  }
}

void onGotIp() {
  if (sta != Sta::Attempting) {
    publish();
    return;
  }
  sta = Sta::Connected;
  connectedSsid = attempt.ssid.c_str();
  lastError = "";
  failingSinceMs = 0;
  gaveUpReported = false;
  backoff.reset();
  if (submitting) {
    submitting = false;
    loadSaved();
    wifi_policy::remember(saved, attempt, Settings::kMaxWifiNetworks);
    storeSaved();
  }
  LOG_I(kTag, "Connected to \"%s\", IP %s, RSSI %d", connectedSsid.c_str(), WiFi.localIP().toString().c_str(),
        WiFi.RSSI());
  startServices();
  notifyApp(app::NetEvent::Connected);
  publish();
}

void onDisconnected(uint8_t reason) {
  if (sta == Sta::Attempting) {
    if (reason != kReasonAssocLeave) attemptFailed(reason);
    return;
  }
  if (sta == Sta::Connected) {
    LOG_W(kTag, "Lost \"%s\" (reason %u)", connectedSsid.c_str(), reason);
    sta = Sta::Waiting;
    failingSinceMs = millis();
    nextRoundAtMs = millis();  // Reconnect straight away; backoff applies after a failed round
    notifyApp(app::NetEvent::Disconnected, reason);
    publish();
  }
  // Waiting or Idle: a late event from our own disconnect.
}

void startPortalNow(bool autoStop) {
  apAutoStop = autoStop;
  apStopAtMs = 0;
  apLastClientMs = millis();
  if (!apActive) {
    const IPAddress ip(192, 168, 4, 1);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apSsid.c_str(), settings->device().apPassword.c_str());
    dns.setErrorReplyCode(DNSReplyCode::NoError);
    dns.start(53, "*", ip);  // Every name resolves to us: the captive portal
    apActive = true;
    LOG_I(kTag, "Setup AP \"%s\" on %s%s", apSsid.c_str(), config::kPortalIp, autoStop ? " (auto-stop)" : "");
  }
  notifyApp(app::NetEvent::Changed);
  publish();
}

void stopPortalNow() {
  if (!apActive) return;
  dns.stop();
  WiFi.softAPdisconnect(true);  // Disables the AP interface; the station stays up
  apActive = false;
  apClients = 0;
  apStopAtMs = 0;
  LOG_I(kTag, "Setup AP stopped");
  notifyApp(app::NetEvent::Changed);
  publish();
}

void refreshHost() {
  host = wifi_policy::hostname(settings->device().hostname.c_str()).c_str();
  WiFi.setHostname(host.c_str());  // DHCP name, from the next connection
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
    if (sta == Sta::Connected) startServices();
  }
  publish();
}

void process(const Command &c) {
  switch (c.type) {
    case CmdType::Submit:
      submitting = true;
      lastError = "";
      notifyApp(app::NetEvent::SubmitStarted);
      beginAttempt({c.a.c_str(), c.b.c_str()});
      break;
    case CmdType::Forget:
      loadSaved();
      if (wifi_policy::forget(saved, c.a.c_str())) storeSaved();
      publish();
      break;
    case CmdType::StartPortal:
      startPortalNow(c.value != 0);
      break;
    case CmdType::StopPortal:
      if (c.value == 0) {
        stopPortalNow();
      } else if (apActive) {
        apStopAtMs = millis() + c.value;
        if (apStopAtMs == 0) apStopAtMs = 1;
      }
      break;
    case CmdType::Scan: {
      bool start = false;
      {
        std::lock_guard<std::mutex> lock(statusMutex);
        if (!scanning) scanning = start = true;
      }
      if (start && WiFi.scanNetworks(true, false) == WIFI_SCAN_FAILED) {
        std::lock_guard<std::mutex> lock(statusMutex);
        scanning = false;
      }
      break;
    }
    case CmdType::RefreshHostname:
      refreshHost();
      break;
    case CmdType::EvGotIp:
      onGotIp();
      break;
    case CmdType::EvDisconnected:
      onDisconnected(static_cast<uint8_t>(c.value));
      break;
    case CmdType::EvApClients:
      apClients = WiFi.softAPgetStationNum();
      apLastClientMs = millis();
      LOG_I(kTag, "Setup AP clients: %u", apClients);
      notifyApp(app::NetEvent::Changed);
      publish();
      break;
    case CmdType::EvScanDone: {
      const int count = WiFi.scanComplete();
      if (count >= 0) {
        readScanResults(count);
      } else {
        std::lock_guard<std::mutex> lock(statusMutex);
        scanning = false;
      }
      break;
    }
  }
}

void tick() {
  const uint32_t now = millis();

  if (sta == Sta::Attempting && now - attemptStartedMs >= config::kWifiAttemptMs) {
    WiFi.disconnect(false);
    attemptFailed(0);
  }
  // Background retries pause while a phone is on the setup AP: switching
  // channels to try a network would keep knocking it off.
  if (sta == Sta::Waiting && reached(now, nextRoundAtMs) && apClients == 0) startRound();

  if (failingSinceMs && !gaveUpReported && now - failingSinceMs >= config::kWifiGiveUpMs) {
    gaveUpReported = true;
    LOG_W(kTag, "Saved networks have failed for %lu min", config::kWifiGiveUpMs / 60000);
    notifyApp(app::NetEvent::GaveUp);
  }

  if (apActive) {
    if (apStopAtMs && reached(now, apStopAtMs)) {
      stopPortalNow();
    } else if (apAutoStop && apClients == 0 && now - apLastClientMs >= config::kPortalIdleMs) {
      LOG_I(kTag, "Setup AP unused for %lu min", config::kPortalIdleMs / 60000);
      stopPortalNow();
    }
  }

  if (ntpStarted && !timeSynced && time(nullptr) > kValidEpoch) {
    timeSynced = true;
    LOG_I(kTag, "Clock set by NTP");
    notifyApp(app::NetEvent::Changed);
    publish();
  }

  if (sta == Sta::Connected && now - rssiAtMs >= 5000) {
    rssiAtMs = now;
    publish();
  }
}

// Runs on the Arduino event task: hand everything to the net task.
void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      enqueue({CmdType::EvGotIp});
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      enqueue({CmdType::EvDisconnected, String(), String(), info.wifi_sta_disconnected.reason});
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      enqueue({CmdType::EvApClients});
      break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
      enqueue({CmdType::EvScanDone});
      break;
    default:
      break;
  }
}

void run(void *) {
  esp_task_wdt_add(nullptr);
  for (;;) {
    std::deque<Command> batch;
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      batch.swap(commands);
    }
    for (const Command &c : batch) process(c);
    tick();
    esp_task_wdt_reset();
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(config::kNetLoopMs));
  }
}

}  // namespace

void begin(Settings &s) {
  settings = &s;

  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  apSsid = String(config::kApPrefix) + suffix;
  host = wifi_policy::hostname(settings->device().hostname.c_str()).c_str();

  WiFi.persistent(false);         // Networks live in our own settings (§10)
  WiFi.setAutoReconnect(false);   // The supervisor decides when and where to reconnect
  WiFi.onEvent(onWifiEvent);
  WiFi.setHostname(host.c_str());
  WiFi.mode(WIFI_STA);

  loadSaved();
  sta = saved.empty() ? Sta::Idle : Sta::Waiting;
  nextRoundAtMs = millis();
  publish();
}

void startTask() {
  const config::TaskSpec &spec = config::kNetTask;
  xTaskCreatePinnedToCore(run, spec.name, spec.stackBytes, nullptr, spec.priority, &task, spec.core);
}

Status status() {
  std::lock_guard<std::mutex> lock(statusMutex);
  return snapshot;
}

std::vector<ScanEntry> scanResults(bool &scanningNow) {
  bool stale;
  std::vector<ScanEntry> results;
  {
    std::lock_guard<std::mutex> lock(statusMutex);
    results = scanCache;
    stale = scanAtMs == 0 || millis() - scanAtMs > config::kScanStaleMs;
    scanningNow = scanning || stale;
  }
  if (stale) enqueue({CmdType::Scan});
  return results;
}

void submit(const String &ssid, const String &password) { enqueue({CmdType::Submit, ssid, password}); }
void forget(const String &ssid) { enqueue({CmdType::Forget, ssid}); }
void startPortal(bool autoStop) { enqueue({CmdType::StartPortal, String(), String(), autoStop ? 1u : 0u}); }
void stopPortal(uint32_t afterMs) { enqueue({CmdType::StopPortal, String(), String(), afterMs}); }
void refreshHostname() { enqueue({CmdType::RefreshHostname}); }

}  // namespace net
