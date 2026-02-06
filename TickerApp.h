#pragma once
#include "App.h"
#include "RichText.h"

class TickerApp : public App {
private:
    RichText richText;
    
    // Unser Demo-Text mit allen Features
    String message = "{c:highlight}{b}+++ NEWS +++{c:white} Das ist ein {c:success}Lauftext {c:white}mit {c:warn}{u}Formatierung{u} {c:white}und {c:info}Icons {sun} {heart} {smile} {c:white}in der Matrix!";
    
    int scrollX = M_WIDTH;
    int totalWidth = -1;
    
    unsigned long lastScrollTime = 0;
    int scrollDelay = 30; 

public:
    void draw(DisplayManager& display) override {
        // Initialisierung: Breite des RICH Textes messen!
        if (totalWidth == -1) {
            totalWidth = richText.getTextWidth(display, message, "Medium");
            scrollX = M_WIDTH;
            lastScrollTime = millis();
        }

        // Zeichnen (Lauftext in Medium Font)
        // y=38 ist eine gute vertikale Mitte für Medium (Baseline)
        richText.drawString(display, scrollX, 38, message, "Medium");

        // Bewegen
        if (millis() - lastScrollTime >= scrollDelay) {
            scrollX--; 
            lastScrollTime = millis();
            
            // Reset wenn durchgelaufen
            if (scrollX < -totalWidth) {
                scrollX = M_WIDTH;
            }
        }
    }
    
    void setMessage(String msg) {
        message = msg;
        totalWidth = -1; // Trigger für Neuberechnung
        scrollX = M_WIDTH; 
    }
    
    void setSpeed(int delayMs) {
        scrollDelay = delayMs;
    }
};