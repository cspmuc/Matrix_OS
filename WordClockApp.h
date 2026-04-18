#pragma once
#include "App.h"
#include "RichText.h" 
#include <time.h>
#include <stdio.h>
#include "ConfigManager.h"

extern ConfigManager configManager; 

enum AnimState {
    IDLE,
    FADE_TO_GREEN,
    MATRIX_RAIN,
    FADE_TO_GOLD
};

class WordClockApp : public App {
private:
    RichText richText;
    const char* stundenNamen[13] = {"zwölf", "eins", "zwei", "drei", "vier", "fünf", "sechs", "sieben", "acht", "neun", "zehn", "elf", "zwölf"};
    
    struct tm lastKnownTime;
    bool hasValidTime = false;
    
    int lastCalculatedHour = -1;
    int lastCalculatedMinute = -1; 
    int lastCalculated_mR = -1;
    bool firstRun = true;
    bool justActivated = false; // <--- NEU: Verhindert den Animations-Rückstand
    int currentJitterX = 0;

    unsigned long lastTimeCheck = 0;
    const unsigned long TIME_CHECK_INTERVAL = 1000; 
    unsigned long activeSince = 0; 

    String z1, z2, z3, lEsIst; 
    const String* activeLines[4];
    int activeLineCount = 0;

    String oldLines[4];
    int oldLineCount = 0;
    int oldJitterX = 0;
    float oldAnimYOffset = 0;

    AnimState animState = IDLE;
    unsigned long animStateStartTime = 0;
    float animYOffset = 0;
    unsigned long lastAnimFrame = 0;
    
    int targetHour = -1;
    int targetMinute = -1;
    int target_mR = -1;

    struct RainDrop {
        float x;
        float y;
        float speed;
        int length;
    };
    RainDrop drops[20];

    String getBlendedColor(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2, float ratio) {
        ratio = max(0.0f, min(1.0f, ratio));
        uint8_t r = r1 + (r2 - r1) * ratio;
        uint8_t g = g1 + (g2 - g1) * ratio;
        uint8_t b = b1 + (b2 - b1) * ratio;
        char hex[10];
        snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
        return String(hex);
    }

    void updateClockText(int h, int m, String cHigh, String cDim) {
        String tagBold = "{b}";
        String sUhr = cDim + "Uhr";
        String sVor = cDim + "vor";
        String sNach = cDim + "nach";
        String sHalb = cDim + "halb";
        lEsIst = tagBold + cDim + "Es ist"; 

        int mR = (m / 5) * 5; 
        int s = h % 12;
        int nextS = (s + 1) % 12;
        String s_curr = stundenNamen[(s==0)?12:s];
        String s_next = stundenNamen[(nextS==0)?12:nextS];

        z1 = ""; z2 = ""; z3 = "";

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
        activeLines[activeLineCount++] = &lEsIst;
        if (z1 != "") activeLines[activeLineCount++] = &z1;
        if (z2 != "") activeLines[activeLineCount++] = &z2;
        if (z3 != "") activeLines[activeLineCount++] = &z3;
    }

public:
    void onActive() override {
        activeSince = millis(); 
        justActivated = true; // <--- NEU: Setzt den Trigger, dass wir frisch aufgewacht sind
    }

    bool isReadyToSwitch(float durationMultiplier = 1.0) override {
        unsigned long durationMs = (configManager.autoMode.wordclock_duration_sec * 1000) * durationMultiplier;
        return (millis() - activeSince >= durationMs);
    }

