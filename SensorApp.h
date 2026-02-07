#pragma once
#include "App.h"
#include "RichText.h"
#include <map>
#include <vector>

// Datenstruktur für einen einzelnen Wert
struct SensorItem {
    String icon;  // z.B. "temp", "rain"
    String text;  // z.B. "22.5 °C"
    String color; // z.B. "warm", "blue"
};

// Datenstruktur für eine ganze Seite
struct SensorPage {
    String title;
    int layoutType; // 0=Auto, 1=Big, 2=List, 4=Grid
    unsigned long ttl; // Time to live in Sekunden
    unsigned long lastReceived;
    std::vector<SensorItem> items;
};

class SensorApp : public App {
private:
    RichText richText;
    
    // Speicher für alle Seiten (ID -> Page)
    std::map<String, SensorPage> pages;
    
    // Navigation
    std::map<String, SensorPage>::iterator currentPageIt;
    unsigned long lastPageSwitch = 0;
    const int SWITCH_DELAY = 5000; // 5 Sekunden pro Seite

public:
    SensorApp() {
        currentPageIt = pages.begin();
    }

    // Wird vom NetworkManager aufgerufen, um Daten zu speichern
    void updatePage(String id, String title, int ttl, const std::vector<SensorItem>& newItems) {
        SensorPage& p = pages[id]; // Erstellt neu oder holt existierende
        p.title = title;
        p.ttl = ttl;
        p.items = newItems;
        p.lastReceived = millis();
        
        // Layout Logik: Auto-Detect anhand der Item-Anzahl
        if (newItems.size() == 1) p.layoutType = 1; // Big
        else if (newItems.size() == 2) p.layoutType = 2; // List
        else p.layoutType = 4; // Grid (bis 4 Items)
        
        // Fix: Falls die Liste leer war und currentPageIt ungültig ist
        if (pages.size() == 1) currentPageIt = pages.begin();
    }

    void draw(DisplayManager& display) override {
        unsigned long now = millis();

        // 1. GARBAGE COLLECTION
        auto it = pages.begin();
        while (it != pages.end()) {
            // Wenn TTL abgelaufen (TTL * 1000 ms)
            if (now - it->second.lastReceived > (it->second.ttl * 1000)) {
                
                bool isCurrent = (it == currentPageIt);
                
                // WICHTIG: it wird VOR dem Zugriff aktualisiert
                it = pages.erase(it); 
                
                // Wenn die aktive Seite gelöscht wurde, Zeiger korrigieren
                if (isCurrent) {
                    currentPageIt = it;
                    if (currentPageIt == pages.end()) {
                        currentPageIt = pages.begin();
                    }
                }
            } else {
                ++it;
            }
        }

        // Wenn keine Seiten da sind -> Leermeldung
        if (pages.empty()) {
            // Y=39 ist die neue optische Mitte
            richText.drawCentered(display, 39, "{c:muted}No Sensor Data", "Small");
            return;
        }

        // 2. Rotation: Nächste Seite wählen
        if (now - lastPageSwitch > SWITCH_DELAY) {
            lastPageSwitch = now;
            currentPageIt++;
            if (currentPageIt == pages.end()) currentPageIt = pages.begin();
        }
        
        // Sicherheitscheck
        if (currentPageIt == pages.end()) currentPageIt = pages.begin();

        // 3. Zeichnen
        if (currentPageIt != pages.end()) {
            drawPage(display, currentPageIt->second);
        }
        
        // Page Indicator (Kleine Punkte unten, wenn > 1 Seite)
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
        // --- HEADER ---
        // KORREKTUR: Zurück auf 12 (Perfekt für Umlaute)
        richText.drawCentered(display, 12, "{c:silver}" + p.title, "Small");
        // KORREKTUR: Zurück auf 15
        display.drawFastHLine(0, 15, M_WIDTH, display.color565(50, 50, 50));

        // --- CONTENT (Bleibt tiefer) ---
        
        // LAYOUT 1: SINGLE BIG
        if (p.layoutType == 1 && p.items.size() > 0) {
            SensorItem& item = p.items[0];
            String content = "{c:" + item.color + "}";
            if(item.icon != "") content += "{" + item.icon + "} ";
            content += "{b}" + item.text;
            // Baseline 47 (Tiefer als ursprünglich 46)
            richText.drawCentered(display, 47, content, "Large");
        }
        
        // LAYOUT 2: LIST
        else if (p.layoutType == 2) {
            int yPos = 33; // Start auf 33 (Tiefer als ursprünglich 32)
            for (const auto& item : p.items) {
                String content = "{c:" + item.color + "}";
                if(item.icon != "") content += "{" + item.icon + "} ";
                content += "{c:white}" + item.text;
                richText.drawCentered(display, yPos, content, "Medium");
                yPos += 22; 
            }
        }
        
        // LAYOUT 4: GRID
        else {
            int i = 0;
            for (const auto& item : p.items) {
                if (i > 3) break; 
                
                int row = i / 2;
                int col = i % 2;
                
                int xBase = col * 64; 
                // Start Y auf 31 (Tiefer als ursprünglich 30)
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