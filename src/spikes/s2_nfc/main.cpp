// Spike S2 — PN532 over SPI3, NDEF on MIFARE Classic 1K, RF field control.
// See docs/spikes.md for the procedure and what to record.

#include <Arduino.h>
#include <SPI.h>

#include <NfcAdapter.h>
#include <PN532/PN532/PN532.h>
#include <PN532/PN532_SPI/PN532_SPI.h>

#include "pins.h"
#include "../common/Console.h"

namespace {

SPIClass nfcSpi(HSPI);  // SPI3; the LCD uses FSPI (SPI2)
PN532_SPI pn532Spi(nfcSpi, pins::NFC_SS);
PN532 pn532(pn532Spi);     // Direct access: firmware version, RF field, retries
NfcAdapter nfc(pn532Spi);  // NDEF read/write/format

// Field handling between searches (PLAN.md §9.5, option 1).
enum class FieldMode { Keep, Off, Explicit };
FieldMode fieldMode = FieldMode::Off;

bool ready = false;
bool polling = true;
bool trace = false;
uint32_t intervalMs = 200;   // 750 in the Screen off profile
uint16_t searchTimeoutMs = 100;
uint8_t retries = 0x01;      // MxRtyPassiveActivation: 1 found cards 50/50 and keeps empty searches shortest (spike S2 bench)
uint32_t settleMs = 0;       // Explicit mode: wait after switching the field on, so the card can power up
uint32_t spiHz = 2000000;

// Presence tracking (PLAN.md §7.2)
String presentUid;
uint8_t misses = 0;
uint32_t lastSearch = 0;

// Stats
uint32_t searches = 0, hits = 0;
uint64_t searchMicrosTotal = 0;
uint32_t searchMicrosMax = 0;

// The Seeed library's SAMConfig(), setPassiveActivationRetries() and setRFField()
// return `0 < readResponse(...)`, but these commands reply with zero data bytes,
// so they report failure on success. Send them directly and treat any
// non-negative response length as success.
bool command(std::initializer_list<uint8_t> bytes) {
  uint8_t buffer[8];
  uint8_t length = 0;
  for (uint8_t b : bytes) buffer[length++] = b;
  if (pn532Spi.writeCommand(buffer, length) != 0) return false;
  uint8_t response[16];
  return pn532Spi.readResponse(response, sizeof(response), 1000) >= 0;
}
bool samConfig() { return command({0x14, 0x01, 0x14, 0x01}); }  // SAMConfiguration: normal mode, 1 s, use IRQ
bool setRetries(uint8_t maxRetries) { return command({0x32, 0x05, 0xFF, 0x01, maxRetries}); }  // RFConfiguration item 5
bool setField(bool on) { return command({0x32, 0x01, static_cast<uint8_t>(on ? 0x01 : 0x00)}); }  // RFConfiguration item 1

const char *fieldModeName() {
  return fieldMode == FieldMode::Keep ? "keep" : fieldMode == FieldMode::Off ? "off" : "explicit";
}

bool initReader() {
  pn532.begin();  // Also runs PN532_SPI::begin(): mode 0, LSB first
  nfcSpi.setFrequency(spiHz);
  uint32_t version = pn532.getFirmwareVersion();
  if (!version) {
    Serial.println("PN532 not found. Check: switches I0=L I1=H (SPI), wiring IO4-7, 3V3 and GND. Type 'init' to retry.");
    return false;
  }
  Serial.printf("Found PN5%02lX, firmware %lu.%lu, SPI %lu Hz\n", (version >> 24) & 0xFF, (version >> 16) & 0xFF,
                (version >> 8) & 0xFF, spiHz);
  // Not nfc.begin(): it re-initialises SPI, repeats the firmware check and halts
  // forever (while(1)) if that fails. SAMConfig is chip state, so sending it
  // through our PN532 object configures the chip for the adapter too.
  uint32_t start = millis();
  bool ok = samConfig();
  Serial.printf("  SAMConfig: %s (%lu ms)\n", ok ? "ok" : "FAILED", millis() - start);
  start = millis();
  ok = setRetries(retries);
  Serial.printf("  Passive activation retries %u: %s (%lu ms)\n", retries, ok ? "ok" : "FAILED", millis() - start);
  if (fieldMode != FieldMode::Keep) {
    start = millis();
    ok = setField(false);
    Serial.printf("  RF field off between searches: %s (%lu ms)\n", ok ? "ok" : "FAILED", millis() - start);
  }
  return true;
}

// One card search, applying the field mode. Returns the UID as hex, or "" if none.
String search(uint32_t *micros_out = nullptr) {
  if (fieldMode == FieldMode::Explicit) {
    setField(true);
    if (settleMs) delay(settleMs);
  }
  uint8_t id[7] = {0};
  uint8_t idLength = 0;
  const uint32_t start = micros();
  bool found = pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, id, &idLength, searchTimeoutMs);
  const uint32_t elapsed = micros() - start;
  String uid;
  for (uint8_t i = 0; found && i < idLength; i++) {
    char hex[4];
    snprintf(hex, sizeof(hex), i ? " %02X" : "%02X", id[i]);
    uid += hex;
  }
  if (fieldMode != FieldMode::Keep && !found) setField(false);
  if (micros_out) *micros_out = elapsed;
  return uid;
}

