#pragma once
#include "App.h"
#include "RichText.h"
#include <map>
#include <vector>

extern DisplayManager display;

struct SensorItem {
    String icon;
    String text;
    String color;
};

struct SensorPage {
    String title;
    int layoutType;
    unsigned long ttl; 
    unsigned long lastReceived;
    std::vector<SensorItem> items;
};

class SensorApp : public App {
private:
    RichText richText;
    std::map<String, SensorPage> pages;
    std::map<String, SensorPage>::iterator currentPageIt;
    unsigned long lastPageSwitch = 0;
    const int SWITCH_DELAY = 5000; 

    bool needsRedraw = true;

public:
    SensorApp() { currentPageIt = pages.begin(); }

    void updatePage(String id, String title, int ttl, const std::vector<SensorItem>& newItems) {
        SensorPage& p = pages[id];
        p.title = title; p.ttl = ttl; p.items = newItems; p.lastReceived = millis();
        if (newItems.size() == 1) p.layoutType = 1; 
        else if (newItems.size() == 2) p.layoutType = 2; 
        else p.layoutType = 4; 
        if (pages.size() == 1) currentPageIt = pages.begin();
        
        needsRedraw = true;
    }

    bool draw(DisplayManager& display, bool force) override {
        unsigned long now = millis();
        
        // 1. Garbage Collection
        auto it = pages.begin();
        bool listChanged = false;
        while (it != pages.end()) {
            if (now - it->second.lastReceived > (it->second.ttl * 1000)) {
                bool isCurrent = (it == currentPageIt);
                it = pages.erase(it); 
                listChanged = true;
                if (isCurrent) {
                    currentPageIt = it;
                    if (currentPageIt == pages.end()) currentPageIt = pages.begin();
                }
            } else { ++it; }
        }
        if (listChanged) needsRedraw = true;

        // 2. Leere Liste
        if (pages.empty()) {
            if (force || needsRedraw) {
                display.clear();
                richText.drawCentered(display, 39, "{c:muted}No Sensor Data", "Small");
                needsRedraw = false;
                return true;
            }
            return false;
        }

        // 3. Seitenwechsel
        if (now - lastPageSwitch > SWITCH_DELAY) {
            lastPageSwitch = now;
            currentPageIt++;
            if (currentPageIt == pages.end()) currentPageIt = pages.begin();
            needsRedraw = true; 
        }
        if (currentPageIt == pages.end()) currentPageIt = pages.begin();

        // 4. Update Check
        if (!force && !needsRedraw) {
            return false; 
        }

        // 5. Zeichnen
        display.clear(); 

        if (currentPageIt != pages.end()) {
            drawPage(display, currentPageIt->second);
        }
        
        // Page Indicators
        if (pages.size() > 1) {
            int totalW = pages.size() * 4; 
            int startX = (M_WIDTH - totalW) / 2;
            int idx = 0;
            for(auto pIt = pages.begin(); pIt != pages.end(); ++pIt) {
                uint16_t c = (pIt == currentPageIt) ? COL_WHITE : display.color565(50,50,50);
                display.drawPixel(startX + (idx * 4), 63, c);
                display.drawPixel(startX + (idx * 4) + 1, 63, c);
                idx++;
            }
        }
        
        needsRedraw = false;
        return true;
    }

private:
    void drawPage(DisplayManager& display, SensorPage& p) {
        richText.drawCentered(display, 12, "{c:peach}" + p.title, "Small");
        display.drawFastHLine(0, 15, M_WIDTH, display.color565(50, 50, 50));
        
        if (p.layoutType == 1 && p.items.size() > 0) {
            SensorItem& item = p.items[0];
            // FIX: Farbe explizit auflösen (unterstützt jetzt "gold", "Gold" etc.)
            uint16_t color = richText.getColorByName(display, item.color);
            
            String content = "";
            if(item.icon != "") content += "{" + item.icon + "} ";
            content += "{b}" + item.text;
            
            // Übergabe der Farbe als Default Color
            richText.drawCentered(display, 46, content, "Medium", color);
            
        } else if (p.layoutType == 2) {
             int yPos = 33;
            for (const auto& item : p.items) {
                uint16_t color = richText.getColorByName(display, item.color);
                String content = "";
                if(item.icon != "") content += "{" + item.icon + "} ";
                content += item.text; // Kein {c:white} Zwang mehr
                richText.drawCentered(display, yPos, content, "Medium", color);
                yPos += 22; 
            }
        } else {
             int i = 0;
            for (const auto& item : p.items) {
                if (i > 3) break; 
                uint16_t color = richText.getColorByName(display, item.color);
                int row = i / 2; int col = i % 2;
                int xBase = col * 64; int yBase = 31 + (row * 22); 
                
                String content = "";
                if(item.icon != "") content += "{" + item.icon + "} ";
                content += item.text; 
                richText.drawString(display, xBase + 4, yBase, content, "Small", color);
                i++;
            }
        }
    }
};