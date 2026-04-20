// UI layout constants: screen dimensions, panel positions, colors, UI-coupled
// timing. Pure #defines — no runtime state, no hardware coupling. Safe to
// include in both the firmware build and native_test.
//
// Panel layout is heavily inter-dependent — NOTIF_X / ART_X / VU_X / TIMER_W
// / PANEL_* all cascade from SCREEN_W, MARGIN, and ALBUM_ART_W. Changing
// ALBUM_ART_W moves the entire right side of the UI.
#pragma once

// ============================================================
// COLORS (RGB565)
// ============================================================
#define COL_BLACK       0x0000
#define COL_WHITE       0xFFFF
#define COL_YELLOW      0xFEA0
#define COL_CYAN        0x07FF
#define COL_GREEN       0x3CA4
#define COL_VU_GREEN    0x07E0
#define COL_RED         0xF800
#define COL_ORANGE      0xFC40
#define COL_GRAY        0x52AA
#define COL_DARK_GRAY   0x31A6
#define COL_SPOTIFY_BG  0x1807
#define COL_SCANNER     0x055F
#define COL_GRID        0x2115

// ============================================================
// LAYOUT CONSTANTS (800x480)
// ============================================================
#define SCREEN_W 800
#define SCREEN_H 480
#define MARGIN 15
#define Y_OFFSET 12  // Shift entire UI down for enclosure alignment

// Notification box
#define NOTIF_W 305
#define NOTIF_H 75
#define NOTIF_X (SCREEN_W - NOTIF_W - MARGIN)
#define NOTIF_Y (MARGIN + Y_OFFSET - 17)

// Friend buttons
#define BTN_Y (3 + Y_OFFSET)
#define BTN_H 65
#define BTN_W 60
#define BTN_GAP 6
#define COMMIT_W 80

#define BOTTOM_LINE (SCREEN_H - MARGIN + Y_OFFSET)

// Spotify/Album art area
#define ALBUM_ART_W 225
#define ALBUM_ART_H 280
#define ALBUM_ART_DISPLAY_H 260  // art area height: covers header, flush with scan code
#define SPOT_HEADER_H 45
#define SPOT_TOTAL_H (SPOT_HEADER_H + ALBUM_ART_H)
#define SPOT_BOTTOM BOTTOM_LINE
#define SPOT_TOP (SPOT_BOTTOM - SPOT_TOTAL_H)
#define ART_X (SCREEN_W - ALBUM_ART_W - MARGIN)
#define ART_AREA_Y (SPOT_TOP + SPOT_HEADER_H)
#define QR_OFFSET_X 22
#define QR_OFFSET_Y 10

// VU meters
#define VU_W 38
#define VU_GAP 10
#define VU_TOTAL_W (VU_W * 2 + VU_GAP)
#define VU_TO_ART_GAP 15
#define VU_TO_PANEL_GAP 8
#define VU_X (ART_X - VU_TO_ART_GAP - VU_TOTAL_W)
#define VU_TOP SPOT_TOP
#define VU_BOTTOM BOTTOM_LINE
#define VU_H (VU_BOTTOM - VU_TOP)

// Timer
#define TIMER_H 140
#define TIMER_BOTTOM BOTTOM_LINE
#define TIMER_Y (TIMER_BOTTOM - TIMER_H)
#define TIMER_X MARGIN
#define TIMER_W (VU_X - VU_TO_PANEL_GAP - MARGIN)

// Weather panel
#define PANEL_X MARGIN
#define PANEL_TOP SPOT_TOP
#define PANEL_BOTTOM (TIMER_Y - 8)
#define PANEL_H (PANEL_BOTTOM - PANEL_TOP)
#define PANEL_W TIMER_W
#define PANEL_Y PANEL_TOP

// Days row
#define DAY_H 28
#define DAYS_Y (PANEL_TOP - 5 - DAY_H)
#define HEADER_Y (DAYS_Y + DAY_H / 2)

// Weather bars
#define WEATHER_START_Y (PANEL_Y + 30)
#define WEATHER_ROW_GAP ((PANEL_H - 25) / 3)
#define BLOCK_SIZE 28
#define BLOCK_GAP 4

// Grid pattern
#define GRID_SPACING 25

// ============================================================
// UI TIMING
// ============================================================
#define SPLASH_DURATION_MS 2500
#define MSG_DISPLAY_TIME_MS 60000
#define MSG_HIGHLIGHT_TIME_MS 45000
#define DAY_AUTO_RESET_MS 20000
#define COMMIT_ANIM_DURATION 3000
#define MAX_WIFI_NETWORKS 4
#define MAX_BOUNCES 16
#define SCANNER_SPEED 8
#define COMMIT_CONFIRM_MS 3000

// LED breathing cadence
#define BREATH_NORMAL_CYCLE 480   // 8 seconds (4+4)
#define BREATH_FAST_CYCLE 360     // 6 seconds (3+3)
#define BREATH_FASTER_CYCLE 120   // 2 seconds (1+1)
