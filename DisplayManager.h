#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <U8g2_for_Adafruit_GFX.h> 
#include <Adafruit_GFX.h>
#include "config.h"

class DisplayManager {
private:
    MatrixPanel_I2S_DMA* dma;
    GFXcanvas16* canvas; // Soft Double Buffer im PSRAM
    U8G2_FOR_ADAFRUIT_GFX u8g2; 

    uint8_t gammaTable[256];
    uint8_t baseBrightness;
    float fadeMultiplier;

public:
    DisplayManager() : dma(nullptr), canvas(nullptr), baseBrightness(150), fadeMultiplier(1.0) {
        // Gamma Tabelle berechnen
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
        
        // SOFT DOUBLE BUFFERING: Wir nutzen unseren eigenen PSRAM Canvas
        mxconfig.double_buff = false; 

        dma = new MatrixPanel_I2S_DMA(mxconfig);
        if (!dma->begin()) return false;
        
        // PSRAM Canvas erstellen
        canvas = new GFXcanvas16(M_WIDTH, M_HEIGHT);
        if (!canvas) return false; 
        
        // U8g2 auf den Canvas binden
        u8g2.begin(*canvas);                 
        u8g2.setFontMode(1); // Transparent Modus aktivieren
        u8g2.setFontDirection(0);
        
        canvas->setTextWrap(false);
        dma->setTextWrap(false);
        
        updateHardwareBrightness();
        return true;
    }

    // --- Font Support Methoden ---
    void setU8g2Font(const uint8_t* font) { u8g2.setFont(font); }

    void drawString(int x, int y, String text, uint16_t color) {
        u8g2.setFontMode(1); // WICHTIG: Transparenz erzwingen
        u8g2.setForegroundColor(color); 
        u8g2.setCursor(x, y); 
        u8g2.print(text);
    }

    void drawCenteredString(int y, String text, uint16_t color) {
        u8g2.setFontMode(1); // Transparenz erzwingen
        u8g2.setForegroundColor(color);
        int w = u8g2.getUTF8Width(text.c_str());
        int x = (M_WIDTH - w) / 2;
        u8g2.setCursor(x, y); 
        u8g2.print(text);
    }

    void drawUnderlinedString(int y, String text, uint16_t color) {
        u8g2.setFontMode(1); // Transparenz erzwingen
        u8g2.setForegroundColor(color);
        int w = u8g2.getUTF8Width(text.c_str());
        int x = (M_WIDTH - w) / 2; 
        u8g2.setCursor(x, y); 
        u8g2.print(text);
        if(canvas) canvas->drawFastHLine(x, y + 2, w, color);
    }

    int getTextWidth(String text) { return u8g2.getUTF8Width(text.c_str()); }

    // --- Grafik Wrapper ---
    void setFade(float f) {
        if (f < 0.0) f = 0.0; if (f > 1.0) f = 1.0;
        fadeMultiplier = f; updateHardwareBrightness();
    }

    void setBrightness(uint8_t b) {
        baseBrightness = b; updateHardwareBrightness();
    }

    void updateHardwareBrightness() {
        if (!dma) return;
        uint8_t finalBright = (uint8_t)(baseBrightness * fadeMultiplier);
        dma->setBrightness8(gammaTable[finalBright]);
    }

    void show() { 
        if(canvas && dma) {
            dma->drawRGBBitmap(0, 0, canvas->getBuffer(), M_WIDTH, M_HEIGHT);
        }
    }
    
    void clear() { if(canvas) canvas->fillScreen(0); }
    
    void drawPixel(int16_t x, int16_t y, uint16_t c) { if(canvas) canvas->drawPixel(x, y, c); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { if(canvas) canvas->fillRect(x, y, w, h, c); }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) { if(canvas) canvas->drawFastHLine(x, y, w, c); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) { if(canvas) canvas->drawFastVLine(x, y, h, c); }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { if(canvas) canvas->drawRect(x, y, w, h, c); }
    
    // --- ECHTES DIMMING
    void dimRect(int x, int y, int w, int h) {
        if (!canvas) return;
        
        // Bounds Check
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x + w > M_WIDTH) w = M_WIDTH - x;
        if (y + h > M_HEIGHT) h = M_HEIGHT - y;
        
        uint16_t* buffer = canvas->getBuffer();
        
        for (int j = y; j < y + h; j++) {
            for (int i = x; i < x + w; i++) {
                int index = j * M_WIDTH + i;
                uint16_t color = buffer[index];
                
                // RGB565 zerlegen
                uint16_t r = (color >> 11) & 0x1F;
                uint16_t g = (color >> 5) & 0x3F;
                uint16_t b = color & 0x1F;
                
                // Helligkeit vierteln
                r = r >> 2;
                g = g >> 2;
                b = b >> 2;
                
                // Wieder zusammensetzen
                buffer[index] = (r << 11) | (g << 5) | b;
            }
        }
    }
    
    // --- Text Wrapper ---
    void setTextColor(uint16_t c) { if(canvas) canvas->setTextColor(c); }
    void setCursor(int16_t x, int16_t y) { if(canvas) canvas->setCursor(x, y); }
    void print(String t) { if(canvas) canvas->print(t); }
    void setTextSize(uint8_t s) { if(canvas) canvas->setTextSize(s); }
    void setFont(const GFXfont *f = NULL) { if(canvas) canvas->setFont(f); }
    void setTextWrap(bool w) { if(canvas) canvas->setTextWrap(w); }
    
    // U8g2 Wrapper
    void printCentered(String text, int y) {
        u8g2.setFontMode(1); // Transparenz erzwingen
        u8g2.setForegroundColor(0xFFFF); 
        int w = u8g2.getUTF8Width(text.c_str());
        int x = (M_WIDTH - w) / 2;
        u8g2.setCursor(x, y); 
        u8g2.print(text);
    }

    void drawScrollingText(String text, int y, int xPos, uint16_t color) {
        u8g2.setFontMode(1); // Transparenz erzwingen
        u8g2.setForegroundColor(color);
        u8g2.setCursor(xPos, y);
        u8g2.print(text);
    }

    // --- Farben ---
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