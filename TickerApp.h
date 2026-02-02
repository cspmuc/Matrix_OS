#pragma once
#include "App.h"

class TickerApp : public App {
private:
    String message = "+++ Matrix OS - Modulare Architektur - Lauftext Test +++";
    int scrollX = M_WIDTH;
    int textWidth = -1;
    uint16_t color;
    
    // NEU: Zeitsteuerung
    unsigned long lastScrollTime = 0;
    int scrollDelay = 30; // Millisekunden pro Pixel (höher = langsamer)

public:
    void draw(DisplayManager& display) override {
        // Initialisierung
        if (textWidth == -1) {
            textWidth = display.getTextWidth(message);
            color = display.color565(0, 255, 255); 
            scrollX = M_WIDTH;
            lastScrollTime = millis();
        }

        // Zeichnen (immer!)
        display.drawScrollingText(message, 28, scrollX, color);

        // Bewegen (nur wenn Zeit abgelaufen ist!)
        if (millis() - lastScrollTime >= scrollDelay) {
            scrollX--; 
            lastScrollTime = millis();
            
            // Reset wenn durchgelaufen
            if (scrollX < -textWidth) {
                scrollX = M_WIDTH;
            }
        }
    }
    
    // Per MQTT Text UND Geschwindigkeit ändern
    void setMessage(String msg) {
        message = msg;
        textWidth = -1; 
        scrollX = M_WIDTH; 
    }
    
    void setSpeed(int delayMs) {
        scrollDelay = delayMs;
    }
};