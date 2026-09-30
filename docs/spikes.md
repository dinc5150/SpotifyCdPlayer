# Phase 0 spikes — runbook and results

Each spike is its own PlatformIO environment. Build, flash and open the serial
console from **PowerShell or the VS Code PlatformIO sidebar**, not Git Bash:
ESP-IDF's installer refuses to run under MSYS/Git Bash.

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run -e s2_nfc -t upload
& $pio device monitor -e s2_nfc      # type commands, Enter to send; 'help' lists them
```

If upload can't find the board, hold **BOOT**, tap **RESET**, release **BOOT**, then upload again.

Record results in the tables below. Gate G1 ("Hardware proven", PLAN.md §13) passes when every row
marked **must** passes.

**Gate G1: passed (2026-09-29).** Every must row passes. Deferred, not blocking:
- S1 current table: measure with a USB power meter before Phase 8 (its acceptance needs the figures).
- S5: relay page browser tests, done in Phase 3 with the real portal.

**What changed in PLAN.md:** NFC field off between searches with 1 retry (§7.2); LCD at 80 MHz (§3.4);
show and Liked Songs cards play directly as contexts, so their fallbacks are gone (§6.7); Spotify-owned
playlists play, only their name may be missing (§7.3); latency budget uses the measured figures (§5.4).

---

## Wiring (for S2)

Set the PN532 V3 switches to **SPI: I0 = L, I1 = H**.

| PN532 | ESP32-S3 | Header P1 pin |
|---|---|---|
| VCC | 3V3 | 8 |
| GND | GND | 6 |
| SCK | IO4 | 17 |
| MISO | IO5 | 19 |
| MOSI | IO6 | 21 |
| SS | IO7 | 22 |
| IRQ | IO8 | 20 (not used yet) |

---

## S1 — display, touch, LVGL, power (`s1_display`)

At boot the screen shows a raw test pattern for 4 s, then the LVGL test screen: four 72×54 corner
buttons, a QR code, and live touch coordinates.

| # | Check | How | Must? | Result |
|---|---|---|---|---|
| 1.1 | Landscape offsets correct | Boot pattern (or `pattern`): red border visible on all 4 edges; green TL, blue TR, yellow BL, white BR | must | yes|
| 1.2 | Touch controller answers | Boot log shows `Touch ok, chip id ...` | must | === Spike S1: display, touch, LVGL, power ===
Display 320x172, rotation 1
Touch ok, chip id 51 06 01
heap internal free 306264 (min 300996), psram free 8384396 (min 8384396)
LVGL running. Type 'help' for commands.|
| 1.3 | Touch mapping correct | Tap each corner button: serial logs `Button TL/TR/BL/BR clicked` for the one under your finger. If wrong, `touchlog on` and try `txf <swap> <mx> <my>` until it is; record the values | must | yes |
| 1.4 | Held press doesn't flicker | Press and hold a button: it stays highlighted | must |yes |
| 1.5 | QR code scans | Phone camera on the centre QR offers to join `SpotifyCD-TEST` | must | yes |
| 1.6 | Rendering speed | `bench` at 40 MHz, then `lcdhz 80` (saves and reboots; boot log shows the clock) and `bench` again; record ms. Any glitches at 80? `lcdhz 40` restores the default | | 40 MHz: fill 23.6 ms, LVGL full redraw 49.3 ms. 80 MHz: fill 12.6 ms, LVGL full redraw 38.5 ms, stable, no glitches (the earlier glitch was the failed runtime re-clock) |
| 1.7 | Backlight dimming | `bl 100`, `bl 20`, `bl 0` | must | worked |
| 1.8 | Panel sleep and wake | `sleep`: dark 3 s, wakes cleanly with no garbage; record the wake time | must | worked |
| 1.9 | Wake sources | `profile off`, then touch: wakes, and that touch does **not** click a button. Repeat with BOOT | must | worked|
| 1.10 | CPU switching | `cpu 80`, `cpu 240` with Wi-Fi joined (`wifi <ssid> <pass>`): no crash | must | yes |
| 1.11 | Upside-down option | `rot 3`: image flipped 180° and touch still correct | | yes |
| 1.12 | Rail stability | With Wi-Fi joined, `bl 100`, and the PN532 wired with its field on (flash S2 and run `field on`, then reflash S1), no brownout resets over 10 min | must | no brownout resets over 10 min |

**Current per power profile** (USB power meter; Wi-Fi joined; average over ~30 s):

| Profile | Command | Current (mA) |
|---|---|---|
| Active | `profile active` | |
| Dim | `profile dim` | |
| Screen off | `profile off` | |
| Screen off with Wi-Fi `ps min` instead of max | `profile off` then `ps min` | |

Touch transform found (1.3): swap=1 mirrorX=0 mirrorY=0 (the default) · Best stable LCD SPI clock (1.6): 80 MHz

---

## S2 — NFC (`s2_nfc`)

Polling starts at boot (200 ms interval, RF field off between searches). Tap a card; the log shows
the UID, search time, NDEF read time and the records.

| # | Check | How | Must? | Result |
|---|---|---|---|---|
| 2.1 | Reader found | Boot log `Found PN532, firmware 1.6` (or similar) | must | yes |
| 2.2 | Detect a card with field off between searches | Tap a card: `Card detected` within ~0.3 s; `Card removed` when lifted | must | yes |
| 2.3 | Field-off doesn't hurt detection | Leave a card on the reader for 60 s, then `stats`: hits ≈ searches. Repeat with `fieldmode explicit` and `fieldmode keep` | must | Interleaved `bench`, card still: 50/50 for every setting. Field off (auto re-enabled by the search, no settle needed): 21.1 ms; keep: 27.0 ms. Earlier misses were the card moving. Chosen: field off, retries 1 |
|
| 2.4 | Screen-off interval | `interval 750`, tap 10 times: every tap detected; note worst delay | must | every tap detected. Worse delay was around 1.5s|
| 2.5 | Write to a factory-blank card | `write https://open.spotify.com/playlist/37i9dQZF1DXcBWIGoYBM5M?sh=1` (53-byte payload): formats, writes, verifies; record ms | must | yes |
| 2.6 | Re-write an NDEF card | `write https://open.spotify.com/album/4LH4d3cOWNNsVw41Gqt2kv` on the same card | must | yes |
| 2.7 | NFC Tools card | Format and write a card with the NFC Tools phone app, then `read` it here; `write` over it | | yes |
| 2.8 | Phone reads our card | Android phone with NFC: tap the card from 2.6; Spotify opens (informational; iPhones can't read MIFARE Classic) | | yes |
| 2.9 | Clean | `clean` returns a card to factory state; `write` then works again | | works |
| 2.10 | SPI clock | `spihz 1000000` and `spihz 4000000`: detection still works (default 2 MHz) | | yes |

**Timings:** search with no card ___ ms · search with card ___ ms · NDEF read ___ ms · write + verify ___ ms

**Current:** `field on` ___ mA · `field off` ___ mA · polling 200 ms ___ mA · polling 750 ms ___ mA

If 2.1 fails: check the switches, then try UART (HSU) wiring (PLAN.md §3.3) and tell me; the spike would need an HSU variant.

---

## S3 — TLS (`s3_net`)

Run `wifi <ssid> <password>` once (saved), then:

| # | Check | How | Must? | Result |
|---|---|---|---|---|
| 3.1 | Certificate bundle works | `tls`: connects ok, and the time is printed | must | TLS connect ok in 288 ms at 240 MHz |
| 3.2 | Handshake time at 240 MHz | `cpu 240`, `tls` ×3: record ms | must | TLS connect ok in 288 ms at 240 MHz |
| 3.3 | Handshake time at 80 MHz | `cpu 80`, `tls` ×3 (shows why we switch to 240 before connecting) | |TLS connect ok in 687 ms at 80 MHz |
| 3.4 | Keep-alive reuse | `ka 20`: first is `new`, the rest `reused`; record typical ms | must | Yes roughly 200 - 500ms|
| 3.5 | Idle timeout | `idle 30`, `idle 60`, `idle 120`: record when Spotify closes the idle connection | must | Still open after 120 s (`OPEN`; follow-up reused in 217 ms). Follow-ups at 30/60 s: 307/207 ms |
| 3.6 | Memory per session | `heap` before and after `tls` | | [before] internal free 262416 (largest block 176116), psram free 8374972, TLS connect ok in 692 ms at 80 MHz, [connected] internal free 220708 (largest block 176116), psram free 8374408|
| 3.7 | Power save impact | `ps max`, then `ka 5`: latency vs `ps min` | | ps min is roughly 200ms quicker than ps max|

Handshake: 288 ms at 240 MHz, 687 ms at 80 MHz · reused request 200–500 ms · idle connection still open after 120 s (confirmed: `OPEN`, next request reused in 217 ms) · TLS RAM ~42 KB

---

## S4 — Spotify behaviour (`s3_net`)

**Get a token (once):**
1. In your Spotify developer app, add the redirect URI `http://127.0.0.1:8888/callback`.
2. On the PC: `& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\spotify_token.py <client_id>`
3. Paste the printed `token <client_id> <refresh_token>` line into the spike console.

Open Spotify on a speaker so it appears in `devices`. For each row, run `play <uri> <device_id>`, then
`state` after ~2 s.

| # | Context | URI to use | Play result (HTTP, plays?) | `get` metadata result |
|---|---|---|---|---|
| 4.1 | Album | `spotify:album:4LH4d3cOWNNsVw41Gqt2kv` | plays | `get /v1/albums/4LH4d3cOWNNsVw41Gqt2kv` |
| 4.2 | Your own playlist | (one of yours) | plays | `get /v1/playlists/<id>?fields=name,owner(id)` |
| 4.3 | Followed user-made playlist | (someone else's) | plays | same |
| 4.4 | Spotify editorial playlist | `spotify:playlist:37i9dQZF1DXcBWIGoYBM5M` | yes | same (expect 404?) |
| 4.5 | Artist | `spotify:artist:0du5cEVh5yTK9QJze8zA0C` | yes | `get /v1/artists/0du5cEVh5yTK9QJze8zA0C` |
| 4.6 | Show (podcast) | `spotify:show:<id>` | yes | `get /v1/shows/<id>/episodes?limit=1` |
| 4.7 | Liked Songs | `spotify:user:<your user id>:collection` (id from `me`) | yes | `get /v1/me/tracks?limit=1` (does it still exist?) |

| # | Check | How | Result |
|---|---|---|---|
| 4.8 | Transfer | `transfer <id>` to each available device type | works |
| 4.9 | Volume on unsupported speaker | `vol 30` on a device listed with "(no remote volume)" | `HTTP 403`, reason `VOLUME_CONTROL_DISALLOW` ("Cannot control device volume"). The earlier error was a 411 from a missing `Content-Length: 0`, now fixed |
| 4.10 | Shuffle, repeat, next, prev, pause, resume | each once | Works |
| 4.11 | No active device | Close all Spotify apps, `play spotify:album:...` without a device id: record the error body | HTTP 404 in 505 ms (new connection)
{
  "error" : {
    "status" : 404,
    "message" : "Player command failed: No active device found",
    "reason" : "NO_ACTIVE_DEVICE"
  }
} |

---

## S5 — relay page

1. Push this repo to GitHub, then Settings → Pages → Source: **GitHub Actions**. The workflow publishes `relay/`.
2. Add `https://<user>.github.io/<repo>/` as a redirect URI in the Spotify app.
3. Until the device portal exists (Phase 3), test forwarding by opening this in each browser on your phone and PC,
   with a real LAN IP of any machine on your network:
   `https://<user>.github.io/<repo>/?code=test&state=abc~192.168.1.50~80`

| Browser | Forwards to `http://192.168.1.50/spotify/callback?...`? | Fallback UI shown if nothing answers? |
|---|---|---|
| iOS Safari | | |
| Android Chrome | | |
| Desktop Chrome | | |
| Desktop Edge | | |
| Desktop Firefox | | |

Also confirm the page refuses `state=abc~8.8.8.8~80` ("This link can't be used").
