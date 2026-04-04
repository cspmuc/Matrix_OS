#pragma once
#include <Arduino.h>
#include "DisplayManager.h"

class WeatherRenderer {
private:
    uint16_t cYellow, cGold, cOrange, cBlue, cWhite, cLightGray, cDarkGray, cRain, cRed, cGreenLeaf, cMoon;

    void initColors(DisplayManager& d) {
        cYellow = d.color565(255, 220, 0);
        cGold = d.color565(255, 180, 0);
        cOrange = d.color565(255, 120, 0);
        cBlue = d.color565(50, 100, 255);
        cWhite = d.color565(255, 255, 255);
        cLightGray = d.color565(200, 200, 200);
        cDarkGray = d.color565(100, 100, 100);
        cRain = d.color565(80, 150, 255);
        cRed = d.color565(255, 50, 50);
        cGreenLeaf = d.color565(50, 200, 50);
        cMoon = d.color565(180, 180, 130); // <--- NEU: Grau-Gelb für den Mond
    }

    // Hilfsfunktion: Pulsierende RGBA-Farbe erzeugen
    uint16_t getPulseColor(DisplayManager& d, uint16_t color1, uint16_t color2, int f, int totalFrames) {
        float ratio = 0.5f + 0.5f * sin(f * (2.0f * PI / totalFrames));
        uint8_t r1 = (color1 >> 11) << 3, g1 = ((color1 >> 5) & 0x3F) << 2, b1 = (color1 & 0x1F) << 3;
        uint8_t r2 = (color2 >> 11) << 3, g2 = ((color2 >> 5) & 0x3F) << 2, b2 = (color2 & 0x1F) << 3;
        uint8_t r = r1 + ratio * (r2 - r1), g = g1 + ratio * (g2 - g1), b = b1 + ratio * (b2 - b1);
        return d.color565(r, g, b);
    }

    void drawSun(DisplayManager& d, int x, int y, int s, int f, bool pulsating = false) {
        int cx = x + s/2, cy = y + s/2;
        int rCore = s * 0.25;
        
        // Kern mit weicher Kante
        d.fillCircle(cx, cy, rCore, getPulseColor(d, cYellow, cOrange, f, 16));
        d.drawCircle(cx, cy, rCore, getPulseColor(d, cGold, cOrange, f, 16));

        // Pulsierende Strahlen mit sanfter Rotation
        int numRays = 8;
        float rayAngleOffset = f * (2.0f * PI / 64.0f); // Langsame Rotation

        for (int i = 0; i < numRays; i++) {
            float angle = i * (PI / (numRays/2.0f)) + rayAngleOffset;
            
            // Sub-Pixel-Pulsieren
            float pulseRatio = 0.8f + 0.2f * sin((f + i * 2) * (2.0f * PI / 16.0f)); 
            int rInner = rCore + (s * 0.1);
            int rOuter = rCore + (s * 0.25) * pulseRatio;

            uint16_t rayC = getPulseColor(d, cYellow, cGold, f + i * 2, 16);
            d.drawLine(cx + cos(angle)*rInner, cy + sin(angle)*rInner, cx + cos(angle)*rOuter, cy + sin(angle)*rOuter, rayC);
        }
    }

    void drawMoon(DisplayManager& d, int x, int y, int s, int f) {
        int cx = x + s/2, cy = y + s/2;
        int r = s * 0.35; // Größer und sichtbarer
        
        // Mond zeichnen (Sichel mathematisch)
        float maskXOffset = (s * 0.15); // Maske versetzt für Sichel
        float maskYOffset = (s * 0.1);

        for(int iy = -r; iy <= r; iy++) {
            for(int ix = -r; ix <= r; ix++) {
                if (ix*ix + iy*iy <= r*r) { // Innerhalb Mond
                    int mx = ix - maskXOffset, my = iy - maskYOffset;
                    if (mx*mx + my*my > r*r) { // Außerhalb Maske
                        d.drawPixel(cx + ix, cy + iy, cMoon); // <--- NEU: Grau-Gelb
                    }
                }
            }
        }

        // Weicher "Glow"-Rand (1-Pixel-Punkte, halbtransparent simuliert)
        for (int i=0; i<4; i++) {
            float a = i * (PI/2.0f);
            d.drawPixel(cx + cos(a)*(r+1), cy + sin(a)*(r+1), d.color565(60, 60, 40)); // Glow passend zum Mond
        }

        // 3 weich funkelnde Sterne (mit Fade-In/Out)
        d.drawPixel(x + s*0.8, y + s*0.2, getPulseColor(d, d.color565(30,30,30), cWhite, f, 16));
        d.drawPixel(x + s*0.2, y + s*0.8, getPulseColor(d, d.color565(30,30,30), cWhite, f+4, 16));
        d.drawPixel(x + s*0.5, y + s*0.1, getPulseColor(d, d.color565(10,10,10), cLightGray, f+8, 16)); 
    }

