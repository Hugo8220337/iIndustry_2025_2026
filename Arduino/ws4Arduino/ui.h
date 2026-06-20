#pragma once

#include <Arduino.h>
#include <Arduino_GigaDisplay_GFX.h>

constexpr int SCREEN_W = 800;
constexpr int SCREEN_H = 480;

enum Page : uint8_t {
  PAGE_HOME = 0,
  PAGE_EVALUATE,
  PAGE_REJECT_REASON,
  PAGE_COLOR_SELECT
};


#ifdef BLACK
#undef BLACK
#endif
#ifdef WHITE
#undef WHITE
#endif
#ifdef RED
#undef RED
#endif
#ifdef GREEN
#undef GREEN
#endif
#ifdef BLUE
#undef BLUE
#endif
#ifdef YELLOW
#undef YELLOW
#endif
#ifdef ORANGE
#undef ORANGE
#endif
#ifdef GREY
#undef GREY
#endif
#ifdef DARKGREY
#undef DARKGREY
#endif
#ifdef CYAN
#undef CYAN
#endif
#ifdef CARD
#undef CARD
#endif
#ifdef CARD2
#undef CARD2
#endif

constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t RED = 0xF800;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t BLUE = 0x001F;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t ORANGE = 0xFD20;
constexpr uint16_t GREY = 0x8410;
constexpr uint16_t DARKGREY = 0x4208;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t CARD = 0x2104;
constexpr uint16_t CARD2 = 0x3186;

struct Button {
  int x;
  int y;
  int w;
  int h;
  const char *label;
  uint16_t color;
};

extern GigaDisplay_GFX display;

extern int currentPage;
extern bool started;
extern String pendingState;

extern String manufacturingOrder;
extern String workOrder;
extern String productCode;
extern String articleColor;

extern float availability;
extern float performance;
extern float quality;
extern float oee;

extern int qualityState;
extern String currentLabel;
extern float currentScore;

extern const Button btnStart;
extern const Button btnEvaluate;
extern const Button btnConfirm;
extern const Button btnReject;
extern const Button btnRejectColor;
extern const Button btnRejectDefectColor;
extern const Button btnRejectDefectEdge;
extern const Button btnRejectDefectScratch;
extern const Button btnBack;

constexpr int COLOR_BUTTON_COUNT = 4;
extern const Button colorButtons[COLOR_BUTTON_COUNT];

void clearScreen();
void drawStatus(const String &text, uint16_t color);
void drawHome();
void drawEvaluateEvaluating();
void drawEvaluate();
void drawRejectReason();
void drawColorSelect();

bool isInside(int x, int y, const Button &button);
