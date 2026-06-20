#include "ui.h"

#include <string.h>

GigaDisplay_GFX display;

const Button btnStart = {90, 365, 250, 75, "START", GREEN};
const Button btnEvaluate = {460, 365, 250, 75, "AVALIAR", CYAN};

const Button btnConfirm = {90, 370, 280, 75, "CONFIRMAR", GREEN};
const Button btnReject = {430, 370, 280, 75, "REJEITAR", RED};

const Button btnRejectColor = {55, 110, 330, 85, "Cor", CARD2};
const Button btnRejectDefectColor = {425, 110, 330, 85, "Defeito_Mistura", CARD2};
const Button btnRejectDefectEdge = {55, 245, 330, 85, "Defeito_Borda", CARD2};
const Button btnRejectDefectScratch = {425, 245, 330, 85, "Defeito_Risco", CARD2};
const Button btnBack = {25, 370, 150, 75, "VOLTAR", GREY};

const Button colorButtons[COLOR_BUTTON_COUNT] = {
  {80, 125, 280, 85, "Vermelha", RED},
  {440, 125, 280, 85, "Azul", BLUE},
  {80, 250, 280, 85, "Amarela", YELLOW},
  {440, 250, 280, 85, "Verde", GREEN}
};

namespace {
constexpr int CARD_RADIUS = 14;
constexpr int HEADER_X = 25;
constexpr int HEADER_Y = 15;
constexpr int HEADER_W = 750;
constexpr int HEADER_H = 60;

int approxTextWidth(const char *text, int textSize) {
  return strlen(text) * 6 * textSize;
}

int approxTextWidth(const String &text, int textSize) {
  return text.length() * 6 * textSize;
}

String fitText(const String &text, int maxChars) {
  if (text.length() <= maxChars) return text;
  if (maxChars <= 3) return text.substring(0, maxChars);
  return text.substring(0, maxChars - 3) + "...";
}

void drawCard(int x, int y, int w, int h) {
  display.fillRoundRect(x, y, w, h, CARD_RADIUS, CARD);
  display.drawRoundRect(x, y, w, h, CARD_RADIUS, GREY);
}

void drawCenteredText(const String &text, int x, int y, int w, int h, int textSize, uint16_t color) {
  display.setTextColor(color);
  display.setTextSize(textSize);

  int textX = x + ((w - approxTextWidth(text, textSize)) / 2);
  int textY = y + ((h - (8 * textSize)) / 2);

  if (textX < x + 6) textX = x + 6;

  display.setCursor(textX, textY);
  display.println(text);
}

void drawHeader(const String &title) {
  drawCard(HEADER_X, HEADER_Y, HEADER_W, HEADER_H);
  drawCenteredText(title, HEADER_X, HEADER_Y, HEADER_W, HEADER_H, title.length() > 18 ? 3 : 4, WHITE);
}

uint16_t readableTextColor(uint16_t background) {
  return (background == WHITE || background == YELLOW) ? BLACK : WHITE;
}

uint16_t articleColorValue() {
  String color = articleColor;
  color.toLowerCase();

  if (color.indexOf("vermelh") >= 0) return RED;
  if (color.indexOf("azul") >= 0) return BLUE;
  if (color.indexOf("amarel") >= 0) return YELLOW;
  if (color.indexOf("verde") >= 0) return GREEN;

  return GREY;
}

uint16_t stateColor() {
  if (qualityState == 0) return GREEN;
  if (qualityState == 1) return YELLOW;
  if (qualityState == 2) return RED;
  return GREY;
}

const char *stateText() {
  if (qualityState == 0) return "OK";
  if (qualityState == 1) return "CHECK";
  if (qualityState == 2) return "DEFEITO";
  return "SEM SINAL";
}

void drawButton(const Button &button, bool active) {
  display.fillRoundRect(button.x, button.y, button.w, button.h, CARD_RADIUS, button.color);

  if (active) {
    display.drawRoundRect(button.x - 3, button.y - 3, button.w + 6, button.h + 6, 16, WHITE);
    display.drawRoundRect(button.x - 1, button.y - 1, button.w + 2, button.h + 2, 15, WHITE);
  } else {
    display.drawRoundRect(button.x, button.y, button.w, button.h, CARD_RADIUS, GREY);
  }

  drawCenteredText(button.label, button.x, button.y, button.w, button.h, 3, readableTextColor(button.color));
}

void drawDisabledButton(const Button &button) {
  display.fillRoundRect(button.x, button.y, button.w, button.h, CARD_RADIUS, DARKGREY);
  display.drawRoundRect(button.x, button.y, button.w, button.h, CARD_RADIUS, GREY);
  drawCenteredText(button.label, button.x, button.y, button.w, button.h, 3, GREY);
}

void drawArticleHeader() {
  String product = productCode.length() > 0 ? productCode : "SEM ORDEM";
  String title = "QUALIDADE - " + product;

  drawCard(HEADER_X, HEADER_Y, HEADER_W, HEADER_H);

  constexpr int textSize = 3;
  int textW = approxTextWidth(title, textSize);
  int textH = 8 * textSize;
  int textX = HEADER_X + ((HEADER_W - textW) / 2);
  int textY = HEADER_Y + ((HEADER_H - textH) / 2);

  if (textX < 45) textX = 45;

  display.setTextColor(WHITE);
  display.setTextSize(textSize);
  display.setCursor(textX, textY);
  display.println(title);

  int dotX = min(textX + textW + 18, 745);
  int dotY = 45;

  display.fillCircle(dotX, dotY, 14, articleColorValue());
  display.drawCircle(dotX, dotY, 14, WHITE);
}

void drawInfoCard(int x, int y, const char *title, const String &value, uint16_t color) {
  constexpr int w = 250;
  constexpr int h = 105;

  drawCard(x, y, w, h);
  drawCenteredText(title, x, y + 12, w, 24, 2, WHITE);

  int valueSize = 3;
  if (value.length() <= 5) valueSize = 4;
  if (value.length() > 12) valueSize = 2;

  drawCenteredText(value, x, y + 48, w, 40, valueSize, color);
}

void drawOrderCards() {
  drawInfoCard(35, 95, "MO", fitText(manufacturingOrder.length() > 0 ? manufacturingOrder : "---", 18), CYAN);
  drawInfoCard(35, 230, "WORK ORDER", fitText(workOrder.length() > 0 ? workOrder : "---", 18), CYAN);
}

void drawMetricCard(int x, int y, int w, int h, const char *title, const String &value, uint16_t color) {
  drawCard(x, y, w, h);

  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(x + 20, y + 18);
  display.println(title);

  display.setTextColor(color);
  display.setTextSize(4);
  display.setCursor(w > 300 ? x + 165 : x + 35, y + 62);
  display.println(value);
}

void drawMetricsCards() {
  drawMetricCard(320, 95, 205, 105, "DISPONIB.", String(availability, 1) + "%", GREEN);
  drawMetricCard(555, 95, 205, 105, "PERFORM.", String(performance, 1) + "%", CYAN);
  drawMetricCard(320, 230, 205, 105, "QUALIDADE", String(quality, 1) + "%", YELLOW);
  drawMetricCard(555, 230, 205, 105, "OEE", String(oee, 1) + "%", ORANGE);
}

void drawHomeButtons() {
  if (pendingState.length() > 0) {
    drawDisabledButton(btnStart);
    drawDisabledButton(btnEvaluate);
    return;
  }

  if (workOrder.length() == 0 || manufacturingOrder.length() == 0) {
    drawDisabledButton(btnStart);
  } else {
    drawButton(btnStart, started);
  }

  started ? drawButton(btnEvaluate, false) : drawDisabledButton(btnEvaluate);
}

void drawEvaluationShell() {
  drawHeader("AVALIACAO DA PECA");
  drawCard(35, 95, 500, 250);

  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(65, 118);
  display.println("DETECAO");

  display.drawLine(65, 155, 505, 155, GREY);
}

void drawTrafficLightBase() {
  drawCard(575, 95, 185, 250);

  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(625, 120);
  display.println("STATUS");
}

void drawEvaluateButtons() {
  drawButton(btnConfirm, false);
  drawButton(btnReject, false);
}

void drawTrafficLightEvaluating() {
  drawTrafficLightBase();

  display.fillCircle(667, 172, 27, DARKGREY);
  display.fillCircle(667, 242, 27, YELLOW);
  display.fillCircle(667, 312, 27, DARKGREY);
  display.drawCircle(667, 172, 27, WHITE);
  display.drawCircle(667, 242, 27, WHITE);
  display.drawCircle(667, 312, 27, WHITE);

  display.setTextColor(YELLOW);
  display.setTextSize(2);
  display.setCursor(605, 350);
  display.println("A AVALIAR");
}

void drawDetectionCard() {
  drawEvaluationShell();

  String label = currentLabel;
  if (label.length() > 18) label = label.substring(0, 18);

  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(65, 180);
  display.println("CLASSE");

  display.setTextColor(stateColor());
  display.setTextSize(label.length() <= 10 ? 4 : 3);
  display.setCursor(65, 215);
  display.println(label);

  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(65, 285);
  display.println("CERTEZA");

  display.setTextColor(CYAN);
  display.setTextSize(5);
  display.setCursor(170, 280);
  display.println(currentScore, 2);
}

void drawTrafficLight() {
  drawTrafficLightBase();

  uint16_t redColor = qualityState == 2 ? RED : DARKGREY;
  uint16_t yellowColor = qualityState == 1 ? YELLOW : DARKGREY;
  uint16_t greenColor = qualityState == 0 ? GREEN : DARKGREY;

  display.fillCircle(667, 172, 27, redColor);
  display.fillCircle(667, 242, 27, yellowColor);
  display.fillCircle(667, 312, 27, greenColor);
  display.drawCircle(667, 172, 27, WHITE);
  display.drawCircle(667, 242, 27, WHITE);
  display.drawCircle(667, 312, 27, WHITE);

  const char *label = stateText();
  int textX = 610;
  if (strcmp(label, "OK") == 0) textX = 652;
  else if (strcmp(label, "CHECK") == 0 || strcmp(label, "DEFEITO") == 0) textX = 625;

  display.setTextColor(stateColor());
  display.setTextSize(2);
  display.setCursor(textX, 350);
  display.println(label);
}
}  // namespace

