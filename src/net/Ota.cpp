#include "net/Ota.h"

#include <Update.h>
#include <esp_ota_ops.h>

#include <atomic>

#include "app/Events.h"
#include "util/Log.h"

// The Arduino core marks a new image valid at boot unless this returns true;
// we mark it ourselves once it has proven healthy (ota::markValid()).
extern "C" bool verifyRollbackLater() { return true; }

namespace ota {

namespace {

constexpr const char *kTag = "ota";

std::atomic<bool> busy{false};
bool failed = false;
String error;

void fail(const String &message) {
  failed = true;
  error = message;
  LOG_E(kTag, "Update failed: %s", message.c_str());
  app::post(app::EventType::Ota, static_cast<uint8_t>(app::OtaEvent::Failed));
}

}  // namespace

const char *stateName(const esp_partition_t *partition) {
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(partition, &state) != ESP_OK) return "no state";  // e.g. flashed over USB
  switch (state) {
    case ESP_OTA_IMG_NEW: return "new";
    case ESP_OTA_IMG_PENDING_VERIFY: return "pending verify";
    case ESP_OTA_IMG_VALID: return "valid";
    case ESP_OTA_IMG_INVALID: return "invalid";
    case ESP_OTA_IMG_ABORTED: return "aborted";
    case ESP_OTA_IMG_UNDEFINED: return "undefined";
  }
  return "?";
}

void begin() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  LOG_I(kTag, "Running from %s%s", running ? running->label : "?", pendingVerify() ? " (new image, not yet verified)" : "");
  // Every slot's state, so a failed update or rollback can be traced from the log.
  for (uint8_t i = 0; i < esp_ota_get_app_partition_count(); i++) {
    const auto subtype = static_cast<esp_partition_subtype_t>(ESP_PARTITION_SUBTYPE_APP_OTA_MIN + i);
    const esp_partition_t *slot = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, nullptr);
    if (slot) LOG_I(kTag, "Slot %s: %s%s", slot->label, stateName(slot), slot == running ? " (running)" : "");
  }
  if (rolledBack()) LOG_W(kTag, "An earlier update failed and was rolled back");
}

bool pendingVerify() {
  esp_ota_img_states_t state;
  const esp_partition_t *running = esp_ota_get_running_partition();
  return running && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
}

bool rolledBack() { return esp_ota_get_last_invalid_partition() != nullptr; }

void markValid() {
  if (!pendingVerify()) return;
  const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    LOG_I(kTag, "New image marked valid");
  } else {
    LOG_E(kTag, "Marking the image valid failed: %s", esp_err_to_name(err));
  }
}

bool start() {
  bool expected = false;
  if (!busy.compare_exchange_strong(expected, true)) return false;
  failed = false;
  error = "";
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    fail(Update.errorString());
    busy = false;
    return true;  // Started (and failed): the caller reports lastError()
  }
  LOG_I(kTag, "Update started");
  app::post(app::EventType::Ota, static_cast<uint8_t>(app::OtaEvent::Started));
  return true;
}

bool write(const uint8_t *data, size_t length) {
  if (failed) return false;
  // Update checks the image header (magic 0xE9) on the first block.
  if (Update.write(const_cast<uint8_t *>(data), length) != length) {
    fail(Update.errorString());
    Update.abort();
    busy = false;
    return false;
  }
  return true;
}

bool finish() {
  if (failed) return false;
  if (!Update.end(true)) {
    fail(Update.errorString());
    busy = false;
    return false;
  }
  LOG_I(kTag, "Update written (%u bytes); restarting into it", Update.progress());
  busy = false;
  app::post(app::EventType::Ota, static_cast<uint8_t>(app::OtaEvent::Succeeded));
  return true;
}

void abort() {
  if (!busy) return;
  Update.abort();
  if (!failed) fail("Upload interrupted");
  busy = false;
}

String lastError() { return error; }

}  // namespace ota
