#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <map>
#include <list>
#include "DisplayManager.h"
#include <esp_heap_caps.h> 

struct IconDef { String sheetName; int index; };
struct SheetDef { String filePath; int cols; int tileW; int tileH; };
struct CachedIcon { String name; uint16_t* pixels; uint8_t* alpha; unsigned long lastUsed; int width; int height; };

class IconManager {
private:
    std::map<String, IconDef> iconCatalog;
    std::map<String, SheetDef> sheetCatalog;
    std::list<CachedIcon*> iconCache;
    
    // PSRAM Cache Limit (Anzahl Icons)
    const size_t MAX_CACHE_SIZE = 150;

    // --- Helper für BMP Parsing ---
    uint32_t read32(const uint8_t* data, int offset) {
        return data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
    }
    
    uint16_t read16(const uint8_t* data, int offset) {
        return data[offset] | (data[offset+1] << 8);
    }

    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    CachedIcon* loadIconFromSheet(String sheetName, int index) {
        if (sheetCatalog.find(sheetName) == sheetCatalog.end()) {
            Serial.println("ICON ERROR: Sheet not found: " + sheetName);
            return nullptr;
        }
        SheetDef& sheet = sheetCatalog[sheetName];
        if (!LittleFS.exists(sheet.filePath)) {
            Serial.println("ICON ERROR: File missing: " + sheet.filePath);
            return nullptr;
        }
        
        // STREAMING MODE: Wir öffnen die Datei nur zum Lesen
        File f = LittleFS.open(sheet.filePath, "r");
        if (!f) return nullptr;

        // 1. Header lesen (54 Bytes Standard BMP Header)
        uint8_t header[54];
        if (f.read(header, 54) != 54) { f.close(); return nullptr; }
        
        if (header[0] != 'B' || header[1] != 'M') {
            Serial.println("ICON ERROR: Not a BMP file"); f.close(); return nullptr;
        }
        
        uint32_t dataOffset = read32(header, 10);
        int32_t width = read32(header, 18);
        int32_t height = read32(header, 22);
        uint16_t bpp = read16(header, 28);
        
        if (bpp != 32) {
            Serial.printf("ICON ERROR: BMP must be 32-bit (is %d)\n", bpp);
            f.close(); return nullptr;
        }

        bool flipY = true;
        if (height < 0) { height = -height; flipY = false; }

        if (sheet.tileW == 0) {
            sheet.tileW = width / sheet.cols;
            sheet.tileH = sheet.tileW; 
        }

        // 2. Ziel-Speicher im PSRAM reservieren
        CachedIcon* newIcon = new CachedIcon();
        newIcon->width = sheet.tileW;
        newIcon->height = sheet.tileH;
        
        size_t numPixels = sheet.tileW * sheet.tileH;
        newIcon->pixels = (uint16_t*)heap_caps_malloc(numPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        newIcon->alpha = (uint8_t*)heap_caps_malloc(numPixels * sizeof(uint8_t), MALLOC_CAP_SPIRAM);

        if (!newIcon->pixels || !newIcon->alpha) {
            Serial.println("ICON ERROR: PSRAM Alloc failed");
            delete newIcon; f.close(); return nullptr;
        }

        // 3. Streaming: Zeile für Zeile lesen und kopieren
        int col = index % sheet.cols;
        int row = index / sheet.cols;
        int startX = col * sheet.tileW;
        int startY = row * sheet.tileH; 

        // Kleiner Puffer für EINE Zeile (im Stack oder Heap)
        size_t lineSize = sheet.tileW * 4; // 4 Bytes pro Pixel (RGBA)
        uint8_t* lineBuffer = (uint8_t*)malloc(lineSize);
        if (!lineBuffer) {
             Serial.println("ICON ERROR: Line Buffer Alloc failed");
             free(newIcon->pixels); free(newIcon->alpha); delete newIcon; f.close(); return nullptr;
        }

        for (int y = 0; y < sheet.tileH; y++) {
            int srcY_Visual = startY + y;
            // BMP speichert oft von unten nach oben
            int bmpRow = flipY ? (height - 1 - srcY_Visual) : srcY_Visual;
            
            // Berechne exakte Position in der Datei
            size_t filePos = dataOffset + ((size_t)bmpRow * width * 4) + ((size_t)startX * 4);
            
            f.seek(filePos);
            f.read(lineBuffer, lineSize); 

            for (int x = 0; x < sheet.tileW; x++) {
                int targetIdx = y * sheet.tileW + x;
                int bufIdx = x * 4;
                
                uint8_t b = lineBuffer[bufIdx];
                uint8_t g = lineBuffer[bufIdx+1];
                uint8_t r = lineBuffer[bufIdx+2];
                uint8_t a = lineBuffer[bufIdx+3];

                newIcon->pixels[targetIdx] = color565(r, g, b);
                newIcon->alpha[targetIdx] = a;
            }
        }

        free(lineBuffer);
        f.close();
        return newIcon;
    }

public:
    IconManager() {}
    ~IconManager() {}

    void begin() {
        if (!LittleFS.exists("/catalog.json")) { Serial.println("ICON ERROR: No catalog.json"); return; }
        
        File f = LittleFS.open("/catalog.json", "r");
        DynamicJsonDocument* doc = new DynamicJsonDocument(8192); 
        DeserializationError error = deserializeJson(*doc, f); f.close();
        
        if (error) { Serial.println("ICON ERROR: Bad JSON"); delete doc; return; }

        JsonObject sheets = (*doc)["sheets"];
        for (JsonPair kv : sheets) {
            String key = kv.key().c_str(); JsonObject val = kv.value();
            SheetDef def; 
            def.filePath = val["file"].as<String>(); 
            def.cols = val["cols"] | 1; 
            def.tileW = 0; def.tileH = 0; 
            sheetCatalog[key] = def;
        }
        JsonObject icons = (*doc)["icons"];
        int count = 0;
        for (JsonPair kv : icons) {
            String key = kv.key().c_str(); JsonObject val = kv.value();
            IconDef def; 
            def.sheetName = val["sheet"].as<String>(); 
            def.index = val["index"] | 0; 
            iconCatalog[key] = def;
            count++;
        }
        delete doc; 
        Serial.print("IconEngine (Streaming): Loaded "); Serial.print(count); Serial.println(" definitions.");
    }

    CachedIcon* getIcon(String name) {
        for (auto it = iconCache.begin(); it != iconCache.end(); ++it) {
            if ((*it)->name == name) {
                (*it)->lastUsed = millis();
                if (it != iconCache.begin()) iconCache.splice(iconCache.begin(), iconCache, it);
                return *it;
            }
        }
        
        if (iconCatalog.find(name) == iconCatalog.end()) return nullptr;
        
        // Serial.println("IconEngine: Streaming '" + name + "'..."); // Debug Spam reduziert
        IconDef& def = iconCatalog[name];
        CachedIcon* newIcon = loadIconFromSheet(def.sheetName, def.index);
        
        if (newIcon) {
            newIcon->name = name; 
            newIcon->lastUsed = millis();
            if (iconCache.size() >= MAX_CACHE_SIZE) {
                CachedIcon* old = iconCache.back(); 
                iconCache.pop_back(); 
                free(old->pixels); free(old->alpha); delete old;
            }
            iconCache.push_front(newIcon);
        }
        return newIcon;
    }

    // --- NEU: Helper für Layout-Berechnungen ---
    int getIconWidth(String name) {
        // 1. Im Cache schauen
        for (auto* i : iconCache) if (i->name == name) return i->width;
        // 2. In Definition schauen (Fallback ohne Laden)
        if (iconCatalog.find(name) != iconCatalog.end()) {
            String s = iconCatalog[name].sheetName;
            if (sheetCatalog.find(s) != sheetCatalog.end()) {
                // Falls tileW noch 0 ist (noch nie geladen), nehmen wir Standard 16 an
                int w = sheetCatalog[s].tileW;
                return (w > 0) ? w : 16;
            }
        }
        return 16; // Absoluter Fallback
    }

    int getIconHeight(String name) {
        for (auto* i : iconCache) if (i->name == name) return i->height;
        if (iconCatalog.find(name) != iconCatalog.end()) {
            String s = iconCatalog[name].sheetName;
            if (sheetCatalog.find(s) != sheetCatalog.end()) {
                int h = sheetCatalog[s].tileH;
                return (h > 0) ? h : 16;
            }
        }
        return 16;
    }
    // -------------------------------------------

    void drawIcon(DisplayManager& display, int x, int y, String name) {
        CachedIcon* icon = getIcon(name); if (!icon) return; 
        for (int iy = 0; iy < icon->height; iy++) {
            for (int ix = 0; ix < icon->width; ix++) {
                int screenX = x + ix; int screenY = y + iy;
                if (screenX < 0 || screenX >= M_WIDTH || screenY < 0 || screenY >= M_HEIGHT) continue;
                int i = iy * icon->width + ix;
                if (icon->alpha[i] > 10) display.drawPixel(screenX, screenY, icon->pixels[i]);
            }
        }
    }
};