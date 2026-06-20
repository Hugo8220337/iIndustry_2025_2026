#include <Arduino.h>
#include "ui.h"

extern GigaDisplay_GFX display;
extern String currentState;
extern String pendingState;
extern String workOrder;
extern bool choosingPauseReason;
extern bool choosingPauseType;

const Button BTN_START = {80,  335, 180, 90, COLOR_GREEN,  "START"};
const Button BTN_PAUSE = {310, 335, 180, 90, COLOR_YELLOW, "PAUSE"};
const Button BTN_STOP  = {540, 335, 180, 90, COLOR_RED,    "STOP"};

// Pause reason menu — 3 top row, 2 bottom row
const char *PAUSE_REASONS[NUM_PAUSE_REASONS] = {
    "Break", "Maintenance", "Material", "Quality", "Setup"
};
const char *PAUSE_REASON_VALUES[NUM_PAUSE_REASONS] = {
    "OPERATOR_BREAK", "MAINTENANCE", "MATERIAL_MISSING", "QUALITY_CHECK", "SETUP"
};
const Button BTN_REASONS[NUM_PAUSE_REASONS] = {
    { 20, 70, 240, 120, COLOR_ORANGE, "Break"       },
    {280, 70, 240, 120, COLOR_ORANGE, "Maintenance"  },
    {540, 70, 240, 120, COLOR_ORANGE, "Material"     },
    {150, 215, 240, 120, COLOR_ORANGE, "Quality"     },
    {410, 215, 240, 120, COLOR_ORANGE, "Setup"       },
};
const Button BTN_CANCEL    = {310, 365, 180,  85, COLOR_RED,    "CANCEL"};

const Button BTN_PLANNED   = { 80, 160, 280, 160, COLOR_GREEN,  "PLANNED"};
const Button BTN_UNPLANNED = {440, 160, 280, 160, COLOR_ORANGE, "UNPLANNED"};

static void drawCentered(const char *text, int cx, int cy, int sz, uint16_t color)
{
    int w = strlen(text) * CHAR_W(sz);
    display.setTextSize(sz);
    display.setTextColor(color);
    display.setCursor(cx - w / 2, cy - CHAR_H(sz) / 2);
    display.print(text);
}

static uint16_t stateColor(const String &s)
{
    if (s == "Start") return COLOR_GREEN;
    if (s == "Pause") return COLOR_YELLOW;
    if (s == "Stop")  return COLOR_RED;
    if (s == "Ready") return COLOR_CYAN;
    return COLOR_WHITE;
}

void drawButton(const Button &btn)
{
    display.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 14, btn.color);
    drawCentered(btn.label, btn.x + btn.w / 2, btn.y + btn.h / 2, 3, COLOR_BLACK);
}

void redrawTitle()
{
    if (choosingPauseReason || choosingPauseType) return;
    display.fillRect(0, TITLE_BAND_Y, SCREEN_W, TITLE_BAND_H, COLOR_BG);
    if (workOrder.length() > 0) {
        String title = "Work Order: " + workOrder;
        drawCentered(title.c_str(), SCREEN_W / 2, 44, 3, COLOR_WHITE);
    } else {
        drawCentered("No work order loaded", SCREEN_W / 2, 44, 3, COLOR_GRAY);
    }
}

void redrawStateLabel()
{
    if (choosingPauseReason || choosingPauseType) return;
    display.fillRect(0, STATE_BAND_Y, SCREEN_W, STATE_BAND_H, COLOR_BG);
    if (pendingState.length() > 0) {
        drawCentered(pendingState.c_str(),
                     SCREEN_W / 2, STATE_BAND_Y + 102,
                     5, COLOR_GRAY);
    } else {
        drawCentered(currentState.c_str(),
                     SCREEN_W / 2, STATE_BAND_Y + 102,
                     5, stateColor(currentState));
    }
}

void drawPauseTypeMenu()
{
    display.fillScreen(COLOR_BG);
    drawCentered("Planned or Unplanned?", SCREEN_W / 2, 80, 3, COLOR_WHITE);
    drawButton(BTN_PLANNED);
    drawButton(BTN_UNPLANNED);
    drawButton(BTN_CANCEL);
}

void drawPauseReasonMenu()
{
    display.fillScreen(COLOR_BG);
    drawCentered("Select Pause Reason", SCREEN_W / 2, 30, 3, COLOR_WHITE);
    for (int i = 0; i < NUM_PAUSE_REASONS; i++)
        drawButton(BTN_REASONS[i]);
    drawButton(BTN_CANCEL);
}

void drawUI()
{
    display.setRotation(1);
    display.fillScreen(COLOR_BG);

    redrawTitle();
    display.drawFastHLine(50, 88,  700, COLOR_GRAY);
    display.drawFastHLine(50, 308, 700, COLOR_GRAY);

    redrawStateLabel();

    drawButton(BTN_START);
    drawButton(BTN_PAUSE);
    drawButton(BTN_STOP);
}

bool hitTest(const Button &btn, int tx, int ty)
{
    return tx >= btn.x && tx <= btn.x + btn.w &&
           ty >= btn.y && ty <= btn.y + btn.h;
}
