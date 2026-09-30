#pragma once

#include "storage/Settings.h"

// Web portal on port 80 (PLAN.md §8.3), served by ESPAsyncWebServer on the
// AsyncTCP task. Handlers never block: they read snapshots, queue work for the
// net and app tasks, and return.
//
// Pages: /  (status, open)   /setup  (first-run wizard)   /manage  (admin)
// Admin routes use HTTP Digest auth with user "admin". Phones on the setup AP
// skip it (they already needed the AP password from the screen), and so does
// everyone while no admin password is set.
namespace portal {

void begin(Settings &settings);

// Digest HA1 for the admin password, as stored in settings (dev/admin).
String adminHash(const String &password);

}  // namespace portal