    // NEU: Der Schalter "nightCloud" macht die dunkle Wolke etwas heller
    void drawCloud(DisplayManager& d, int x, int y, int s, int f, bool dark = false, float breathingRatio = 1.0f, bool nightCloud = false) {
        // Weiches Atmen: Sinus-Welle (0 bis 2 Pixel bei 24px)
        float breathe = sin(f * (2.0f * PI / 16.0f)) * breathingRatio * (s * 0.05f);
        int cy = (y + s * 0.55) - breathe;
        
        // Farben dynamisch anpassen
        uint16_t cMain = dark ? (nightCloud ? d.color565(110,110,110) : d.color565(80,80,80)) : cLightGray;
        uint16_t cShadow = dark ? (nightCloud ? d.color565(80,80,80) : d.color565(50,50,50)) : d.color565(170,170,170);
        
        int rMid = s * 0.25, rSide = s * 0.15;
        int cxM = x + s * 0.5, cxL = x + s * 0.25, cxR = x + s * 0.75;
        
        // Kreise mit dezentem Schatten
        d.fillCircle(cxM, cy - (s * 0.1), rMid, cMain);
        d.drawCircle(cxM, cy - (s * 0.1), rMid, cShadow);
        
        d.fillCircle(cxL, cy, rSide, cMain);
        d.fillCircle(cxR, cy, rSide, cMain);
        
        // Den Boden mit Schatten glätten
        d.fillRect(cxL, cy - rSide, cxR - cxL, rSide + 1, cMain);
        d.drawLine(cxL, cy + rSide, cxR, cy + rSide, cShadow);
    }

    void drawPrecipitation(DisplayManager& d, int x, int y, int s, int f, String type) {
        int startY = y + s * 0.6;
        
        if (type == "rainy") {
            // Leicht: Weniger Tropfen, aber ein wenig länger (drawLine statt drawPixel)
            for(int i = 0; i < 3; i++) {
                int dropX = x + s * 0.2 + (i * (s * 0.2));
                int dropY = startY + ((f + i * 4) % (int)(s * 0.4));
                d.drawLine(dropX, dropY, dropX, dropY + 1, cRain); // <--- Längerer Regenstrich
            }
        } else if (type == "pouring") {
            // Stark: Mehr, längere Linien, fallen schnell und leicht schräg (Wind)
            for(int i = 0; i < 5; i++) {
                int dropXStart = x + s * 0.1 + (i * (s * 0.15));
                float dropFall = ((f + i * 2) * (s * 0.08f)); // Sub-Pixel-Falleffekt
                int dropX = dropXStart + (dropFall * 0.1f); // Schrägfall
                int dropY = startY + ((int)dropFall % (int)(s * 0.4));
                d.drawLine(dropX, dropY, dropX + 1, dropY + 3, cRain); // Längere Striche
            }
        } else if (type == "snowy") {
            // Schnee: Kleine Punkte, die tanzen (Sinus-Kurve)
            for(int i = 0; i < 4; i++) {
                float flakeFall = ((f + i * 3) * (s * 0.04f));
                int flakeY = startY + ((int)flakeFall % (int)(s * 0.4));
                float sway = sin((f + i * 4) * (2.0f * PI / 16.0f)) * (s * 0.08f); // Tanzen (Sub-Pixel)
                int flakeX = x + s * 0.2 + (i * (s * 0.15)) + sway; 
                d.drawPixel(flakeX, flakeY, cWhite);
            }
        }
    }

    void drawLightning(DisplayManager& d, int x, int y, int s, int f) {
        // Blitz: Hält für 2-3 Frames, pulsierend weiß/gelb
        if (f == 1 || f == 2 || f == 6 || f == 7) {
            int cx = x + s/2, cy = y + s*0.5;
            uint16_t cL = (f%2==0) ? cWhite : cYellow;
            d.drawLine(cx, cy, cx - 3, cy + s*0.2, cL);
            d.drawLine(cx - 3, cy + s*0.2, cx + 2, cy + s*0.2, cL);
            d.drawLine(cx + 2, cy + s*0.2, cx - 2, cy + s*0.4, cL);
        }
    }

