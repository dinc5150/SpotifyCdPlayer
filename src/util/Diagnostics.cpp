#include "util/Diagnostics.h"

#include <esp_core_dump.h>
#include <esp_system.h>

#include "util/Log.h"

namespace diag {

namespace {

constexpr const char *kTag = "diag";

String reason;
String crash;

const char *describe(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "reset pin";
    case ESP_RST_SW: return "software restart";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    default: return "unknown";
  }
}

}  // namespace

void logBootDiagnostics() {
  const esp_reset_reason_t r = esp_reset_reason();
  reason = describe(r);
  if (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ||
      r == ESP_RST_BROWNOUT) {
    LOG_W(kTag, "Reset reason: %s", reason.c_str());
  } else {
    LOG_I(kTag, "Reset reason: %s", reason.c_str());
  }

  if (esp_core_dump_image_check() != ESP_OK) return;  // No dump saved
  esp_core_dump_summary_t summary;
  if (esp_core_dump_get_summary(&summary) == ESP_OK) {
    char line[200];
    int n = snprintf(line, sizeof(line), "task %s, PC 0x%08lx, cause %lu; backtrace", summary.exc_task,
                     static_cast<unsigned long>(summary.exc_pc), static_cast<unsigned long>(summary.ex_info.exc_cause));
    for (uint32_t i = 0; i < summary.exc_bt_info.depth && i < 16 && n < static_cast<int>(sizeof(line)) - 12; i++) {
      n += snprintf(line + n, sizeof(line) - n, " 0x%08lx", static_cast<unsigned long>(summary.exc_bt_info.bt[i]));
    }
    crash = line;
    LOG_W(kTag, "Last crash: %s%s", line, summary.exc_bt_info.corrupted ? " (backtrace corrupted)" : "");
  }
  esp_core_dump_image_erase();
}

const String &resetReason() { return reason; }
const String &lastCrash() { return crash; }

}  // namespace diag
