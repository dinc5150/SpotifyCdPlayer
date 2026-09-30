# Test checklist

One section per phase, with the acceptance criteria from PLAN.md §13. Record results in the tables.

Build, flash and open the console from **PowerShell or the VS Code PlatformIO sidebar**, not Git Bash:

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run -e dev -t upload
& $pio device monitor -e dev      # 'help' lists the console commands
```

---

## Phase 1 — Foundations

The PN532 can stay wired; this phase doesn't use it. Wi-Fi and Spotify aren't built yet, so the
firmware pretends both are connected and boots to the Idle screen (`kStubConnectivity` in
`src/app/AppController.cpp`, removed in Phase 2).

| # | Check | How | Pass when | Result |
|---|---|---|---|---|
| 1.1 | Boot to Idle | Press RESET and time it. RESET drops the USB serial port, so the boot lines are gone before the monitor reconnects: reopen the monitor if needed and type `log` to see them | Idle ("Tap a card to play", pulsing card) visible in under 2 s; `log` shows `Idle on screen N ms after start` | Under 1 s by eye (2026-09-29) |
| 1.2 | No garbage at power-on | Unplug and replug USB, watch the screen | Dark, then the boot screen, then Idle; no noise or half-drawn frames | no garbage |
| 1.3 | Press feedback | `presslog on`, then tap the ≡ button and the speaker chip a few times each | Each press highlights at once; the log shows `Press to pixels` under 50 ms | 7.5–19.0 ms (3 presses) |
| 1.4 | Navigation | ≡ → Menu → each tile → its placeholder → ← back | Tiles open "Arrives in Phase N" screens; ← returns to Menu, then to Idle | works |
| 1.5 | Speaker chip | Tap the chip on Idle | "Choose speaker" placeholder opens; ← returns to Idle | works |
| 1.6 | Sliding off cancels | Press a tile, slide the finger off it, lift | Nothing opens | Works |
| 1.7 | Settings persist | `set bright 30`, `set name Kitchen`, `set speaker Living Room`, then RESET; then `settings` | Screen stays at 30% brightness, the boot screen shows "Kitchen", the chip shows "Living Room"; `settings` prints them | works |
| 1.8 | Settings are sanitised | `set bright 0`, `set dim 45` | `settings` shows bright=5 and dim=60 | yes |
| 1.9 | BOOT short press | Press and release BOOT | Toast "Play/pause needs Spotify (Phase 4)" | yes |
| 1.10 | BOOT long press | Open Menu → About, hold BOOT ~2 s | Returns to Idle | yes |
| 1.11 | Factory reset dialog | Hold BOOT 10 s (or `factory`); tap **Cancel**; repeat and tap **Erase** | Cancel closes it and nothing changes. Erase restarts with defaults (`settings`: bright=80, name "Spotify CD Player", a new `ap_pw`) | yes |
| 1.12 | Toast and dialog | `toast Hello there`, `confirm` | Toast shows for ~2.5 s; the dialog's buttons log `Dialog 1 accepted/cancelled` | yes |
| 1.13 | State machine drive | `ev wifi_lost`, `ev wifi_restored`, `ev login_expired`, `ev spotify_linked`, `ev open_overlay about`, `ev home` | wifi_lost: Idle status reads "No connection"; wifi_restored clears it; login_expired shows the Link Spotify placeholder (no back button); spotify_linked returns to Idle; About opens and home closes it. Each command logs the new state | yes |
| 1.14 | Stays up | Leave it running 10 min with the console open | No reboots or watchdog resets; the 60 s heap lines stay flat | yes |
| 1.15 | Unit tests | Close the monitor (the runner needs the port), then in PowerShell: `$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"; & $pio test -e test_device` | All tests pass (`pio test -e native` does the same on the PC once a host gcc/g++ is installed) | all passed |

Boot log line to copy here (1.1): ______

**Phase 1: all checks passed (2026-09-29).**

---

## Phase 2 — Connectivity and portal

Spotify isn't built yet, so once Wi-Fi connects the firmware acts as if Spotify is linked and goes to
Idle (`kStubSpotifyLinked` in `src/app/AppController.cpp`, removed in Phase 3). The Idle status line
shows the Wi-Fi network.

Start from a **factory-fresh device**: `factory` in the console (tap **Erase**), or Manage → Factory reset.
You need a phone (ideally one iPhone and one Android) and your home Wi-Fi password.

