#pragma once
#include "App.h"
#include "RichText.h"

class TickerApp : public App {
private:
    RichText richText;
    String message = "{c:mint}Matrix OS RichTextEngine:{c:white}  Font Icons: {sun}{star}{arrow_u}  Bitmap Icons PNG: {ic:grinning_face}{ic:cowboy_hat_face}{ic:clown_face}  LaMetric Static: {ln:8441}  Animated: {la:61}";
    int scrollX = M_WIDTH;
    int totalWidth = -1;
    unsigned long lastScrollTime = 0;
    int scrollDelay = 30; 

public:
    bool draw(DisplayManager& display, bool force) override {
        display.clear(); // Ticker muss immer clean sein

        if (totalWidth == -1) {
            totalWidth = richText.getTextWidth(display, message, "Medium");
            scrollX = M_WIDTH;
            lastScrollTime = millis();
        }

        richText.drawString(display, scrollX, 38, message, "Medium");

        if (millis() - lastScrollTime >= scrollDelay) {
            scrollX--; 
            lastScrollTime = millis();
            if (scrollX < -totalWidth) {
                scrollX = M_WIDTH;
            }
        }
        return true; // Immer Update nötig
    }
    
    void setMessage(String msg) {
        message = msg;
        totalWidth = -1; 
        scrollX = M_WIDTH; 
    }
    
    void setSpeed(int delayMs) {
        scrollDelay = delayMs;
    }
};