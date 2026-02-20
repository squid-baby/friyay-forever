# Friyay Forever - ESP32 Display Console

A custom-built smart display console that helps a group of friends coordinate their Friday riding sessions. Built on the ESP32-S3 platform with a 4.3" touchscreen, this device features real-time weather forecasting, Telegram bot integration, Spotify sharing, air quality monitoring, and a countdown timer to Friday 3pm.

## Overview

The Friyay Forever console is designed for a crew of riders (NM, ST, GO, TD, MD) to track who's committed to Friday sessions. Each unit displays:
- **Commitment Status**: Visual indicators showing who's in or out
- **Countdown Timer**: Hours, minutes, and seconds until Friday 3pm
- **Weather Forecasting**: 7-day forecast with ride condition scoring
- **Spotify Integration**: Share music via Telegram - displays album art and scannable codes
- **Air Quality Monitoring**: Real-time AQI and CO2 levels via VU meters
- **Ambient LED Lighting**: 7-LED strip with breathing and morse code animations

## Hardware Specifications

| Component | Specification |
|-----------|---------------|
| **Board** | ESP32-8048S043C |
| **Chip** | ESP32-S3 with 8MB PSRAM |
| **Display** | 4.3" 800x480 IPS TFT RGB Panel |
| **Touch** | GT911 Capacitive Touch Controller |
| **Flash** | 16MB |
| **Memory Type** | QIO OPI |
| **Air Quality** | MQ135 Sensor via ADS1115 ADC |
| **LEDs** | WS2812B Strip (7 LEDs) |

### Pin Configuration

```
Display Backlight    GPIO 2
MQ135 Sensor         GPIO 12
LED Strip            GPIO 13

GT911 Touch:
  SDA                GPIO 19
  SCL                GPIO 20
  INT                GPIO 18
  RST                GPIO 38

RGB Panel:
  DE                 GPIO 40
  VSYNC              GPIO 41
  HSYNC              GPIO 39
  PCLK               GPIO 42
  R0-R4              GPIO 45, 48, 47, 21, 14
  G0-G5              GPIO 5, 6, 7, 15, 16, 4
  B0-B4              GPIO 8, 3, 46, 9, 1
```

## Software Features

### Display Interface
- **Friend Buttons** (NM, GO, ST, MD, TD): Touch to toggle commitment status
- **Commit Button**: Quick toggle for the unit owner
- **Day Selection Bar**: Tap any day (SAT-FRI) to view that day's weather forecast
- **Weather Panel**: Three bar indicators showing WET (precipitation), TMP (temperature), and FUK (ride score)
- **Countdown Timer**: Large display showing time remaining until Friday 3pm
- **VU Meters**: Dual vertical meters for AQI and CO2 levels
- **Spotify Area**: Shows QR code for Telegram bot, or album art when music is shared
- **Notification Bar**: Animated scanner effect for new messages/commits

### Telegram Bot Commands
| Command | Description |
|---------|-------------|
| `/commit` or `in` | Mark yourself as committed |
| `/uncommit` or `out` | Remove your commitment |
| `/status` | View who's riding and countdown |
| `/weather` | Get current conditions and ride score |
| `/help` | Show available commands |
| `[Spotify Link]` | Share a track - displays album art on console |

### Weather API
- Uses Open-Meteo API (free, no API key required)
- Current conditions: temperature and precipitation
- 7-day forecast with high temps and rain amounts
- Automatic hourly updates
- **Ride Score (FUK)**: Calculated from temperature (optimal ~65°F) minus rain penalty

### LED Animations
- **Breathing**: Default cyan color with dynamic speed based on Friday countdown
- **Purple Morse**: Triggers on messages, commits, and Spotify shares
- **Red Morse**: Triggers when countdown reaches zero ("SHUT IT DOWN!")

#### Friday Countdown Anticipation
As Friday 3pm approaches, the LED breathing animation speeds up to build excitement:

| Time | Breathing Cycle | Phase Timing |
|------|-----------------|--------------|
| Normal (default) | 8 seconds | 4s dim, 4s brighten |
| Friday 2:50-2:58 PM | 6 seconds | 3s dim, 3s brighten |
| Friday 2:59 PM | 2 seconds | 1s dim, 1s brighten |
| Friday 3:00 PM | Red morse code fires | "SHUT IT DOWN!" |
| After morse completes | 8 seconds | Returns to normal |

### WiFi Setup
1. If no saved credentials, device creates AP named `FRIYAY-Setup`
2. Touch-based network selection (up to 4 networks displayed)
3. On-screen keyboard for password entry
4. Credentials saved to NVS for automatic reconnection

## Building & Uploading

### Prerequisites

```bash
# Install PlatformIO CLI or VS Code extension
brew install platformio

# Or use the VS Code extension
```

### Compile