| # | Check | How | Pass when | Result |
|---|---|---|---|---|
| 2.1 | Setup screen | After the factory reset | Screen shows STEP 1 OF 3, a QR code, and the network `SpotifyCD-XXXX` + password | yes |
| 2.2 | Join by QR | Scan the QR with the phone camera and join | Phone joins; the screen switches to STEP 2 OF 3 with a second QR | yes |
| 2.3 | Captive portal | Wait a few seconds after joining | The phone pops up the setup page by itself (iOS: sign-in sheet; Android: "Sign in to network" notification). If not, scan the step 2 QR | yes |
| 2.4 | Wrong password | In the wizard pick your network, enter a **wrong** password, go through the steps, Save and connect | Within ~20 s: "Wrong password?" and Try again. The screen goes back to setup | yes |
| 2.5 | Setup completes | Try again with the right password | "Connected" with `http://spotify-cd.local/` and the IP. The device shows Idle with "Wi-Fi: <network>". The setup Wi-Fi disappears ~2 min later | yes |
| 2.6 | Both phones | Repeat 2.1–2.5 with the other phone type (factory reset first) | Same result on iOS and Android | yes |
| 2.7 | Reconnect after reboot | RESET | Connecting screen briefly, then Idle | yes |
| 2.8 | Router off / on | Switch the router off; wait for Idle to say "No connection"; switch it back on | Back to "Wi-Fi: <network>" within ~1 min of the router being up, without a reboot | yes |
| 2.9 | Status page | On a PC or phone on your Wi-Fi, open `http://spotify-cd.local/` (or the IP) | Status page loads with name, Wi-Fi, address, firmware | yes |
| 2.10 | Admin password | `/manage` → set an admin password → Save. Open `/manage` in a private window | The browser asks for a login; `admin` + the password works; a wrong one doesn't | yes (first try used a password under 6 characters; the error is now shown in red) |
| 2.11 | Rename and web address | `/manage` → Name "Kitchen", Web address "Kitchen.local" → Save | Saved as `kitchen`; `http://kitchen.local/` works within ~1 min and `spotify-cd.local` stops; the boot screen shows "Kitchen" after RESET | yes |
| 2.12 | Wi-Fi screen | On the device: ≡ → Wi-Fi | Network, signal, address and web name shown; signal updates every 5 s | yes |
| 2.13 | Set up network (later) | Wi-Fi screen → Set up network | QR codes appear while the device stays connected (the status page still loads); Stop setup removes the AP | yes |
| 2.14 | Add a second network | `/manage` → pick or type a second network (e.g. phone hotspot) → Connect | "Connected to …"; Saved networks lists both; RESET reconnects to the stronger one | yes |
| 2.15 | Clock | Console `wifi` (dev build) | `time=1` within ~10 s of connecting | yes |
| 2.16 | OTA update | Build `dev` (`& $pio run -e dev`), then `/manage` → Firmware → choose `.pio\build\dev\firmware.bin` → Install | Progress bar, "Installed", device restarts on the same version. After ~30 s online the log shows `New image marked valid` | yes |
| 2.17 | OTA rejects junk | Install any non-firmware file (e.g. a .txt renamed .bin) | "Update failed: …" (magic byte); the device keeps running | yes |
| 2.18 | OTA rollback | Build `& $pio run -e ota_crash_test`, install `.pio\build\ota_crash_test\firmware.bin` via `/manage` | It restarts, crashes ~10 s later, restarts again on the previous firmware. The status page shows "The last firmware update didn't start properly…" | yes |
| 2.19 | Offline 10 min → setup | Forget all networks except one, switch that router off, RESET, wait 10 min | Connecting screen (with Set up network) for 10 min, then the setup QR screen. Switching the router back on returns to Idle by itself | yes |
| 2.20 | Stays up | 30 min on Wi-Fi with the console open | No reboots; the 60 s heap lines stay flat | yes |
| 2.21 | Unit tests | `& $pio test -e test_device` (monitor closed) | All tests pass | yes |

**Phase 2: all checks passed (2026-09-30).**

Open issue: on the first 2.18 run, some time after the rollback the screen went black and stayed black
across resets. It was reflashed over USB before it could be diagnosed, and it hasn't happened again.
Boot diagnostics were added for next time: the reset reason, both firmware slots' states and a
last-crash summary are logged at boot (`log`), and the status page shows "Last restart".
