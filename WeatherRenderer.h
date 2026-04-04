#pragma once
#include <Arduino.h>
#include "DisplayManager.h"

class WeatherRenderer {
private:
    uint16_t cYellow, cGold, cOrange, cBlue, cWhite, cLightGray, cDarkGray, cRain, cRed, cGreenLeaf, cMoon, cWindArrow;

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
        cMoon = d.color565(180, 180, 130); 
        cWindArrow = d.color565(255, 120, 120); // <--- NEU: Leichtes Rot für die Windrose
    }

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
        
        d.fillCircle(cx, cy, rCore, getPulseColor(d, cYellow, cOrange, f, 16));
        d.drawCircle(cx, cy, rCore, getPulseColor(d, cGold, cOrange, f, 16));

        int numRays = 8;
        float rayAngleOffset = f * (2.0f * PI / 64.0f); 

        for (int i = 0; i < numRays; i++) {
            float angle = i * (PI / (numRays/2.0f)) + rayAngleOffset;
            float pulseRatio = 0.8f + 0.2f * sin((f + i * 2) * (2.0f * PI / 16.0f)); 
            int rInner = rCore + (s * 0.1);
            int rOuter = rCore + (s * 0.25) * pulseRatio;

            uint16_t rayC = getPulseColor(d, cYellow, cGold, f + i * 2, 16);
            d.drawLine(cx + cos(angle)*rInner, cy + sin(angle)*rInner, cx + cos(angle)*rOuter, cy + sin(angle)*rOuter, rayC);
        }
    }

    void drawMoon(DisplayManager& d, int x, int y, int s, int f) {
        int cx = x + s/2, cy = y + s/2;
        int r = s * 0.35; 
        
        float maskXOffset = (s * 0.15); 
        float maskYOffset = (s * 0.1);

        for(int iy = -r; iy <= r; iy++) {
            for(int ix = -r; ix <= r; ix++) {
                if (ix*ix + iy*iy <= r*r) { 
                    int mx = ix - maskXOffset, my = iy - maskYOffset;
                    if (mx*mx + my*my > r*r) { 
                        d.drawPixel(cx + ix, cy + iy, cMoon); 
                    }
                }
            }
        }

        for (int i=0; i<4; i++) {
            float a = i * (PI/2.0f);
            d.drawPixel(cx + cos(a)*(r+1), cy + sin(a)*(r+1), d.color565(60, 60, 40)); 
        }

        d.drawPixel(x + s*0.8, y + s*0.2, getPulseColor(d, d.color565(30,30,30), cWhite, f, 16));
        d.drawPixel(x + s*0.2, y + s*0.8, getPulseColor(d, d.color565(30,30,30), cWhite, f+4, 16));
        d.drawPixel(x + s*0.5, y + s*0.1, getPulseColor(d, d.color565(10,10,10), cLightGray, f+8, 16)); 
    }

    // --- ERWEITERT: Kann nun horizontal oder vertikal schaukeln ---
    void drawCloud(DisplayManager& d, int x, int y, int s, int f, bool dark = false, bool horizontalMotion = false, float motionRatio = 1.0f, bool nightCloud = false) {
        float motionValue = sin(f * (2.0f * PI / 16.0f)) * motionRatio * (s * 0.05f);
        
        // Basis-Koordinaten
        int cxM = x + s * 0.5;
        int cxL = x + s * 0.25;
        int cxR = x + s * 0.75;
        int cy = y + s * 0.55; 

        if (horizontalMotion) {
            // NEU: Horizontal schaukeln
            cxM += motionValue;
            cxL += motionValue;
            cxR += motionValue;
        } else {
            // Altes vertikales Atmen
            cy -= motionValue; 
        }
        
        uint16_t cMain = dark ? (nightCloud ? d.color565(110,110,110) : d.color565(80,80,80)) : cLightGray;
        uint16_t cShadow = dark ? (nightCloud ? d.color565(80,80,80) : d.color565(50,50,50)) : d.color565(170,170,170);
        
        int rMid = s * 0.25, rSide = s * 0.15;
        
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
            for(int i = 0; i < 3; i++) {
                int dropX = x + s * 0.2 + (i * (s * 0.2));
                int dropY = startY + ((f + i * 4) % (int)(s * 0.4));
                d.drawLine(dropX, dropY, dropX, dropY + 1, cRain); 
            }
        } else if (type == "pouring") {
            for(int i = 0; i < 5; i++) {
                int dropXStart = x + s * 0.1 + (i * (s * 0.15));
                float dropFall = ((f + i * 2) * (s * 0.08f)); 
                int dropX = dropXStart + (dropFall * 0.1f); 
                int dropY = startY + ((int)dropFall % (int)(s * 0.4));
                d.drawLine(dropX, dropY, dropX + 1, dropY + 3, cRain); 
            }
        } else if (type == "snowy") {
            for(int i = 0; i < 4; i++) {
                float flakeFall = ((f + i * 3) * (s * 0.04f));
                int flakeY = startY + ((int)flakeFall % (int)(s * 0.4));
                float sway = sin((f + i * 4) * (2.0f * PI / 16.0f)) * (s * 0.08f); 
                int flakeX = x + s * 0.2 + (i * (s * 0.15)) + sway; 
                d.drawPixel(flakeX, flakeY, cWhite);
            }
        }
    }

    void drawLightning(DisplayManager& d, int x, int y, int s, int f, int swayOffset = 0) {
        // Blitz: Hält für 2-3 Frames, pulsierend weiß/gelb
        if (f == 1 || f == 2 || f == 6 || f == 7) {
            // NEU: swayOffset annehmen, damit der Blitz mit der Wolke schaukelt
            int cx = (x + s/2) + swayOffset, cy = y + s*0.5;
            uint16_t cL = (f%2==0) ? cWhite : cYellow;
            d.drawLine(cx, cy, cx - 3, cy + s*0.2, cL);
            d.drawLine(cx - 3, cy + s*0.2, cx + 2, cy + s*0.2, cL);
            d.drawLine(cx + 2, cy + s*0.2, cx - 2, cy + s*0.4, cL);
        }
    }

    void drawBlowingCloud(DisplayManager& d, int x, int y, int s, int f) {
        float swayX = sin(f * (2.0f * PI / 16.0f)) * (s * 0.06f); 
        int cloudS = s * 0.8;
        int cloudX = x + (s * 0.2) + swayX;
        
        // Vertikales Atmen deaktivieren (breathingRatio = 0.0f)
        drawCloud(d, cloudX, y, cloudS, f, false, false, 0.0f);

        for (int i = 0; i < 3; i++) {
            int lineY = y + s * 0.45 + (i * s * 0.15); 
            float t = ((f + (i * 5)) % 16) / 16.0f; 
            int startX = cloudX + (cloudS * 0.1) - (t * s * 0.8);
            int length = s * 0.2; 
            int endX = startX - length;

            uint8_t bright = 50 + sin(t * PI) * 150; 
            uint16_t windC = d.color565(bright, bright, bright);
            
            if (startX > x) {
                if (endX < x) endX = x; 
                d.drawLine(endX, lineY, startX, lineY, windC);
            }
        }
    }

    void drawFogRedesign(DisplayManager& d, int x, int y, int s, int f) {
        for(int i=0; i<4; i++) {
            int lineY = y + s*0.2 + (i * s*0.2) + (sin((f+i*2) * (2.0f * PI / 16.0f)) * 1.0f); 
            uint16_t c = getPulseColor(d, d.color565(50,50,50), d.color565(150,150,150), f + i*2, 16);
            d.drawLine(x + s*0.1, lineY, x + s*0.9, lineY, c);
            if (s > 16) d.drawPixel(x + s*(0.3f+i*0.1f), lineY+1, d.color565(30,30,30)); 
        }
    }

