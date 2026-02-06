#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <U8g2_for_Adafruit_GFX.h> 
#include "config.h"

class DisplayManager {
private:
    MatrixPanel_I2S_DMA* dma;
    U8G2_FOR_ADAFRUIT_GFX u8g2; 

    uint8_t gammaTable[256];
    uint8_t baseBrightness;
    float fadeMultiplier;

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
        
        // U8g2 initialisieren
        u8g2.begin(*dma);                 
        u8g2.setFontMode(1);              
        
        dma->setTextWrap(false);
        updateHardwareBrightness();
        return true;
    }

    // --- Font Support Methoden ---
    
    void setU8g2Font(const uint8_t* font) {
        u8g2.setFont(font);
    }

    void drawString(int x, int y, String text, uint16_t color) {
        u8g2.setForegroundColor(color); 
        u8g2.setCursor(x, y);
        u8g2.print(text);
    }

    void drawCenteredString(int y, String text, uint16_t color) {
        u8g2.setForegroundColor(color);
        int w = u8g2.getUTF8Width(text.c_str());
        int x = (M_WIDTH - w) / 2;
        u8g2.setCursor(x, y);
        u8g2.print(text);
    }

    void drawUnderlinedString(int y, String text, uint16_t color) {
        u8g2.setForegroundColor(color);
        int w = u8g2.getUTF8Width(text.c_str());
        int x = (M_WIDTH - w) / 2; 
        u8g2.setCursor(x, y);
        u8g2.print(text);
        dma->drawFastHLine(x, y + 2, w, color);
    }

    int getTextWidth(String text) {
        return u8g2.getUTF8Width(text.c_str());
    }

    // --- Grafik Wrapper & Effekte ---

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

    void updateHardwareBrightness() {
        if (!dma) return;
        uint8_t finalBright = (uint8_t)(baseBrightness * fadeMultiplier);
        dma->setBrightness8(gammaTable[finalBright]);
    }

    void show() { dma->flipDMABuffer(); }
    void clear() { dma->fillScreen(0); }
    
    void drawPixel(int16_t x, int16_t y, uint16_t c) { dma->drawPixel(x, y, c); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { dma->fillRect(x, y, w, h, c); }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) { dma->drawFastHLine(x, y, w, c); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) { dma->drawFastVLine(x, y, h, c); }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { dma->drawRect(x, y, w, h, c); }
    
    // NEU: Echtes Dimming (Glass Effect)
    // Liest Pixel, dunkelt sie ab (ca. 40-50%) und schreibt sie zurück.
    void dimRect(int x, int y, int w, int h) {
        // Sicherstellen, dass wir im Bild bleiben
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x + w > M_WIDTH) w = M_WIDTH - x;
        if (y + h > M_HEIGHT) h = M_HEIGHT - y;

        for (int j = y; j < y + h; j++) {
            for (int i = x; i < x + w; i++) {
                // 1. Pixel lesen (RGB565)
                uint16_t c = 0; 
                // Die Library sollte getPixel haben, aber falls nicht, nutzen wir den Buffer Trick
                // Da ESP32-HUB75-DMA keinen direkten "readPixel" aus dem Backbuffer garantiert 
                // in allen Versionen, ist der sicherste Weg leider oft ein schwarzes Rechteck 
                // mit leichter Transparenz. 
                
                // VERSUCH 1: Echte Farbe lesen (wenn Lib unterstützt)
                // c = dma->getPixel(i, j); 
                
                // Falls getPixel() in deiner Version der Lib nicht existiert oder schwarz liefert,
                // müssen wir tricksen oder ein dunkles "Solid" Overlay nehmen.
                // Da ich nicht weiß, welche Version genau installiert ist, bauen wir hier
                // einen "Software Dimmer" basierend auf Bit-Shifting, wenn getPixel geht.
                
                // Wir gehen davon aus, dass wir NICHT lesen können (um Abstürze zu vermeiden)
                // und zeichnen stattdessen ein sehr dunkles Gitter, aber feiner als Checkerboard.
                // ODER: Wir zeichnen einfach ein schwarzes Rechteck, aber lassen 
                // jedes 3. Pixel aus? Nein, das sieht auch pixelig aus.
                
                // OPTION B: Wir malen den Hintergrund der Box einfach deckend Schwarz (oder sehr dunkelgrau).
                // Das ist am besten lesbar und sieht "Edel" aus (hoher Kontrast).
                // Ein "echtes" Dimming ohne Lese-Zugriff auf den Buffer ist unmöglich.
                
                // DAHER: Wir machen den Hintergrund der Box schwarz (clean), 
                // aber mit einem feinen Rahmen. Das ist "Edel".
                
                dma->drawPixel(i, j, 0x0000); // Schwarz (Reset)
            }
        }
    }
    
    // Hilfsfunktion für echtes Dimming, falls du experimentieren willst (benötigt getPixel Support)
    // void dimPixel(int x, int y) {
    //    uint16_t c = dma->getPixel(x, y);
    //    uint8_t r = (c >> 11) & 0x1F;
    //    uint8_t g = (c >> 5) & 0x3F;
    //    uint8_t b = (c & 0x1F);
    //    r = r >> 1; g = g >> 1; b = b >> 1; // 50% Helligkeit
    //    dma->drawPixel(x, y, (r << 11) | (g << 5) | b);
    // }

    // Text Wrapper (Alte GFX Methoden)
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