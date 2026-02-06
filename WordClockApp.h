#pragma once
#include "App.h"
#include "RichText.h" 
#include <time.h>
#include <vector>

class WordClockApp : public App {
private:
    RichText richText;
    // Rechtschreibung: Zahlen im Satz klein!
    const char* stundenNamen[13] = {"zwölf", "eins", "zwei", "drei", "vier", "fünf", "sechs", "sieben", "acht", "neun", "zehn", "elf", "zwölf"};

public:
    void draw(DisplayManager& display) override {
        struct tm ti;
        if(!getLocalTime(&ti)) return;
        int h = ti.tm_hour, m = ti.tm_min, mR = (m / 5) * 5; 
        int s = h % 12, nextS = (s + 1) % 12;

        // --- EDLE ECKEN (Minuten) ---
        uint16_t dotCol = COL_GOLD; 
        if (m % 5 >= 1) display.drawPixel(0, 0, dotCol);
        if (m % 5 >= 2) display.drawPixel(M_WIDTH - 1, 0, dotCol);
        if (m % 5 >= 3) display.drawPixel(M_WIDTH - 1, M_HEIGHT - 1, dotCol);
        if (m % 5 >= 4) display.drawPixel(0, M_HEIGHT - 1, dotCol);

        // --- TEXT LOGIK (Two-Tone Design & Korrekte Rechtschreibung) ---
        String cDim = "{c:silver}"; // Füllwörter (silber)
        String cHigh = "{c:gold}";  // Zeitangaben (gold)
        
        // Satzanfang Groß
        String z0 = cDim + "Es ist", z1 = "", z2 = "", z3 = "";
        
        String s_curr = stundenNamen[(s==0)?12:s];
        String s_next = stundenNamen[(nextS==0)?12:nextS];

        if (mR == 0) { 
            // "Es ist ein Uhr" (ein = klein, Uhr = groß)
            String std = (s == 1) ? "ein" : s_curr;
            
            z1 = ""; 
            z2 = cHigh + std; 
            z3 = cDim + "Uhr"; 
        }
        else if (mR == 5)  { z1 = cHigh + "fünf";      z2 = cDim + "nach"; z3 = cHigh + s_curr; }
        else if (mR == 10) { z1 = cHigh + "zehn";      z2 = cDim + "nach"; z3 = cHigh + s_curr; }
        else if (mR == 15) { z1 = cHigh + "viertel";   z2 = cDim + "nach"; z3 = cHigh + s_curr; }
        else if (mR == 20) { z1 = cHigh + "zwanzig";   z2 = cDim + "nach"; z3 = cHigh + s_curr; }
        else if (mR == 25) { z1 = cHigh + "fünf vor";  z2 = cDim + "halb"; z3 = cHigh + s_next; }
        else if (mR == 30) { z1 = cDim  + "halb";      z2 = cHigh + s_next; } 
        else if (mR == 35) { z1 = cHigh + "fünf nach"; z2 = cDim + "halb"; z3 = cHigh + s_next; }
        else if (mR == 40) { z1 = cHigh + "zwanzig";   z2 = cDim + "vor";  z3 = cHigh + s_next; } 
        
        // "dreiviertel" (ein Wort, klein)
        else if (mR == 45) { z1 = cHigh + "dreiviertel"; z2 = cHigh + s_next; z3 = ""; } 
        
        else if (mR == 50) { z1 = cHigh + "zehn";      z2 = cDim + "vor";  z3 = cHigh + s_next; }
        else if (mR == 55) { z1 = cHigh + "fünf";      z2 = cDim + "vor";  z3 = cHigh + s_next; }

        // Zeilen sammeln
        std::vector<String> lines;
        lines.push_back(z0);
        if (z1 != "") lines.push_back(z1);
        if (z2 != "") lines.push_back(z2);
        if (z3 != "") lines.push_back(z3);

        // --- BERECHNUNG ---
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
            // WICHTIG: Fett ({b}) wirkt auch bei Kleinbuchstaben super lesbar
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