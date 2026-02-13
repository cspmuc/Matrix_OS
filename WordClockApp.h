#pragma once
#include "App.h"
#include "RichText.h" 
#include <time.h>

class WordClockApp : public App {
private:
    RichText richText;
    const char* stundenNamen[13] = {"zwölf", "eins", "zwei", "drei", "vier", "fünf", "sechs", "sieben", "acht", "neun", "zehn", "elf", "zwölf"};
    
    struct tm lastKnownTime;
    bool hasValidTime = false;
    int lastCalculatedMinute = -1; 

    unsigned long lastTimeCheck = 0;
    const unsigned long TIME_CHECK_INTERVAL = 1000; 

    String z1, z2, z3; 
    const String* activeLines[4];
    int activeLineCount = 0;
    
    const String sUhr = "{c:silver}Uhr";
    const String sVor = "{c:silver}vor";
    const String sNach = "{c:silver}nach";
    const String sHalb = "{c:silver}halb";
    const String sEsIst = "{b}{c:silver}Es ist"; 
    const String cDim = "{c:silver}"; 
    const String cHigh = "{c:gold}";  
    const String tagBold = "{b}"; 

    void updateClockText(int h, int m) {
        int mR = (m / 5) * 5; 
        int s = h % 12;
        int nextS = (s + 1) % 12;
        String s_curr = stundenNamen[(s==0)?12:s];
        String s_next = stundenNamen[(nextS==0)?12:nextS];

        if (mR == 0) { 
            String std = (s == 1) ? "ein" : s_curr;
            z1 = ""; z2 = tagBold + cHigh + std; z3 = tagBold + sUhr; 
        }
        else if (mR == 5)  { z1 = tagBold + cHigh + "fünf";      z2 = tagBold + sNach; z3 = tagBold + cHigh + s_curr; }
        else if (mR == 10) { z1 = tagBold + cHigh + "zehn";      z2 = tagBold + sNach; z3 = tagBold + cHigh + s_curr; }
        else if (mR == 15) { z1 = tagBold + cHigh + "viertel";   z2 = tagBold + sNach; z3 = tagBold + cHigh + s_curr; }
        else if (mR == 20) { z1 = tagBold + cHigh + "zwanzig";   z2 = tagBold + sNach; z3 = tagBold + cHigh + s_curr; }
        else if (mR == 25) { z1 = tagBold + cHigh + "fünf vor";  z2 = tagBold + sHalb; z3 = tagBold + cHigh + s_next; }
        else if (mR == 30) { z1 = tagBold + sHalb;               z2 = tagBold + cHigh + s_next; z3 = ""; } 
        else if (mR == 35) { z1 = tagBold + cHigh + "fünf nach"; z2 = tagBold + sHalb; z3 = tagBold + cHigh + s_next; }
        else if (mR == 40) { z1 = tagBold + cHigh + "zwanzig";   z2 = tagBold + sVor;  z3 = tagBold + cHigh + s_next; } 
        else if (mR == 45) { z1 = tagBold + cHigh + "dreiviertel"; z2 = tagBold + cHigh + s_next; z3 = ""; } 
        else if (mR == 50) { z1 = tagBold + cHigh + "zehn";      z2 = tagBold + sVor;  z3 = tagBold + cHigh + s_next; }
        else if (mR == 55) { z1 = tagBold + cHigh + "fünf";      z2 = tagBold + sVor;  z3 = tagBold + cHigh + s_next; }
        
        activeLineCount = 0;
        activeLines[activeLineCount++] = &sEsIst;
        if (z1 != "") activeLines[activeLineCount++] = &z1;
        if (z2 != "") activeLines[activeLineCount++] = &z2;
        if (z3 != "") activeLines[activeLineCount++] = &z3;
    }

public:
    bool draw(DisplayManager& display, bool force) override {
        unsigned long now = millis();

        if (now - lastTimeCheck > TIME_CHECK_INTERVAL) {
            lastTimeCheck = now;
            // FIX: Verwende time(nullptr) statt getLocalTime
            time_t nowTime = time(nullptr);
            // Plausibilitätscheck: Ist die Zeit > 2020?
            if (nowTime > 1600000000) { 
                localtime_r(&nowTime, &lastKnownTime);
                hasValidTime = true;
            }
        }
        
        if (!hasValidTime) {
            if (!force && lastCalculatedMinute == -2) return false;
            display.clear();
            richText.drawCentered(display, 36, "{c:red}NTP not synced", "Small");
            lastCalculatedMinute = -2; 
            return true;
        }

        int h = lastKnownTime.tm_hour;
        int m = lastKnownTime.tm_min;

        if (!force && m == lastCalculatedMinute) return false; 

        display.clear();
        if (m != lastCalculatedMinute) { updateClockText(h, m); lastCalculatedMinute = m; }

        uint16_t dotCol = COL_GOLD; 
        if (m % 5 >= 1) display.drawPixel(0, 0, dotCol);
        if (m % 5 >= 2) display.drawPixel(M_WIDTH - 1, 0, dotCol);
        if (m % 5 >= 3) display.drawPixel(M_WIDTH - 1, M_HEIGHT - 1, dotCol);
        if (m % 5 >= 4) display.drawPixel(0, M_HEIGHT - 1, dotCol);

        int lineHeight = richText.getLineHeight("Small");      
        int baselineOffset = richText.getBaselineOffset("Small"); 
        int spacing = 1; 
        int totalTextHeight = (activeLineCount * lineHeight);
        if (activeLineCount > 1) totalTextHeight += (activeLineCount - 1) * spacing;
        int topY = (M_HEIGHT - (totalTextHeight - (lineHeight - baselineOffset))) / 2;
        int currentY = topY + baselineOffset;
        int mR = (m / 5) * 5;
        srand(h * 60 + mR);
        int jX = (rand() % 5) - 2; 

        for (int i = 0; i < activeLineCount; i++) {
            const String& text = *(activeLines[i]);
            int w = richText.getTextWidth(display, text, "Small");
            int x = (M_WIDTH - w) / 2;
            if (i == 0 && activeLineCount > 1) x -= 20; 
            else if (i == activeLineCount - 1 && activeLineCount > 1) x += 20; 
            x += jX;
            richText.drawString(display, x, currentY, text, "Small", COL_SILVER);
            currentY += lineHeight + spacing;
        }
        return true; 
    }
};