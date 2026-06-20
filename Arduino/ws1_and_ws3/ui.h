#pragma once
#include <Arduino_GigaDisplay_GFX.h>

// --- RGB565 Colors ---
#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xFFFF
#define COLOR_GREEN  0x07E0
#define COLOR_YELLOW 0xFFE0
#define COLOR_RED    0xF800
#define COLOR_CYAN   0x07FF
#define COLOR_GRAY   0x8410
#define COLOR_BG     0x2104
#define COLOR_ORANGE 0xFD00

// --- Screen layout (800 x 480 landscape) ---
#define SCREEN_W 800
#define SCREEN_H 480

#define CHAR_W(s) (6 * (s))
#define CHAR_H(s) (8 * (s))

#define TITLE_BAND_Y  0
#define TITLE_BAND_H  88
#define STATE_BAND_Y  95
#define STATE_BAND_H  205

// --- Button ---
struct Button {
    int x, y, w, h;
    uint16_t color;
    const char *label;
};

extern const Button BTN_START;
extern const Button BTN_PAUSE;
extern const Button BTN_STOP;

// --- Pause reason menu ---
#define NUM_PAUSE_REASONS 5
extern const char *PAUSE_REASONS[NUM_PAUSE_REASONS];       // display labels
extern const char *PAUSE_REASON_VALUES[NUM_PAUSE_REASONS]; // Sparkplug B values
extern const Button BTN_REASONS[NUM_PAUSE_REASONS];
extern const Button BTN_CANCEL;

// --- Pause type menu ---
extern const Button BTN_PLANNED;
extern const Button BTN_UNPLANNED;

// --- Function declarations ---
void drawUI();
void drawPauseReasonMenu();
void drawPauseTypeMenu();
void redrawTitle();
void redrawStateLabel();
void drawButton(const Button &btn);
bool hitTest(const Button &btn, int tx, int ty);