```bash
cd ~/Documents/PlatformIO/Projects/FriyayConsole
pio run
```

### Upload Firmware

**Preferred method** (uses PlatformIO's bundled Python — avoids pyserial issues):

```bash
# Check your port first
ls /dev/cu.usb*

# Build and flash in one step
pio run -t upload --upload-port /dev/cu.usbserial-2130
```

**Alternative: manual esptool** (full flash including bootloader):

1. Put board in bootloader mode: hold BOOT, press RESET, release BOOT
2. Use usbserial port (NOT usbmodem):

```bash
python3 ~/.platformio/packages/tool-esptoolpy/esptool.py \
  --chip esp32s3 \
  --port /dev/cu.usbserial-2130 \
  --baud 115200 \
  write_flash \
  0x0 .pio/build/esp32s3/bootloader.bin \
  0x8000 .pio/build/esp32s3/partitions.bin \
  0x10000 .pio/build/esp32s3/firmware.bin
```

**Quick firmware-only flash** (skip bootloader/partitions if unchanged):

```bash
pio run -t upload --upload-port /dev/cu.usbserial-2130
```

### Monitor Serial Output

```bash
pio device monitor --port /dev/cu.usbserial-2130 --baud 115200
```

**Note**: Serial port varies between machines. NM's unit uses `/dev/cu.usbserial-2130`. Check `ls /dev/cu.usb*` to find yours.

## Configuration

### Unit Identity (MAC-based)

Unit identity is resolved automatically at boot from the ESP32's hardware MAC address. No need to recompile per unit. The MAC lookup table is in `src/main.cpp`:

```cpp
const MacMapping MAC_TABLE[] = {
  {0x85, 0x6C, 0x38, 0},    // NM - MAC 10:51:DB:85:6C:38
  // {0xXX, 0xXX, 0xXX, 1},  // ST - TODO: get MAC from Simon
  // {0xXX, 0xXX, 0xXX, 2},  // GO - TODO: add MAC
  // {0xXX, 0xXX, 0xXX, 3},  // TD - TODO: add MAC
  // {0xXX, 0xXX, 0xXX, 4},  // MN - TODO: add MAC
};
```

To add a new unit: boot it, read the MAC from serial output (`MAC: XX:XX:XX:XX:XX:XX`), and add the last 3 octets to the table. Units with unknown MACs default to index 0 (NM).

**Important**: Because identity is MAC-based, a single firmware binary works for all units via OTA. Never hardcode `MY_FRIEND_INDEX` — that's how the ST/NM identity swap bug happened (v1.0.2 OTA pushed a binary compiled with `MY_FRIEND_INDEX=1` to all units).

### Other Settings

```cpp
// Friend list with Telegram IDs
Friend friends[] = {
  {"NM", 7612996805LL, false},
  {"ST", 7015581601LL, false},
  {"GO", 0LL, false},
  {"TD", 8293810017LL, false},
  {"MD", 0LL, false}
};

// Weather location (latitude/longitude)
#define LATITUDE 35.9132
#define LONGITUDE -79.0558
```

## Dependencies

Managed via `platformio.ini`:

| Library | Version | Purpose |
|---------|---------|---------|
| GFX Library for Arduino | 1.3.9 | Display driver |
| ArduinoJson | ^7.4.2 | JSON parsing for APIs |
| UniversalTelegramBot | ^1.3.0 | Telegram integration |
| JPEGDEC | ^1.4.1 | Album art decoding |
| gt911-arduino | GitHub | Touch controller |
| Adafruit ADS1X15 | ^2.4.0 | ADC for air sensor |
| FastLED | ^3.6.0 | LED strip control |

## Project Structure

```
FriyayConsole/
├── platformio.ini              # PlatformIO configuration
├── src/
│   ├── main.cpp               # Main firmware (v27)
│   └── qr_code.h              # Embedded QR code for Telegram bot
├── known last functional Set of code/
│   ├── main-v26.cpp           # v26 cleaned up version
│   ├── code_review_v25.md     # Code review notes
│   ├── FriyayQR.jpg           # QR code source image
│   └── TelegramQR.jpg         # Alternative QR code
└── .pio/
    └── build/
        └── esp32s3/           # Compiled binaries
            ├── bootloader.bin
            ├── partitions.bin
            └── firmware.bin
```

## Timing Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `SPLASH_DURATION_MS` | 2500ms | Boot splash screen duration |
| `MSG_DISPLAY_TIME_MS` | 34000ms | Message display duration |
| `MSG_HIGHLIGHT_TIME_MS` | 30000ms | Cyan border highlight duration |
| `DAY_AUTO_RESET_MS` | 20000ms | Auto-reset day selection to current |
| `COMMIT_ANIM_DURATION` | 3000ms | "Cha Boi!" animation duration |
| `BREATH_NORMAL_CYCLE` | 480 frames | 8-second breathing (default) |
| `BREATH_FAST_CYCLE` | 360 frames | 6-second breathing (2:50 PM Friday) |
| `BREATH_FASTER_CYCLE` | 120 frames | 2-second breathing (2:59 PM Friday) |

## Troubleshooting

### Upload Fails
- **Use correct port**: `/dev/cu.usbserial-1140` (not usbmodem)
- **Enter bootloader mode** before running esptool
- **Lower baud rate** if unstable: try 115200 instead of 460800

### Display Issues
- Press RESET button after upload
- Check backlight pin (GPIO 2) is HIGH
- Monitor serial output for initialization errors

### Touch Not Working
- GT911 requires I2C initialization
- Check serial output for "GT911 initialized!" message
- Verify I2C pins (SDA=19, SCL=20)

### WiFi Connection Fails
- Device creates `FRIYAY-Setup` AP if no saved credentials
- Connect to AP and use touchscreen to configure
- WiFi can take 10-15 seconds to connect

### Telegram Not Responding
- Bot checks every 15 seconds
- Verify bot token in source code
- Check serial output for "Msg from..." logs

## Version History

| Version | Changes |
|---------|---------|
| **v27 (1.0.4)** | Fixed album art overflow: PSRAM buffer decode + software scale to 260x260, MAC-based unit identity (single binary OTA), scan code/QR positions unchanged |
| **v26** | Code cleanup: fixed ADS1115 crash bug, fixed morse bounds check, removed ~100 lines dead code, extracted `drawCyberpunkGrid()` helper, added named constants for magic numbers |
| **v25** | Fixed QR code layering, scanner animation on commit, message scroll clipping, increased font sizes and timings, LED breathing speed-up as Friday 3pm approaches |
| **v24** | Added 7-day weather forecast, day selection |
| **v23** | LED strip integration with FastLED |
| **v20** | Fixed Telegram ID types, full feature set |

## Album Art Rendering (SOLVED)

### The Problem
The Spotify oembed API returns 300x300px album art. JPEGDEC only supports power-of-2 scaling (half=150, quarter=75) — neither fit the panel. The old URL hash swap (`ab67616d0000b273` -> `ab67616d00001e02`) to request a 64px thumbnail no longer works (Spotify changed their URL format). Rendering at 1:1 caused the art to overflow behind VU meters, off-screen right, and behind the scan code.

### The Fix
Decode the full 300px JPEG into a PSRAM buffer (~175KB at RGB565), software-scale to 260x260 using nearest-neighbor, then blit to screen. The art renders flush right to the screen edge and flush left against the VU meters, covering the LISTEN header and sitting flush on top of the scan code bar.

### Lessons Learned
- **JPEGDEC `decode(offsetX, offsetY)` with negative offsets** does NOT clip — pixels draw freely past all edges. The callback's bounds check (`pDraw->x >= W`) only skips whole tiles, not partial overflow.
- **JPEGDEC callback bounds checking is critical** when decoding to a buffer. Tiles can extend beyond image dimensions. Always check `dstX < bufferW && dstY < bufferH` per-pixel or you get `CORRUPT HEAP` crashes in PSRAM.
- **`ALBUM_ART_W` is a layout constant**, not just the art size. It cascades to `ART_X`, `VU_X`, `TIMER_W`, and `PANEL_TOP`. Changing it moves the entire right side of the UI. To render art larger than the panel, handle it locally in `decodeAndDisplayJpeg()` with its own render coordinates.

## OTA Lessons

### Single Binary for All Units
`MY_FRIEND_INDEX` was originally a `#define`, meaning each unit needed a separately compiled binary. OTA pushed the same binary to all consoles, causing identity swaps (e.g., NM's unit became ST). Fixed by resolving identity from the ESP32's hardware MAC address at boot — one binary works for everyone.

### OTA Cannot Downgrade
The OTA system uses semver comparison and only installs newer versions. To "downgrade" (revert a bad build), you must bump the version number higher (e.g., v1.0.2 bad -> v1.0.3 with reverted code) so the console sees it as an update.

### Testing Before OTA
Flash via USB first (`pio run -t upload --upload-port /dev/cu.usbserial-XXXX`) to iterate quickly. Only push to GitHub Releases for OTA after the firmware is verified on hardware. The serial port may differ between machines — check `ls /dev/cu.usb*`.

## APIs Used

- **Open-Meteo**: Weather forecast (free, no key required)
- **Telegram Bot API**: Messaging and commands
- **Spotify oEmbed**: Album art thumbnails
- **Spotify Scannables**: Scannable codes for tracks

## Credits

Created December 2025 for the Friyay crew.

**Protocol 1.0 v27 - ALBUM ART FIX + MAC IDENTITY**
