#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <map>
#include <list>
#include <HTTPClient.h> 
#include <PNGdec.h>     
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
    
    PNG png; // Instanz für PNG Decoder

    // --- Helper für BMP Parsing ---
    uint32_t read32(const uint8_t* data, int offset) {
        return data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
    }
    
    uint16_t read16(const uint8_t* data, int offset) {
        return data[offset] | (data[offset+1] << 8);
    }
    
    void write32(uint8_t* data, int offset, uint32_t val) {
        data[offset] = val & 0xFF;
        data[offset+1] = (val >> 8) & 0xFF;
        data[offset+2] = (val >> 16) & 0xFF;
        data[offset+3] = (val >> 24) & 0xFF;
    }
    
    void write16(uint8_t* data, int offset, uint16_t val) {
        data[offset] = val & 0xFF;
        data[offset+1] = (val >> 8) & 0xFF;
    }

    // Generiert BMP Header für 32-Bit (für Downloads)
    void writeBmpHeader(File& f, int w, int h) {
        uint8_t header[54] = {0};
        header[0] = 'B'; header[1] = 'M';
        uint32_t dataSize = w * h * 4;
        write32(header, 2, dataSize + 54);
        write32(header, 10, 54);
        write32(header, 14, 40);
        write32(header, 18, w);
        write32(header, 22, h); 
        write16(header, 26, 1);
        write16(header, 28, 32); 
        write32(header, 30, 0);
        write32(header, 34, dataSize);
        f.write(header, 54);
    }

    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    // ORIGINAL: Lädt aus Dotto-Sheets
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
        
        File f = LittleFS.open(sheet.filePath, "r");
        if (!f) return nullptr;

        uint8_t header[54];
        if (f.read(header, 54) != 54) { f.close(); return nullptr; }
        
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

        int col = index % sheet.cols;
        int row = index / sheet.cols;
        int startX = col * sheet.tileW;
        int startY = row * sheet.tileH; 

        size_t lineSize = sheet.tileW * 4; 
        uint8_t* lineBuffer = (uint8_t*)malloc(lineSize);
        if (!lineBuffer) {
             Serial.println("ICON ERROR: Line Buffer Alloc failed");
             free(newIcon->pixels); free(newIcon->alpha); delete newIcon; f.close(); return nullptr;
        }

        for (int y = 0; y < sheet.tileH; y++) {
            int srcY_Visual = startY + y;
            int bmpRow = flipY ? (height - 1 - srcY_Visual) : srcY_Visual;
            
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

    // NEU: Lädt eine einzelne BMP Datei (für heruntergeladene Icons)
    CachedIcon* loadBmpFile(String filename) {
        File f = LittleFS.open(filename, "r");
        if (!f) return nullptr;

        uint8_t header[54];
        if (f.read(header, 54) != 54) { f.close(); return nullptr; }
        
        uint32_t dataOffset = read32(header, 10);
        int32_t width = read32(header, 18);
        int32_t height = read32(header, 22);
        
        bool flipY = true;
        if (height < 0) { height = -height; flipY = false; }
        
        CachedIcon* newIcon = new CachedIcon();
        newIcon->width = width;
        newIcon->height = height;
        
        size_t numPixels = width * height;
        newIcon->pixels = (uint16_t*)heap_caps_malloc(numPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        newIcon->alpha = (uint8_t*)heap_caps_malloc(numPixels * sizeof(uint8_t), MALLOC_CAP_SPIRAM);

        if (!newIcon->pixels || !newIcon->alpha) {
            delete newIcon; f.close(); return nullptr;
        }

        size_t lineSize = width * 4;
        uint8_t* lineBuffer = (uint8_t*)malloc(lineSize);
        if(!lineBuffer) {
             free(newIcon->pixels); free(newIcon->alpha); delete newIcon; f.close(); return nullptr;
        }

        for (int y = 0; y < height; y++) {
            int srcY_Visual = y;
            int bmpRow = flipY ? (height - 1 - srcY_Visual) : srcY_Visual;
            
            size_t filePos = dataOffset + ((size_t)bmpRow * width * 4);
            f.seek(filePos);
            f.read(lineBuffer, lineSize);

            for (int x = 0; x < width; x++) {
                int targetIdx = y * width + x;
                int bufIdx = x * 4;
                newIcon->pixels[targetIdx] = color565(lineBuffer[bufIdx+2], lineBuffer[bufIdx+1], lineBuffer[bufIdx]);
                newIcon->alpha[targetIdx] = lineBuffer[bufIdx+3];
            }
        }
        free(lineBuffer);
        f.close();
        return newIcon;
    }

    // NEU: Online Downloader (LaMetric API -> PNG -> BMP)
    bool downloadAndConvertIcon(String id) {
        if (WiFi.status() != WL_CONNECTED) return false;

        String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".png";
        Serial.println("[ICON] Downloading: " + url);

        HTTPClient http;
        http.begin(url);
        http.setReuse(false);
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            // 1. In temporäre Datei speichern
            File f = LittleFS.open("/temp_icon.png", "w");
            http.writeToStream(&f);
            f.close();
            http.end();
            
            // 2. PNG Decodieren
            int rc = png.open("/temp_icon.png", myOpen, myClose, myRead, mySeek, myDraw);
            if (rc == PNG_SUCCESS) {
                int w = png.getWidth();
                int h = png.getHeight();
                
                uint8_t* rgbaBuffer = (uint8_t*)malloc(w * h * 4);
                if (!rgbaBuffer) { png.close(); return false; }

                png.decode(rgbaBuffer, 0); 
                png.close();

                // 3. Als 32-Bit BMP speichern
                if (!LittleFS.exists("/icons")) LittleFS.mkdir("/icons");
                String outName = "/icons/" + id + ".bmp";
                
                File fOut = LittleFS.open(outName, "w");
                writeBmpHeader(fOut, w, h);
                
                size_t lineSize = w * 4;
                uint8_t* lineOut = (uint8_t*)malloc(lineSize);
                
                // Wir speichern Bottom-Up (Standard BMP)
                for (int y = h - 1; y >= 0; y--) {
                    uint8_t* srcRow = rgbaBuffer + (y * lineSize);
                    for(int x=0; x<w; x++) {
                        int i = x * 4;
                        // PNGDec (RGBA) -> BMP (BGRA)
                        lineOut[i]   = srcRow[i+2]; // B
                        lineOut[i+1] = srcRow[i+1]; // G
                        lineOut[i+2] = srcRow[i];   // R
                        lineOut[i+3] = srcRow[i+3]; // A
                    }
                    fOut.write(lineOut, lineSize);
                }
                
                free(lineOut);
                fOut.close();
                free(rgbaBuffer);
                LittleFS.remove("/temp_icon.png");
                Serial.println("[ICON] Saved: " + outName);
                return true;
            } else {
                 Serial.println("[ICON] PNG Decode Fail");
            }
        } else {
            Serial.printf("[ICON] HTTP Fail: %d\n", httpCode);
            http.end();
        }
        return false;
    }

    // NEU: Helper für PNGDec Lib
    static void* myOpen(const char *filename, int32_t *size) {
        File f = LittleFS.open(filename, "r");
        *size = f.size();
        return new File(f);
    }
    static void myClose(void *handle) {
        File* f = (File*)handle; f->close(); delete f;
    }
    static int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
        File* f = (File*)handle->fHandle; return f->read(buffer, length);
    }
    static int32_t mySeek(PNGFILE *handle, int32_t position) {
        File* f = (File*)handle->fHandle; return f->seek(position);
    }
    
    // FIX: Rückgabetyp int statt void!
    static int myDraw(PNGDRAW *pDraw) {
        uint8_t* buffer = (uint8_t*)pDraw->pUser; 
        // Kopiere Pixel in unseren Buffer
        memcpy(buffer + (pDraw->y * pDraw->iWidth * 4), pDraw->pPixels, pDraw->iWidth * 4);
        return 1; // 1 = Continue, 0 = Abort
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
        // 1. Suche im RAM Cache
        for (auto it = iconCache.begin(); it != iconCache.end(); ++it) {
            if ((*it)->name == name) {
                (*it)->lastUsed = millis();
                if (it != iconCache.begin()) iconCache.splice(iconCache.begin(), iconCache, it);
                return *it;
            }
        }
        
        CachedIcon* newIcon = nullptr;

        // 2. Suche im Katalog (Dotto Sheets)
        if (iconCatalog.find(name) != iconCatalog.end()) {
            IconDef& def = iconCatalog[name];
            newIcon = loadIconFromSheet(def.sheetName, def.index);
        }
        // 3. Suche nach heruntergeladener Datei
        else if (LittleFS.exists("/icons/" + name + ".bmp")) {
             newIcon = loadBmpFile("/icons/" + name + ".bmp");
        }
        // 4. NEU: Versuch Online zu laden (nur wenn Name numerisch ist)
        else {
             bool isNumeric = true;
             for(unsigned int i=0; i<name.length(); i++) if(!isDigit(name[i])) isNumeric = false;
             
             if (isNumeric && name.length() > 0) {
                 if (downloadAndConvertIcon(name)) {
                     newIcon = loadBmpFile("/icons/" + name + ".bmp");
                 }
             }
        }
        
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

    // NEU: Helper für Layout
    int getIconWidth(String name) {
        CachedIcon* i = getIcon(name);
        return i ? i->width : 16; // Fallback
    }

    int getIconHeight(String name) {
        CachedIcon* i = getIcon(name);
        return i ? i->height : 16;
    }

    // NEU: drawIcon mit Skalierungs-Option
    void drawIcon(DisplayManager& display, int x, int y, String name, bool scaleTo16 = false) {
        CachedIcon* icon = getIcon(name); if (!icon) return; 
        
        // Soll skaliert werden UND ist das Icon klein (8x8)?
        bool doUpscale = scaleTo16 && (icon->width == 8) && (icon->height == 8);

        if (doUpscale) {
            // Skalierung 8x8 -> 16x16 (Pixel verdoppeln)
            for (int iy = 0; iy < 16; iy++) {
                for (int ix = 0; ix < 16; ix++) {
                    int screenX = x + ix; int screenY = y + iy;
                    if (screenX < 0 || screenX >= M_WIDTH || screenY < 0 || screenY >= M_HEIGHT) continue;
                    
                    // Nächster Nachbar: Einfach durch 2 teilen
                    int srcX = ix / 2;
                    int srcY = iy / 2;
                    int i = srcY * icon->width + srcX;
                    
                    if (icon->alpha[i] > 10) display.drawPixel(screenX, screenY, icon->pixels[i]);
                }
            }
        } else {
            // Normales Zeichnen (1:1)
            for (int iy = 0; iy < icon->height; iy++) {
                for (int ix = 0; ix < icon->width; ix++) {
                    int screenX = x + ix; int screenY = y + iy;
                    if (screenX < 0 || screenX >= M_WIDTH || screenY < 0 || screenY >= M_HEIGHT) continue;
                    int i = iy * icon->width + ix;
                    if (icon->alpha[i] > 10) display.drawPixel(screenX, screenY, icon->pixels[i]);
                }
            }
        }
    }
};