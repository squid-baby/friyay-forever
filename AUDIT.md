# Friyay Forever — Codebase Audit

> Audited: 2026-04-19 | Branch: `claude/codebase-audit-6Ypx9` | Firmware: v1.0.15

---

## Summary

The codebase is a working, well-intentioned embedded project that has grown organically. The core logic is solid and the README/CLAUDE.md are a cut above typical hobby ESP32 projects. The main issues fall into four buckets: **secrets in source**, **monolithic file structure**, **zero test coverage**, and **documentation drift**. None of these are blockers for a friend-group device, but fixing them would make the project significantly more maintainable and easier to iterate on — especially with AI assistance.

---

## 1. AI-First & Self-Documenting Improvements

### 1.1 Secrets are hardcoded in source (critical)

`src/main.cpp:74` commits a live Telegram bot token and friend Telegram user IDs to git:

```cpp
#define BOT_TOKEN "8274851974:AAEao868jidxcQEnY8IxPK91ujLmOsA_Alg"
```

And all five friends' real Telegram IDs are in the `friends[]` array at line 89. This repository appears to be public. The token should be rotated immediately and moved to a `secrets.h` (already in `.gitignore` — the scaffolding is right, just not used).

**Recommended pattern:**

```cpp
// src/secrets.h  (gitignored, never committed)
#define BOT_TOKEN "..."
#define FRIEND_TG_NM 7612996805LL
// ...

// src/secrets.example.h  (committed, documents the shape)
#define BOT_TOKEN "YOUR_BOT_TOKEN_HERE"
```

### 1.2 `firmware.bin` (1.1 MB) is committed to git

This serves no purpose in the repo — OTA pulls from GitHub Releases, not from the repo tree. It inflates clone size and will bloat git history permanently.

**Fix:** Add `firmware.bin` to `.gitignore` and remove with `git rm --cached firmware.bin`.

### 1.3 `version.json` is hand-maintained and stale

`version.json` says `1.0.6`; `platformio.ini` says `1.0.15`. These are out of sync because the file is edited by hand. This will break any tooling that reads it.

**Fix:** Generate `version.json` from a PlatformIO `extra_scripts` post-build step that reads `FIRMWARE_VERSION` from the INI. Zero maintenance cost after the one-time setup.

### 1.4 Version strings live in at least four places

| Location | What it says |
|---|---|
| `platformio.ini` line 17 | `1.0.15` |
| `version.json` | `1.0.6` |
| `README.md` footer | "Protocol 1.0 v27" |
| `main.cpp` header comment | "v28 - KEYBOARD M FIX" |

Canonical truth should be `platformio.ini` only; everything else should derive from it.

### 1.5 Touch calibration values are unnamed magic numbers

`src/main.cpp:558`:
```cpp
savedTouchX = map(rawX, 792, 325, 0, 800);
savedTouchY = map(rawY, 471, 209, 0, 480);
```

These are hardware calibration points measured from a specific unit. An AI assistant (or future contributor) has no idea where they came from or how to re-derive them.

**Fix:** Name them and add a one-line comment explaining how to recalibrate:
```cpp
#define TOUCH_RAW_X_HIGH 792  // measured top-left corner tap
#define TOUCH_RAW_X_LOW  325  // measured bottom-right corner tap
// ... etc.
```

### 1.6 Public MQTT broker with no authentication

Commit state is synced via `broker.emqx.io` (a shared public test broker) with no credentials, TLS, or topic namespacing beyond `friyay-forever-2026/`. Anyone who knows or guesses the topic can spoof commits for any friend. This is low-risk for a private friend group but worth noting.

**Improvement:** Use a unique UUID-suffixed topic (already partially done with the year), or set up a free HiveMQ/Mosquitto cloud instance with basic auth.

---

## 2. Refactoring & Re-architecture

### 2.1 `main.cpp` is a 2,492-line god file

Everything — hardware init, UI drawing, WiFi provisioning, Telegram bot, MQTT, weather API, Spotify art, OTA, sensors, LED animations, touch handling — lives in one file. This makes it very hard for an AI assistant to reason about one subsystem without loading the entire context, and makes incremental changes risky.

**Recommended module split:**

