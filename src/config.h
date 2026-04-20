// Deployment config: hardware pins, friend roster shape, Telegram/MQTT/weather
// endpoints, MAC-to-owner mapping type. Pure declarations + #defines only — no
// allocated data tables (those live in main.cpp so they can keep their literal
// values co-located with the board assembly).
//
// Per PLAN.md §1.2, existing BOT_TOKEN and friend Telegram IDs stay in source
// (tracked, inline) rather than moving to gitignored secrets.h. Risk is scoped
// to the bot itself, not any personal account.
#pragma once

#include <stdint.h>

// ============================================================
// PIN DEFINITIONS — ESP32-8048S043C
// ============================================================
#define GFX_BL 2
#define MQ135_PIN 12

// GT911 Touch
#define TOUCH_SDA 19
#define TOUCH_SCL 20
#define TOUCH_INT 18
#define TOUCH_RST 38

// WS2812B LED Strip
#define LED_PIN 13
#define LED_COUNT 7
#define LED_BRIGHTNESS 128

// ============================================================
// WEATHER (Open-Meteo location — no API key required)
// ============================================================
#define LATITUDE  35.9132
#define LONGITUDE -79.0558

// ============================================================
// MQTT commit sync (public broker, topic namespaced by year)
// ============================================================
#define MQTT_BROKER     "broker.emqx.io"
#define MQTT_PORT       1883
#define MQTT_TOPIC_BASE "friyay-forever-2026/commit"  // per-friend: .../0, .../1, etc.

// ============================================================
// FRIEND ROSTER
// ============================================================
#define NUM_FRIENDS 5

struct Friend {
  const char* initials;
  const char* name;
  int64_t     telegramId;
  bool        committed;
};

// MAC-to-owner lookup (last 3 bytes of MAC). Table lives in main.cpp.
struct MacMapping {
  uint8_t mac3, mac4, mac5;
  int     friendIndex;
};

// Resolve a MAC's last 3 octets against a mapping table. Returns the friend
// index, or -1 if not found. Pure — safe to call from native_test.
int lookupMacOwner(const uint8_t mac[6], const MacMapping* table, int tableSize);
