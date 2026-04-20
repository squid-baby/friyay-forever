# Changelog

All notable changes to Friyay Forever firmware. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/).

Source of truth for the **current** version is `FIRMWARE_VERSION` in
`platformio.ini`. OTA resolves the latest version from GitHub Releases —
this file is the human-readable history.

## [Unreleased]

### Added
- `PLAN.md` — phased execution plan, living checklist.
- `src/layout.h`, `src/config.h`, `src/state.h` — lightweight module split.
  `state.h` is scaffolding only; migration from globals is a follow-up.
- `src/secrets.h` + `src/secrets.example.h` — landing spot for **new** secrets.
  Existing `BOT_TOKEN` and friend Telegram IDs stay inline per PLAN §1.2.
- `scripts/gen_version_json.py` — auto-generates `version.json` from
  `FIRMWARE_VERSION` at build time; wired via `platformio.ini` `extra_scripts`.
- Pure, testable helpers: `lookupMacOwner`, `parseMqttCommit`,
  `keyboardCharAt`, `calcWeatherLevels`.

### Changed
- Touch-calibration magic numbers named (`TOUCH_RAW_X_HIGH` / `_LOW` etc.).
- `dayMap[]` hoisted to file-scope `DISPLAY_DAY_MAP` constant.
- `calcWeather` / `calcWeatherForDay` deduplicated via `calcWeatherLevels`.
- Existing `enum TouchState` renamed to `TouchPhase` so `state.h` can claim
  `TouchState` for the scaffolding struct.

### Removed
- `firmware.bin` (1.1 MB) and hand-maintained `version.json` — both untracked
  and gitignored. Shipped as GitHub Release assets instead.
- Stale `"Protocol 1.0 (v28 - KEYBOARD M FIX)"` comment block in `main.cpp`
  and matching README footer.

## [1.0.15] — 2026-04-19
### Fixed
- Retained MQTT commit state now persists across reconnects.

## [1.0.14]
### Fixed
- MQTT publish on dropped connection.

## [1.0.13]
### Added
- Show commit message on receiving units.
### Fixed
- Splash version string.

## [1.0.12]
### Changed
- Switch MQTT broker to `broker.emqx.io` (earlier `test.mosquitto.org` attempt
  preserved in history).
- Tune MQTT keepalive.

## [1.0.11]
### Added
- MQTT commit sync — replaces the broken Telegram-self-read approach.

## [1.0.10]
### Fixed
- Commit sync: drop chatId check, match on `FRIYAY:` prefix.

## [1.0.9]
### Added
- Console commit sync.
### Changed
- WiFi and sleep-mode tweaks.

## [1.0.8]
### Added
- Sleep mode: dim purple LEDs 22:30–07:30.

## [1.0.7]
### Fixed
- WiFi setup keyboard: missing `M` key now visible and tappable.

## [1.0.6]
### Changed
- UI polish.
### Fixed
- Notification clipping / scanner glow on left edge, scrolling message bleed
  into AQI meter area.

## [1.0.5]
### Fixed
- DST handling.
### Added
- Simon's MAC discovered and registered.

## [1.0.4]
### Fixed
- Album art rendering — PSRAM decode + software scale to 260×260, flush with
  scan code and VU meters.
### Added
- MAC-based unit identity — one binary, all units, OTA no longer causes
  identity swaps. Never hardcode `MY_FRIEND_INDEX`.

## [1.0.3]
### Reverted
- Rolled back to v1.0.1 behavior after the album-art shrink regressed UX.

## [1.0.2]
### Changed
- Shrunk album-art render to avoid CO2 slider overlap. Reverted in 1.0.3.

## [1.0.1]
### Fixed
- Commit button spam — 3 s debounce (later extended to 15 s in v27-era work).

## [1.0.0] — Initial OTA-enabled release
### Added
- Baseline Friyay Forever firmware — touch, display, weather, Telegram,
  Spotify, AQI, LEDs, countdown, OTA via GitHub Releases.
