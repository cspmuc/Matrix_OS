#pragma once
#include <Arduino.h>
#include "DisplayManager.h"

class WeatherRenderer {
private:
    uint16_t cYellow, cBlue, cWhite, cLightGray, cDarkGray, cRain, cRed;

    void initColors(DisplayManager& d) {
        cYellow = d.color565(255, 200, 0);
        cBlue = d.color565(100, 150, 255);
        cWhite = d.color565(255, 255, 255);
        cLightGray = d.color565(200, 200, 200);
        cDarkGray = d.color565(100, 100, 100);
        cRain = d.color565(50, 150, 255);
        cRed = d.color565(255, 50, 50);
    }

    void drawSun(DisplayManager& d, int x, int y, int s, int f) {
        int cx = x + s/2, cy = y + s/2, r = s * 0.25;
        d.fillCircle(cx, cy, r, cYellow);
        int pulse = (f % 2 == 0) ? (s * 0.05) : 0;
        int rayIn = r + (s * 0.1), rayOut = r + (s * 0.25) + pulse;
        for (int i = 0; i < 8; i++) {
            float a = i * (PI / 4.0);
            d.drawLine(cx + cos(a)*rayIn, cy + sin(a)*rayIn, cx + cos(a)*rayOut, cy + sin(a)*rayOut, cYellow);
        }
    }

    void drawMoon(DisplayManager& d, int x, int y, int s, int f) {
        int cx = x + s/2, cy = y + s/2, r = s * 0.3;
        int maskX = cx - (s * 0.1), maskY = cy - (s * 0.1); // Maske versetzt für Sichel
        // Sichel mathematisch zeichnen
        for(int iy = -r; iy <= r; iy++) {
            for(int ix = -r; ix <= r; ix++) {
                if (ix*ix + iy*iy <= r*r) { // Innerhalb Mond
                    int mx = (cx + ix) - maskX, my = (cy + iy) - maskY;
                    if (mx*mx + my*my > r*r) { // Außerhalb Maske
                        d.drawPixel(cx + ix, cy + iy, cBlue);
                    }
                }
            }
        }
        // Funkelnde Sterne
        if (f % 4 != 0) d.drawPixel(x + s*0.8, y + s*0.2, cWhite);
        if (f % 3 != 0) d.drawPixel(x + s*0.2, y + s*0.8, cWhite);
    }

    void drawCloud(DisplayManager& d, int x, int y, int s, int f, bool dark = false) {
        int bounce = (f < 4) ? (f / 2) : ((7 - f) / 2);
        int cy = (y + s * 0.55) - bounce;
        uint16_t c = dark ? cDarkGray : cLightGray;
        int rMid = s * 0.25, rSide = s * 0.15;
        int cxM = x + s * 0.5, cxL = x + s * 0.25, cxR = x + s * 0.75;
        
        d.fillCircle(cxM, cy - (s * 0.1), rMid, c);
        d.fillCircle(cxL, cy, rSide, c);
        d.fillCircle(cxR, cy, rSide, c);
        d.fillRect(cxL, cy - rSide, cxR - cxL, rSide + 1, c);
    }

    void drawRain(DisplayManager& d, int x, int y, int s, int f, bool heavy) {
        int startY = y + s * 0.6;
        for(int i = 0; i < (heavy ? 5 : 3); i++) {
            int dropX = x + s * 0.2 + (i * (s * 0.15));
            int dropY = startY + ((f * 2 + i * 3) % (int)(s * 0.4));
            d.drawLine(dropX, dropY, dropX, dropY + (heavy ? 3 : 2), cRain);
        }
    }

    void drawSnow(DisplayManager& d, int x, int y, int s, int f) {
        int startY = y + s * 0.6;
        for(int i = 0; i < 4; i++) {
            int flakeY = startY + ((f + i * 2) % (int)(s * 0.4));
            int flakeX = x + s * 0.2 + (i * (s * 0.15)) + (f % 2 == 0 ? 1 : 0); // Wackeln
            d.drawPixel(flakeX, flakeY, cWhite);
        }
    }

