// Runtime state — grouped structs for the subsystems that currently live as a
// wall of ~50 globals in main.cpp. These are SCAFFOLDING for a follow-up PR;
// main.cpp still reads/writes raw globals directly. Landing the structs now so
// (a) PLAN.md §1.5 closes and (b) subsequent PRs can migrate one subsystem at
// a time against a stable interface without churning this header.
//
// Fields mirror the existing globals 1:1 to make the eventual migration a
// mechanical rename. Anything marked TODO(state-migration) is intentional.
#pragma once

#include <Arduino.h>
#include "layout.h"     // BREATH_NORMAL_CYCLE

// ------------------------------------------------------------
// Weather — current conditions + 7-day forecast + render scores
// ------------------------------------------------------------
struct WeatherState {
  float        currTemp       = 70;
  float        precipitation  = 0;
  int          wetLvl         = 5;
  int          tmpLvl         = 5;
  int          fukLvl         = 5;
  bool         ok             = false;
  int          selectedDay    = -1;
  unsigned long lastDaySelectTime = 0;
  float        forecastHighTemp[7] = {70, 70, 70, 70, 70, 70, 70};
  float        forecastRain[7]     = {0, 0, 0, 0, 0, 0, 0};
  bool         forecastLoaded = false;
};

// ------------------------------------------------------------
// Spotify — active track + album art + sender
// ------------------------------------------------------------
struct SpotifyState {
  bool    active          = false;
  String  trackId         = "";
  String  albumArtUrl     = "";
  String  spotifyCodeUrl  = "";
  String  senderInitials  = "";
  bool    showingQRCode   = false;
};

// ------------------------------------------------------------
// Commit — per-unit pending/latched commit tracking
// ------------------------------------------------------------
struct CommitState {
  unsigned long lastCommitTime    = 0;
  bool          pending           = false;   // second-tap window open
  unsigned long pendingTime       = 0;
};

// ------------------------------------------------------------
// Touch — raw → mapped input + rejection tracking
// Lifecycle phase (IDLE / PRESSED / HELD) uses the existing TouchPhase enum.
// ------------------------------------------------------------
struct TouchState {
  int           x              = 0;
  int           y              = 0;
  int           savedX         = 0;
  int           savedY         = 0;
  bool          wasTouched     = false;
  bool          ok             = false;
  unsigned long ghostCount     = 0;
  // Phase lives as the existing global TouchPhase touchState during migration.
};

// ------------------------------------------------------------
// LED — animation mode + breathing/morse progress
// ------------------------------------------------------------
enum LedAnimationMode { LED_ANIM_BREATHING, LED_ANIM_MORSE_PURPLE, LED_ANIM_MORSE_RED };

struct LedState {
  LedAnimationMode mode           = LED_ANIM_BREATHING;
  int              breathPhase    = 0;
  bool             morseActive    = false;
  unsigned long    morseStepStart = 0;
  int              morseStep      = 0;
  int              lastCycleFrames = BREATH_NORMAL_CYCLE;
};
