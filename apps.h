#pragma once
#include <Adafruit_Protomatter.h>
#include <time.h> // <--- WICHTIG: Erforderlich für Zeit-Strukturen
#include "config.h"

extern Adafruit_Protomatter matrix;

uint16_t dimColor(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  if (bright == 0) return 0;
  uint8_t gBright = gammaTable[bright];
  uint8_t rd = ( (uint32_t)r * gBright + 127 ) / 255;
  uint8_t gd = ( (uint32_t)g * gBright + 127 ) / 255;
  uint8_t bd = ( (uint32_t)b * gBright + 127 ) / 255;
  if (gBright > 0) {
    if (r > 0 && rd < 8) rd = 8; 
    if (g > 0 && gd < 4) gd = 4;
    if (b > 0 && bd < 8) bd = 8;
  }
  return matrix.color565(rd, gd, bd);
}

void centerPrint(String text, int y, int jitterMax) {
  if (text == "") return;
  int16_t x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (M_WIDTH - w) / 2;
  if (jitterMax > 0 && (M_WIDTH - w) > 12) {
    int shift = (rand() % 3 - 1) * 6;
    if (x + shift >= 0 && x + w + shift <= M_WIDTH) x += shift;
  }
  matrix.setCursor(x, y);
  matrix.print(text);
}

void drawWordClock() {
  struct tm ti;
  if(!getLocalTime(&ti)) return;
  int h = ti.tm_hour, m = ti.tm_min, mR = (m / 5) * 5; 
  int s = h % 12, nextS = (s + 1) % 12;

  matrix.fillScreen(0);
  uint16_t dotCol = dimColor(255, 255, 255, brightness);
  // 2x2 Dots
  if (m % 5 >= 1) matrix.fillRect(0, 0, 2, 2, dotCol);
  if (m % 5 >= 2) matrix.fillRect(M_WIDTH - 2, 0, 2, 2, dotCol);
  if (m % 5 >= 3) matrix.fillRect(M_WIDTH - 2, M_HEIGHT - 2, 2, 2, dotCol);
  if (m % 5 >= 4) matrix.fillRect(0, M_HEIGHT - 2, 2, 2, dotCol);

  String z0 = "Es ist", z1 = "", z2 = "", z3 = "";
  String st[] = {"Zwoelf", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf"};

  if (mR == 0)      { z1 = "Punkt"; z2 = (s == 1) ? "Eins" : st[s]; z3 = "Uhr"; }
  else if (mR == 5)  { z1 = "fuenf"; z2 = "nach"; z3 = st[s]; }
  else if (mR == 10) { z1 = "zehn"; z2 = "nach"; z3 = st[s]; }
  else if (mR == 15) { z1 = "Viertel"; z2 = "nach"; z3 = st[s]; }
  else if (mR == 20) { z1 = "zwanzig"; z2 = "nach"; z3 = st[s]; }
  else if (mR == 25) { z1 = "fuenf vor"; z2 = "halb"; z3 = st[nextS]; }
  else if (mR == 30) { z1 = "halb"; z2 = st[nextS]; }
  else if (mR == 35) { z1 = "fuenf nach"; z2 = "halb"; z3 = st[nextS]; }
  else if (mR == 40) { z1 = "zwanzig"; z2 = "vor"; z3 = st[nextS]; } // <--- FIX hier
  else if (mR == 45) { z1 = "Drei-"; z2 = "viertel"; z3 = st[nextS]; }
  else if (mR == 50) { z1 = "zehn"; z2 = "vor"; z3 = st[nextS]; }
  else if (mR == 55) { z1 = "fuenf"; z2 = "vor"; z3 = st[nextS]; }

  srand(h * 60 + mR);
  int gY = (rand() % 5) - 2;
  matrix.setTextSize(1);
  matrix.setTextColor(dimColor(255, 120, 0, brightness));
  int y = 10 + gY;
  centerPrint(z0, y, 1); 
  if (z1 != "") centerPrint(z1, y + 12, 1);
  if (z2 != "") centerPrint(z2, y + 24, 1);
  if (z3 != "") centerPrint(z3, y + 36, 1);
}

void drawSensors() {
  matrix.fillScreen(0);
  matrix.setTextColor(dimColor(0, 255, 0, brightness));
  centerPrint("Temp: 22.5 C", 25, 0); 
}

void drawOverlay() {
  matrix.fillRect(0, 18, M_WIDTH, 28, dimColor(0, 0, 150, brightness));
  matrix.setTextColor(dimColor(255, 255, 255, brightness));
  centerPrint(overlayMsg, 28, 0);
}

void drawTestPattern() {
  matrix.fillScreen(0);
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 16; col++) {
      uint8_t lvl = (col + 1) * 16 - 1; 
      uint8_t r = 0, g = 0, b = 0;
      switch (row) {
        case 0: r = lvl; break; case 1: g = lvl; break; case 2: b = lvl; break;
        case 3: r = g = b = lvl; break; case 4: r = lvl; g = lvl; break;
        case 7: r = lvl; g = (lvl * 120) / 255; break;
      }
      matrix.fillRect(col * 8, row * 8, 8, 8, dimColor(r, g, b, brightness));
    }
  }
}