void clearScreen() {
  display.fillScreen(BLACK);
}

void drawStatus(const String &text, uint16_t color) {
  display.fillRect(0, 452, SCREEN_W, 28, BLACK);
  display.setTextColor(color);
  display.setTextSize(2);
  display.setCursor(35, 457);
  display.println(text);
}

void drawHome() {
  currentPage = PAGE_HOME;
  clearScreen();
  drawArticleHeader();
  drawOrderCards();
  drawMetricsCards();
  drawHomeButtons();

  if (pendingState.length() > 0) {
    drawStatus("STATUS: Pedido " + pendingState + " enviado. A aguardar ACK.", YELLOW);
  } else if (started) {
    drawStatus("STATUS: Linha iniciada. Pode avaliar a peca.", GREEN);
  } else if (workOrder.length() == 0 || manufacturingOrder.length() == 0) {
    drawStatus("STATUS: A aguardar WorkOrder do Odoo", YELLOW);
  } else {
    drawStatus("STATUS: WorkOrder atribuida. Pode carregar START.", WHITE);
  }
}

void drawEvaluateEvaluating() {
  currentPage = PAGE_EVALUATE;
  clearScreen();
  drawEvaluationShell();

  display.setTextColor(YELLOW);
  display.setTextSize(4);
  display.setCursor(95, 215);
  display.println("A AVALIAR...");

  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(95, 285);
  display.println("A recolher leituras da camara");

  drawTrafficLightEvaluating();
  drawEvaluateButtons();
  drawStatus("STATUS: A avaliar durante 5 segundos...", YELLOW);
}

