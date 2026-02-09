#pragma once
#include "App.h"
#include <math.h>

class PlasmaApp : public App {
private:
    uint8_t sinLUT[256];      // Tabelle für Sinus-Werte (0-255)
    uint16_t palette[256];    // Tabelle für Farben (HSV vorberechnet)
    int timePos1 = 0;
    int timePos2 = 0;

public:
    PlasmaApp() {
    }

    void setupPalette(DisplayManager& display) {
        static bool paletteReady = false;
        if (paletteReady) return;
        
        for (int i = 0; i < 256; i++) {
            // Ein schöner Regenbogen-Verlauf
            palette[i] = display.colorHSV(i * 256, 255, 255);
        }
        paletteReady = true;
    }

    void draw(DisplayManager& display) override {
        static bool initialized = false;
        if (!initialized) {
            // Einmalige Berechnung beim ersten Aufruf
            for (int i = 0; i < 256; i++) {
                sinLUT[i] = (uint8_t)((sin(i * 2.0 * PI / 256.0) + 1.0) * 127.5);
            }
            initialized = true;
        }

        setupPalette(display);

        timePos1 += 2; 
        timePos2 += 3;

        for (int y = 0; y < M_HEIGHT; y++) {
            
            uint8_t yBase = sinLUT[(y + timePos1) & 255];
            uint8_t yBase2 = sinLUT[(y * 2 + timePos2) & 255];

            for (int x = 0; x < M_WIDTH; x++) {
                
                uint8_t xVal = sinLUT[(x + timePos2) & 255];
                uint8_t xyVal = sinLUT[(x + y + timePos1) & 255];

                uint8_t total = (yBase + yBase2 + xVal + xyVal) / 2; // Ergebnis 0-255

                display.drawPixel(x, y, palette[total]);
            }
        }
    }
};