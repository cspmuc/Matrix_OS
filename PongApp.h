#pragma once
#include "App.h"
#include "RichText.h"
#include "config.h" 
#include "qrcode.h" 

class PongApp : public App {
private:
    RichText richText;
    enum GameState { WAIT_PLAYERS, READY, COUNTDOWN, PLAYING, SCORED, GAME_OVER };
    GameState state = WAIT_PLAYERS;

    float p1_y, p2_y;
    float bx, by, bvx, bvy;
    float b_spin = 0.0f; // Der physikalische Spin

    int score1 = 0, score2 = 0;
    const int padH = 16; 
    unsigned long lastFrame = 0;
    
    unsigned long stateTimer = 0;
    int countdownValue = 3;
    bool serveToP1 = true; 

    static bool qrMatrix[35][35];
    static int qrSize;
    bool needsRedraw = true; 

    static void qrCallback(esp_qrcode_handle_t qrcode) {
        qrSize = esp_qrcode_get_size(qrcode);
        if (qrSize > 35) qrSize = 35; 
        for (int y = 0; y < qrSize; y++) {
            for (int x = 0; x < qrSize; x++) {
                qrMatrix[y][x] = esp_qrcode_get_module(qrcode, x, y);
            }
        }
    }

    void resetBall(bool toP1) {
        bx = M_WIDTH / 2; 
        by = M_HEIGHT / 2;
        bvx = toP1 ? -1.0f : 1.0f; 
        bvy = (random(-10, 11) / 10.0f);
        
        // Verhindert zu flache, horizontale Bälle
        if (abs(bvy) < 0.3f) {
            bvy = (bvy < 0) ? -0.3f : 0.3f;
        }
        
        b_spin = 0.0f; 
    }

public:
    void onActive() override {
        p1_y = M_HEIGHT / 2 - padH / 2;
        p2_y = M_HEIGHT / 2 - padH / 2;
        score1 = 0; score2 = 0;
        pong_p1_ready = false; pong_p2_ready = false;
        pong_p1_dir = 0; pong_p2_dir = 0;
        pong_start_trigger = false;
        serveToP1 = true;
        
        state = WAIT_PLAYERS;
        needsRedraw = true;

        esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
        cfg.display_func = qrCallback;
        esp_qrcode_generate(&cfg, "http://matrixos.local/pong");
    }

    bool isReadyToSwitch(float durationMultiplier = 1.0) override {
        return false; 
    }