```
src/
  main.cpp          (~150 lines — setup/loop/wiring only)
  config.h          (all #defines, MAC table, friend list)
  secrets.h         (gitignored)
  state.h           (global state struct — see 2.2)
  ui/
    layout.h        (all layout #defines)
    buttons.cpp/h
    weather_panel.cpp/h
    timer_panel.cpp/h
    vu_meters.cpp/h
    spotify_area.cpp/h
    wifi_setup.cpp/h
  net/
    telegram.cpp/h
    mqtt_sync.cpp/h
    weather_api.cpp/h
    spotify_api.cpp/h
  hw/
    touch.cpp/h
    leds.cpp/h
    sensors.cpp/h
  ota_updates.h     (already separate — good)
```

This is a substantial refactor but it's the right shape for an embedded project at this size. Even splitting out just `ui/wifi_setup` and `net/telegram` would help significantly.

### 2.2 ~50 globals should be a state struct

The global state section runs from line 265 to line 387 — 122 lines of unstructured globals. This is hard to reason about because mutation can come from anywhere.

**Recommended approach:** Group related state:

```cpp
struct WeatherState { float temp; float precip; int wet, tmp, fuk; bool loaded; };
struct SpotifyState { bool active; String trackId, artUrl, codeUrl, senderInitials; };
struct CommitState  { bool pending; unsigned long pendingTime; unsigned long lastTime; };
// etc.
```

This makes function signatures honest (`void drawWeatherBars(const WeatherState&)`) and makes AI-assisted changes much safer.

### 2.3 `calcWeather()` and `calcWeatherForDay()` are ~90% duplicated

`src/main.cpp:2303` and `2330`. Both take `(temp, rain)` → `(wetLvl, tmpLvl, fukLvl)` but one reads from globals and the other takes a day index. Extract to one function:

```cpp
void calcWeatherLevels(float temp, float rainMm, int& wet, int& tmp, int& fuk);
```

### 2.4 Touch hit-testing is hardcoded coordinate arithmetic

`handleTouch()` and `handleSetupTouch()` contain inline `if (touchX >= X && touchX <= X+W && touchY >= ...)` chains. Adding or moving a UI element requires hunting down every reference.

**Better pattern:** A static hit-test table:
```cpp
struct TouchZone { int x, y, w, h; void (*handler)(); };
static const TouchZone MAIN_ZONES[] = {
  {MARGIN, BTN_Y, BTN_W * NUM_FRIENDS + ..., BTN_H, handleFriendButtons},
  // ...
};
```

This would be easy to unit-test and easy for an AI to modify without introducing regressions.

### 2.5 Telegram command dispatch is a 150-line if-else chain

`checkTelegram()` handles all commands inline (`/commit`, `/uncommit`, `/status`, `/weather`, `/version`, `/update`, `/install`). A dispatch table or map is cleaner:

```cpp
struct BotCommand { const char* cmd; void (*handler)(String chatId, int friendIdx); };
```

### 2.6 `drawTimer()` has five different rendering modes in one function

Priority 1 (commit anim) → Priority 2 (message) → Priority 3 (shutdown) → Priority 4 (countdown), plus border color logic and scroll mutation. This is extremely hard to follow. Each mode should be its own helper.

### 2.7 Day-of-week mapping is duplicated

The `dayMap[] = {6, 0, 1, 2, 3, 4, 5}` appears identically in both `drawDays()` (line 1188) and `handleTouch()` (line 998). Extract to a named constant.

### 2.8 `qr_code.h` is a 5,294-line generated binary blob

This header contains a raw JPEG as a C array. It's the largest file in the project and completely opaque — the source image isn't tracked, so if it ever needs to change there's no way to regenerate it.

**Fix:**
- Commit the source image (e.g., `assets/telegram_qr.jpg`)  
- Add a one-line `xxd -i` or `python` script to regenerate the header
- Or use SPIFFS/LittleFS to store assets on the device and flash them separately

---

## 3. Testing Gaps

There are **zero tests** of any kind. No PlatformIO native test target, no CI, no `.github/` directory.

Critically, much of the business logic is **host-testable** — it doesn't touch any hardware:

