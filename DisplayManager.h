#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "config.h"

class DisplayManager {
private:
    MatrixPanel_I2S_DMA* dma;
    uint8_t gammaTable[256];
    uint8_t baseBrightness; // Der Wert von HA (0-255)
    float fadeMultiplier;    // Für die Animation (0.0 bis 1.0)

public:
    DisplayManager() : dma(nullptr), baseBrightness(150), fadeMultiplier(1.0) {
        // Gamma Tabelle berechnen (2.4 Kurve)
        const uint8_t minHardwareBright = 2; 
        const uint8_t maxHardwareBright = 255;
        const int inputMax = 242;

        gammaTable[0] = 0;
        for (int i = 1; i < 256; i++) {
            if (i >= inputMax) {
                gammaTable[i] = maxHardwareBright;
            } else {
                float normalized = (float)(i - 1) / (float)(inputMax - 1);
                float val = minHardwareBright + pow(normalized, 2.4) * (maxHardwareBright - minHardwareBright);
                gammaTable[i] = (uint8_t)val;
            }
        }
    }

    bool begin() {
        HUB75_I2S_CFG mxconfig(M_WIDTH, M_HEIGHT, 1);
        mxconfig.gpio.r1 = R1_PIN; mxconfig.gpio.g1 = G1_PIN; mxconfig.gpio.b1 = B1_PIN;
        mxconfig.gpio.r2 = R2_PIN; mxconfig.gpio.g2 = G2_PIN; mxconfig.gpio.b2 = B2_PIN;
        mxconfig.gpio.a = A_PIN; mxconfig.gpio.b = B_PIN; mxconfig.gpio.c = C_PIN;
        mxconfig.gpio.d = D_PIN; mxconfig.gpio.e = E_PIN;
        mxconfig.gpio.lat = LAT_PIN; mxconfig.gpio.oe = OE_PIN; mxconfig.gpio.clk = CLK_PIN;
        mxconfig.clkphase = false;
        mxconfig.double_buff = true; 

        dma = new MatrixPanel_I2S_DMA(mxconfig);
        if (!dma->begin()) return false;
        
        dma->setTextWrap(false);
        updateHardwareBrightness();
        return true;
    }

    // --- NEU: Fade Steuerung ---
    // Setzt den Faktor für die Animation (0.0 = Schwarz, 1.0 = Volle baseBrightness)
    void setFade(float f) {
        if (f < 0.0) f = 0.0;
        if (f > 1.0) f = 1.0;
        fadeMultiplier = f;
        updateHardwareBrightness();
    }

    void setBrightness(uint8_t b) {
        baseBrightness = b;
        updateHardwareBrightness();
    }

    // Berechnet die echte Helligkeit aus Basiswert * FadeFaktor
    void updateHardwareBrightness() {
        if (!dma) return;
        uint8_t finalBright = (uint8_t)(baseBrightness * fadeMultiplier);
        dma->setBrightness8(gammaTable[finalBright]);
    }

    void show() { dma->flipDMABuffer(); }
    void clear() { dma->fillScreen(0); }
    
    // Grafik Wrapper
    void drawPixel(int16_t x, int16_t y, uint16_t c) { dma->drawPixel(x, y, c); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { dma->fillRect(x, y, w, h, c); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) { dma->drawFastVLine(x, y, h, c); }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { dma->drawRect(x, y, w, h, c); }
    
    // Text
    void setTextColor(uint16_t c) { dma->setTextColor(c); }
    void setCursor(int16_t x, int16_t y) { dma->setCursor(x, y); }
    void print(String t) { dma->print(t); }
    void setTextSize(uint8_t s) { dma->setTextSize(s); }
    void setFont(const GFXfont *f = NULL) { dma->setFont(f); }
    void setTextWrap(bool w) { dma->setTextWrap(w); }
    
    void printCentered(String text, int y) {
        dma->setTextWrap(false);
        int16_t x1, y1;
        uint16_t w, h;
        dma->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        int x = (M_WIDTH - w) / 2;
        dma->setCursor(x, y);
        dma->print(text);
    }

    int getTextWidth(String text) {
        int16_t x1, y1;
        uint16_t w, h;
        dma->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
        return w;
    }

    void drawScrollingText(String text, int y, int xPos, uint16_t color) {
        dma->setTextWrap(false);
        setTextSize(1);
        setFont(NULL);
        setTextColor(color);
        setCursor(xPos, y);
        print(text);
    }

    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return dma->color565(r, g, b); }
    
    uint16_t colorHSV(long hue, uint8_t sat, uint8_t val) {
        uint8_t r, g, b;
        if (sat == 0) { r = g = b = val; } 
        else {
            uint16_t h = (hue >= 65536) ? 0 : hue;     
            uint32_t base = ((uint32_t)val * (255 - sat)) / 255;
            uint32_t p = ((uint32_t)val * sat) / 255;
            uint16_t sextant = h / 10923;
            uint16_t rem     = h % 10923; 
            uint32_t part = (p * rem) / 10923;
            switch (sextant) {
              case 0: r = val; g = base + part; b = base; break;
              case 1: r = val - part; g = val; b = base; break;
              case 2: r = base; g = val; b = base + part; break;
              case 3: r = base; g = val - part; b = val; break;
              case 4: r = base + part; g = base; b = val; break;
              case 5: r = val; g = base; b = val - part; break;
              default: r = val; g = base; b = val - part; break;
            }
        }
        return dma->color565(r, g, b);
    }
};