void drawEvaluate() {
  currentPage = PAGE_EVALUATE;
  clearScreen();
  drawDetectionCard();
  drawTrafficLight();
  drawEvaluateButtons();
  drawStatus("STATUS: Camara ligada. Confirmar ou rejeitar resultado.", CYAN);
}

void drawRejectReason() {
  currentPage = PAGE_REJECT_REASON;
  clearScreen();
  drawHeader("MOTIVO DE REJEICAO");

  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(55, 85);
  display.println("Selecione o motivo da rejeicao:");

  drawButton(btnRejectColor, false);
  drawButton(btnRejectDefectColor, false);
  drawButton(btnRejectDefectEdge, false);
  drawButton(btnRejectDefectScratch, false);
  drawButton(btnBack, false);
  drawStatus("STATUS: Rejeitou a classe detetada. Escolha o motivo.", YELLOW);
}

void drawColorSelect() {
  currentPage = PAGE_COLOR_SELECT;
  clearScreen();
  drawHeader("COR VERDADEIRA");

  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(55, 85);
  display.println("Selecione a cor verdadeira da peca:");

  for (int i = 0; i < COLOR_BUTTON_COUNT; i++) drawButton(colorButtons[i], false);

  drawButton(btnBack, false);
  drawStatus("STATUS: Escolha a cor correta.", CYAN);
}

bool isInside(int x, int y, const Button &button) {
  return x >= button.x && x <= button.x + button.w && y >= button.y && y <= button.y + button.h;
}