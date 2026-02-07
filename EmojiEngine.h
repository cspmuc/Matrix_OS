#pragma once
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <map>
#include "DisplayManager.h"

// Definition eines Icons
struct IconDef {
    String sheetName; // Name des Sheets (z.B. "dotto") oder leer für Einzeldatei
    int index;        // Index im Sheet
    String filename;  // Falls Einzeldatei (Überschreibt Sheet)
};

// Definition eines Sheets (Raster-Grafik)
struct SheetDef {
    String filename;
    int cols;
    int rows;
};

class EmojiEngine {
private:
    std::map<String, IconDef> iconMap;
    std::map<String, SheetDef> sheetMap;
    int defaultSize = 16;
    bool ready = false;

public:
    void begin() {
        if (!LittleFS.begin(true)) {
            Serial.println("LittleFS Mount Failed");
            return;
        }
        loadCatalog();
        ready = true;
    }

    void loadCatalog() {
        if (!LittleFS.exists("/catalog.json")) {
            Serial.println("Catalog not found!");
            return;
        }

        File file = LittleFS.open("/catalog.json", "r");
        
        // JSON Parsen (Puffer für ESP32 groß genug wählen)
        DynamicJsonDocument doc(4096); 
        DeserializationError error = deserializeJson(doc, file);
        
        if (error) {
            Serial.print("JSON Error: "); Serial.println(error.c_str());
            return;
        }

        // 1. Sheets laden
        JsonObject sheets = doc["sheets"];
        for (JsonPair kv : sheets) {
            String name = kv.key().c_str();
            JsonObject val = kv.value();
            sheetMap[name] = {
                val["file"].as<String>(),
                val["cols"] | 16, 
                val["rows"] | 16  
            };
            Serial.printf("Sheet geladen: %s -> %s\n", name.c_str(), sheetMap[name].filename.c_str());
        }

        // 2. Icons laden
        JsonObject icons = doc["icons"];
        for (JsonPair kv : icons) {
            String name = kv.key().c_str();
            JsonObject val = kv.value();
            
            IconDef def;
            if (val.containsKey("file")) {
                def.filename = val["file"].as<String>();
                def.sheetName = "";
            } else {
                def.sheetName = val["sheet"].as<String>();
                def.index = val["index"];
            }
            iconMap[name] = def;
        }
        
        file.close();
        Serial.printf("EmojiEngine: %d Icons bereit.\n", iconMap.size());
    }

    // Zeichnet Icon an X,Y
    void drawIcon(DisplayManager& d, String name, int x, int y) {
        if (!ready) return;

        // Prüfen ob Icon existiert
        if (iconMap.find(name) == iconMap.end()) {
            // Unbekanntes Icon -> Rotes Platzhalter-Quadrat (Debug)
            // d.drawRect(x, y, defaultSize, defaultSize, 0xF800);
            return;
        }

        IconDef& icon = iconMap[name];

        if (icon.sheetName.length() > 0) {
            // Ist Teil eines Sheets
            if (sheetMap.find(icon.sheetName) != sheetMap.end()) {
                drawFromSheet(d, sheetMap[icon.sheetName], icon.index, x, y);
            }
        } else if (icon.filename.length() > 0) {
            // Ist Einzeldatei
            drawBmpFile(d, icon.filename, x, y);
        }
    }

private:
    void drawFromSheet(DisplayManager& d, SheetDef& sheet, int index, int x, int y) {
        File f = LittleFS.open(sheet.filename, "r");
        if (!f) return;

        // Position berechnen
        int row = index / sheet.cols;
        int col = index % sheet.cols;

        // BMP Header Infos lesen
        uint32_t dataOffset = 54; 
        f.seek(10); f.read((uint8_t*)&dataOffset, 4);
        
        int32_t bmpWidth = 0;
        f.seek(18); f.read((uint8_t*)&bmpWidth, 4);
        
        int32_t bmpHeight = 0;
        f.seek(22); f.read((uint8_t*)&bmpHeight, 4);
        
        bool isTopDown = (bmpHeight < 0); // BMP Konvention
        bmpHeight = abs(bmpHeight);

        // Pixel-Startkoordinaten im Sheet
        int sheetX = col * defaultSize;
        int sheetY = row * defaultSize;

        // BMP Row Padding (Zeilen sind auf 4 Byte ausgerichtet)
        int rowSize = (bmpWidth * 3 + 3) & ~3; 
        uint8_t lineBuffer[defaultSize * 3]; 

        for (int i = 0; i < defaultSize; i++) {
            // Zeile berechnen
            int lineInBmp;
            if (isTopDown) lineInBmp = sheetY + i;
            else lineInBmp = (bmpHeight - 1) - (sheetY + i); 

            // Seek zur Position
            f.seek(dataOffset + (lineInBmp * rowSize) + (sheetX * 3));
            f.read(lineBuffer, defaultSize * 3);

            // Pixel zeichnen
            for (int j = 0; j < defaultSize; j++) {
                // BMP ist BGR Format!
                uint8_t b = lineBuffer[j * 3 + 0];
                uint8_t g = lineBuffer[j * 3 + 1];
                uint8_t r = lineBuffer[j * 3 + 2];

                // Transparenz (Schwarz = Transparent)
                if (r == 0 && g == 0 && b == 0) continue; 

                d.drawPixel(x + j, y + i, d.color565(r, g, b));
            }
        }
        f.close();
    }

    void drawBmpFile(DisplayManager& d, String filename, int x, int y) {
        File f = LittleFS.open(filename, "r");
        if (!f) return;

        uint32_t dataOffset = 54;
        f.seek(10); f.read((uint8_t*)&dataOffset, 4);
        
        int32_t bmpWidth = 0; int32_t bmpHeight = 0;
        f.seek(18); f.read((uint8_t*)&bmpWidth, 4);
        f.seek(22); f.read((uint8_t*)&bmpHeight, 4);
        bool isTopDown = (bmpHeight < 0);
        bmpHeight = abs(bmpHeight);

        int rowSize = (bmpWidth * 3 + 3) & ~3;
        uint8_t lineBuffer[defaultSize * 3];

        for (int i = 0; i < defaultSize; i++) {
             int lineInBmp = isTopDown ? i : (bmpHeight - 1) - i;
             f.seek(dataOffset + (lineInBmp * rowSize));
             f.read(lineBuffer, defaultSize * 3);
             
             for (int j = 0; j < defaultSize; j++) {
                 uint8_t b = lineBuffer[j * 3 + 0];
                 uint8_t g = lineBuffer[j * 3 + 1];
                 uint8_t r = lineBuffer[j * 3 + 2];
                 if (r == 0 && g == 0 && b == 0) continue; 
                 d.drawPixel(x + j, y + i, d.color565(r, g, b));
             }
        }
        f.close();
    }
};