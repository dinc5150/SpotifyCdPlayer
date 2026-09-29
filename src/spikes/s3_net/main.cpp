// Spikes S3 + S4 — TLS to api.spotify.com (handshake cost, keep-alive, idle close)
// and a Spotify Web API console for checking playback behaviour per card type.
// See docs/spikes.md for the procedure and what to record.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include "Console.h"

namespace {

Preferences prefs;
NetworkClientSecure apiClient;      // Persistent keep-alive connection to api.spotify.com
NetworkClientSecure accountsClient; // accounts.spotify.com, used for token refresh
String accessToken;
uint32_t accessTokenExpiresAt = 0;

struct Response {
  int status = 0;
  String body;
  uint32_t ms = 0;
  bool reused = false;
};

void printHeap(const char *label) {
  Serial.printf("[%s] internal free %u (largest block %u), psram free %u\n", label,
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

bool connectWifi(const String &ssid, const String &password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("Joining %s", ssid.c_str());
  for (int i = 0; i < 60 && !WiFi.isConnected(); i++) {
    delay(250);
    Serial.print('.');
  }
  Serial.printf("\n%s  IP %s  RSSI %d dBm\n", WiFi.isConnected() ? "Connected" : "FAILED",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return WiFi.isConnected();
}

bool syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  const uint32_t start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < 10000) delay(100);
  const time_t now = time(nullptr);
  Serial.printf("NTP %s in %lu ms: %s", now > 1700000000 ? "synced" : "FAILED", millis() - start, ctime(&now));
  return now > 1700000000;
}

// One request over a persistent client. `reused` reports whether the TLS
// connection was already open (no handshake) when the request started.
Response request(NetworkClientSecure &client, const char *method, const String &url, const String &body = "",
                 const char *contentType = "application/json", bool auth = true) {
  Response r;
  // One long-lived HTTPClient per host: ~HTTPClient() calls stop() on the
  // socket, so a per-request HTTPClient would close the keep-alive connection.
  static HTTPClient apiHttp, accountsHttp;
  HTTPClient &http = &client == &apiClient ? apiHttp : accountsHttp;
  http.setReuse(true);
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  r.reused = client.connected();
  const uint32_t start = millis();
  if (!http.begin(client, url)) {
    r.status = -1;
    return r;
  }
  if (auth && !accessToken.isEmpty()) http.addHeader("Authorization", "Bearer " + accessToken);
  if (!body.isEmpty() || strcmp(method, "GET") != 0) http.addHeader("Content-Type", contentType);
  // HTTPClient only sends Content-Length for a non-empty body, and Spotify's
  // front end rejects a body-less PUT/POST (pause, next, volume...) with 411.
  if (body.isEmpty() && strcmp(method, "GET") != 0) http.addHeader("Content-Length", "0");
  r.status = http.sendRequest(method, body);
  if (r.status > 0) r.body = http.getString();
  r.ms = millis() - start;
  http.end();  // Keeps the socket open when the server allows keep-alive
  return r;
}

void printResponse(const Response &r, size_t maxBody = 1500) {
  Serial.printf("HTTP %d in %lu ms (%s connection)\n", r.status, r.ms, r.reused ? "reused" : "new");
  if (!r.body.isEmpty()) {
    Serial.println(r.body.length() > maxBody ? r.body.substring(0, maxBody) + "\n...(truncated)" : r.body);
  }
}

bool refreshAccessToken() {
  const String clientId = prefs.getString("client_id");
  const String refreshToken = prefs.getString("refresh");
  if (clientId.isEmpty() || refreshToken.isEmpty()) {
    Serial.println("No token saved. Use: token <client_id> <refresh_token>  (get one with tools/spotify_token.py)");
    return false;
  }
  const String form = "grant_type=refresh_token&refresh_token=" + refreshToken + "&client_id=" + clientId;
  Response r = request(accountsClient, "POST", "https://accounts.spotify.com/api/token", form,
                       "application/x-www-form-urlencoded", false);
  if (r.status != 200) {
    printResponse(r);
    if (r.body.indexOf("invalid_grant") >= 0) Serial.println("invalid_grant: the refresh token expired or was revoked.");
    return false;
  }
  JsonDocument doc;
  deserializeJson(doc, r.body);
  accessToken = doc["access_token"].as<String>();
  accessTokenExpiresAt = millis() + doc["expires_in"].as<uint32_t>() * 1000;
  if (doc["refresh_token"].is<const char *>()) {
    prefs.putString("refresh", doc["refresh_token"].as<String>());
    Serial.println("Refresh token rotated; saved the new one.");
  }
  Serial.printf("Access token ok (expires in %lu s), %lu ms\n", doc["expires_in"].as<uint32_t>(), r.ms);
  return true;
}

bool ensureToken() {
  if (!accessToken.isEmpty() && static_cast<int32_t>(accessTokenExpiresAt - millis()) > 60000) return true;
  return refreshAccessToken();
}

Response api(const char *method, const String &path, const String &body = "") {
  Response r;
  if (!ensureToken()) return r;
  r = request(apiClient, method, "https://api.spotify.com" + path, body);
  if (r.status == 401 && refreshAccessToken()) r = request(apiClient, method, "https://api.spotify.com" + path, body);
  return r;
}

void printPlaybackState() {
  Response r = api("GET", "/v1/me/player?additional_types=episode");
  if (r.status == 204) {
    Serial.printf("Nothing playing (204), %lu ms\n", r.ms);
    return;
  }
  if (r.status != 200) {
    printResponse(r);
    return;
  }
  JsonDocument filter;
  filter["is_playing"] = true;
  filter["progress_ms"] = true;
  filter["shuffle_state"] = true;
  filter["repeat_state"] = true;
  filter["device"]["name"] = true;
  filter["device"]["id"] = true;
  filter["device"]["volume_percent"] = true;
  filter["device"]["supports_volume"] = true;
  filter["context"]["uri"] = true;
  filter["context"]["type"] = true;
  filter["item"]["name"] = true;
  filter["item"]["type"] = true;
  filter["item"]["duration_ms"] = true;
  filter["item"]["album"]["name"] = true;
  filter["item"]["album"]["uri"] = true;
  filter["item"]["artists"][0]["name"] = true;
  filter["item"]["show"]["name"] = true;
  filter["item"]["show"]["uri"] = true;
  JsonDocument doc;
  deserializeJson(doc, r.body, DeserializationOption::Filter(filter));
  Serial.printf("HTTP 200 in %lu ms (%s connection), %u bytes\n", r.ms, r.reused ? "reused" : "new", r.body.length());
  serializeJsonPretty(doc, Serial);
  Serial.println();
}

void listDevices() {
  Response r = api("GET", "/v1/me/player/devices");
  if (r.status != 200) {
    printResponse(r);
    return;
  }
  JsonDocument doc;
  deserializeJson(doc, r.body);
  Serial.printf("%u devices (%lu ms):\n", doc["devices"].size(), r.ms);
  for (JsonObject d : doc["devices"].as<JsonArray>()) {
    Serial.printf("  %s%-24s %-12s vol %3d%% %s%s  id=%s\n", d["is_active"] ? "* " : "  ", d["name"].as<const char *>(),
                  d["type"].as<const char *>(), d["volume_percent"] | -1,
                  d["supports_volume"] ? "" : "(no remote volume) ", d["is_restricted"] ? "(restricted)" : "",
                  d["id"].as<const char *>());
  }
}

void registerCommands() {
  console::add("wifi", "<ssid> <password>  join and save Wi-Fi", [](String args) {
    String ssid = console::nextWord(args);
    if (connectWifi(ssid, args)) {
      prefs.putString("ssid", ssid);
      prefs.putString("pass", args);
      syncTime();
    }
  });
  console::add("ps", "<none|min|max>  Wi-Fi power save mode", [](String args) {
    wifi_ps_type_t mode = args == "none" ? WIFI_PS_NONE : args == "max" ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM;
    Serial.printf("Wi-Fi power save %s: %s\n", args.c_str(), WiFi.setSleep(mode) ? "ok" : "FAILED");
  });
  console::add("cpu", "<80|160|240>  set CPU clock", [](String args) {
    setCpuFrequencyMhz(args.toInt());
    Serial.printf("CPU %lu MHz\n", getCpuFrequencyMhz());
  });
  console::add("ntp", "sync the clock", [](String) { syncTime(); });
  console::add("tls", "close and re-open the api.spotify.com connection; time the handshake", [](String) {
    apiClient.stop();
    printHeap("before");
    const uint32_t start = millis();
    bool ok = apiClient.connect("api.spotify.com", 443);
    Serial.printf("TLS connect %s in %lu ms at %lu MHz\n", ok ? "ok" : "FAILED", millis() - start, getCpuFrequencyMhz());
    printHeap("connected");
  });
  console::add("ka", "<n>  n requests on the persistent connection (401 without a token is fine)", [](String args) {
    const int n = max(1L, args.toInt());
    for (int i = 0; i < n; i++) {
      Response r = request(apiClient, "GET", "https://api.spotify.com/v1/", "", "application/json", false);
      Serial.printf("  #%d HTTP %d in %lu ms (%s)\n", i + 1, r.status, r.ms, r.reused ? "reused" : "new");
    }
  });
  console::add("idle", "<seconds>  request, wait, check whether Spotify closed the idle connection", [](String args) {
    const uint32_t seconds = max(1L, args.toInt());
    Response first = request(apiClient, "GET", "https://api.spotify.com/v1/", "", "application/json", false);
    Serial.printf("Request %d (%s). Waiting %lu s...\n", first.status, first.reused ? "reused" : "new", seconds);
    delay(seconds * 1000);
    Serial.printf("After %lu s idle the connection is %s.\n", seconds, apiClient.connected() ? "OPEN" : "CLOSED");
    Response second = request(apiClient, "GET", "https://api.spotify.com/v1/", "", "application/json", false);
    Serial.printf("Next request %d in %lu ms (%s)\n", second.status, second.ms, second.reused ? "reused" : "new");
  });
  console::add("token", "<client_id> <refresh_token>  save Spotify credentials", [](String args) {
    prefs.putString("client_id", console::nextWord(args));
    prefs.putString("refresh", args);
    accessToken = "";
    refreshAccessToken();
  });
  console::add("refresh", "refresh the access token now", [](String) { refreshAccessToken(); });
  console::add("me", "GET /v1/me", [](String) { printResponse(api("GET", "/v1/me")); });
  console::add("state", "playback state (filtered, as the device will parse it)", [](String) { printPlaybackState(); });
  console::add("devices", "list Spotify Connect devices", [](String) { listDevices(); });
  console::add("play", "<spotify:uri> [device_id]  play a context (album/playlist/artist/show/collection) or track/episode",
               [](String args) {
                 String uri = console::nextWord(args);
                 String path = "/v1/me/player/play";
                 if (!args.isEmpty()) path += "?device_id=" + args;
                 const bool single = uri.startsWith("spotify:track:") || uri.startsWith("spotify:episode:");
                 String body = single ? "{\"uris\":[\"" + uri + "\"]}" : "{\"context_uri\":\"" + uri + "\"}";
                 printResponse(api("PUT", path, body));
               });
  console::add("transfer", "<device_id>  move playback to a device", [](String args) {
    printResponse(api("PUT", "/v1/me/player", "{\"device_ids\":[\"" + args + "\"],\"play\":true}"));
  });
  console::add("pause", "pause", [](String) { printResponse(api("PUT", "/v1/me/player/pause")); });
  console::add("resume", "resume", [](String) { printResponse(api("PUT", "/v1/me/player/play")); });
  console::add("next", "next track", [](String) { printResponse(api("POST", "/v1/me/player/next")); });
  console::add("prev", "previous track", [](String) { printResponse(api("POST", "/v1/me/player/previous")); });
  console::add("vol", "<0-100>  set volume", [](String args) {
    printResponse(api("PUT", "/v1/me/player/volume?volume_percent=" + args));
  });
  console::add("shuffle", "<true|false>", [](String args) {
    printResponse(api("PUT", "/v1/me/player/shuffle?state=" + args));
  });
  console::add("repeat", "<off|context|track>", [](String args) {
    printResponse(api("PUT", "/v1/me/player/repeat?state=" + args));
  });
  console::add("get", "<path>  raw GET, e.g. get /v1/playlists/<id>?fields=name,owner(id)", [](String args) {
    printResponse(api("GET", args));
  });
  console::add("put", "<path> [json]  raw PUT", [](String args) {
    String path = console::nextWord(args);
    printResponse(api("PUT", path, args));
  });
  console::add("post", "<path> [json]  raw POST", [](String args) {
    String path = console::nextWord(args);
    printResponse(api("POST", path, args));
  });
  console::add("heap", "print free memory", [](String) { printHeap("now"); });
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== Spikes S3 + S4: TLS and Spotify Web API ===");
  prefs.begin("spike", false);
  apiClient.useBuiltinCACertBundle();
  accountsClient.useBuiltinCACertBundle();
  registerCommands();

  const String ssid = prefs.getString("ssid");
  if (!ssid.isEmpty() && connectWifi(ssid, prefs.getString("pass"))) syncTime();
  else Serial.println("No saved Wi-Fi. Use: wifi <ssid> <password>");
  printHeap("boot");
  Serial.println("Type 'help' for commands.");
}

void loop() {
  console::poll();
  delay(5);
}
