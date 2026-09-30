#include "net/Portal.h"

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WebAuthentication.h>

#include <memory>

#include "app/AppController.h"
#include "app/Events.h"
#include "config.h"
#include "net/Ota.h"
#include "net/WebAssets.h"
#include "net/WifiPolicy.h"
#include "net/WifiSupervisor.h"
#include "util/Diagnostics.h"
#include "util/Log.h"

namespace portal {

namespace {

constexpr const char *kTag = "portal";
constexpr size_t kLogBytes = 16 * 1024;

Settings *settings = nullptr;
AsyncWebServer server(80);
AsyncAuthenticationMiddleware adminAuth;
AsyncWebServerRequest *uploadOwner = nullptr;

enum class UploadResult : int { None, Rejected, Busy, Done };

bool fromSetupAp(AsyncWebServerRequest *r) {
  return net::status().apActive && r->client()->localIP() == IPAddress(192, 168, 4, 1);
}

void applyAdminHash(const String &hash) { adminAuth.setPasswordHash(hash.c_str()); }

String param(AsyncWebServerRequest *r, const char *name) {
  const AsyncWebParameter *p = r->getParam(name, true);
  return p ? p->value() : String();
}

void sendJson(AsyncWebServerRequest *r, const JsonDocument &doc, int code = 200) {
  String body;
  serializeJson(doc, body);
  r->send(code, "application/json", body);
}

void sendError(AsyncWebServerRequest *r, int code, const char *message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  sendJson(r, doc, code);
}

void sendOk(AsyncWebServerRequest *r) {
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(r, doc);
}

void sendAsset(AsyncWebServerRequest *r, const char *path) {
  for (const web::Asset &a : web::kAssets) {
    if (strcmp(a.path, path) == 0) {
      AsyncWebServerResponse *res = r->beginResponse(200, a.mime, a.data, a.length);
      res->addHeader("Content-Encoding", "gzip");
      res->addHeader("Cache-Control", "no-cache");
      r->send(res);
      return;
    }
  }
  r->send(404, "text/plain", "Not found");
}

void handleStatus(AsyncWebServerRequest *r) {
  const net::Status n = net::status();
  const DeviceSettings device = settings->device();
  const SpotifySettings sp = settings->spotify();

  JsonDocument doc;
  doc["name"] = device.name;
  doc["fw"] = config::kFirmwareVersion;
  doc["uptime_s"] = millis() / 1000;
  doc["state"] = app::name(app::currentPhase());
  doc["setup_ap"] = fromSetupAp(r);
  doc["admin_set"] = !device.adminHash.isEmpty();
  doc["time_synced"] = n.timeSynced;

  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["connected"] = n.staConnected;
  wifi["ssid"] = n.ssid;
  wifi["rssi"] = n.rssi;
  wifi["ip"] = n.ip;
  wifi["host"] = n.hostname;
  wifi["connecting"] = n.connecting;
  wifi["connecting_ssid"] = n.connectingSsid;
  wifi["error"] = n.lastError;
  wifi["saved"] = n.savedNetworks;
  JsonObject ap = wifi["ap"].to<JsonObject>();
  ap["active"] = n.apActive;
  ap["ssid"] = n.apSsid;
  ap["clients"] = n.apClients;

  JsonObject spotify = doc["spotify"].to<JsonObject>();
  spotify["linked"] = !sp.refreshToken.isEmpty();
  spotify["client_id_set"] = !sp.clientId.isEmpty();

  JsonObject update = doc["ota"].to<JsonObject>();
  update["pending_verify"] = ota::pendingVerify();
  update["rolled_back"] = ota::rolledBack();

  doc["reset_reason"] = diag::resetReason();
  doc["last_crash"] = diag::lastCrash();

  JsonObject heap = doc["heap"].to<JsonObject>();
  heap["internal"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  heap["internal_min"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
  heap["psram"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  sendJson(r, doc);
}

void handleWifiGet(AsyncWebServerRequest *r) {
  bool scanning = false;
  const std::vector<net::ScanEntry> scan = net::scanResults(scanning);
  JsonDocument doc;
  JsonArray saved = doc["saved"].to<JsonArray>();
  for (const WifiNetwork &n : settings->wifiNetworks()) saved.add(n.ssid);  // Never the passwords
  doc["scanning"] = scanning;
  JsonArray list = doc["scan"].to<JsonArray>();
  for (const net::ScanEntry &e : scan) {
    JsonObject o = list.add<JsonObject>();
    o["ssid"] = e.ssid;
    o["rssi"] = e.rssi;
    o["secure"] = e.secure;
  }
  sendJson(r, doc);
}

void handleWifiPost(AsyncWebServerRequest *r) {
  const String ssid = param(r, "ssid");
  const String password = param(r, "password");
  if (ssid.isEmpty() || ssid.length() > 32) return sendError(r, 400, "Network name must be 1-32 characters");
  if (!password.isEmpty() && (password.length() < 8 || password.length() > 63)) {
    return sendError(r, 400, "Wi-Fi passwords are 8-63 characters");
  }
  net::submit(ssid, password);
  sendOk(r);
}

void handleWifiForget(AsyncWebServerRequest *r) {
  const String ssid = param(r, "ssid");
  if (ssid.isEmpty()) return sendError(r, 400, "Which network?");
  net::forget(ssid);
  sendOk(r);
}

void handleDeviceGet(AsyncWebServerRequest *r) {
  const DeviceSettings device = settings->device();
  const SpotifySettings sp = settings->spotify();
  JsonDocument doc;
  doc["name"] = device.name;
  doc["host"] = device.hostname;
  doc["admin_set"] = !device.adminHash.isEmpty();
  doc["client_id"] = sp.clientId;
  doc["relay_url"] = sp.relayUrl;
  sendJson(r, doc);
}

bool validClientId(const String &id) {
  if (id.isEmpty() || id.length() > 64) return false;
  for (char c : id) {
    if (!isalnum(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

// Every field is optional; only the ones sent change. The reply carries the
// web name as saved, since what was typed is cleaned up (spaces, ".local").
void handleDevicePost(AsyncWebServerRequest *r) {
  DeviceSettings device = settings->device();
  SpotifySettings sp = settings->spotify();
  bool hostChanged = false;

  if (r->hasParam("name", true)) {
    String name = param(r, "name");
    name.trim();
    if (name.isEmpty() || name.length() > 32) return sendError(r, 400, "Name must be 1-32 characters");
    device.name = name;
  }
  if (r->hasParam("host", true)) {
    String typed = param(r, "host");
    typed.trim();
    if (typed.isEmpty()) return sendError(r, 400, "Enter a web address, e.g. spotify-cd");
    const String host = wifi_policy::hostname(typed.c_str()).c_str();
    hostChanged = host != device.hostname;
    device.hostname = host;
  }
  if (r->hasParam("admin_password", true)) {
    const String password = param(r, "admin_password");
    if (!password.isEmpty() && password.length() < 6) return sendError(r, 400, "Use at least 6 characters");
    device.adminHash = password.isEmpty() ? String() : adminHash(password);
  }
  if (r->hasParam("client_id", true)) {
    String id = param(r, "client_id");
    id.trim();
    if (!id.isEmpty() && !validClientId(id)) return sendError(r, 400, "That doesn't look like a Client ID");
    sp.clientId = id;
  }
  if (r->hasParam("relay_url", true)) {
    String url = param(r, "relay_url");
    url.trim();
    if (!url.isEmpty() && !url.startsWith("https://")) return sendError(r, 400, "The relay URL must start with https://");
    if (!url.isEmpty() && !url.endsWith("/")) url += "/";
    sp.relayUrl = url;
  }

  const bool adminChanged = device.adminHash != settings->device().adminHash;
  settings->setDevice(device);
  settings->setSpotify(sp);
  applyAdminHash(device.adminHash);
  if (hostChanged) net::refreshHostname();
  app::post(app::EventType::SettingsChanged);
  LOG_I(kTag, "Settings saved: name \"%s\", web address %s.local%s", device.name.c_str(), device.hostname.c_str(),
        !adminChanged ? "" : device.adminHash.isEmpty() ? ", admin password removed" : ", admin password set");

  JsonDocument doc;
  doc["ok"] = true;
  doc["host"] = device.hostname;
  sendJson(r, doc);
}

void handleLogs(AsyncWebServerRequest *r) {
  std::unique_ptr<char, decltype(&free)> buffer(static_cast<char *>(heap_caps_malloc(kLogBytes, MALLOC_CAP_SPIRAM)),
                                                &free);
  if (!buffer) return sendError(r, 500, "Out of memory");
  logging::copyRecent(buffer.get(), kLogBytes);
  r->send(200, "text/plain; charset=utf-8", buffer.get());
}

void handleMaintenance(AsyncWebServerRequest *r, app::Maintenance action) {
  sendOk(r);
  app::post(app::EventType::Maintenance, static_cast<uint8_t>(action));
}

UploadResult &uploadResult(AsyncWebServerRequest *r) {
  if (!r->_tempObject) {
    r->_tempObject = malloc(sizeof(UploadResult));  // Freed with the request
    *static_cast<UploadResult *>(r->_tempObject) = UploadResult::None;
  }
  return *static_cast<UploadResult *>(r->_tempObject);
}

// Runs as the body arrives, BEFORE the auth middleware (which only runs once
// the whole request is in). So it checks the admin login itself before any
// byte reaches flash.
void handleUploadChunk(AsyncWebServerRequest *r, const String &filename, size_t index, uint8_t *data, size_t length,
                       bool final) {
  if (index == 0) {
    if (!adminAuth.allowed(r)) {
      uploadResult(r) = UploadResult::Rejected;
      return;
    }
    if (!ota::start()) {
      uploadResult(r) = UploadResult::Busy;
      return;
    }
    LOG_I(kTag, "Firmware upload: %s", filename.c_str());
    uploadOwner = r;
    r->onDisconnect([r] {
      if (uploadOwner == r) {
        uploadOwner = nullptr;
        ota::abort();
      }
    });
  }
  if (r != uploadOwner) return;
  if (length) ota::write(data, length);
  if (final) {
    ota::finish();
    uploadOwner = nullptr;
    uploadResult(r) = UploadResult::Done;
  }
}

void handleUploadDone(AsyncWebServerRequest *r) {
  switch (uploadResult(r)) {
    case UploadResult::Busy:
      return sendError(r, 409, "Another update is in progress");
    case UploadResult::Rejected:
      return sendError(r, 401, "Admin password required");
    case UploadResult::None:
      return sendError(r, 400, "No firmware file received");
    case UploadResult::Done:
      break;
  }
  const String error = ota::lastError();
  if (!error.isEmpty()) return sendError(r, 400, error.c_str());
  sendOk(r);  // The app restarts into the new image shortly
}

// Phones on the setup AP ask for connectivity-check URLs on other hosts
// (connectivitycheck.gstatic.com, captive.apple.com, ...). Redirecting every
// foreign host to the wizard makes them open it as a captive portal.
void captiveRedirect(AsyncWebServerRequest *r, ArMiddlewareNext next) {
  if (fromSetupAp(r)) {
    String host = r->host();
    const int colon = host.indexOf(':');
    if (colon >= 0) host = host.substring(0, colon);
    if (host != config::kPortalIp) {
      r->redirect(String("http://") + config::kPortalIp + "/setup");
      return;
    }
  }
  next();
}

}  // namespace

String adminHash(const String &password) {
  return generateDigestHash(config::kAdminUser, password.c_str(), config::kAdminRealm);
}

void begin(Settings &s) {
  settings = &s;

  adminAuth.setUsername(config::kAdminUser);
  adminAuth.setRealm(config::kAdminRealm);
  adminAuth.setAuthType(AsyncAuthType::AUTH_DIGEST);
  adminAuth.setAuthFailureMessage("Admin password required");
  adminAuth.setAuthentificationFunction([](AsyncWebServerRequest *r) {
    return fromSetupAp(r) || r->authenticate(config::kAdminUser, adminAuth.credentials().c_str(),
                                             config::kAdminRealm, true);
  });
  applyAdminHash(settings->device().adminHash);

  server.addMiddleware(captiveRedirect);

  // Exact matching throughout: a plain string also matches everything below it
  // ("/api/wifi" would catch "/api/wifi/forget"), and handlers are tried in order.
  using M = AsyncURIMatcher;
  server.on(M::exact("/"), HTTP_GET, [](AsyncWebServerRequest *r) { sendAsset(r, "/index.html"); });
  server.on(M::exact("/setup"), HTTP_GET, [](AsyncWebServerRequest *r) { sendAsset(r, "/setup.html"); })
      .addMiddleware(&adminAuth);
  server.on(M::exact("/manage"), HTTP_GET, [](AsyncWebServerRequest *r) { sendAsset(r, "/manage.html"); })
      .addMiddleware(&adminAuth);
  server.on(M::exact("/app.js"), HTTP_GET, [](AsyncWebServerRequest *r) { sendAsset(r, "/app.js"); });
  server.on(M::exact("/style.css"), HTTP_GET, [](AsyncWebServerRequest *r) { sendAsset(r, "/style.css"); });

  server.on(M::exact("/api/status"), HTTP_GET, handleStatus);
  server.on(M::exact("/api/wifi"), HTTP_GET, handleWifiGet).addMiddleware(&adminAuth);
  server.on(M::exact("/api/wifi"), HTTP_POST, handleWifiPost).addMiddleware(&adminAuth);
  server.on(M::exact("/api/wifi/forget"), HTTP_POST, handleWifiForget).addMiddleware(&adminAuth);
  server.on(M::exact("/api/device"), HTTP_GET, handleDeviceGet).addMiddleware(&adminAuth);
  server.on(M::exact("/api/device"), HTTP_POST, handleDevicePost).addMiddleware(&adminAuth);
  server.on(M::exact("/api/logs"), HTTP_GET, handleLogs).addMiddleware(&adminAuth);
  server.on(M::exact("/api/reboot"), HTTP_POST, [](AsyncWebServerRequest *r) {
    handleMaintenance(r, app::Maintenance::Reboot);
  }).addMiddleware(&adminAuth);
  server.on(M::exact("/api/factory-reset"), HTTP_POST, [](AsyncWebServerRequest *r) {
    handleMaintenance(r, app::Maintenance::FactoryReset);
  }).addMiddleware(&adminAuth);
  server.on(M::exact("/update"), HTTP_POST, handleUploadDone, handleUploadChunk).addMiddleware(&adminAuth);

  server.onNotFound([](AsyncWebServerRequest *r) { r->send(404, "text/plain", "Not found"); });
  server.begin();
  LOG_I(kTag, "Web portal on port 80");
}

}  // namespace portal