    void drawWindRedesign(DisplayManager& d, int x, int y, int s, int f) {
        // Redesign: Ein fliegendes Pixel-Blatt wirbelt in einer Acht. Abstrakte Windlinien dahinter.
        for(int i=0; i<3; i++) {
            int lineY = y + s*0.2 + (i * s*0.3);
            int startOffset = (f * (2 + i)) % s;
            int length = s * (0.3f + (i * 0.1f));
            d.drawLine(x + startOffset, lineY, x + (startOffset + length) % s, lineY, getPulseColor(d, d.color565(30,30,30), d.color565(150,150,150), f+i*3, 16));
        }

        float t = f * (2.0f * PI / 16.0f);
        float leafX = x + s*0.5 + (sin(t) * s*0.35f);
        float leafY = y + s*0.5 + (sin(t*2.0f) * s*0.2f);
        d.fillCircle(leafX, leafY, 1, getPulseColor(d, cGreenLeaf, d.color565(20, 100, 20), f, 16)); 
    }

    void drawFogRedesign(DisplayManager& d, int x, int y, int s, int f) {
        // Redesign: Wabern. Sanft pulsierende, überlappende Linien, sehr geringer Kontrast.
        for(int i=0; i<4; i++) {
            int lineY = y + s*0.2 + (i * s*0.2) + (sin((f+i*2) * (2.0f * PI / 16.0f)) * 1.0f); 
            uint16_t c = getPulseColor(d, d.color565(50,50,50), d.color565(150,150,150), f + i*2, 16);
            d.drawLine(x + s*0.1, lineY, x + s*0.9, lineY, c);
            if (s > 16) d.drawPixel(x + s*(0.3f+i*0.1f), lineY+1, d.color565(30,30,30)); 
        }
    }

public:
    WeatherRenderer() {}

    void drawWeatherIcon(DisplayManager& display, int x, int y, int size, String cond, int frame) {
        initColors(display);
        int f = frame % 16; 

        if (cond == "sunny" || cond == "clear-day") {
            drawSun(display, x, y, size, f);
        } else if (cond == "clear-night") {
            drawMoon(display, x, y, size, f);
        } else if (cond == "cloudy") {
            drawCloud(display, x, y, size, f, false);
        } else if (cond == "partlycloudy") {
            drawSun(display, x - size*0.1, y - size*0.1, size*0.8, f + 4); 
            drawCloud(display, x + size*0.1, y + size*0.1, size*0.9, f, false, 0.5f);
        } else if (cond == "partlycloudy-night") {
            // NEU: Nacht-Wolke etwas heller (nightCloud = true)
            drawMoon(display, x - size*0.1, y - size*0.1, size*0.9, f); 
            drawCloud(display, x + size*0.1, y + size*0.2, size*0.8, f, true, 0.4f, true);
        } else if (cond == "rainy") {
            drawCloud(display, x, y - size*0.1, size, f, false);
            drawPrecipitation(display, x, y, size, f, "rainy");
        } else if (cond == "pouring") {
            drawCloud(display, x, y - size*0.1, size, f, true, 0.5f);
            drawPrecipitation(display, x, y, size, f, "pouring");
        } else if (cond == "snowy") {
            drawCloud(display, x, y - size*0.1, size, f, false);
            drawPrecipitation(display, x, y, size, f, "snowy");
        } else if (cond == "lightning") {
            drawCloud(display, x, y - size*0.1, size, f, true);
            drawLightning(display, x, y, size, f);
        } else if (cond == "lightning-rain") {
            drawCloud(display, x, y - size*0.1, size, f, true);
            drawPrecipitation(display, x, y, size, f, "pouring"); 
            drawLightning(display, x, y, size, f);
        } else if (cond == "windy") {
            drawWindRedesign(display, x, y, size, f);
        } else if (cond == "fog") {
            drawFogRedesign(display, x, y, size, f);
        } else if (cond == "exceptional") {
            int r = size * 0.3 + (f%2);
            display.fillCircle(x + size/2, y + size/2, r, cRed);
            display.drawLine(x + size/2, y + size*0.3, x + size/2, y + size*0.6, cWhite);
            display.drawPixel(x + size/2, y + size*0.75, cWhite);
        } else {
            display.drawRect(x, y, size, size, cRed);
        }
    }
};