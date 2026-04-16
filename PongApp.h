#pragma once
#include "App.h"
#include "RichText.h"
#include "config.h" // Für die globalen Steuervariablen

class PongApp : public App {
private:
    RichText richText;

    enum GameState { WAIT_P1, WAIT_P2, READY, PLAYING, SCORED };
    GameState state = WAIT_P1;

    float p1_y, p2_y;
    float bx, by, bvx, bvy;
    int score1 = 0, score2 = 0;
    const int padH = 16; 
    unsigned long lastFrame = 0;
    unsigned long waitTimer = 0;

    void resetBall(bool toP1) {
        bx = M_WIDTH / 2;
        by = M_HEIGHT / 2;
        bvx = toP1 ? -1.8f : 1.8f;
        bvy = (random(-15, 15) / 10.0f);
    }

public:
    void onActive() override {
        // App Start-Werte
        p1_y = M_HEIGHT / 2 - padH / 2;
        p2_y = M_HEIGHT / 2 - padH / 2;
        score1 = 0; score2 = 0;
        
        // Globale Variablen für Web zurücksetzen
        pong_p1_ready = false;
        pong_p2_ready = false;
        pong_p1_dir = 0;
        pong_p2_dir = 0;
        pong_start_trigger = false;
        
        state = WAIT_P1;
    }

    bool isReadyToSwitch(float durationMultiplier = 1.0) override {
        return false; // Pong beendet sich nie von selbst
    }

    bool draw(DisplayManager& display, bool force) override {
        unsigned long now = millis();
        if (now - lastFrame < 20 && !force) return false; 
        lastFrame = now;

        // Zustands-Check (Warten auf Web-Controller)
        if (state == WAIT_P1 && pong_p1_ready) state = WAIT_P2;
        if (state == WAIT_P2 && pong_p2_ready) state = READY;
        
        // Falls jemand im Webbrowser auf "Join" klickt, während ein Spiel läuft, resetten
        if (state > WAIT_P1 && !pong_p1_ready) state = WAIT_P1;
        if (state > WAIT_P2 && !pong_p2_ready) state = WAIT_P2;

        if (state == READY) {
            if (pong_start_trigger) {
                pong_start_trigger = false;
                state = PLAYING;
                resetBall(true);
            }
        } else if (state == PLAYING) {
            // Steuerung über die Web-Variablen
            p1_y += (pong_p1_dir * 2.0f);
            p2_y += (pong_p2_dir * 2.0f);

            p1_y = constrain(p1_y, 0, M_HEIGHT - padH);
            p2_y = constrain(p2_y, 0, M_HEIGHT - padH);

            bx += bvx; by += bvy;

            // Wandkollision
            if (by <= 0 || by >= M_HEIGHT - 1) bvy = -bvy;

            // Schlägerkollision
            if (bx <= 3 && by >= p1_y && by <= p1_y + padH) { bvx = -bvx * 1.05f; bx = 4; }
            if (bx >= M_WIDTH - 4 && by >= p2_y && by <= p2_y + padH) { bvx = -bvx * 1.05f; bx = M_WIDTH - 5; }

            // Tore
            if (bx < 0) { score2++; state = SCORED; waitTimer = now; pong_p1_dir = 0; pong_p2_dir = 0; }
            if (bx > M_WIDTH) { score1++; state = SCORED; waitTimer = now; pong_p1_dir = 0; pong_p2_dir = 0; }
        } else if (state == SCORED && (now - waitTimer > 1500)) {
            pong_start_trigger = false;
            state = READY;
        }

        // Rendern
        display.clear();
        for(int i=0; i<M_HEIGHT; i+=4) display.drawFastVLine(M_WIDTH/2, i, 2, display.color565(40,40,40));

        if (state >= WAIT_P2) display.fillRect(1, p1_y, 2, padH, display.color565(0, 255, 255));
        if (state >= READY) display.fillRect(M_WIDTH-3, p2_y, 2, padH, display.color565(255, 0, 255));
        
        if (state == PLAYING || state == SCORED || state == READY) {
            display.fillRect(bx-1, by-1, 2, 2, COL_WHITE);
            richText.drawString(display, M_WIDTH/2 - 15, 10, String(score1), "Small", display.color565(0, 255, 255));
            richText.drawString(display, M_WIDTH/2 + 8, 10, String(score2), "Small", display.color565(255, 0, 255));
        }

        if (state == WAIT_P1) richText.drawCentered(display, 35, "{c:cyan}Warte auf P1...", "Small");
        else if (state == WAIT_P2) richText.drawCentered(display, 35, "{c:magenta}Warte auf P2...", "Small");
        else if (state == READY) richText.drawCentered(display, 45, "{b}{c:gold}START DRUCKEN", "Small");

        return true;
    }
    int getPriority() override { return 10; }
};