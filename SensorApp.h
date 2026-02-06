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
    }

    void draw(DisplayManager& display) override {
        unsigned long now = millis();

        // 1. Garbage Collection: Alte Seiten löschen
        auto it = pages.begin();
        while (it != pages.end()) {
            // Wenn TTL abgelaufen (TTL * 1000 ms)
            if (now - it->second.lastReceived > (it->second.ttl * 1000)) {
                // Wenn wir gerade diese Seite anzeigen, Iterator retten
                if (it == currentPageIt) {
                    currentPageIt = pages.erase(it);
                    if (currentPageIt == pages.end()) currentPageIt = pages.begin();
                } else {
                    it = pages.erase(it);
                }
            } else {
                ++it;
            }
        }

        // Wenn keine Seiten da sind -> Leermeldung
        if (pages.empty()) {
            richText.drawCentered(display, 35, "{c:muted}No Sensor Data", "Small");
            return;
        }

        // 2. Rotation: Nächste Seite wählen
        if (now - lastPageSwitch > SWITCH_DELAY) {
            lastPageSwitch = now;
            currentPageIt++;
            if (currentPageIt == pages.end()) currentPageIt = pages.begin();
        }
        
        // Sicherheitscheck, falls Iterator ungültig wurde
        if (currentPageIt == pages.end()) currentPageIt = pages.begin();

        // 3. Zeichnen
        drawPage(display, currentPageIt->second);
        
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
        // Linie unter dem Titel
        display.drawFastHLine(0, 13, M_WIDTH, display.color565(50, 50, 50));
        // Titel Zentriert
        richText.drawCentered(display, 10, "{c:silver}" + p.title, "Small");

        // --- CONTENT ---
        int yStart = 16; 
        
        // LAYOUT 1: SINGLE BIG (Ein riesiger Wert)
        if (p.layoutType == 1 && p.items.size() > 0) {
            SensorItem& item = p.items[0];
            // Icon links riesig, Text rechts riesig? 
            // Oder einfach zentriert "Large"
            String content = "{c:" + item.color + "}";
            if(item.icon != "") content += "{" + item.icon + "} ";
            content += "{b}" + item.text; // Text fett
            
            richText.drawCentered(display, 42, content, "Large");
        }
        
        // LAYOUT 2: LIST (2 Zeilen untereinander)
        else if (p.layoutType == 2) {
            int yPos = 30;
            for (const auto& item : p.items) {
                String content = "{c:" + item.color + "}";
                if(item.icon != "") content += "{" + item.icon + "} ";
                content += "{c:white}" + item.text;
                
                // Links zentriert via drawBox/String oder mittig? 
                // Mittig sieht bei Listen oft gut aus auf kleinen Screens
                richText.drawCentered(display, yPos, content, "Medium");
                yPos += 22; // Zeilenabstand
            }
        }
        
        // LAYOUT 4: GRID (2x2 Raster)
        else {
            // Wir haben 4 Slots: (0,0), (1,0), (0,1), (1,1)
            // Breite pro Slot: 64px. Höhe: 24px.
            int i = 0;
            for (const auto& item : p.items) {
                if (i > 3) break; // Max 4 Items
                
                int row = i / 2;
                int col = i % 2;
                
                int xBase = col * 64; 
                int yBase = 28 + (row * 22); // Start Y (unter Header)
                
                // Aufbau: Icon links, Wert rechts daneben
                String content = "{c:" + item.color + "}";
                if(item.icon != "") content += "{" + item.icon + "} ";
                content += "{c:white}" + item.text; // Wert in Weiß
                
                // Wir nutzen drawString, um es im Slot zu zentrieren? 
                // Nein, linksbündig im Slot mit Padding sieht sauberer aus.
                // Padding left: 4px
                richText.drawString(display, xBase + 4, yBase, content, "Small");
                
                i++;
            }
            // Vertikale Trennlinie (optional)
            // display.drawFastVLine(64, 16, 48, display.color565(30,30,30));
        }
    }
};