    bool draw(DisplayManager& display, bool force) override {
        unsigned long now = millis();
        GameState oldState = state;

        // ==========================================
        // 1. SPIEL-LOGIK & TIMER 
        // ==========================================
        if (state == WAIT_PLAYERS && (pong_p1_ready || pong_p2_ready)) state = READY;
        if (state > WAIT_PLAYERS && !pong_p1_ready && !pong_p2_ready) state = WAIT_PLAYERS;

        if (state >= READY) {
            float old_p1 = p1_y;
            float old_p2 = p2_y;
            p1_y = constrain(p1_y + (pong_p1_dir * 1.5f), 0, M_HEIGHT - padH);
            p2_y = constrain(p2_y + (pong_p2_dir * 1.5f), 0, M_HEIGHT - padH);
            if (old_p1 != p1_y || old_p2 != p2_y) needsRedraw = true;
        }

        if (state == READY && pong_start_trigger) {
            pong_start_trigger = false; 
            score1 = 0; score2 = 0; 
            state = COUNTDOWN; 
            countdownValue = 3;
            stateTimer = now;
            needsRedraw = true;
        } 
        else if (state == COUNTDOWN) {
            if (now - stateTimer >= 1000) {
                stateTimer = now;
                countdownValue--;
                needsRedraw = true;
                if (countdownValue <= 0) {
                    state = PLAYING;
                    resetBall(serveToP1);
                }
            }
        }
        else if (state == PLAYING) {
            bx += bvx; 
            by += bvy;
            
            // --- KINEMATIK: Wand Kollision (Nutzt die BALL-AUSSENSEITE: by-1, by+1) ---
            if (by - 1 <= 0) {
                by = 1;
                bvy = -bvy;
                if (b_spin != 0.0f) {
                    bvy += b_spin * 0.2f; 
                    bvx += (bvx > 0 ? 1 : -1) * abs(b_spin) * 0.1f; 
                    b_spin = 0.0f; // Spin aufgebraucht
                }
            } 
            else if (by + 1 >= M_HEIGHT - 1) {
                by = M_HEIGHT - 2;
                bvy = -bvy;
                if (b_spin != 0.0f) {
                    bvy -= b_spin * 0.2f; 
                    bvx += (bvx > 0 ? 1 : -1) * abs(b_spin) * 0.1f; 
                    b_spin = 0.0f; // Spin aufgebraucht
                }
            }
            
            // --- KINEMATIK: Paddle Kollision ---
            
            // PADDLE 1 (Links)
            // bx-1 ist die linke Außenseite des Balls. by ist die Mitte (entscheidet ob getroffen).
            if (bx - 1 <= 2 && by >= p1_y && by <= p1_y + padH) { 
                bvx = -bvx * 1.05f; 
                if (bvx > 3.0f) bvx = 3.0f; // Deckelung der X-Geschwindigkeit
                bx = 4; 
                
                // Wo wurde getroffen? (0 bis 16)
                float hitPoint = by - p1_y; 
                
                // Abgerundete Ecken - verändern den Winkel stark
                if (hitPoint <= 2.0f) bvy -= 0.5f; 
                else if (hitPoint >= padH - 2.0f) bvy += 0.5f; 
                
                // Spin - nur wenn Pad in Bewegung!
                if (pong_p1_dir != 0) {
                    b_spin = pong_p1_dir * 0.5f; // Leichter Spin aufgeladen
                    bvy += pong_p1_dir * 0.2f;   // Direkter minimaler Kick
                } else {
                    b_spin = 0.0f; // Kein Spin
                }
            }
            
            // PADDLE 2 (Rechts)
            if (bx + 1 >= M_WIDTH - 3 && by >= p2_y && by <= p2_y + padH) { 
                bvx = -bvx * 1.05f; 
                if (bvx < -3.0f) bvx = -3.0f;
                bx = M_WIDTH - 5; 
                
                float hitPoint = by - p2_y;
                if (hitPoint <= 2.0f) bvy -= 0.5f;
                else if (hitPoint >= padH - 2.0f) bvy += 0.5f;
                
                if (pong_p2_dir != 0) {
                    b_spin = pong_p2_dir * 0.5f; 
                    bvy += pong_p2_dir * 0.2f;
                } else {
                    b_spin = 0.0f;
                }
            }
            
            // Globale Y-Geschwindigkeits-Drossel (verhindert durch-glitchen)
            if (bvy > 3.5f) bvy = 3.5f;
            if (bvy < -3.5f) bvy = -3.5f;
            
            // --- TORE ---
            if (bx < 0) { 
                score2++; 
                serveToP1 = true; 
                stateTimer = now; 
                needsRedraw = true; 
                if (score2 >= 10) state = GAME_OVER;
                else state = SCORED;
            }
            if (bx > M_WIDTH) { 
                score1++; 
                serveToP1 = false;
                stateTimer = now; 
                needsRedraw = true; 
                if (score1 >= 10) state = GAME_OVER;
                else state = SCORED;
            }
            needsRedraw = true; 
        } 
        else if (state == SCORED) {
            if (now - stateTimer > 3000) {
                state = COUNTDOWN; 
                countdownValue = 3;
                stateTimer = now;
                needsRedraw = true;
            }
        }
        else if (state == GAME_OVER) {
            if (now - stateTimer > 5000) { 
                state = READY; 
                needsRedraw = true;
            }
        }

        if (state != oldState) needsRedraw = true;

        // ==========================================
        // 2. FRAMERATE-DROSSEL 
        // ==========================================
        if (state == WAIT_PLAYERS || state == SCORED || state == GAME_OVER) {
            if (!needsRedraw && !force) return false; 
        } else if (state == READY) {
            if (now - lastFrame < 500 && !needsRedraw && !force) return false;
        } else {
            if (now - lastFrame < 30 && !needsRedraw && !force) return false;
        }

        lastFrame = now;
        needsRedraw = false;

        // ==========================================
        // 3. RENDERING
        // ==========================================
        display.clear();

        if (state == WAIT_PLAYERS) {
            if (qrSize > 0) {
                int offsetX = 8;
                int offsetY = (M_HEIGHT - qrSize) / 2;

                display.fillRect(offsetX - 2, offsetY - 2, qrSize + 4, qrSize + 4, 0xFFFF);

                for (int y = 0; y < qrSize; y++) {
                    for (int x = 0; x < qrSize; x++) {
                        if (qrMatrix[y][x]) display.drawPixel(x + offsetX, y + offsetY, 0x0000);
                    }
                }

                int tx = offsetX + qrSize + (M_WIDTH - (offsetX + qrSize)) / 2;
                
                int lineH = richText.getLineHeight("Small");
                int totalH = (lineH * 3) + 4; 
                int startY = (M_HEIGHT - totalH) / 2 + richText.getBaselineOffset("Small");
                
                int w1 = richText.getTextWidth(display, "{c:cyan}Scan", "Small");
                richText.drawString(display, tx - w1/2, startY, "{c:cyan}Scan", "Small");

                int w2 = richText.getTextWidth(display, "{c:muted}to", "Small");
                richText.drawString(display, tx - w2/2, startY + lineH + 2, "{c:muted}to", "Small");

                int w3 = richText.getTextWidth(display, "{c:cyan}join", "Small");
                richText.drawString(display, tx - w3/2, startY + (lineH + 2) * 2, "{c:cyan}join", "Small");
            }
        } else {
            // --- Z-Index 1: Scores ---
            if (state >= READY) {
                uint16_t darkCyan = display.color565(0, 110, 110);    
                uint16_t darkMagenta = display.color565(110, 0, 110); 
                
                String s1 = String(score1);
                String s2 = String(score2);
                
                int baseOffset = richText.getBaselineOffset("Medium");
                int yCenter = (M_HEIGHT / 2) + (baseOffset / 2); 
                
                int w1 = richText.getTextWidth(display, s1, "Medium");
                richText.drawString(display, M_WIDTH/2 - 12 - w1, yCenter, s1, "Medium", darkCyan);
                richText.drawString(display, M_WIDTH/2 + 12, yCenter, s2, "Medium", darkMagenta);
            }

            // --- Z-Index 2: Netz ---
            for(int i=0; i<M_HEIGHT; i+=4) display.drawFastVLine(M_WIDTH/2, i, 2, display.color565(70, 70, 70)); 
            
            // --- Z-Index 3: PADDLES (Mit abgerundeten, dunklen Ecken) ---
            if (pong_p1_ready) {
                display.fillRect(1, p1_y + 1, 2, padH - 2, 0x07FF); // Hauptkörper
                display.drawPixel(1, p1_y, display.color565(0, 150, 150)); // Dunkle Ecke Oben
                display.drawPixel(1, p1_y + padH - 1, display.color565(0, 150, 150)); // Dunkle Ecke Unten
                // Die Front-Pixel (x=2) bleiben leer, das erzeugt die Rundung!
            }
            if (pong_p2_ready) {
                display.fillRect(M_WIDTH-3, p2_y + 1, 2, padH - 2, 0xF81F);
                display.drawPixel(M_WIDTH-2, p2_y, display.color565(150, 0, 150));
                display.drawPixel(M_WIDTH-2, p2_y + padH - 1, display.color565(150, 0, 150));
            }
            
            // --- Z-Index 4: BALL (3x3 gerundet) ---
            if (state == PLAYING) {
                int ix = (int)bx; 
                int iy = (int)by;
                uint16_t cWhite = 0xFFFF;
                uint16_t cGray = display.color565(180, 180, 180);
                uint16_t cDark = display.color565(70, 70, 70);
                
                // Zentrum & Flanken
                display.drawPixel(ix, iy, cWhite);
                display.drawPixel(ix - 1, iy, cGray); 
                display.drawPixel(ix + 1, iy, cGray);
                display.drawPixel(ix, iy - 1, cGray); 
                display.drawPixel(ix, iy + 1, cGray);
                
                // Ecken des 3x3 Balls dunkel schattiert
                display.drawPixel(ix - 1, iy - 1, cDark); 
                display.drawPixel(ix + 1, iy - 1, cDark);
                display.drawPixel(ix - 1, iy + 1, cDark); 
                display.drawPixel(ix + 1, iy + 1, cDark);
            }
            
            // --- Z-Index 5: Text Overlays ---
            if (state == READY) {
                richText.drawCentered(display, 18, "{c:warn}Press", "Small");
                if ((now/500)%2==0) {
                    richText.drawCentered(display, 32, "{b}{c:gold}START", "Small");
                }
                richText.drawCentered(display, 46, "{c:warn}GAME", "Small");
                needsRedraw = true; 
            }
            else if (state == COUNTDOWN) {
                richText.drawCentered(display, 35, "{b}{c:white}" + String(countdownValue), "Medium");
            }
            else if (state == SCORED) {
                richText.drawCentered(display, 35, "{b}{c:gold}GOAL!", "Medium"); 
            }
            else if (state == GAME_OVER) {
                if (score1 >= 10) richText.drawCentered(display, 35, "{b}{c:cyan}P1 WINS!", "Small");
                else richText.drawCentered(display, 35, "{b}{c:magenta}P2 WINS!", "Small");
            }
        }
        return true;
    }
    int getPriority() override { return 10; }
};

bool PongApp::qrMatrix[35][35];
int PongApp::qrSize = 0;