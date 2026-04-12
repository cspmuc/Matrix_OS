#pragma once
#include "App.h"
#include "RichText.h" 
#include <time.h>
#include "ConfigManager.h"

extern ConfigManager configManager; 

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
    int currentJitterX = 0;

    unsigned long lastTimeCheck = 0;
    const unsigned long TIME_CHECK_INTERVAL = 1000; 
    unsigned long activeSince = 0; 

    String z1, z2, z3, lEsIst; 
    const String* activeLines[4];
    int activeLineCount = 0;

    // --- NEU: Speicher für den fallenden "alten" Text ---
    String oldLines[4];
    int oldLineCount = 0;
    int oldJitterX = 0;
    float oldAnimYOffset = 0;

    // --- Matrix Rain Variablen ---
    bool isAnimating = false;
    float animYOffset = 0;
    unsigned long lastAnimFrame = 0;

    struct RainDrop {
        float x;
        float y;
        float speed;
        int length;
    };
    RainDrop drops[20];

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

        // Die Uhrzeit hat sich in einen neuen Wort-Block geändert!
        if (mR != lastCalculated_mR) {
            
            if (!firstRun) {
                // 1. Den ALTEN Text in grellem Grün generieren und speichern
                updateClockText(lastCalculatedHour, lastCalculatedMinute, "{c:#88FF88}", "{c:#00AA00}");
                oldLineCount = activeLineCount;
                for(int i = 0; i < activeLineCount; i++) {
                    oldLines[i] = *(activeLines[i]);
                }
                oldJitterX = currentJitterX;
                
                // 2. Animation starten
                isAnimating = true;
                animYOffset = M_HEIGHT + 30.0f; // Neuer Text startet weit oben
                oldAnimYOffset = 0.0f;          // Alter Text startet in der Mitte
                
                // 3. Regentropfen initialisieren (langsamer und über volle Breite!)
                for(int i=0; i<20; i++) {
                    drops[i].x = random(M_WIDTH);
                    drops[i].y = random(M_HEIGHT) - M_HEIGHT; 
                    drops[i].speed = random(6, 18) / 10.0f; // Deutlich langsamer (0.6 bis 1.8)
                    drops[i].length = random(5, 15);         
                }
            }
            
            // 4. Status auf die NEUE Zeit aktualisieren
            lastCalculated_mR = mR;
            lastCalculatedMinute = m; 
            lastCalculatedHour = h;
            
            srand(h * 60 + mR);
            currentJitterX = (rand() % 5) - 2; 
            
            if (firstRun) {
                // Beim Neustart einfach statisch anzeigen
                updateClockText(h, m, "{c:#FFC800}", "{c:#AAAAAA}");
                firstRun = false;
            } else {
                // Den NEUEN Text in grellem Grün für die Animation vorbereiten
                updateClockText(h, m, "{c:#88FF88}", "{c:#00AA00}"); 
            }
            needsRedraw = true;
        } 
        else if (m != lastCalculatedMinute) {
            // Nur ein Minutenpunkt (ohne Wortwechsel) ändert sich
            lastCalculatedMinute = m;
            needsRedraw = true;
        }

        if (!needsRedraw && !isAnimating) return false; 
        
        display.clear();

        // ==========================================
        // 3. MATRIX RAIN ANIMATION (läuft bei ~30 FPS)
        // ==========================================
        if (isAnimating) {
            if (now - lastAnimFrame > 30) {
                lastAnimFrame = now;
                
                // A) Der NEUE Text fällt nach unten ein
                if (animYOffset > -50.0f) {
                    animYOffset -= 1.5f; 
                    if (animYOffset <= -50.0f) {
                        // Klick! Neuer Text ist eingerastet -> Zurück zu Gold/Silber
                        updateClockText(h, m, "{c:#FFC800}", "{c:#AAAAAA}"); 
                    }
                }

                // B) Der ALTE Text fällt nach unten aus dem Bild
                oldAnimYOffset += 1.5f;

                bool dropsRemaining = false;
                
                // C) Regentropfen fallen lassen
                for(int i=0; i<20; i++) {
                    drops[i].y += drops[i].speed;
                    
                    if (drops[i].y > M_HEIGHT + drops[i].length) {
                        // Oben neu spawnen, solange der Text noch nicht eingerastet ist
                        if (animYOffset > -50.0f) {
                            drops[i].y = random(10) - 20;
                            drops[i].x = random(M_WIDTH);
                            dropsRemaining = true;
                        }
                    } else {
                        dropsRemaining = true; 
                    }
                }

                // Komplett stoppen, wenn alles vorbei ist
                if (animYOffset <= -50.0f && !dropsRemaining && oldAnimYOffset > M_HEIGHT + 30.0f) { 
                    isAnimating = false;
                }
            }

            // Regentropfen zeichnen
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

        // ==========================================
        // 4. ZEIT RENDER (Punkte & Text)
        // ==========================================
        
        // Die 4 Minuten-Punkte (Grün während die Buchstaben fliegen)
        uint16_t dotCol = (isAnimating && animYOffset > -50.0f) ? display.color565(0, 255, 0) : display.color565(255, 200, 0);
        if (m % 5 >= 1) display.drawPixel(0, 0, dotCol);
        if (m % 5 >= 2) display.drawPixel(M_WIDTH - 1, 0, dotCol);
        if (m % 5 >= 3) display.drawPixel(M_WIDTH - 1, M_HEIGHT - 1, dotCol);
        if (m % 5 >= 4) display.drawPixel(0, M_HEIGHT - 1, dotCol);

        int lineHeight = richText.getLineHeight("Small");      
        int baselineOffset = richText.getBaselineOffset("Small"); 
        int spacing = 1; 
        
        // --- A) ALTEN TEXT ZEICHNEN (Fällt nach unten raus) ---
        if (isAnimating) {
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
                
                // Unterste Zeilen fallen zuerst (Delay-Effekt)
                float delay = (oldLineCount - 1 - i) * 15.0f;
                float lineOffset = max(0.0f, oldAnimYOffset - delay);
                int yPos = oldCurrentY + (int)lineOffset;

                // Nur zeichnen, wenn sich die Zeile auf dem Display befindet
                if (yPos > -10 && yPos < M_HEIGHT + 10) {
                    richText.drawString(display, x, yPos, text, "Small", display.color565(170, 170, 170));
                }
                oldCurrentY += lineHeight + spacing;
            }
        }

        // --- B) NEUEN TEXT ZEICHNEN (Fällt von oben rein) ---
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
            if (isAnimating) {
                // Oberste Zeilen fallen zuerst von oben rein
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