    bool draw(DisplayManager& display, bool force) override {
        unsigned long now = millis();
        bool needsRedraw = force;

        if (now - lastTimeCheck > TIME_CHECK_INTERVAL) {
            lastTimeCheck = now;
            time_t nowTime = time(nullptr);
            if (nowTime > 1600000000) { 
                localtime_r(&nowTime, &lastKnownTime);
                hasValidTime = true;
            }
        }
        
        if (!hasValidTime) {
            if (!force && lastCalculatedMinute == -2) return false;
            display.clear();
            richText.drawCentered(display, M_HEIGHT / 2 + 4, "{c:red}NTP not synced", "Small");
            lastCalculatedMinute = -2; 
            return true;
        }

        int h = lastKnownTime.tm_hour;
        int m = lastKnownTime.tm_min;
        int mR = (m / 5) * 5; 

        // --- NEU: Geräuschloses Aufholen der Zeit beim App-Wechsel ---
        if (justActivated) {
            justActivated = false;
            if (mR != lastCalculated_mR || firstRun) {
                updateClockText(h, m, "{c:#FFC800}", "{c:#AAAAAA}");
                lastCalculatedHour = h;
                lastCalculatedMinute = m;
                lastCalculated_mR = mR;
                srand(h * 60 + mR);
                currentJitterX = (rand() % 5) - 2; 
                firstRun = false;
            } else {
                lastCalculatedMinute = m;
            }
            animState = IDLE;
            needsRedraw = true;
        }
        // --- Trigger: Die Uhrzeit ändert sich, WÄHREND die App aktiv ist ---
        else if (mR != lastCalculated_mR && animState == IDLE) {
            animState = FADE_TO_GREEN;
            animStateStartTime = now;
            targetHour = h;
            targetMinute = m;
            target_mR = mR;
            needsRedraw = true;
        } 
        else if (m != lastCalculatedMinute && animState == IDLE) {
            lastCalculatedMinute = m;
            needsRedraw = true;
        }

        if (!needsRedraw && animState == IDLE) return false; 
        
        display.clear();

        uint16_t currentDotCol = display.color565(255, 200, 0); 

        if (animState == FADE_TO_GREEN) {
            float ratio = (now - animStateStartTime) / 1000.0f; 
            
            if (ratio >= 1.0f) {
                animState = MATRIX_RAIN;
                
                updateClockText(lastCalculatedHour, lastCalculatedMinute, "{c:#88FF88}", "{c:#00AA00}");
                oldLineCount = activeLineCount;
                for(int i = 0; i < activeLineCount; i++) oldLines[i] = *(activeLines[i]);
                oldJitterX = currentJitterX;
                
                lastCalculatedHour = targetHour;
                lastCalculatedMinute = targetMinute;
                lastCalculated_mR = target_mR;
                
                srand(targetHour * 60 + target_mR);
                currentJitterX = (rand() % 5) - 2; 
                
                animYOffset = M_HEIGHT + 30.0f; 
                oldAnimYOffset = 0.0f;          
                for(int i=0; i<20; i++) {
                    drops[i].x = random(M_WIDTH);
                    drops[i].y = random(M_HEIGHT) - M_HEIGHT; 
                    drops[i].speed = random(6, 18) / 10.0f; 
                    drops[i].length = random(5, 15);         
                }
                
                updateClockText(lastCalculatedHour, lastCalculatedMinute, "{c:#88FF88}", "{c:#00AA00}"); 
            } else {
                String cHigh = getBlendedColor(255, 200, 0, 136, 255, 136, ratio); 
                String cDim  = getBlendedColor(170, 170, 170, 0, 170, 0, ratio);   
                updateClockText(lastCalculatedHour, lastCalculatedMinute, "{c:" + cHigh + "}", "{c:" + cDim + "}");
                
                uint8_t r = 255 + (0 - 255) * ratio;
                uint8_t g = 200 + (255 - 200) * ratio;
                currentDotCol = display.color565(r, g, 0);
            }
            needsRedraw = true;
        }
        else if (animState == MATRIX_RAIN) {
            currentDotCol = display.color565(0, 255, 0);

            if (now - lastAnimFrame > 30) {
                lastAnimFrame = now;
                
                if (animYOffset > -50.0f) {
                    animYOffset -= 1.5f; 
                    if (animYOffset < -50.0f) animYOffset = -50.0f; 
                }

                oldAnimYOffset += 1.5f;

                bool dropsRemaining = false;
                for(int i=0; i<20; i++) {
                    drops[i].y += drops[i].speed;
                    
                    if (drops[i].y > M_HEIGHT + drops[i].length) {
                        if (animYOffset > -50.0f) {
                            drops[i].y = random(10) - 20;
                            drops[i].x = random(M_WIDTH);
                            dropsRemaining = true;
                        }
                    } else {
                        dropsRemaining = true; 
                    }
                }

                if (animYOffset <= -50.0f && !dropsRemaining && oldAnimYOffset > M_HEIGHT + 30.0f) { 
                    animState = FADE_TO_GOLD;
                    animStateStartTime = now;
                }
            }

            for(int i=0; i<20; i++) {
                int dx = drops[i].x;
                int dy = (int)drops[i].y;
                display.drawPixel(dx, dy, display.color565(150, 255, 150)); 
                
                for(int l=1; l<drops[i].length; l++) {
                    int tailY = dy - l;
                    if (tailY >= 0 && tailY < M_HEIGHT) {
                        int fade = 255 - (l * 255 / drops[i].length);
                        display.drawPixel(dx, tailY, display.color565(0, fade/2, 0)); 
                    }
                }
            }
            needsRedraw = true; 
        }
        else if (animState == FADE_TO_GOLD) {
            float ratio = (now - animStateStartTime) / 1000.0f; 
            
            if (ratio >= 1.0f) {
                animState = IDLE;
                updateClockText(lastCalculatedHour, lastCalculatedMinute, "{c:#FFC800}", "{c:#AAAAAA}");
                currentDotCol = display.color565(255, 200, 0);
            } else {
                String cHigh = getBlendedColor(136, 255, 136, 255, 200, 0, ratio); 
                String cDim  = getBlendedColor(0, 170, 0, 170, 170, 170, ratio);   
                updateClockText(lastCalculatedHour, lastCalculatedMinute, "{c:" + cHigh + "}", "{c:" + cDim + "}");
                
                uint8_t r = 0 + (255 - 0) * ratio;
                uint8_t g = 255 + (200 - 255) * ratio;
                currentDotCol = display.color565(r, g, 0);
            }
            needsRedraw = true;
        }

        int displayMinute = lastCalculatedMinute;
        if (displayMinute % 5 >= 1) display.drawPixel(0, 0, currentDotCol);
        if (displayMinute % 5 >= 2) display.drawPixel(M_WIDTH - 1, 0, currentDotCol);
        if (displayMinute % 5 >= 3) display.drawPixel(M_WIDTH - 1, M_HEIGHT - 1, currentDotCol);
        if (displayMinute % 5 >= 4) display.drawPixel(0, M_HEIGHT - 1, currentDotCol);

        int lineHeight = richText.getLineHeight("Small");      
        int baselineOffset = richText.getBaselineOffset("Small"); 
        int spacing = 1; 
        
        if (animState == MATRIX_RAIN) {
            int oldTotalHeight = (oldLineCount * lineHeight);
            if (oldLineCount > 1) oldTotalHeight += (oldLineCount - 1) * spacing;
            int oldTopY = (M_HEIGHT - (oldTotalHeight - (lineHeight - baselineOffset))) / 2;
            int oldCurrentY = oldTopY + baselineOffset;
            
            for (int i = 0; i < oldLineCount; i++) {
                const String& text = oldLines[i];
                int w = richText.getTextWidth(display, text, "Small");
                int x = (M_WIDTH - w) / 2;
                
                if (i == 0 && oldLineCount > 1) x -= 20; 
                else if (i == oldLineCount - 1 && oldLineCount > 1) x += 20; 
                x += oldJitterX;
                
                float delay = (oldLineCount - 1 - i) * 15.0f;
                float lineOffset = max(0.0f, oldAnimYOffset - delay);
                int yPos = oldCurrentY + (int)lineOffset;

                if (yPos > -10 && yPos < M_HEIGHT + 10) {
                    richText.drawString(display, x, yPos, text, "Small", display.color565(170, 170, 170));
                }
                oldCurrentY += lineHeight + spacing;
            }
        }

        int totalTextHeight = (activeLineCount * lineHeight);
        if (activeLineCount > 1) totalTextHeight += (activeLineCount - 1) * spacing;
        int topY = (M_HEIGHT - (totalTextHeight - (lineHeight - baselineOffset))) / 2;
        int currentY = topY + baselineOffset;
        
        for (int i = 0; i < activeLineCount; i++) {
            const String& text = *(activeLines[i]);
            int w = richText.getTextWidth(display, text, "Small");
            int x = (M_WIDTH - w) / 2;
            
            if (i == 0 && activeLineCount > 1) x -= 20; 
            else if (i == activeLineCount - 1 && activeLineCount > 1) x += 20; 
            x += currentJitterX;
            
            int yPos = currentY;
            if (animState == MATRIX_RAIN) {
                float lineOffset = max(0.0f, animYOffset - (i * 15.0f));
                yPos -= (int)lineOffset;
            }

            if (yPos > -10 && yPos < M_HEIGHT + 10) {
                richText.drawString(display, x, yPos, text, "Small", display.color565(170, 170, 170));
            }
            currentY += lineHeight + spacing;
        }

        return true; 
    }
};