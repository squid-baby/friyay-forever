# Friyay Forever — Execution Plan

> Source: [codebase audit](https://github.com/squid-baby/friyay-forever/pull/2) + [PR #2 review](https://github.com/squid-baby/friyay-forever/pull/2#pullrequestreview-4136749433) | Branch: `claude/codebase-audit-6Ypx9` | Firmware: v1.0.15
>
> **This is an AI-first repository.** All work is agent-driven. The repository must be **self-documenting** and **maintain full test coverage** on pure logic. Every phase lands as its own PR against `main` and updates this document to mark work complete.

---

## Phase ordering

Hygiene and test infrastructure come first. Runtime fixes come second because (a) they require restructured, testable code to verify the fix sticks, and (b) without tests any runtime refactor risks silent regressions on already-flaky behavior. AI documentation lands last so it can describe the system as it actually is, not as we're mid-refactoring it.

| Phase | Theme | Blocks | Status |
|---|---|---|---|
| 1 | Organization & hygiene | Phases 2, 3 | partial (in PR #3 / PR #4) |
| 2 | Test coverage | Phase 3 (tests prove runtime fix) | ✅ merged (PR #4) |
| 3 | Runtime fluidity (kill the freeze) | — | 3.2.1 + 3.2.3 landed; 3.2.2 deferred |
| 4 | AI integration & self-documentation | — | pending |

---

## Phase 1 — Organization & Hygiene

Goal: make the codebase legible enough that a fresh agent session can reason about one subsystem without loading everything, and set up the boundaries the test phase depends on.

### 1.1 Repo hygiene
- [ ] Remove `firmware.bin` from git tree; add to `.gitignore`
- [ ] Make `platformio.ini` `FIRMWARE_VERSION` the single source of truth
- [ ] Auto-generate `version.json` from a PlatformIO `extra_scripts` post-build hook
- [ ] Seed `CHANGELOG.md` from git tags; make version-bump workflow update it

### 1.2 Secrets pattern (new secrets only)
Per owner decision on PR #2, the existing Telegram bot token **stays in source** — risk is scoped to the bot itself. The secrets pattern is for everything going forward.
- [ ] Create `src/secrets.example.h` (committed, documents the shape)
- [ ] Create `src/secrets.h` (gitignored — already gitignored, just needs to be the landing spot)
- [ ] Migration: new secrets land in `secrets.h`. Existing `BOT_TOKEN`, friend Telegram IDs, MQTT broker URL stay inline for now; note in `CLAUDE.md` that this is deliberate.

### 1.3 Name the magic numbers
- [ ] Touch calibration: `TOUCH_RAW_X_HIGH`, `TOUCH_RAW_X_LOW`, `TOUCH_RAW_Y_HIGH`, `TOUCH_RAW_Y_LOW` with a one-line comment describing how to re-derive them
- [ ] Hoist the `dayMap[] = {6, 0, 1, 2, 3, 4, 5}` duplicate (`main.cpp:998` and `main.cpp:1188`) to one named constant

### 1.4 Extract pure logic (enables Phase 2)
- [ ] `calcWeatherLevels(float temp, float rainMm, int& wet, int& tmp, int& fuk)` — consolidates the ~90% duplication between `calcWeather()` and `calcWeatherForDay()`
- [ ] `lookupMacOwner(uint8_t mac[6]) -> int` — pure function, currently inlined in `setup()`
- [ ] `parseMqttCommit(const String& text, int& idx, bool& committed) -> bool` — pure function, currently inlined in `mqttCallback()`
- [ ] `keyboardCharAt(int x, int y, bool capsOn) -> char` — pure function, currently inlined in `handleKBTouch()`

### 1.5 File splits (lightweight — not the full module split)
- [ ] `src/config.h` — MAC table, `friends[]`, Telegram/MQTT/weather config, pin definitions
- [ ] `src/layout.h` — all UI layout `#defines` (screen dimensions, panel positions, color constants)
- [ ] `src/state.h` — grouped state structs: `WeatherState`, `SpotifyState`, `CommitState`, `TouchState`, `LedState`

Keep `main.cpp` as the wiring file. Full `src/ui/` `src/net/` `src/hw/` split is **deferred** until `main.cpp` exceeds 3k lines — diminishing returns before then.

**Exit criteria:** `main.cpp` drops below ~1,800 lines; all pure functions extracted; repo clones without a 1.1 MB binary; `version.json` and `platformio.ini` can never drift.

---

## Phase 2 — Test Coverage

Goal: **full coverage on pure logic.** Every extracted pure function from Phase 1 gets a test. CI blocks merge on test failure.

### 2.1 Test infrastructure
- [ ] Add `[env:native_test]` to `platformio.ini` using Unity (host-native, no hardware required)
- [ ] `test/` directory structure:
  ```
  test/
    test_semver/       test_main.cpp  (isNewerVersion, parseVersion)
    test_weather/      test_main.cpp  (calcWeatherLevels)
    test_message/      test_main.cpp  (sanitizeMessage)
    test_friends/      test_main.cpp  (getFriendIdx, lookupMacOwner)
    test_mqtt/         test_main.cpp  (parseMqttCommit)
    test_countdown/    test_main.cpp  (calcCountdown — injectable clock)
    test_keyboard/     test_main.cpp  (keyboardCharAt)
  ```
- [ ] CI: `.github/workflows/build.yml`
  - Job 1: `pio run` against the `esp32s3` env (build regression check)
  - Job 2: `pio test -e native_test` (host tests)

### 2.2 Coverage targets

| Module | Cases |
|---|---|
| `isNewerVersion` / `parseVersion` | `1.0.0 < 1.0.1 < 1.1.0 < 2.0.0`; `v` prefix handling; `unknown` current; malformed input |
| `calcWeatherLevels` | 32°F / 100°F boundaries; zero rain; heavy rain penalty; optimal 65°F; negative temps |
| `sanitizeMessage` | ASCII passthrough; emoji stripping; newline preservation; null/empty |
| `getFriendIdx` / `lookupMacOwner` | Known IDs/MACs; unknown fallback; default-ST behavior |
| `parseMqttCommit` | Valid `FRIYAY:2:1`; invalid prefix; out-of-range index; own-echo skip; missing colons |
| `calcCountdown` | Friday 14:59:59 → 1s; Friday 15:00:00 → 0s; Saturday → ~6d; DST transition weeks |
| `keyboardCharAt` | Every key in every row; caps on/off; out-of-bounds returns 0 |

### 2.3 Test discipline (this becomes a rule in CLAUDE.md in Phase 4)
- Pure logic added to the codebase **must** land with tests in the same PR
- Hardware-coupled code is exempt but should be minimized — push logic behind pure interfaces that are testable

**Exit criteria:** 100% of Phase 1's extracted pure functions have tests; CI green on `main`; branch protection requires both jobs to pass.

---

## Phase 3 — Runtime Fluidity (kill the freeze)

Goal: eliminate the ~20-second UI freezes caused by blocking network I/O on the main loop.

### 3.1 Root cause
Every HTTP call runs on the single main-loop task with long default timeouts:
- `bot.getUpdates()` at `src/main.cpp:1879` — no explicit timeout, inherits HTTPClient default (60s)
- `getWeather()` / `fetchSpotifyArt()` / `downloadImageFromUrl()` — 5–10s timeouts, still blocking
- `tryConnect()` WiFi reconnect — 10s blocking
- `mqttClient.loop()` is non-blocking but starves when the above block

Symptom: touch unresponsive, LED animation stutters, countdown visibly jumps.

### 3.2 Fix — in order, least-invasive first

**3.2.1 Explicit short timeouts (quickest win, no arch change)**
- [x] Set `http.setConnectTimeout(2000)` + `http.setTimeout(3000)` on every HTTPClient instance
- [x] ~~Swap `UniversalTelegramBot` for `AsyncTelegram2` (non-blocking)~~ — kept bot; capped `WiFiClientSecure.setHandshakeTimeout(3)` + preserved `setTimeout(1500)` to cap Telegram blocking at ~4.5 s
- [x] Shrink WiFi reconnect to non-blocking `WiFi.begin()` + single-attempt check per loop iteration

**3.2.2 Dual-core task split** _(deferred — separate PR)_
- [ ] Network task pinned to **core 0**: WiFi, Telegram poll, MQTT loop, weather, Spotify, OTA check
- [ ] UI task pinned to **core 1**: touch, drawing, LED animation, countdown timer
- [ ] State handoff via FreeRTOS queues:
  - `commitEventQueue` — UI → network (outbound MQTT + Telegram broadcast)
  - `stateUpdateQueue` — network → UI (commit sync, messages, spotify, weather)
  - `touchEventQueue` — touch ISR → UI task (already partially there)

**3.2.3 Freeze detection (regression guard)**
- [x] Instrument main-loop iteration time; log when a tick exceeds 100 ms
- [x] Add a counter exposed via `/diag` Telegram command so we can verify the fix sticks in production

### 3.3 Explicitly NOT in Phase 3
Per reviewer scoping: `AsyncMqttClient`, `esp32FOTA`, signed OTA, A/B partition OTA, MQTT broker auth, LVGL migration. All deferred.

**Exit criteria:** no visible UI stall during any background network operation; main-loop tick <50 ms 99th percentile; Phase 2 tests still green.

---

## Phase 4 — AI Integration & Self-Documentation

Goal: the repo should be immediately legible to a fresh Claude session. No tribal knowledge, no drift, no hunting across five files to answer "why is this like this?"

### 4.1 CLAUDE.md — becomes the AI-integration manifest
Currently thin. Sections to add:

- [ ] **Architecture & data flow** — ASCII or mermaid diagram:
  - `touch → state → UI redraw`
  - `Telegram poll → state → UI redraw + MQTT publish`
  - `MQTT inbound → state → UI redraw`
  - `timer tick → countdown/timer redraw + breathing LED`
- [ ] **Key invariants** (one-liners with the rule and the trap)
  - Never hardcode `MY_FRIEND_INDEX` — MAC resolves identity, single binary OTA
  - `ALBUM_ART_W` cascades to `ART_X`, `VU_X`, `TIMER_W`, `PANEL_TOP` — don't change it in isolation
  - Touch calibration values are unit-specific; re-derive by tapping known corners and reading `[TOUCH] DOWN raw=...` logs
  - `client.setInsecure()` is required; Telegram's cert chain doesn't validate on this ESP32 toolchain
  - OTA is semver-only and cannot downgrade — revert by bumping past the bad build
- [ ] **MQTT topic schema** — `friyay-forever-2026/commit/<friendIndex>` with retain=true; format `FRIYAY:<idx>:<0|1>`
- [ ] **Module boundaries** — what `config.h`, `layout.h`, `state.h` each own
- [ ] **Test discipline** — pure logic PRs must include tests; `pio test -e native_test` runs locally in <5s
- [ ] **Secrets** — future secrets land in `secrets.h`; existing inline token is deliberate legacy
- [ ] **Pitfalls log** — section that grows over time; every fixed regression lands here with prevention rule

### 4.2 Inline self-documentation
- [ ] Every header file opens with a module-level doc comment: purpose, owned state, I/O boundaries
- [ ] Every pure function has a one-line purpose comment
- [ ] Comments explain **why**, not what (the code already says what)

### 4.3 README alignment
- [ ] Fix version references (remove "v27", "v28" — firmware is v1.0.15)
- [ ] Delete the "known last functional Set of code/" entry in project structure (directory doesn't exist)
- [ ] Add MQTT commit sync to feature list
- [ ] Add LED sleep-mode behavior (21:30–07:30 dim)
- [ ] Reconcile upload-port references with CLAUDE.md

### 4.4 Agent-driven workflow doc
- [ ] Add `CONTRIBUTING.md` (or absorb into `CLAUDE.md`): all work is agent-driven; PRs follow plan phases; tests are non-negotiable for pure logic; `CHANGELOG.md` updates in the version-bump PR

**Exit criteria:** a fresh Claude session opening this repo can answer: "what does this do?", "where does touch handling live?", "what are the MQTT semantics?", "what's the test command?", "what invariants will bite me?" — **without reading `main.cpp`.**

---

## Deferred (explicit — not forgotten)

| Item | Reason | Revisit when |
|---|---|---|
| Rotate Telegram bot token | Owner decision; risk scoped to bot | Never, unless token is abused |
| Full `src/ui/` `src/net/` `src/hw/` module split | Diminishing returns at current size | `main.cpp` > 3k lines |
| `qr_code.h` source-image tracking | Only matters if the QR ever changes | QR image needs updating |
| `AsyncMqttClient` migration | `PubSubClient` handles current load | MQTT becomes a bottleneck |
| `esp32FOTA` / signed OTA / A-B partition OTA | Current OTA works; low abuse surface | Production deployment or public distribution |
| MQTT broker auth / private broker | Low risk for friend-group scope | Public/abused topic namespace |
| LVGL migration | Large rewrite, not an active pain point | Post-Phase 3 if UI logic becomes unmanageable |
| Public MQTT topic namespace | Topic prefix is discoverable alongside the in-source token; minor threat-model note only | Same as broker auth |
