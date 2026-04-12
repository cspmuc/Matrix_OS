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

    // --- NEU: Matrix Rain Variablen ---
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

    // --- NEU: Farben sind jetzt dynamisch! ---
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

        // 1. Zeit synchronisieren
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
            richText.drawCentered(display, 36, "{c:red}NTP not synced", "Small");
            lastCalculatedMinute = -2; 
            return true;
        }

        int h = lastKnownTime.tm_hour;
        int m = lastKnownTime.tm_min;
        int mR = (m / 5) * 5; // Der 5-Minuten Block

        // 2. Prüfen, ob sich die "Worte" geändert haben
        if (mR != lastCalculated_mR) {
            lastCalculated_mR = mR;
            lastCalculatedMinute = m; 
            
            // Zufälligen horizontalen Jitter für diese Uhrzeit berechnen
            srand(h * 60 + mR);
            currentJitterX = (rand() % 5) - 2; 
            
            if (firstRun) {
                // Beim Systemstart direkt normal anzeigen (Keine Animation)
                updateClockText(h, m, "{c:#FFC800}", "{c:#AAAAAA}");
                firstRun = false;
            } else {
                // ANIMATION STARTEN!
                isAnimating = true;
                animYOffset = 90.0f; // Startet weit oben außerhalb des Bildschirms
                
                for(int i=0; i<20; i++) {
                    drops[i].x = random(64);
                    drops[i].y = random(64) - 64; 
                    drops[i].speed = random(15, 35) / 10.0f; // Fallgeschwindigkeit
                    drops[i].length = random(5, 15);         // Schweif-Länge
                }
                // Zeige den Text im grellen Matrix-Grün an!
                updateClockText(h, m, "{c:#88FF88}", "{c:#00AA00}"); 
            }
            needsRedraw = true;
        } 
        else if (m != lastCalculatedMinute) {
            // Nur die Minuten-Punkte haben sich geändert
            lastCalculatedMinute = m;
            needsRedraw = true;
        }

        // Wenn nichts animiert und nichts geupdatet wurde -> Abbrechen um CPU zu sparen
        if (!needsRedraw && !isAnimating) return false; 
        
        display.clear();

        // ==========================================
        // 3. MATRIX RAIN ANIMATION (läuft bei ~30 FPS)
        // ==========================================
        if (isAnimating) {
            if (now - lastAnimFrame > 30) {
                lastAnimFrame = now;
                animYOffset -= 2.0f; // Text fällt nach unten
                
                // Regentropfen fallen lassen
                for(int i=0; i<20; i++) {
                    drops[i].y += drops[i].speed;
                    // Wenn Tropfen unten raus ist und die Text-Animation noch läuft -> oben neu starten
                    if (drops[i].y > 64 + drops[i].length && animYOffset > -20) {
                        drops[i].y = random(10) - 20;
                        drops[i].x = random(64);
                    }
                }

                // Wenn der Text komplett eingefahren ist und der Regen durchfällt
                if (animYOffset <= -50.0f) { 
                    isAnimating = false;
                    // Zurück zu Gold und Silber schalten!
                    updateClockText(h, m, "{c:#FFC800}", "{c:#AAAAAA}"); 
                }
            }

            // Regentropfen zeichnen
            for(int i=0; i<20; i++) {
                int dx = drops[i].x;
                int dy = (int)drops[i].y;
                display.drawPixel(dx, dy, display.color565(150, 255, 150)); // Tropfenkopf (hellgrün)
                
                // Schweif zeichnen und ausfaden lassen
                for(int l=1; l<drops[i].length; l++) {
                    int tailY = dy - l;
                    if (tailY >= 0 && tailY < 64) {
                        int fade = 255 - (l * 255 / drops[i].length);
                        display.drawPixel(dx, tailY, display.color565(0, fade/2, 0)); // Dunkelgrüner Schweif
                    }
                }
            }
            needsRedraw = true; // Zwingt das System, das nächste Frame direkt zu zeichnen
        }

        // ==========================================
        // 4. ZEIT RENDER (Punkte & Text)
        // ==========================================
        
        // Minuten Punkte (werden während der Animation passend grün!)
        uint16_t dotCol = isAnimating ? display.color565(0, 255, 0) : display.color565(255, 200, 0);
        if (m % 5 >= 1) display.drawPixel(0, 0, dotCol);
        if (m % 5 >= 2) display.drawPixel(M_WIDTH - 1, 0, dotCol);
        if (m % 5 >= 3) display.drawPixel(M_WIDTH - 1, M_HEIGHT - 1, dotCol);
        if (m % 5 >= 4) display.drawPixel(0, M_HEIGHT - 1, dotCol);

        // Text Layout berechnen
        int lineHeight = richText.getLineHeight("Small");      
        int baselineOffset = richText.getBaselineOffset("Small"); 
        int spacing = 1; 
        int totalTextHeight = (activeLineCount * lineHeight);
        if (activeLineCount > 1) totalTextHeight += (activeLineCount - 1) * spacing;
        
        int topY = (M_HEIGHT - (totalTextHeight - (lineHeight - baselineOffset))) / 2;
        int currentY = topY + baselineOffset;
        
        for (int i = 0; i < activeLineCount; i++) {
            const String& text = *(activeLines[i]);
            int w = richText.getTextWidth(display, text, "Small");
            int x = (M_WIDTH - w) / 2;
            
            // Jitter & Treppen-Design
            if (i == 0 && activeLineCount > 1) x -= 20; 
            else if (i == activeLineCount - 1 && activeLineCount > 1) x += 20; 
            x += currentJitterX;
            
            // Fall-Animation berechnen (Zeilen fallen leicht versetzt!)
            int yPos = currentY;
            if (isAnimating) {
                float lineOffset = max(0.0f, animYOffset - (i * 15.0f));
                yPos -= (int)lineOffset;
            }

            // Nur zeichnen, wenn sich die Zeile auf dem Display befindet
            if (yPos > -10 && yPos < 74) {
                // Die Farbe wird automatisch durch den String (cHigh/cDim) überschrieben
                richText.drawString(display, x, yPos, text, "Small", display.color565(170, 170, 170));
            }
            currentY += lineHeight + spacing;
        }

        return true; 
    }
};