    void drawLightning(DisplayManager& d, int x, int y, int s, int f) {
        if (f == 1 || f == 2 || f == 5) {
            int cx = x + s/2, cy = y + s*0.5;
            d.drawLine(cx, cy, cx - 3, cy + s*0.2, cYellow);
            d.drawLine(cx - 3, cy + s*0.2, cx + 2, cy + s*0.2, cYellow);
            d.drawLine(cx + 2, cy + s*0.2, cx - 2, cy + s*0.4, cYellow);
        }
    }

    void drawWind(DisplayManager& d, int x, int y, int s, int f) {
        int offset1 = (f * 2) % s;
        int offset2 = (f * 3) % s;
        d.drawLine(x + offset1, y + s*0.4, x + offset1 + s*0.3, y + s*0.4, cLightGray);
        d.drawLine(x + offset2, y + s*0.6, x + offset2 + s*0.4, y + s*0.6, cLightGray);
        // Fliegendes Blatt
        d.drawPixel(x + ((f*4)%s), y + s*0.5 + (f%2==0?1:-1), d.color565(50, 200, 50));
    }

public:
    WeatherRenderer() {}

    void drawWeatherIcon(DisplayManager& display, int x, int y, int size, String cond, int frame) {
        initColors(display);
        int f = frame % 8;

        if (cond == "clear-day" || cond == "sunny") {
            drawSun(display, x, y, size, f);
        } else if (cond == "clear-night") {
            drawMoon(display, x, y, size, f);
        } else if (cond == "cloudy") {
            drawCloud(display, x, y, size, f, false);
        } else if (cond == "partlycloudy") {
            drawSun(display, x - size*0.1, y - size*0.1, size*0.8, f);
            drawCloud(display, x + size*0.1, y + size*0.1, size*0.9, f, false);
        } else if (cond == "partlycloudy-night") {
            drawMoon(display, x - size*0.1, y - size*0.1, size*0.8, f);
            drawCloud(display, x + size*0.1, y + size*0.1, size*0.9, f, false);
        } else if (cond == "rainy") {
            drawCloud(display, x, y - size*0.1, size, f, false);
            drawRain(display, x, y, size, f, false);
        } else if (cond == "pouring") {
            drawCloud(display, x, y - size*0.1, size, f, true);
            drawRain(display, x, y, size, f, true);
        } else if (cond == "snowy") {
            drawCloud(display, x, y - size*0.1, size, f, false);
            drawSnow(display, x, y, size, f);
        } else if (cond == "lightning") {
            drawCloud(display, x, y - size*0.1, size, f, true);
            drawLightning(display, x, y, size, f);
        } else if (cond == "lightning-rain") {
            drawCloud(display, x, y - size*0.1, size, f, true);
            drawRain(display, x, y, size, f, true);
            drawLightning(display, x, y, size, f);
        } else if (cond == "windy") {
            drawWind(display, x, y, size, f);
        } else if (cond == "fog") {
            // Nebel: Langsam wabernde Linien
            int y1 = y + size*0.4 + (f%2);
            int y2 = y + size*0.7 - (f%2);
            display.drawLine(x + size*0.1, y1, x + size*0.9, y1, cDarkGray);
            display.drawLine(x + size*0.2, y2, x + size*0.8, y2, cLightGray);
        } else if (cond == "exceptional") {
            // Warnung (Ausrufezeichen pulsierend)
            int r = size * 0.3 + (f%2);
            display.fillCircle(x + size/2, y + size/2, r, cRed);
            display.drawLine(x + size/2, y + size*0.3, x + size/2, y + size*0.6, cWhite);
            display.drawPixel(x + size/2, y + size*0.75, cWhite);
        } else {
            // Unbekannt
            display.drawRect(x, y, size, size, cRed);
        }
    }
};