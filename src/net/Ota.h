#pragma once

#include <Arduino.h>

// Firmware updates and rollback (PLAN.md §4.8, §8.4, §11).
// A new image boots in "pending verify". The app calls markValid() once the
// image has run healthily for config::kOtaHealthyAfterMs; if it crashes or
// reboots before then, the bootloader goes back to the previous image.
namespace ota {

void begin();  // Logs the running slot and whether the last update rolled back

bool pendingVerify();  // This boot is a new image not yet marked valid
bool rolledBack();     // An earlier update failed and was rolled back
void markValid();

// Upload, fed by the portal's /update handler (one upload at a time).
bool start();                                   // False if another upload is running
bool write(const uint8_t *data, size_t length);  // False once anything failed
bool finish();                                  // True if the image is complete and valid
void abort();
String lastError();

}  // namespace ota
