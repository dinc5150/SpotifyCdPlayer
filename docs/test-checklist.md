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
| 1.11 | Factory reset dialog | Hold BOOT 10 s (or `factory`); tap **Cancel**; repeat and tap **Erase** | Cancel closes it and nothing changes. Erase restarts with defaults (`settings`: bright=80, name "Card Player", a new `ap_pw`) | yes |
| 1.12 | Toast and dialog | `toast Hello there`, `confirm` | Toast shows for ~2.5 s; the dialog's buttons log `Dialog 1 accepted/cancelled` | yes |
| 1.13 | State machine drive | `ev wifi_lost`, `ev wifi_restored`, `ev login_expired`, `ev spotify_linked`, `ev open_overlay about`, `ev home` | wifi_lost: Idle status reads "No connection"; wifi_restored clears it; login_expired shows the Link Spotify placeholder (no back button); spotify_linked returns to Idle; About opens and home closes it. Each command logs the new state | yes |
| 1.14 | Stays up | Leave it running 10 min with the console open | No reboots or watchdog resets; the 60 s heap lines stay flat | yes |
| 1.15 | Unit tests | Close the monitor (the runner needs the port), then in PowerShell: `$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"; & $pio test -e test_device` | All tests pass (`pio test -e native` does the same on the PC once a host gcc/g++ is installed) | all passed |

Boot log line to copy here (1.1): ______

**Phase 1: all checks passed (2026-09-29).**
