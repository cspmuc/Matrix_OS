#pragma once
#include "App.h"
#include "RichText.h" 
#include <time.h>
#include <vector>

class WordClockApp : public App {
private:
    RichText richText;
    const char* stundenNamen[13] = {"zwölf", "eins", "zwei", "drei", "vier", "fünf", "sechs", "sieben", "acht", "neun", "zehn", "elf", "zwölf"};
    
    // Speicher für die letzte gültige Zeit (gegen Flackern)
    struct tm lastKnownTime;
    bool hasValidTime = false;

    // NEU: Timer für Performance-Optimierung
    // Wir fragen die Systemzeit nur alle 200ms ab, statt 60x pro Sekunde.
    unsigned long lastTimeCheck = 0;
    const unsigned long TIME_CHECK_INTERVAL = 200; 

public:
    void draw(DisplayManager& display) override {
        unsigned long now = millis();

        // 1. PERFORMANCE BREMSE:
        // Nur alle 200ms die echte Systemzeit holen.
        // Das entlastet Core 0/1 Synchronisation massiv.
        if (now - lastTimeCheck > TIME_CHECK_INTERVAL) {
            lastTimeCheck = now;
            
            struct tm ti;
            // Timeout 0 ist wichtig, damit wir nicht blockieren
            if(getLocalTime(&ti, 0)) {
                lastKnownTime = ti;
                hasValidTime = true;
            } 
        }
        
        // CHECK: Haben wir überhaupt eine Zeit?
        if (!hasValidTime) {
            // Nur wenn wir noch NIE eine Zeit hatten, zeigen wir den Fehler
            richText.drawCentered(display, 36, "{c:red}NTP not synced", "Small");
            return;
        }

        // Ab hier nutzen wir die gespeicherte Zeit (stabilisiert)
        int h = lastKnownTime.tm_hour;
        int m = lastKnownTime.tm_min;
        int mR = (m / 5) * 5; 
        int s = h % 12;
        int nextS = (s + 1) % 12;

        // --- EDLE ECKEN (Minuten) ---
        uint16_t dotCol = COL_GOLD; 
        if (m % 5 >= 1) display.drawPixel(0, 0, dotCol);
        if (m % 5 >= 2) display.drawPixel(M_WIDTH - 1, 0, dotCol);
        if (m % 5 >= 3) display.drawPixel(M_WIDTH - 1, M_HEIGHT - 1, dotCol);
        if (m % 5 >= 4) display.drawPixel(0, M_HEIGHT - 1, dotCol);

        // --- TEXT LOGIK ---
        String cDim = "{c:silver}"; 
        String cHigh = "{c:gold}";  
        
        String z0 = cDim + "Es ist", z1 = "", z2 = "", z3 = "";
        
        String s_curr = stundenNamen[(s==0)?12:s];
        String s_next = stundenNamen[(nextS==0)?12:nextS];

        if (mR == 0) { 
            String std = (s == 1) ? "ein" : s_curr;
            z1 = ""; z2 = cHigh + std; z3 = cDim + "Uhr"; 
        }
        else if (mR == 5)  { z1 = cHigh + "fünf";      z2 = cDim + "nach"; z3 = cHigh + s_curr; }
        else if (mR == 10) { z1 = cHigh + "zehn";      z2 = cDim + "nach"; z3 = cHigh + s_curr; }
        else if (mR == 15) { z1 = cHigh + "viertel";   z2 = cDim + "nach"; z3 = cHigh + s_curr; }
        else if (mR == 20) { z1 = cHigh + "zwanzig";   z2 = cDim + "nach"; z3 = cHigh + s_curr; }
        else if (mR == 25) { z1 = cHigh + "fünf vor";  z2 = cDim + "halb"; z3 = cHigh + s_next; }
        else if (mR == 30) { z1 = cDim  + "halb";      z2 = cHigh + s_next; } 
        else if (mR == 35) { z1 = cHigh + "fünf nach"; z2 = cDim + "halb"; z3 = cHigh + s_next; }
        else if (mR == 40) { z1 = cHigh + "zwanzig";   z2 = cDim + "vor";  z3 = cHigh + s_next; } 
        else if (mR == 45) { z1 = cHigh + "dreiviertel"; z2 = cHigh + s_next; z3 = ""; } 
        else if (mR == 50) { z1 = cHigh + "zehn";      z2 = cDim + "vor";  z3 = cHigh + s_next; }
        else if (mR == 55) { z1 = cHigh + "fünf";      z2 = cDim + "vor";  z3 = cHigh + s_next; }

        std::vector<String> lines;
        lines.push_back(z0);
        if (z1 != "") lines.push_back(z1);
        if (z2 != "") lines.push_back(z2);
        if (z3 != "") lines.push_back(z3);

        int lineHeight = richText.getLineHeight("Small");      
        int baselineOffset = richText.getBaselineOffset("Small"); 
        int spacing = 1; 

        int totalTextHeight = (lines.size() * lineHeight);
        if (lines.size() > 1) totalTextHeight += (lines.size() - 1) * spacing;
        
        int descenderHeight = lineHeight - baselineOffset;
        int visualTextHeight = totalTextHeight - descenderHeight;

        int topY = (M_HEIGHT - visualTextHeight) / 2;
        int currentY = topY + baselineOffset;

        srand(h * 60 + mR);
        int jX = (rand() % 5) - 2; 

        uint16_t fallbackCol = COL_SILVER; 

        for (size_t i = 0; i < lines.size(); i++) {
            String text = "{b}" + lines[i];
            int w = richText.getTextWidth(display, text, "Small");
            int x = 0;
            if (i == 0) x = (M_WIDTH - w) / 2 - 20; 
            else if (i == lines.size() - 1) x = (M_WIDTH - w) / 2 + 20; 
            else x = (M_WIDTH - w) / 2;      
            x += jX;
            richText.drawString(display, x, currentY, text, "Small", fallbackCol);
            currentY += lineHeight + spacing;
        }
    }
};