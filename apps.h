#pragma once
#include <Adafruit_Protomatter.h>
#include "config.h"
#include <Arduino.h>

enum AppType { WORDCLOCK, SENSORS, OFF };
extern Adafruit_Protomatter matrix;

// Hilfsfunktion für Farben ohne manuelle Dimmung
uint16_t matrixColor(uint8_t r, uint8_t g, uint8_t b) {
    return matrix.color565(r, g, b);
}

// Hilfsfunktion zum Dimmen der Farben
uint16_t dimColor(uint8_t r, uint8_t g, uint8_t b) {
  uint8_t r_dim = (r * brightness) / 255;
  uint8_t g_dim = (g * brightness) / 255;
  uint8_t b_dim = (b * brightness) / 255;
  return matrix.color565(r_dim, g_dim, b_dim);
}

void drawWordClock() {
  struct tm ti;
  if(!getLocalTime(&ti)) return;
int h = ti.tm_hour;
    int m = ti.tm_min;
    matrix.fillScreen(0);
  
    // Eckpunkte jetzt in voller Farbstärke
    uint16_t dot = matrixColor(150, 150, 150); 
    if (m % 5 >= 1) matrix.drawPixel(0, 0, dot);
    if (m % 5 >= 2) matrix.drawPixel(63, 0, dot);
    if (m % 5 >= 3) matrix.drawPixel(63, 63, dot);
    if (m % 5 >= 4) matrix.drawPixel(0, 63, dot);

  int s = h % 12;
  int nextS = (s + 1) % 12;
  int mR = (m / 5) * 5; 
  String z1 = "", z2 = "";

  if (mR == 0) { z1 = (s == 1) ? "Ein" : stundenNamen[s]; z2 = "Uhr"; } 
  else if (mR == 5)  { z1 = "Fuenf nach"; z2 = stundenNamen[s]; }
  else if (mR == 10) { z1 = "Zehn nach"; z2 = stundenNamen[s]; }
  else if (mR == 15) { z1 = "Viertel nach"; z2 = stundenNamen[s]; }
  else if (mR == 20) { z1 = "Zehn vor halb"; z2 = stundenNamen[nextS]; }
  else if (mR == 25) { z1 = "Fuenf vor halb"; z2 = stundenNamen[nextS]; }
  else if (mR == 30) { z1 = "halb"; z2 = stundenNamen[nextS]; }
  else if (mR == 35) { z1 = "Fuenf nach halb"; z2 = stundenNamen[nextS]; }
  else if (mR == 40) { z1 = "Zehn nach halb"; z2 = stundenNamen[nextS]; }
  else if (mR == 45) { z1 = "Dreiviertel"; z2 = stundenNamen[nextS]; }
  else if (mR == 50) { z1 = "Zehn vor"; z2 = stundenNamen[nextS]; }
  else if (mR == 55) { z1 = "Fuenf vor"; z2 = stundenNamen[nextS]; }

  matrix.setTextSize(1);
  matrix.setTextColor(matrixColor(255, 100, 0)); // Bernstein (Full Power)
  matrix.setCursor(4, 22); matrix.print(z1);
  matrix.setCursor(4, 34); matrix.print(z2);
}

void drawSensors() {
  matrix.setCursor(4, 10);
  matrix.setTextColor(dimColor(0, 255, 0)); // Grün gedimmt
  matrix.print("Klima:");
  matrix.setCursor(4, 25);
  matrix.setTextColor(dimColor(255, 255, 255)); // Weiß gedimmt
  matrix.print("Temp: 22.5 C");
}

void drawOverlay() {
  matrix.fillRect(0, 18, M_WIDTH, 28, dimColor(0, 0, 150)); // Blau gedimmt
  matrix.drawRect(0, 18, M_WIDTH, 28, dimColor(255, 255, 255));
  matrix.setCursor(4, 28);
  matrix.setTextColor(dimColor(255, 255, 255));
  matrix.print(overlayMsg);
}