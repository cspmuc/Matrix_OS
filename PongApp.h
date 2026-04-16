#pragma once
#include "App.h"
#include "RichText.h"
#include "config.h" 
#include "qrcode.h" 

class PongApp : public App {
private:
    RichText richText;

    enum GameState { WAIT_PLAYERS, READY, PLAYING, SCORED };
    GameState state = WAIT_PLAYERS;

    float p1_y, p2_y;
    float bx, by, bvx, bvy;
    int score1 = 0, score2 = 0;
    const int padH = 16; 
    unsigned long lastFrame = 0;
    unsigned long waitTimer = 0;

    // Wir nutzen jetzt Pointer und reservieren Speicher im PSRAM
    uint8_t* qrBuffer = nullptr; 
    int qrSize = 0;

    // Die Callback-Struktur muss so bleiben, aber wir schreiben in den PSRAM-Buffer
    static PongApp* instance;
    static void pongQrCallback(esp_qrcode_handle_t qrcode) {
        if (!instance) return;
        instance->qrSize = esp_qrcode_get_size(qrcode);
        
        // Speicher im PSRAM reservieren (MALLOC_CAP_SPIRAM)
        if (instance->qrBuffer) heap_caps_free(instance->qrBuffer);
        instance->qrBuffer = (uint8_t*)heap_caps_malloc(instance->qrSize * instance->qrSize, MALLOC_CAP_SPIRAM);
        
        if (instance->qrBuffer) {
            for (int y = 0; y < instance->qrSize; y++) {
                for (int x = 0; x < instance->qrSize; x++) {
                    instance->qrBuffer[y * instance->qrSize + x] = esp_qrcode_get_module(qrcode, x, y);
                }
            }
        }
    }

    void resetBall(bool toP1) {
        bx = M_WIDTH / 2;
        by = M_HEIGHT / 2;
        bvx = toP1 ? -1.8f : 1.8f;
        bvy = (random(-15, 15) / 10.0f);
    }

public:
    PongApp() { instance = this; }

    void onActive() override {
        p1_y = M_HEIGHT / 2 - padH / 2;
        p2_y = M_HEIGHT / 2 - padH / 2;
        score1 = 0; score2 = 0;
        
        pong_p1_ready = false;
        pong_p2_ready = false;
        pong_p1_dir = 0;
        pong_p2_dir = 0;
        pong_start_trigger = false;
        
        state = WAIT_PLAYERS;

        // QR Code Generierung
        esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
        cfg.display_func = pongQrCallback;
        esp_qrcode_generate(&cfg, "http://matrixos.local/pong");
    }

    bool isReadyToSwitch(float durationMultiplier = 1.0) override {
        return false; 
    }

    bool draw(DisplayManager& display, bool force) override {
        unsigned long now = millis();
        if (now - lastFrame < 20 && !force) return false; 
        lastFrame = now;

        if (state == WAIT_PLAYERS && (pong_p1_ready || pong_p2_ready)) state = READY;
        if (state > WAIT_PLAYERS && !pong_p1_ready && !pong_p2_ready) state = WAIT_PLAYERS;

        if (state >= READY) {
            p1_y += (pong_p1_dir * 2.0f);
            p2_y += (pong_p2_dir * 2.0f);
            p1_y = constrain(p1_y, 0, M_HEIGHT - padH);
            p2_y = constrain(p2_y, 0, M_HEIGHT - padH);
        }

        if (state == READY) {
            if (pong_start_trigger) {
                pong_start_trigger = false;
                state = PLAYING;
                resetBall(true);
            }
        } else if (state == PLAYING) {
            bx += bvx; by += bvy;
            if (by <= 0 || by >= M_HEIGHT - 1) bvy = -bvy;
            if (bx <= 3 && by >= p1_y && by <= p1_y + padH) { bvx = -bvx * 1.05f; bx = 4; }
            if (bx >= M_WIDTH - 4 && by >= p2_y && by <= p2_y + padH) { bvx = -bvx * 1.05f; bx = M_WIDTH - 5; }
            if (bx < 0) { score2++; state = SCORED; waitTimer = now; pong_p1_dir = 0; pong_p2_dir = 0; }
            if (bx > M_WIDTH) { score1++; state = SCORED; waitTimer = now; pong_p1_dir = 0; pong_p2_dir = 0; }
        } else if (state == SCORED && (now - waitTimer > 1500)) {
            pong_start_trigger = false;
            state = READY;
        }

        display.clear();

        if (state == WAIT_PLAYERS && qrBuffer) {
            int offsetX = 4;
            int offsetY = (M_HEIGHT - qrSize) / 2;
            display.fillRect(offsetX - 2, offsetY - 2, qrSize + 4, qrSize + 4, display.color565(255, 255, 255));

            for (uint8_t y = 0; y < qrSize; y++) {
                for (uint8_t x = 0; x < qrSize; x++) {
                    if (qrBuffer[y * qrSize + x]) {
                        display.drawPixel(x + offsetX, y + offsetY, display.color565(0, 0, 0)); 
                    }
                }
            }
            // Text Zeichnen...
            int textCenterX = (offsetX + qrSize) + (M_WIDTH - (offsetX + qrSize)) / 2;
            int w1 = richText.getTextWidth(display, "Scan", "Small");
            richText.drawString(display, textCenterX - w1/2, 14, "Scan", "Small", display.color565(0, 255, 255));

            int w2 = richText.getTextWidth(display, "to", "Small");
            richText.drawString(display, textCenterX - w2/2, 28, "to", "Small", display.color565(0, 255, 255));

            int w3 = richText.getTextWidth(display, "join", "Small");
            richText.drawString(display, textCenterX - w3/2, 42, "join", "Small", display.color565(0, 255, 255));
        } else {
            // Spielfeld Zeichnen...
            for(int i=0; i<M_HEIGHT; i+=4) display.drawFastVLine(M_WIDTH/2, i, 2, display.color565(40,40,40));
            if (pong_p1_ready) display.fillRect(1, p1_y, 2, padH, display.color565(0, 255, 255));
            if (pong_p2_ready) display.fillRect(M_WIDTH-3, p2_y, 2, padH, display.color565(255, 0, 255));
            if (state == PLAYING || state == SCORED || state == READY) {
                richText.drawString(display, M_WIDTH/2 - 15, 5, String(score1), "Small", display.color565(0, 255, 255));
                richText.drawString(display, M_WIDTH/2 + 8, 5, String(score2), "Small", display.color565(255, 0, 255));
            }
            if (state == PLAYING || state == SCORED) display.fillRect(bx-1, by-1, 2, 2, display.color565(255, 255, 255));
            if (state == READY) {
                richText.drawCentered(display, 18, "{c:silver}Press", "Small");
                richText.drawCentered(display, 32, (millis()/500)%2==0 ? "{b}{c:gold}START" : "{b}{c:#AA8800}START", "Small");
                richText.drawCentered(display, 46, "{c:silver}GAME", "Small");
            }
        }
        return true;
    }
};
PongApp* PongApp::instance = nullptr;