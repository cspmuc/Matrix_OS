#pragma once
#include <Adafruit_Protomatter.h>
#include "config.h"

extern Adafruit_Protomatter matrix;

uint16_t dimColor(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  if (bright == 0) return 0;
  uint8_t gBright = gammaTable[bright];
  
  // Kaufmännische Rundung (+127) schützt kleine Werte vor dem "Absaufen" auf 0
  uint8_t rd = ( (uint32_t)r * gBright + 127 ) / 255;
  uint8_t gd = ( (uint32_t)g * gBright + 127 ) / 255;
  uint8_t bd = ( (uint32_t)b * gBright + 127 ) / 255;

  // Sichtbarkeits-Boden für das Hardware-Format (565)
  // Das verhindert, dass Farben kippen oder Pixel ausgehen
  if (gBright > 0) {
    if (r > 0 && rd < 8) rd = 8; 
    if (g > 0 && gd < 4) gd = 4;
    if (b > 0 && bd < 8) bd = 8;
  }
  return matrix.color565(rd, gd, bd);
}

void drawWordClock() {
  struct tm ti;
  if(!getLocalTime(&ti)) return;
  int h = ti.tm_hour;
  int m = ti.tm_min;
  
  matrix.fillScreen(0);
  // Von 100 auf 255 anheben, damit sie beim Dimmen genug "mathematisches Fleisch" haben
  uint16_t dotCol = dimColor(255, 255, 255, brightness);
  if (m % 5 >= 1) matrix.drawPixel(0, 0, dotCol);                         
  if (m % 5 >= 2) matrix.drawPixel(M_WIDTH - 1, 0, dotCol);               
  if (m % 5 >= 3) matrix.drawPixel(M_WIDTH - 1, M_HEIGHT - 1, dotCol);    
  if (m % 5 >= 4) matrix.drawPixel(0, M_HEIGHT - 1, dotCol);              

  int s = h % 12;
  int nextS = (s + 1) % 12;
  int mR = (m / 5) * 5; 
  String z1 = "", z2 = "";

  if (mR == 0) { z1 = (s == 1) ? "Ein" : stundenNamen[s]; z2 = "Uhr"; } 
  else if (mR == 15) { z1 = "Viertel nach"; z2 = stundenNamen[s]; }
  else if (mR == 30) { z1 = "halb"; z2 = stundenNamen[nextS]; }
  else if (mR == 45) { z1 = "Dreiviertel"; z2 = stundenNamen[nextS]; }
  else if (mR < 15)  { z1 = String(mR) + " nach"; z2 = stundenNamen[s]; }
  else if (mR < 30)  { z1 = String(30-mR) + " vor halb"; z2 = stundenNamen[nextS]; }
  else if (mR < 45)  { z1 = String(mR-30) + " nach halb"; z2 = stundenNamen[nextS]; }
  else { z1 = String(60-mR) + " vor"; z2 = stundenNamen[nextS]; }

  matrix.setTextSize(1);
  matrix.setTextColor(dimColor(255, 120, 0)); 
  matrix.setCursor(4, 22); matrix.print(z1);
  matrix.setCursor(4, 34); matrix.print(z2);
}

void drawSensors() {
  matrix.fillScreen(0);
  matrix.setCursor(4, 25);
  matrix.setTextColor(dimColor(0, 255, 0));
  matrix.print("Temp: 22.5 C");
}

void drawOverlay() {
  matrix.fillRect(0, 18, M_WIDTH, 28, dimColor(0, 0, 150));
  matrix.setCursor(4, 28);
  matrix.setTextColor(dimColor(255, 255, 255));
  matrix.print(overlayMsg);
}