| Function | What to test |
|---|---|
| `isNewerVersion()` / `parseVersion()` | Semver comparison edge cases |
| `calcWeather()` / `calcWeatherForDay()` | Scoring formula, boundary temps (32°F, 100°F), zero rain |
| `sanitizeMessage()` | Unicode stripping, emoji handling |
| `getFriendIdx()` | Known IDs return correct index; unknown returns -1 |
| `calcCountdown()` | Friday at 15:00 exactly; Saturday after Friday; DST boundary |
| `mqttCallback()` parsing | Valid `FRIYAY:2:1`, invalid prefix, out-of-range index, own echo |
| MAC table lookup | Known MACs, unknown MAC falls back to default |
| Keyboard touch → character | Row/column math for all five rows |

**Recommended setup:**

```ini
; platformio.ini addition
[env:native_test]
platform = native
test_framework = unity
```

```
test/
  test_semver.cpp
  test_weather.cpp
  test_message.cpp
  test_mac_lookup.cpp
  test_countdown.cpp
```

PlatformIO's Unity test framework works out of the box and runs on the host — no hardware needed.

**CI:** Add a `.github/workflows/build.yml` that:
1. Runs `pio run` (build check for ESP32 target)
2. Runs `pio test -e native_test` (host unit tests)

This catches build regressions before they reach the OTA channel.

---

## 4. Documentation Coverage

### 4.1 README drift

| Claim in README | Reality |
|---|---|
| "Protocol 1.0 v27 - ALBUM ART FIX" (footer) | Firmware is v1.0.15 |
| MAC table shows only NM registered | All 5 MACs are now in firmware |
| Project structure lists "known last functional Set of code/" dir | That directory doesn't exist in the repo |
| Upload port example: `/dev/cu.usbserial-2130` | CLAUDE.md says `/dev/cu.usbserial-210` |
| No mention of MQTT commit sync | This is the core commit mechanism since v1.0.11 |
| No mention of sleep mode / LED dim behavior | LEDs dim after 21:30 |

### 4.2 CLAUDE.md is good but thin

`CLAUDE.md` is the right idea and the OTA release process section is genuinely useful. Gaps:
- `~2400 lines` — it's 2492 and growing
- No data flow diagram (touch → MQTT → display update is non-obvious)
- No explanation of why `MY_FRIEND_INDEX` starts as `1` (fallback = ST)
- No notes on the touch calibration values or how to re-derive them
- MQTT topic schema not documented

### 4.3 No `CHANGELOG.md`

Version history lives in three places that all drift apart: the README table, the file-top comment block in `main.cpp`, and git tags. A single `CHANGELOG.md` following Keep a Changelog format, with `platformio.ini` as the single version source, would fix this.

### 4.4 Key invariants are undocumented

Some non-obvious things that should have a comment:
- Why `ALBUM_ART_W` cascades to `ART_X`, `VU_X`, `TIMER_W`, `PANEL_TOP` (mentioned only in README)
- The `map(rawX, 792, 325, 0, 800)` calibration origin (no comment at all)
- Why `client.setInsecure()` is required (Telegram cert chain issue on ESP32)
- The `MY_FRIEND_INDEX = 1` default and why it's ST

---

## Quick-Win Priority List

| Priority | Item | Effort |
|---|---|---|
| 🔴 Now | Rotate the Telegram bot token | 5 min |
| 🔴 Now | Move token + IDs to `secrets.h`, add `secrets.example.h` | 30 min |
| 🔴 Now | Add `firmware.bin` to `.gitignore`, remove from git | 5 min |
| 🟠 Soon | Fix `version.json` to match `1.0.15`, or auto-generate it | 1 hr |
| 🟠 Soon | Update README (MQTT, correct MAC table, correct version) | 1 hr |
| 🟠 Soon | Add PlatformIO native test target + 3–4 unit tests | 2 hr |
| 🟠 Soon | Add `.github/workflows/build.yml` CI | 1 hr |
| 🟡 Next | Extract `calcWeatherLevels()` to remove duplication | 30 min |
| 🟡 Next | Move layout `#defines` to `src/layout.h` | 30 min |
| 🟡 Next | Add `secrets.example.h` and document required secrets | 30 min |
| 🟢 Later | Split `main.cpp` into modules | 1–2 days |
| 🟢 Later | Replace globals with state structs | Half day |
| 🟢 Later | Touch zone dispatch table | Half day |
| 🟢 Later | Commit `assets/telegram_qr.jpg` + regen script | 1 hr |