void printMessage(NfcTag &tag) {
  Serial.printf("Tag %s, type %s\n", tag.getUidString().c_str(), tag.getTagType().c_str());
  if (!tag.hasNdefMessage()) {
    Serial.println("  No NDEF message (blank or not NDEF-formatted)");
    return;
  }
  NdefMessage message = tag.getNdefMessage();
  for (unsigned int i = 0; i < message.getRecordCount(); i++) {
    NdefRecord record = message.getRecord(i);
    const int length = record.getPayloadLength();
    byte payload[length + 1];
    record.getPayload(payload);
    payload[length] = 0;
    Serial.printf("  Record %u: TNF %u, type '%s', %d bytes\n", i, record.getTnf(), record.getType().c_str(), length);
    if (record.getType() == "U" && length > 0) {
      static const char *prefixes[] = {"", "http://www.", "https://www.", "http://", "https://"};
      const char *prefix = payload[0] < 5 ? prefixes[payload[0]] : "?";
      Serial.printf("    URI: %s%s  (prefix byte 0x%02X)\n", prefix, reinterpret_cast<char *>(payload + 1), payload[0]);
    } else {
      Serial.printf("    Payload: %s\n", reinterpret_cast<char *>(payload));
    }
  }
}

// URI record with the https:// prefix byte (0x04), as specified in PLAN.md §7.1.
NdefMessage uriMessage(String url) {
  static const struct {
    uint8_t code;
    const char *prefix;
  } kPrefixes[] = {{0x02, "https://www."}, {0x01, "http://www."}, {0x04, "https://"}, {0x03, "http://"}};
  uint8_t code = 0x00;
  for (const auto &p : kPrefixes) {
    if (url.startsWith(p.prefix)) {
      code = p.code;
      url = url.substring(strlen(p.prefix));
      break;
    }
  }
  byte payload[url.length() + 1];
  payload[0] = code;
  memcpy(payload + 1, url.c_str(), url.length());

  NdefRecord record;
  record.setTnf(TNF_WELL_KNOWN);
  const byte type[] = {'U'};
  record.setType(type, sizeof(type));
  record.setPayload(payload, sizeof(payload));
  NdefMessage message;
  message.addRecord(record);
  return message;
}

bool waitForTag(uint32_t ms = 3000) {
  if (fieldMode != FieldMode::Keep) setField(true);
  const uint32_t until = millis() + ms;
  while (millis() < until) {
    if (nfc.tagPresent(searchTimeoutMs)) return true;
    delay(50);
  }
  Serial.println("No card on the reader.");
  return false;
}

void pollOnce() {
  uint32_t us = 0;
  String uid = search(&us);
  searches++;
  if (trace) Serial.printf("search #%lu: %s in %.1f ms\n", searches, uid.isEmpty() ? "no card" : uid.c_str(), us / 1000.0f);
  searchMicrosTotal += us;
  searchMicrosMax = max(searchMicrosMax, us);
  if (!uid.isEmpty()) {
    hits++;
    misses = 0;
    if (uid != presentUid) {
      presentUid = uid;
      Serial.printf("Card detected: %s (search %.1f ms)\n", uid.c_str(), us / 1000.0f);
      const uint32_t start = millis();
      if (nfc.tagPresent(searchTimeoutMs)) {  // Select it through the adapter, then read NDEF
        NfcTag tag = nfc.read();
        Serial.printf("NDEF read %lu ms\n", millis() - start);
        printMessage(tag);
      } else {
        Serial.println("Card moved away before it could be read.");
      }
    }
    if (fieldMode != FieldMode::Keep) setField(false);
  } else if (!presentUid.isEmpty() && ++misses >= 3) {
    Serial.printf("Card removed: %s\n", presentUid.c_str());
    presentUid = "";
  }
}

