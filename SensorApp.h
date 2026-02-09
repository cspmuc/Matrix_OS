#pragma once
#include "App.h"
#include "RichText.h"
#include <map>
#include <vector>

// CLEANUP: Kein Mutex mehr nötig

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

public:
    SensorApp() {
        currentPageIt = pages.begin();
    }

    // CLEANUP: Mutex Lock entfernt
    void updatePage(String id, String title, int ttl, const std::vector<SensorItem>& newItems) {
        SensorPage& p = pages[id];
        
        p.title = title;
        p.ttl = ttl;
        p.items = newItems;
        p.lastReceived = millis();
        
        if (newItems.size() == 1) p.layoutType = 1; 
        else if (newItems.size() == 2) p.layoutType = 2; 
        else p.layoutType = 4; 
        
        if (pages.size() == 1) currentPageIt = pages.begin();
    }

    void draw(DisplayManager& display) override {
        unsigned long now = millis();

        // --- A. GARBAGE COLLECTION ---
        auto it = pages.begin();
        while (it != pages.end()) {
            if (now - it->second.lastReceived > (it->second.ttl * 1000)) {
                bool isCurrent = (it == currentPageIt);
                it = pages.erase(it); 
                if (isCurrent) {
                    currentPageIt = it;
                    if (currentPageIt == pages.end()) currentPageIt = pages.begin();
                }
            } else {
                ++it;
            }
        }

        // --- B. LEER-CHECK ---
        if (pages.empty()) {
            richText.drawCentered(display, 39, "{c:muted}No Sensor Data", "Small");
            return;
        }

        // --- C. ROTATION ---
        if (now - lastPageSwitch > SWITCH_DELAY) {
            lastPageSwitch = now;
            currentPageIt++;
            if (currentPageIt == pages.end()) currentPageIt = pages.begin();
        }
        
        if (currentPageIt == pages.end()) currentPageIt = pages.begin();

        // --- D. ZEICHNEN ---
        if (currentPageIt != pages.end()) {
            drawPage(display, currentPageIt->second);
        }
        
        // --- E. PAGE INDICATOR ---
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
    }

private:
    void drawPage(DisplayManager& display, SensorPage& p) {
        // (Zeichnen-Logik bleibt identisch wie vorher)
        richText.drawCentered(display, 12, "{c:peach}" + p.title, "Small");
        display.drawFastHLine(0, 15, M_WIDTH, display.color565(50, 50, 50));

        if (p.layoutType == 1 && p.items.size() > 0) {
            SensorItem& item = p.items[0];
            String content = "{c:" + item.color + "}";
            if(item.icon != "") content += "{" + item.icon + "} ";
            content += "{b}" + item.text;
            richText.drawCentered(display, 47, content, "Large");
        }
        else if (p.layoutType == 2) {
            int yPos = 33;
            for (const auto& item : p.items) {
                String content = "{c:" + item.color + "}";
                if(item.icon != "") content += "{" + item.icon + "} ";
                content += "{c:white}" + item.text;
                richText.drawCentered(display, yPos, content, "Medium");
                yPos += 22; 
            }
        }
        else {
            int i = 0;
            for (const auto& item : p.items) {
                if (i > 3) break; 
                int row = i / 2;
                int col = i % 2;
                int xBase = col * 64; 
                int yBase = 31 + (row * 22); 
                String content = "{c:" + item.color + "}";
                if(item.icon != "") content += "{" + item.icon + "} ";
                content += "{c:white}" + item.text; 
                richText.drawString(display, xBase + 4, yBase, content, "Small");
                i++;
            }
        }
    }
};