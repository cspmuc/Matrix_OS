#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <U8g2_for_Adafruit_GFX.h> 
#include <Adafruit_GFX.h>
#include "config.h"

// --- Eigene PSRAM Canvas Klasse ---
class PSRAMCanvas16 : public Adafruit_GFX {
private:
    uint16_t *buffer;
    uint32_t fadeInt = 256; // NEU: High-Speed Integer-Multiplikator (0 bis 256) statt Float

    // NEU: Super-schnelle Ganzzahl-Mathematik mit Bitshifting
    uint16_t applyFade(uint16_t color) {
        // Wenn volle Helligkeit, spare dir jegliche Mathematik!
        if (fadeInt >= 256) return color;
        // Wenn unsichtbar, mach es direkt schwarz!
        if (fadeInt == 0) return 0;
        
        // Farben isolieren
        uint32_t r = (color >> 11) & 0x1F;
        uint32_t g = (color >> 5) & 0x3F;
        uint32_t b = color & 0x1F;
        
        // Schnelle Ganzzahl-Multiplikation und Bitshift (entspricht exakt / 256)
        r = (r * fadeInt) >> 8;
        g = (g * fadeInt) >> 8;
        b = (b * fadeInt) >> 8;
        
        return (r << 11) | (g << 5) | b;
    }

public:
    PSRAMCanvas16(uint16_t w, uint16_t h) : Adafruit_GFX(w, h) {
        buffer = (uint16_t*)heap_caps_malloc(w * h * 2, MALLOC_CAP_SPIRAM);
    }
    ~PSRAMCanvas16() { 
        if(buffer) heap_caps_free(buffer); 
    }
    uint16_t* getBuffer() { return buffer; }
    
    // NEU: Der Float-Wert wird nur noch 1x pro Frame in einen Integer übersetzt!
    void setAppFade(float f) { 
        fadeInt = (uint32_t)(f * 256.0f);
        if (fadeInt > 256) fadeInt = 256;
    }
    
    // --- Überschriebene Zeichenfunktionen (inkl. Fade) ---
    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || y < 0 || x >= _width || y >= _height) return;
        buffer[y * _width + x] = applyFade(color); 
    }
    
    void fillScreen(uint16_t color) override {
        if(buffer) {
            uint16_t fadedColor = applyFade(color);
            uint8_t hi = fadedColor >> 8, lo = fadedColor & 0xFF;
            if(hi == lo) {
                memset(buffer, lo, _width * _height * 2);
            } else {
                uint32_t pixels = _width * _height;
                for(uint32_t i=0; i<pixels; i++) buffer[i] = fadedColor;
            }
        }
    }
    
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
        if(x < 0 || x >= _width || y >= _height) return;
        if(y < 0) { h += y; y = 0; }
        if(y + h > _height) { h = _height - y; }
        if(h <= 0) return;
        uint16_t fadedColor = applyFade(color);
        uint16_t *ptr = buffer + y * _width + x;
        for(int16_t i=0; i<h; i++) { *ptr = fadedColor; ptr += _width; }
    }
    
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
        if(y < 0 || y >= _height || x >= _width) return;
        if(x < 0) { w += x; x = 0; }
        if(x + w > _width) { w = _width - x; }
        if(w <= 0) return;
        uint16_t fadedColor = applyFade(color);
        uint16_t *ptr = buffer + y * _width + x;
        for(int16_t i=0; i<w; i++) { *ptr++ = fadedColor; }
    }
};

class DisplayManager {
private:
    MatrixPanel_I2S_DMA* dma;
    PSRAMCanvas16* canvas; 
    U8G2_FOR_ADAFRUIT_GFX u8g2; 

    uint8_t gammaTable[256];
    uint8_t baseBrightness;

public:
    DisplayManager() : dma(nullptr), canvas(nullptr), baseBrightness(150) {
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
        
        mxconfig.double_buff = false; 

        dma = new MatrixPanel_I2S_DMA(mxconfig);
        if (!dma->begin()) return false;
        
        canvas = new PSRAMCanvas16(M_WIDTH, M_HEIGHT);
        if (!canvas) return false; 
        
        u8g2.begin(*canvas);                 
        u8g2.setFontMode(1); 
        u8g2.setFontDirection(0);
        
        canvas->setTextWrap(false);
        dma->setTextWrap(false);
        
        updateHardwareBrightness();
        return true;
    }

    void setU8g2Font(const uint8_t* font) { u8g2.setFont(font); }

