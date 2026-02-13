#pragma once
#include "App.h"

class TestPatternApp : public App {
public:
    bool draw(DisplayManager& display, bool force) override {
        display.clear(); 
        for (int x = 0; x < M_WIDTH; x++) {
            uint8_t val = (x * 255) / (M_WIDTH - 1);
            display.drawFastVLine(x, 0, 8, display.color565(val, 0, 0));
            display.drawFastVLine(x, 8, 8, display.color565(0, val, 0));
            display.drawFastVLine(x, 16, 8, display.color565(0, 0, val));
            display.drawFastVLine(x, 24, 8, display.color565(val, val, val));
            display.drawFastVLine(x, 32, 8, display.color565(0, val, val));
            display.drawFastVLine(x, 40, 8, display.color565(val, 0, val));
            display.drawFastVLine(x, 48, 8, display.color565(val, val, 0));
            long hue = (x * 65536L) / M_WIDTH;
            display.drawFastVLine(x, 56, 8, display.colorHSV(hue, 255, 255));
        }
        return true;
    }
};