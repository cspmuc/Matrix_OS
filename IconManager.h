#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <map>
#include <list>
#include <vector>
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
    std::map<String, String> aliasMap; // NEU: Mapping für {lt:xxx}
    
    std::list<CachedIcon*> iconCache;
    std::vector<String> failedIcons;
    
    const size_t MAX_CACHE_SIZE = 150;
    
    PNG png; 

    // --- Helper ---
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

    CachedIcon* loadIconFromSheet(String sheetName, int index) {
        if (sheetCatalog.find(sheetName) == sheetCatalog.end()) return nullptr;
        SheetDef& sheet = sheetCatalog[sheetName];
        if (!LittleFS.exists(sheet.filePath)) return nullptr;
        
        File f = LittleFS.open(sheet.filePath, "r");
        if (!f) return nullptr;

        uint8_t header[54];
        if (f.read(header, 54) != 54) { f.close(); return nullptr; }
        
        uint32_t dataOffset = read32(header, 10);
        int32_t width = read32(header, 18);
        int32_t height = read32(header, 22);
        uint16_t bpp = read16(header, 28);
        
        if (bpp != 32) { f.close(); return nullptr; }

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
            delete newIcon; f.close(); return nullptr;
        }

        int col = index % sheet.cols;
        int row = index / sheet.cols;
        int startX = col * sheet.tileW;
        int startY = row * sheet.tileH; 

        size_t lineSize = sheet.tileW * 4; 
        uint8_t* lineBuffer = (uint8_t*)malloc(lineSize);
        if (!lineBuffer) {
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

    bool downloadAndConvertIcon(String id) {
        if (WiFi.status() != WL_CONNECTED) return false;

        String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".png";
        Serial.println("[ICON] Downloading: " + url);

        HTTPClient http;
        http.begin(url);
        http.setReuse(false);
        http.setTimeout(3000); 
        
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            File f = LittleFS.open("/temp_icon.png", "w");
            http.writeToStream(&f);
            f.close();
            http.end();
            
            int rc = png.open("/temp_icon.png", myOpen, myClose, myRead, mySeek, myDraw);
            if (rc == PNG_SUCCESS) {
                int w = png.getWidth();
                int h = png.getHeight();
                
                uint8_t* rgbaBuffer = (uint8_t*)malloc(w * h * 4);
                if (!rgbaBuffer) { png.close(); return false; }

                png.decode(rgbaBuffer, 0); 
                png.close();

                if (!LittleFS.exists("/icons")) LittleFS.mkdir("/icons");
                String outName = "/icons/" + id + ".bmp";
                
                File fOut = LittleFS.open(outName, "w");
                writeBmpHeader(fOut, w, h);
                
                size_t lineSize = w * 4;
                uint8_t* lineOut = (uint8_t*)malloc(lineSize);
                
                for (int y = h - 1; y >= 0; y--) {
                    uint8_t* srcRow = rgbaBuffer + (y * lineSize);
                    for(int x=0; x<w; x++) {
                        int i = x * 4;
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
            } 
        } 
        http.end();
        return false;
    }

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
    static int myDraw(PNGDRAW *pDraw) {
        uint8_t* buffer = (uint8_t*)pDraw->pUser; 
        memcpy(buffer + (pDraw->y * pDraw->iWidth * 4), pDraw->pPixels, pDraw->iWidth * 4);
        return 1;
    }

public:
    IconManager() {}
    ~IconManager() {}

    void begin() {
        if (!LittleFS.exists("/catalog.json")) return; 
        
        File f = LittleFS.open("/catalog.json", "r");
        DynamicJsonDocument* doc = new DynamicJsonDocument(8192); 
        DeserializationError error = deserializeJson(*doc, f); f.close();
        if (error) { delete doc; return; }

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
        
        // NEU: Aliases laden
        JsonObject aliases = (*doc)["aliases"];
        for (JsonPair kv : aliases) {
            String alias = kv.key().c_str();
            String id = kv.value().as<String>();
            aliasMap[alias] = id;
        }
        
        delete doc; 
        Serial.print("IconManager: Loaded "); Serial.print(count); Serial.println(" definitions.");
    }

    // NEU: Helper für Alias Lookup
    String resolveAlias(String tag) {
        if (aliasMap.find(tag) != aliasMap.end()) {
            return aliasMap[tag];
        }
        return "";
    }

    CachedIcon* getIcon(String name) {
        // 1. Cache
        for (auto it = iconCache.begin(); it != iconCache.end(); ++it) {
            if ((*it)->name == name) {
                (*it)->lastUsed = millis();
                if (it != iconCache.begin()) iconCache.splice(iconCache.begin(), iconCache, it);
                return *it;
            }
        }
        
        // Blacklist Check
        for(const String& bad : failedIcons) {
            if (bad == name) return nullptr; 
        }
        
        CachedIcon* newIcon = nullptr;

        // 2. Sheet Catalog ({ic})
        if (iconCatalog.find(name) != iconCatalog.end()) {
            IconDef& def = iconCatalog[name];
            newIcon = loadIconFromSheet(def.sheetName, def.index);
        }
        // 3. Local File ({ln}/{lt})
        else if (LittleFS.exists("/icons/" + name + ".bmp")) {
             newIcon = loadBmpFile("/icons/" + name + ".bmp");
        }
        // 4. Online Download ({ln}/{lt})
        else {
             // Ist es eine ID (numerisch)?
             bool isNumeric = true;
             for(unsigned int i=0; i<name.length(); i++) if(!isDigit(name[i])) isNumeric = false;
             
             if (isNumeric && name.length() > 0) {
                 if (downloadAndConvertIcon(name)) {
                     newIcon = loadBmpFile("/icons/" + name + ".bmp");
                 } else {
                     Serial.println("Icon failed forever: " + name);
                     failedIcons.push_back(name);
                 }
             } else {
                 // Kein gültiges Ziel gefunden
                 failedIcons.push_back(name);
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

    int getIconWidth(String name) {
        CachedIcon* i = getIcon(name);
        return i ? i->width : 16; 
    }
    
    int getIconHeight(String name) {
        CachedIcon* i = getIcon(name);
        return i ? i->height : 16;
    }

    void drawIcon(DisplayManager& display, int x, int y, String name, bool scaleTo16 = false) {
        CachedIcon* icon = getIcon(name); 
        
        if (!icon) {
            display.drawLine(x, y, x + 7, y + 7, display.color565(255, 0, 0));
            display.drawLine(x + 7, y, x, y + 7, display.color565(255, 0, 0));
            return; 
        }
        
        bool doUpscale = scaleTo16 && (icon->width == 8) && (icon->height == 8);

        if (doUpscale) {
            for (int iy = 0; iy < 16; iy++) {
                for (int ix = 0; ix < 16; ix++) {
                    int screenX = x + ix; int screenY = y + iy;
                    if (screenX < 0 || screenX >= M_WIDTH || screenY < 0 || screenY >= M_HEIGHT) continue;
                    
                    int srcX = ix / 2; int srcY = iy / 2;
                    int i = srcY * icon->width + srcX;
                    
                    if (icon->alpha[i] > 10) display.drawPixel(screenX, screenY, icon->pixels[i]);
                }
            }
        } else {
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