    void drawString(int x, int y, String text, uint16_t color) {
        u8g2.setFontMode(1); 
        u8g2.setForegroundColor(color); 
        u8g2.setCursor(x, y); 
        u8g2.print(text);
    }

    void drawCenteredString(int y, String text, uint16_t color) {
        u8g2.setFontMode(1); 
        u8g2.setForegroundColor(color);
        int w = u8g2.getUTF8Width(text.c_str());
        int x = (M_WIDTH - w) / 2;
        u8g2.setCursor(x, y); 
        u8g2.print(text);
    }

    void drawUnderlinedString(int y, String text, uint16_t color) {
        u8g2.setFontMode(1); 
        u8g2.setForegroundColor(color);
        int w = u8g2.getUTF8Width(text.c_str());
        int x = (M_WIDTH - w) / 2; 
        u8g2.setCursor(x, y); 
        u8g2.print(text);
        if(canvas) canvas->drawFastHLine(x, y + 2, w, color);
    }

    int getTextWidth(String text) { return u8g2.getUTF8Width(text.c_str()); }

    // --- NEU: Leitet den Fade an die Canvas weiter ---
    void setAppFade(float f) {
        if (f < 0.0) f = 0.0; if (f > 1.0) f = 1.0;
        if(canvas) canvas->setAppFade(f);
    }

    void setBrightness(uint8_t b) {
        baseBrightness = b; updateHardwareBrightness();
    }

    void updateHardwareBrightness() {
        if (!dma) return;
        dma->setBrightness8(gammaTable[baseBrightness]);
    }

    void show() { 
        if(canvas && dma) {
            dma->drawRGBBitmap(0, 0, canvas->getBuffer(), M_WIDTH, M_HEIGHT);
        }
    }
    
    void clear() { if(canvas) canvas->fillScreen(0); }
    
    void drawPixel(int16_t x, int16_t y, uint16_t c) { if(canvas) canvas->drawPixel(x, y, c); }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) { if(canvas) canvas->drawLine(x0, y0, x1, y1, c); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { if(canvas) canvas->fillRect(x, y, w, h, c); }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) { if(canvas) canvas->drawFastHLine(x, y, w, c); }
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) { if(canvas) canvas->drawFastVLine(x, y, h, c); }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) { if(canvas) canvas->drawRect(x, y, w, h, c); }
    // --- NEU: Für die Geisteraugen ---
    void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t c) { if(canvas) canvas->fillCircle(x0, y0, r, c); }
    void fillEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t c) { if(canvas) canvas->fillEllipse(x0, y0, rx, ry, c); }
    void dimRect(int x, int y, int w, int h) {
        if (!canvas) return;
        
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x + w > M_WIDTH) w = M_WIDTH - x;
        if (y + h > M_HEIGHT) h = M_HEIGHT - y;
        
        uint16_t* buffer = canvas->getBuffer();
        
        for (int j = y; j < y + h; j++) {
            for (int i = x; i < x + w; i++) {
                int index = j * M_WIDTH + i;
                uint16_t color = buffer[index];
                
                uint16_t r = (color >> 11) & 0x1F;
                uint16_t g = (color >> 5) & 0x3F;
                uint16_t b = color & 0x1F;
                
                r = r >> 2;
                g = g >> 2;
                b = b >> 2;
                
                buffer[index] = (r << 11) | (g << 5) | b;
            }
        }
    }
    
    void setTextColor(uint16_t c) { if(canvas) canvas->setTextColor(c); }
    void setCursor(int16_t x, int16_t y) { if(canvas) canvas->setCursor(x, y); }
    void print(String t) { if(canvas) canvas->print(t); }
    void setTextSize(uint8_t s) { if(canvas) canvas->setTextSize(s); }
    void setFont(const GFXfont *f = NULL) { if(canvas) canvas->setFont(f); }
    void setTextWrap(bool w) { if(canvas) canvas->setTextWrap(w); }
    
    void printCentered(String text, int y) {
        u8g2.setFontMode(1); 
        u8g2.setForegroundColor(0xFFFF); 
        int w = u8g2.getUTF8Width(text.c_str());
        int x = (M_WIDTH - w) / 2;
        u8g2.setCursor(x, y); 
        u8g2.print(text);
    }

    void drawScrollingText(String text, int y, int xPos, uint16_t color) {
        u8g2.setFontMode(1); 
        u8g2.setForegroundColor(color);
        u8g2.setCursor(xPos, y);
        u8g2.print(text);
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