# FriyayConsole

ESP32-S3 based 4.3" touchscreen consoles for a friend group. Each unit displays a shared dashboard with weather, Telegram messaging, Spotify sharing, air quality, and a Friday countdown timer.

## Build & Flash

```bash
# Build firmware
~/.platformio/penv/bin/pio run

# Flash via USB (check port with ls /dev/cu.usbserial-*)
~/.platformio/penv/bin/pio run --target upload --upload-port /dev/cu.usbserial-210

# Monitor serial output
~/.platformio/penv/bin/pio device monitor --port /dev/cu.usbserial-210 --baud 115200
```

## OTA Release Process

OTA checks **GitHub Releases** (not the git repo). Pushing to `main` alone does NOT trigger OTA. You must:

1. Bump `FIRMWARE_VERSION` in `platformio.ini` (line 17)
2. Commit and push to `main`
3. Build: `~/.platformio/penv/bin/pio run`
4. Create a GitHub Release with the firmware binary:
   ```bash
   gh release create v1.0.X \
     .pio/build/esp32s3/firmware.bin \
     --title "v1.0.X - Description" \
     --notes "Changelog here"
   ```
5. Consoles detect new version via `/update` command or daily auto-check (staggered 3-7am by unit index)

The OTA system hits `https://api.github.com/repos/squid-baby/friyay-forever/releases/latest`, compares the release `tag_name` against `FIRMWARE_VERSION` using semver, and downloads `firmware.bin` from the release assets.

## Architecture

- **Single binary** — all 5 units run the same firmware. MAC address at boot determines which friend the unit belongs to.
- **MAC table** — `src/main.cpp` ~line 64. Add new units by their last 3 MAC octets + friend index.
- **Telegram Bot** — shared bot token across all consoles. `bot.getUpdates()` returns messages sent FROM users TO the bot. Each console independently polls and processes the same message stream.
- **Commit sync** — button presses post `FRIYAY:<friendIndex>:<0|1>` to a Telegram group (`FRIYAY_SYNC_CHAT`). All consoles read group messages via `getUpdates()` and update button state.

## Key Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Firmware wiring — setup, loop, handlers, hardware init |
| `src/layout.h` | UI layout: screen dimensions, panel positions, colors, UI timing |
| `src/config.h` | Hardware config: pins, Friend/MacMapping types, MQTT/weather endpoints |
| `src/state.h` | Runtime state structs — scaffolding, migration pending |
| `src/secrets.h` | Landing spot for **new** secrets (gitignored); `secrets.example.h` committed |
| `src/ota_updates.h` | OTA updater class, GitHub Releases API |
| `src/qr_code.h` | QR code display helper |
| `platformio.ini` | Board config, `FIRMWARE_VERSION`, dependencies |
| `scripts/gen_version_json.py` | Post-build: regenerates `version.json` from `FIRMWARE_VERSION` |
| `PLAN.md` | Phased execution plan; each phase lands as its own PR |
| `CHANGELOG.md` | Human-readable version history; Keep-a-Changelog format |

## Secrets policy

Existing `BOT_TOKEN`, `FRIYAY_SYNC_CHAT`, and friend Telegram IDs **stay inline in `main.cpp`** — this is deliberate per PLAN §1.2. Risk is scoped to the bot itself (not any personal Telegram account), and moving them to a gitignored file would complicate fresh clones/CI for no real security gain.

**New** secrets (future API keys, private endpoints, etc.) go in `src/secrets.h`, with the shape mirrored in the committed `secrets.example.h`.

## Ports

- `/dev/cu.usbserial-210` and `/dev/cu.usbserial-110` are the two known USB ports for flashing.
