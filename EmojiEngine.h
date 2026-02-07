#pragma once
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <map>
#include "DisplayManager.h"

struct IconDef { String sheetName; int index; String filename; };
struct SheetDef { String filename; int cols; int rows; };

class EmojiEngine {
private:
    std::map<String, IconDef> iconMap;
    std::map<String, SheetDef> sheetMap;
    int defaultSize = 16;
    bool ready = false;

public:
    void begin() {
        if (!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed"); return; }
        loadCatalog();
        ready = true;
    }

    void loadCatalog() {
        if (!LittleFS.exists("/catalog.json")) return;
        File file = LittleFS.open("/catalog.json", "r");
        DynamicJsonDocument doc(4096); 
        DeserializationError error = deserializeJson(doc, file);
        if (error) return;

        JsonObject sheets = doc["sheets"];
        for (JsonPair kv : sheets) {
            JsonObject val = kv.value();
            sheetMap[kv.key().c_str()] = { val["file"].as<String>(), val["cols"] | 16, val["rows"] | 16 };
        }

        JsonObject icons = doc["icons"];
        for (JsonPair kv : icons) {
            JsonObject val = kv.value();
            IconDef def;
            if (val.containsKey("file")) {
                def.filename = val["file"].as<String>();
                def.sheetName = "";
            } else {
                def.sheetName = val["sheet"].as<String>();
                def.index = val["index"];
            }
            iconMap[kv.key().c_str()] = def;
        }
        file.close();
    }

    void drawIcon(DisplayManager& d, String name, int x, int y) {
        if (!ready) return;
        if (iconMap.find(name) == iconMap.end()) return;

        IconDef& icon = iconMap[name];
        if (icon.sheetName.length() > 0) {
            if (sheetMap.find(icon.sheetName) != sheetMap.end()) {
                drawFromSheet(d, sheetMap[icon.sheetName], icon.index, x, y);
            }
        } else if (icon.filename.length() > 0) {
            drawBmpFile(d, icon.filename, x, y);
        }
    }

private:
    void drawFromSheet(DisplayManager& d, SheetDef& sheet, int index, int x, int y) {
        File f = LittleFS.open(sheet.filename, "r");
        if (!f) return;

        int row = index / sheet.cols;
        int col = index % sheet.cols;
        uint32_t dataOffset = 54; 
        f.seek(10); f.read((uint8_t*)&dataOffset, 4);
        int32_t bmpHeight = 0;
        f.seek(22); f.read((uint8_t*)&bmpHeight, 4);
        bool isTopDown = (bmpHeight < 0);
        bmpHeight = abs(bmpHeight);
        
        // Annahme: BMP ist exakt gepackt oder wir ignorieren Padding hier einfachheitshalber für 16px
        // Korrekter wäre: rowSize = (width * 3 + 3) & ~3;
        // Wir lesen hier einfach:
        int32_t bmpWidth = 0; f.seek(18); f.read((uint8_t*)&bmpWidth, 4);
        int rowSize = (bmpWidth * 3 + 3) & ~3;

        int sheetX = col * defaultSize;
        int sheetY = row * defaultSize;
        uint8_t lineBuffer[defaultSize * 3]; 

        for (int i = 0; i < defaultSize; i++) {
            int lineInBmp = isTopDown ? (sheetY + i) : ((bmpHeight - 1) - (sheetY + i)); 
            f.seek(dataOffset + (lineInBmp * rowSize) + (sheetX * 3));
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

    void drawBmpFile(DisplayManager& d, String filename, int x, int y) {
        File f = LittleFS.open(filename, "r");
        if (!f) return;
        uint32_t dataOffset = 54; f.seek(10); f.read((uint8_t*)&dataOffset, 4);
        int32_t w=0, h=0; f.seek(18); f.read((uint8_t*)&w, 4); f.seek(22); f.read((uint8_t*)&h, 4);
        bool isTopDown = (h < 0); h = abs(h);
        int rowSize = (w * 3 + 3) & ~3;
        uint8_t lineBuffer[defaultSize * 3];
        for (int i = 0; i < defaultSize; i++) {
             int lineInBmp = isTopDown ? i : (h - 1) - i;
             f.seek(dataOffset + (lineInBmp * rowSize));
             f.read(lineBuffer, defaultSize * 3);
             for (int j = 0; j < defaultSize; j++) {
                 uint8_t b = lineBuffer[j*3], g = lineBuffer[j*3+1], r = lineBuffer[j*3+2];
                 if (r|g|b) d.drawPixel(x+j, y+i, d.color565(r, g, b));
             }
        }
        f.close();
    }
};