public:
    WeatherRenderer() {}

    // --- NEU: Redesign der Windrose als längliches Dreieck (Chevron) in Rot ---
    void drawWindRose(DisplayManager& d, int cx, int cy, int r, int windDir, int currentFrame) {
        initColors(d); 
        
        // Kompass-Ring mit N/O/S/W Markierungen
        d.drawCircle(cx, cy, r, d.color565(60, 60, 60));
        d.drawPixel(cx, cy - r, cLightGray); // Nord
        d.drawPixel(cx, cy + r, cLightGray); // Süd
        d.drawPixel(cx - r, cy, cLightGray); // West
        d.drawPixel(cx + r, cy, cLightGray); // Ost

        // Sinus-Schwankung (pendelt 15 Grad = PI/12)
        float dir_rad = windDir * (PI / 180.0f);
        float sway_rad = (PI / 12.0f) * sin(currentFrame * (2.0f * PI / 16.0f)); 
        float math_angle_rad = (PI / 2.0f) - dir_rad + sway_rad;

        // Geometrie des länglichen Dreiecks (Chevron)
        // Richtungsvektor und Senkrechte
        float vx_theta = cos(math_angle_rad);
        float vy_theta = -sin(math_angle_rad); // Screen coords adjustment
        float vx_perp = -vy_theta; // Senkrechte
        float vy_perp = vx_theta;

        // 1. Spitze (+0.9 Radius)
        int tx = cx + vx_theta * (r * 0.9f);
        int ty = cy + vy_theta * (r * 0.9f); 
        
        // 2. Rückseite Mitte (Einknickung, -0.3 Radius) -> Geht durch die Mitte
        int rearIndentX = cx - vx_theta * (r * 0.3f);
        int rearIndentY = cy - vy_theta * (r * 0.3f); 
        
        // 3. Rückseite Ecken (-0.6 Radius nach hinten, +/- 0.4 Radius zur Seite) -> Kürzer
        int rearLX = cx - vx_theta * (r * 0.6f) + vx_perp * (r * 0.4f);
        int rearLY = cy - vy_theta * (r * 0.6f) + vy_perp * (r * 0.4f);
        int rearRX = cx - vx_theta * (r * 0.6f) - vx_perp * (r * 0.4f);
        int rearRY = cy - vy_theta * (r * 0.6f) - vy_perp * (r * 0.4f);
        
        // Kontur zeichnen
        d.drawLine(tx, ty, rearLX, rearLY, cWindArrow);
        d.drawLine(rearLX, rearLY, rearIndentX, rearIndentY, cWindArrow);
        d.drawLine(rearIndentX, rearIndentY, rearRX, rearRY, cWindArrow);
        d.drawLine(rearRX, rearRY, tx, ty, cWindArrow);
    }

    void drawWeatherIcon(DisplayManager& display, int x, int y, int size, String cond, int frame) {
        initColors(display);
        int f = frame % 16; 

        if (cond == "sunny" || cond == "clear-day") drawSun(display, x, y, size, f);
        else if (cond == "clear-night") drawMoon(display, x, y, size, f);
        
        // NEU: cloudy schaukelt horizontal
        else if (cond == "cloudy") drawCloud(display, x, y, size, f, false, true, 1.0f);
        
        else if (cond == "partlycloudy") {
            drawSun(display, x - size*0.1, y - size*0.1, size*0.8, f + 4); 
            // NEU: partlycloudy Wolke schaukelt horizontal
            drawCloud(display, x + size*0.1, y + size*0.1, size*0.9, f, false, true, 0.5f);
        } else if (cond == "partlycloudy-night") {
            drawMoon(display, x - size*0.1, y - size*0.1, size*0.9, f); 
            // NEU: partlycloudy-night Wolke schaukelt horizontal
            drawCloud(display, x + size*0.1, y + size*0.2, size*0.8, f, true, true, 0.4f, true);
        } else if (cond == "rainy") {
            drawCloud(display, x, y - size*0.1, size, f, false); // Behält vertikales Atmen
            drawPrecipitation(display, x, y, size, f, "rainy");
        } else if (cond == "pouring") {
            drawCloud(display, x, y - size*0.1, size, f, true, false, 0.5f);
            drawPrecipitation(display, x, y, size, f, "pouring");
        } else if (cond == "snowy") {
            drawCloud(display, x, y - size*0.1, size, f, false);
            drawPrecipitation(display, x, y, size, f, "snowy");
        } 
        
        else if (cond == "lightning") {
            // NEU: lightning Wolke schaukelt horizontal
            drawCloud(display, x, y - size*0.1, size, f, true, true, 1.0f);
            // Blitz-Schaukeln berechnen
            int swayOffset = sin(f * (2.0f * PI / 16.0f)) * (size * 0.05f);
            drawLightning(display, x, y, size, f, swayOffset);
        } 
        
        else if (cond == "lightning-rain") {
            drawCloud(display, x, y - size*0.1, size, f, true);
            drawPrecipitation(display, x, y, size, f, "pouring"); 
            drawLightning(display, x, y, size, f);
        } else if (cond == "windy") {
            drawBlowingCloud(display, x, y, size, f);
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