void registerCommands() {
  console::add("init", "re-initialise the PN532", [](String) { ready = initReader(); });
  console::add("poll", "<on|off>  continuous card search", [](String args) {
    polling = args != "off";
    Serial.printf("Polling %s\n", polling ? "on" : "off");
  });
  console::add("trace", "<on|off>  log every search result", [](String args) {
    trace = args != "off";
    Serial.printf("Trace %s\n", trace ? "on" : "off");
  });
  console::add("settle", "<ms>  explicit mode: delay after field on before searching", [](String args) {
    settleMs = args.toInt();
    Serial.printf("Settle %lu ms\n", settleMs);
  });
  console::add("bench", "leave a card on the reader: hit rate for each field mode / retries / settle", [](String) {
    struct Config {
      FieldMode mode;
      uint8_t retries;
      uint32_t settle;
    };
    // Interleaved: every round runs each config once, so a card that moves during
    // the run affects all configs equally instead of skewing a block of them.
    // "keep" = field left on: it searches twice in a row and scores the second
    // search, i.e. re-detecting a card that is already powered and selected.
    const Config configs[] = {
        {FieldMode::Off, 1, 0},      {FieldMode::Off, 2, 0},      {FieldMode::Off, 5, 0},
        {FieldMode::Explicit, 2, 0}, {FieldMode::Explicit, 2, 2}, {FieldMode::Explicit, 2, 5},
        {FieldMode::Explicit, 1, 5}, {FieldMode::Explicit, 2, 10}, {FieldMode::Keep, 2, 0},
    };
    constexpr size_t kConfigs = sizeof(configs) / sizeof(configs[0]);
    int found[kConfigs] = {0};
    uint64_t totalUs[kConfigs] = {0};
    const FieldMode savedMode = fieldMode;
    const uint8_t savedRetries = retries;
    const uint32_t savedSettle = settleMs;
    constexpr int kRounds = 50;

    auto presenceCheck = [&]() {
      fieldMode = FieldMode::Explicit;
      settleMs = 5;
      setRetries(2);
      bool present = false;
      for (int i = 0; i < 3 && !present; i++) {
        present = !search().isEmpty();
        setField(false);
        delay(20);
      }
      return present;
    };

    const bool presentAtStart = presenceCheck();
    Serial.printf("Card present at start: %s. Running %d rounds of %u configs...\n", presentAtStart ? "yes" : "NO",
                  kRounds, kConfigs);
    for (int round = 0; round < kRounds; round++) {
      for (size_t i = 0; i < kConfigs; i++) {
        const Config &c = configs[i];
        fieldMode = c.mode;
        retries = c.retries;
        settleMs = c.settle;
        setRetries(retries);
        uint32_t us = 0;
        if (c.mode == FieldMode::Keep) {
          setField(true);
          delay(5);
          search();  // First search selects the card; the field stays on
        }
        if (!search(&us).isEmpty()) found[i]++;
        totalUs[i] += us;
        setField(false);
        delay(20);
      }
    }
    const bool presentAtEnd = presenceCheck();

    Serial.println("Mode      Retries Settle  Hits/50  Avg search");
    for (size_t i = 0; i < kConfigs; i++) {
      fieldMode = configs[i].mode;
      Serial.printf("%-9s %7u %4lu ms  %3d/%d   %5.1f ms\n", fieldModeName(), configs[i].retries, configs[i].settle,
                    found[i], kRounds, totalUs[i] / 1000.0f / kRounds);
    }
    Serial.printf("Card present at end: %s%s\n", presentAtEnd ? "yes" : "NO",
                  presentAtStart && presentAtEnd ? "" : "  (results unreliable: keep the card still and re-run)");
    fieldMode = savedMode;
    retries = savedRetries;
    settleMs = savedSettle;
    setRetries(retries);
    setField(fieldMode == FieldMode::Keep);
    Serial.println("Restored previous settings.");
  });
  console::add("interval", "<ms>  time between searches (200 active, 750 screen off)", [](String args) {
    intervalMs = max(20L, args.toInt());
    Serial.printf("Interval %lu ms\n", intervalMs);
  });
  console::add("timeout", "<ms>  search response timeout", [](String args) {
    searchTimeoutMs = max(10L, args.toInt());
    Serial.printf("Search timeout %u ms\n", searchTimeoutMs);
  });
  console::add("retries", "<0-255>  PN532 passive activation retries (0xFF = forever)", [](String args) {
    retries = args.toInt();
    Serial.printf("Retries %u: %s\n", retries, setRetries(retries) ? "ok" : "FAILED");
  });
  console::add("fieldmode", "<keep|off|explicit>  RF field between searches", [](String args) {
    fieldMode = args == "keep" ? FieldMode::Keep : args == "explicit" ? FieldMode::Explicit : FieldMode::Off;
    if (fieldMode != FieldMode::Keep) setField(false);
    Serial.printf("Field mode: %s\n", fieldModeName());
  });
  console::add("field", "<on|off>  hold the RF field (polling off) to measure current", [](String args) {
    polling = false;
    bool on = args == "on";
    Serial.printf("Polling off, RF field %s: %s\n", on ? "on" : "off", setField(on) ? "ok" : "FAILED");
  });
  console::add("spihz", "<Hz>  SPI clock (PN532 max 5 MHz)", [](String args) {
    spiHz = args.toInt();
    nfcSpi.setFrequency(spiHz);
    Serial.printf("SPI %lu Hz; firmware check: %s\n", spiHz, pn532.getFirmwareVersion() ? "ok" : "FAILED");
  });
  console::add("read", "read and print the card's NDEF message", [](String) {
    if (!waitForTag()) return;
    const uint32_t start = millis();
    NfcTag tag = nfc.read();
    Serial.printf("Read in %lu ms\n", millis() - start);
    printMessage(tag);
  });
  console::add("write", "<url>  write a URI record (formats a blank card first), then verify", [](String url) {
    if (url.isEmpty()) {
      Serial.println("Use: write https://open.spotify.com/album/4LH4d3cOWNNsVw41Gqt2kv");
      return;
    }
    if (!waitForTag()) return;
    NdefMessage message = uriMessage(url);
    Serial.printf("Message size %d bytes\n", message.getEncodedSize());
    const uint32_t start = millis();
    bool ok = nfc.write(message);
    if (!ok) {
      // A failed MIFARE Classic authentication drops the card out of the selected
      // state, so select it again before each step.
      Serial.println("Write failed (card not NDEF-formatted?). Formatting with the factory key...");
      ok = waitForTag() && nfc.format();
      Serial.printf("Format: %s\n", ok ? "ok" : "FAILED");
      ok = ok && waitForTag() && nfc.write(message);
    }
    Serial.printf("Write %s in %lu ms\n", ok ? "ok" : "FAILED (locked card or unknown keys?)", millis() - start);
    if (!ok || !waitForTag()) return;
    NfcTag tag = nfc.read();
    printMessage(tag);
  });
  console::add("format", "format a blank card as NDEF (factory key)", [](String) {
    if (waitForTag()) Serial.printf("Format: %s\n", nfc.format() ? "ok" : "FAILED");
  });
  console::add("erase", "write an empty NDEF record", [](String) {
    if (waitForTag()) Serial.printf("Erase: %s\n", nfc.erase() ? "ok" : "FAILED");
  });
  console::add("clean", "reset the card to factory state", [](String) {
    if (waitForTag()) Serial.printf("Clean: %s\n", nfc.clean() ? "ok" : "FAILED");
  });
  console::add("stats", "search count, hit rate and timing (then reset)", [](String) {
    Serial.printf("Searches %lu, hits %lu, avg %.1f ms, max %.1f ms, interval %lu ms, timeout %u ms, field %s\n",
                  searches, hits, searches ? searchMicrosTotal / 1000.0f / searches : 0.0f, searchMicrosMax / 1000.0f,
                  intervalMs, searchTimeoutMs, fieldModeName());
    searches = hits = 0;
    searchMicrosTotal = 0;
    searchMicrosMax = 0;
  });
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== Spike S2: PN532 over SPI, NDEF on MIFARE Classic ===");
  nfcSpi.begin(pins::NFC_SCK, pins::NFC_MISO, pins::NFC_MOSI, pins::NFC_SS);
  registerCommands();  // Before init, so 'help' and 'init' work even if the reader misbehaves
  ready = initReader();
  Serial.printf("Type 'help' for commands. %s\n",
                ready ? "Polling every 200 ms; tap a card." : "Reader not ready; fix wiring, then 'init'.");
}

void loop() {
  console::poll();
  if (ready && polling && millis() - lastSearch >= intervalMs) {
    lastSearch = millis();
    pollOnce();
  }
  delay(2);
}
