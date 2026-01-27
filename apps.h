#pragma once
#include <Adafruit_Protomatter.h>
#include "config.h"

extern Adafruit_Protomatter matrix;

// Hilfsfunktion zum Dimmen mit Hardware-Boden (565-Format)
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

// Hilfsfunktion für zentrierten Text mit Zufalls-Versatz
void centerPrint(String text, int y, int jitterMax) {
  if (text == "") return;
  
  int16_t x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  
  // Grund-Zentrierung
  int x = (M_WIDTH - w) / 2;
  
  // Jitter: +1, 0, -1 Zeichenbreite (ca. 6 Pixel), falls Platz vorhanden
  if (jitterMax > 0 && (M_WIDTH - w) > 12) {
    int shift = (rand() % 3 - 1) * 6; // -6, 0, oder 6 Pixel
    // Sicherstellen, dass Text nicht aus dem Bild geschoben wird
    if (x + shift >= 0 && x + w + shift <= M_WIDTH) {
      x += shift;
    }
  }
  
  matrix.setCursor(x, y);
  matrix.print(text);
}

void drawWordClock() {
  struct tm ti;
  if(!getLocalTime(&ti)) return;
  int h = ti.tm_hour;
  int m = ti.tm_min;
  int mR = (m / 5) * 5; 
  int s = h % 12;
  int nextS = (s + 1) % 12;

  matrix.fillScreen(0);

  // 1. Eckpunkte (Minuten-Dots) als 2x2 Quadrate
  uint16_t dotCol = dimColor(255, 255, 255, brightness);
  
  // Oben Links (0,0 bis 1,1)
  if (m % 5 >= 1) matrix.fillRect(0, 0, 2, 2, dotCol);
  
  // Oben Rechts (M_WIDTH-2, 0 bis M_WIDTH-1, 1)
  if (m % 5 >= 2) matrix.fillRect(M_WIDTH - 2, 0, 2, 2, dotCol);
  
  // Unten Rechts (M_WIDTH-2, M_HEIGHT-2 bis M_WIDTH-1, M_HEIGHT-1)
  if (m % 5 >= 3) matrix.fillRect(M_WIDTH - 2, M_HEIGHT - 2, 2, 2, dotCol);
  
  // Unten Links (0, M_HEIGHT-2 bis 1, M_HEIGHT-1)
  if (m % 5 >= 4) matrix.fillRect(0, M_HEIGHT - 2, 2, 2, dotCol);
  // 2. Zeit-Logik ohne Umlaute
  String z0 = "Es ist", z1 = "", z2 = "", z3 = "";
  
  // Lokale Kopie der Stunden ohne Umlaute für diese Anzeige
  String stunden[] = {"Zwoelf", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf"};

  if (mR == 0)      { z1 = "Punkt"; z2 = (s == 1) ? "Eins" : stunden[s]; z3 = "Uhr"; }
  else if (mR == 5)  { z1 = "fuenf"; z2 = "nach"; z3 = stunden[s]; }
  else if (mR == 10) { z1 = "zehn"; z2 = "nach"; z3 = stunden[s]; }
  else if (mR == 15) { z1 = "Viertel"; z2 = "nach"; z3 = stunden[s]; }
  else if (mR == 20) { z1 = "zwanzig"; z2 = "nach"; z3 = stunden[s]; }
  else if (mR == 25) { z1 = "fuenf vor"; z2 = "halb"; z3 = stunden[nextS]; }
  else if (mR == 30) { z1 = "halb"; z2 = stunden[nextS]; }
  else if (mR == 35) { z1 = "fuenf nach"; z2 = "halb"; z3 = stunden[nextS]; }
  else if (mR == 40) { z1 = "zwanzig"; z2 = "vor"; z3 = stunden[nextS]; }
  else if (mR == 45) { z1 = "Drei-"; z2 = "viertel"; z3 = stunden[nextS]; } // "Dreiviertel" Trennung
  else if (mR == 50) { z1 = "zehn"; z2 = "vor"; z3 = stunden[nextS]; }
  else if (mR == 55) { z1 = "fuenf"; z2 = "vor"; z3 = stunden[nextS]; }

  // 3. Jitter-Seed (alle 5 Minuten neu)
  srand(h * 60 + mR);
  int globalYOffset = (rand() % 5) - 2; // Gesamtes Bild rutscht +/- 2 Pixel hoch/runter

  matrix.setTextSize(1);
  matrix.setTextColor(dimColor(255, 120, 0, brightness));

  // 4. Zentrierte Ausgabe mit Zeilenabstand 12
  int y = 10 + globalYOffset;
  centerPrint(z0, y, 1); 
  if (z1 != "") centerPrint(z1, y + 12, 1);
  if (z2 != "") centerPrint(z2, y + 24, 1);
  if (z3 != "") centerPrint(z3, y + 36, 1);
}

// drawSensors und drawOverlay wie gehabt...

void drawSensors() {
  matrix.fillScreen(0);
  matrix.setCursor(4, 25);
  // KORREKTUR: Hier fehlte das vierte Argument "brightness"
  matrix.setTextColor(dimColor(0, 255, 0, brightness));
  matrix.print("Temp: 22.5 C");
}

void drawOverlay() {
  // KORREKTUR: Hier fehlte das vierte Argument "brightness"
  matrix.fillRect(0, 18, M_WIDTH, 28, dimColor(0, 0, 150, brightness));
  matrix.setCursor(4, 28);
  // KORREKTUR: Hier fehlte das vierte Argument "brightness"
  matrix.setTextColor(dimColor(255, 255, 255, brightness));
  matrix.print(overlayMsg);
}

void drawTestPattern() {
  matrix.fillScreen(0);
  // 8 Reihen für Farben, 8 Spalten für Helligkeit
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      // Helligkeitsstufen: 31, 63, 95, 127, 159, 191, 223, 255
      uint8_t lvl = (col + 1) * 32 - 1; 
      uint8_t r = 0, g = 0, b = 0;

      switch (row) {
        case 0: r = lvl; break;                         // Rot
        case 1: g = lvl; break;                         // Grün
        case 2: b = lvl; break;                         // Blau
        case 3: r = g = b = lvl; break;                 // Weiß
        case 4: r = lvl; g = lvl; break;                // Gelb
        case 5: g = lvl; b = lvl; break;                // Cyan
        case 6: r = lvl; b = lvl; break;                // Magenta
        case 7: r = lvl; g = (lvl * 120) / 255; break;  // Bernstein (unser Sorgenkind)
      }
      
      // Zeichne 8x8 Pixel große Blöcke
      matrix.fillRect(col * 8, row * 8, 8, 8, dimColor(r, g, b, brightness));
    }
  }
}