#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <map>
#include <list>
#include <vector>
#include <HTTPClient.h> 
#include <PNGdec.h>     
#include <AnimatedGIF.h> // WICHTIG: Bibliothek "AnimatedGIF" installieren!
#include "DisplayManager.h"
#include <esp_heap_caps.h> 

struct IconDef { String sheetName; int index; };
struct SheetDef { String filePath; int cols; int tileW; int tileH; };
struct CachedIcon { String name; uint16_t* pixels; uint8_t* alpha; unsigned long lastUsed; int width; int height; int frameCount; };

// Helper Struct für den GIF Decoder Callback
struct GifDrawContext {
    uint8_t* canvas;     // Unser Puffer für den aktuellen Frame
    int width;
    int height;
    int disposalMethod;  // Was passiert nach dem Frame?
    int x, y, w, h;      // Update-Bereich des aktuellen Frames
    int transparentIdx;  // Welcher Index ist transparent?
    bool hasTransparency;
};

class IconManager {
private:
    std::map<String, IconDef> iconCatalog;
    std::map<String, SheetDef> sheetCatalog;
    std::map<String, String> aliasMap; 
    
    std::list<CachedIcon*> iconCache;
    std::vector<String> failedIcons;
    
    const size_t MAX_CACHE_SIZE = 150;
    
    PNG png; 
    AnimatedGIF gif; 

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
        uint32_t dataSize = w * abs(h) * 4; // abs() falls negativ
        write32(header, 2, dataSize + 54);
        write32(header, 10, 54);
        write32(header, 14, 40);
        write32(header, 18, w);
        // WICHTIG: Negative Höhe bedeutet Top-Down (Zeile 0 ist oben).
        write32(header, 22, (uint32_t)h); 
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
        newIcon->frameCount = 1; 
        
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
        
        if (height > width && (height % width == 0)) {
            newIcon->frameCount = height / width;
        } else {
            newIcon->frameCount = 1;
        }
        
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

    // --- PNG DOWNLOADER (FALLBACK / STATIC) ---
    bool downloadAndConvertPng(String id) {
        String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".png";
        Serial.println("[ICON] Try PNG: " + url);

        HTTPClient http;
        http.begin(url);
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
                writeBmpHeader(fOut, w, h); // Standard Bottom-Up
                
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
                Serial.println("[ICON] Saved PNG->BMP: " + outName);
                return true;
            } 
        } 
        http.end();
        return false;
    }

    // --- GIF DOWNLOADER & CONVERTER (ANIMATION) ---
    // Callback für AnimatedGIF Library
    static void GIFDrawCallback(GIFDRAW *pDraw) {
        // FIX: Absturz verhindern, wenn pUser NULL ist (beim Scannen der Frames)
        if (!pDraw->pUser) return;

        // Pointer auf Kontext holen
        GifDrawContext* ctx = (GifDrawContext*)pDraw->pUser;
        
        // Parameter speichern für spätere Disposal-Logik
        ctx->disposalMethod = pDraw->ucDisposalMethod;
        ctx->x = pDraw->iX;
        ctx->y = pDraw->iY;
        ctx->w = pDraw->iWidth;
        ctx->h = pDraw->iHeight;
        ctx->hasTransparency = pDraw->ucHasTransparency;
        ctx->transparentIdx = pDraw->ucTransparent; // FIX: ucTransparent verwenden

        uint8_t *s = pDraw->pPixels;
        // Palette in 24-Bit RGB
        uint8_t *pal = (uint8_t *)pDraw->pPalette; 
        
        // Wir zeichnen direkt in den Canvas (RGBA)
        // Canvas ist ctx->width breit
        int y = pDraw->iY + pDraw->y; // Absolute Y-Position im Frame
        if (y >= ctx->height) return;

        int rowStart = (y * ctx->width * 4); // 4 Bytes pro Pixel

        for (int x = 0; x < pDraw->iWidth; x++) {
            int absX = pDraw->iX + x;
            if (absX >= ctx->width) break;

            uint8_t val = s[x];
            if (ctx->hasTransparency && val == ctx->transparentIdx) {
                // Transparent -> Pixel nicht ändern (altes Bild scheint durch)
                continue;
            }

            int targetIdx = rowStart + (absX * 4);
            // Palette lookup (RGB)
            uint8_t r = pal[val * 3];
            uint8_t g = pal[val * 3 + 1];
            uint8_t b = pal[val * 3 + 2];
            
            // In Canvas schreiben (BGRA für BMP)
            ctx->canvas[targetIdx]     = b;
            ctx->canvas[targetIdx + 1] = g;
            ctx->canvas[targetIdx + 2] = r;
            ctx->canvas[targetIdx + 3] = 255; // Voll deckend
        }
    }

    bool downloadAndConvertGif(String id) {
        String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".gif";
        Serial.println("[ICON] Try GIF: " + url);

        HTTPClient http;
        http.begin(url);
        http.setTimeout(3000);
        int httpCode = http.GET();
        
        if (httpCode != HTTP_CODE_OK) {
            http.end();
            return false; // GIF nicht gefunden -> Fallback zu PNG
        }

        // GIF downloaden
        File f = LittleFS.open("/temp_icon.gif", "w");
        http.writeToStream(&f);
        f.close();
        http.end();

        // 1. Scan: Frames zählen
        if (!gif.open("/temp_icon.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDrawCallback)) {
            return false;
        }
        int width = gif.getCanvasWidth();
        int height = gif.getCanvasHeight();
        
        Serial.printf("[ICON] GIF Info: %dx%d\n", width, height);
        
        // Frames zählen (Dummy Durchlauf)
        int frames = 0;
        while(gif.playFrame(false, NULL)) {
            frames++;
            yield(); // WICHTIG: Watchdog füttern!
        }
        gif.close(); // Reset

        if (frames == 0) return false;

        // 2. Verarbeitung
        // Canvas Buffer allozieren (für EINEN Frame)
        size_t canvasSize = width * height * 4;
        uint8_t* canvas = (uint8_t*)malloc(canvasSize);
        if (!canvas) return false;
        
        // Mit Transparenz/Schwarz initialisieren
        memset(canvas, 0, canvasSize);

        // Kontext für Callback
        GifDrawContext ctx;
        ctx.canvas = canvas;
        ctx.width = width;
        ctx.height = height;

        // BMP Datei öffnen
        if (!LittleFS.exists("/icons")) LittleFS.mkdir("/icons");
        String outName = "/icons/" + id + ".bmp";
        File fOut = LittleFS.open(outName, "w");
        
        // Header schreiben: NEGATIVE Höhe für Top-Down (einfaches Anhängen)
        // Gesamthöhe = Höhe * Anzahl Frames
        writeBmpHeader(fOut, width, -(height * frames));

        gif.open("/temp_icon.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDrawCallback);
        
        for (int i = 0; i < frames; i++) {
            // Frame dekodieren -> ruft Callback -> füllt 'canvas'
            // Wir übergeben &ctx als User Pointer
            gif.playFrame(false, NULL, (void*)&ctx); 
            
            // Den fertig komponierten Frame in die Datei schreiben
            fOut.write(canvas, canvasSize);
            
            // Disposal Handling (für den NÄCHSTEN Frame im Canvas)
            if (ctx.disposalMethod == 2) { 
                // "Restore to Background" -> Bereich löschen
                // Wir löschen nur das Rechteck, das gerade gezeichnet wurde
                for (int y = ctx.y; y < ctx.y + ctx.h; y++) {
                    if(y >= height) break;
                    int rowStart = y * width * 4;
                    for (int x = ctx.x; x < ctx.x + ctx.w; x++) {
                        if(x >= width) break;
                        int idx = rowStart + (x * 4);
                        canvas[idx] = 0; canvas[idx+1] = 0; canvas[idx+2] = 0; canvas[idx+3] = 0;
                    }
                }
            }
            yield(); // WICHTIG: Watchdog füttern
        }
        
        gif.close();
        fOut.close();
        free(canvas);
        LittleFS.remove("/temp_icon.gif");
        
        Serial.printf("[ICON] Converted GIF: %d Frames saved to %s\n", frames, outName.c_str());
        return true;
    }

    // Callbacks für AnimatedGIF File I/O
    static void * GIFOpenFile(const char *fname, int32_t *pSize) {
        File f = LittleFS.open(fname);
        if (f) {
            *pSize = f.size();
            return new File(f);
        }
        return NULL;
    }
    static void GIFCloseFile(void *pHandle) {
        File *f = (File *)pHandle;
        if (f) { f->close(); delete f; }
    }
    static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
        File *f = (File *)pFile->fHandle;
        if (f) return f->read(pBuf, iLen);
        return 0;
    }
    static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
        File *f = (File *)pFile->fHandle;
        if (f) return f->seek(iPosition);
        return 0;
    }

    // Callbacks für PNGdec (unverändert)
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
        return 1; // Wir decoden komplett im Speicher in downloadAndConvertPng
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
        
        // Aliases laden
        JsonObject aliases = (*doc)["aliases"];
        for (JsonPair kv : aliases) {
            String alias = kv.key().c_str();
            String id = kv.value().as<String>();
            aliasMap[alias] = id;
        }
        
        delete doc; 
        
        // Init GIF Lib
        gif.begin(LITTLE_ENDIAN_PIXELS);
        
        Serial.print("IconManager: Loaded "); Serial.print(count); Serial.println(" definitions.");
    }

    String resolveAlias(String tag) {
        if (aliasMap.find(tag) != aliasMap.end()) {
            return aliasMap[tag];
        }
        return "";
    }

    int getIconFrameCount(String name) {
        CachedIcon* i = getIcon(name);
        return i ? i->frameCount : 1;
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
        // 3. Local File ({ln}/{lt}/{la})
        else if (LittleFS.exists("/icons/" + name + ".bmp")) {
             newIcon = loadBmpFile("/icons/" + name + ".bmp");
        }
        // 4. Online Download ({ln}/{lt})
        else {
             // Ist es eine ID (numerisch)?
             bool isNumeric = true;
             for(unsigned int i=0; i<name.length(); i++) if(!isDigit(name[i])) isNumeric = false;
             
             if (isNumeric && name.length() > 0) {
                 // ZUERST VERSUCHEN: Animiertes GIF downloaden und konvertieren
                 bool success = downloadAndConvertGif(name);
                 
                 // FALLS FEHLGESCHLAGEN: Statisches PNG versuchen
                 if (!success) {
                     success = downloadAndConvertPng(name);
                 }

                 if (success) {
                     newIcon = loadBmpFile("/icons/" + name + ".bmp");
                 } else {
                     Serial.println("Icon failed forever: " + name);
                     failedIcons.push_back(name);
                 }
             } else {
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
        if (!i) return 16;
        if (i->frameCount > 1) return i->width; // Frame ist quadratisch
        return i->height;
    }

    void drawIcon(DisplayManager& display, int x, int y, String name, int frame = 0, bool scaleTo16 = false) {
        CachedIcon* icon = getIcon(name); 
        
        if (!icon) {
            display.drawLine(x, y, x + 7, y + 7, display.color565(255, 0, 0));
            display.drawLine(x + 7, y, x, y + 7, display.color565(255, 0, 0));
            return; 
        }
        
        // Frame-Offset berechnen
        int safeFrame = frame % icon->frameCount;
        int yOffset = safeFrame * icon->width; 
        
        int renderHeight = (icon->frameCount > 1) ? icon->width : icon->height;

        bool doUpscale = scaleTo16 && (icon->width == 8) && (renderHeight == 8);

        if (doUpscale) {
            for (int iy = 0; iy < 16; iy++) {
                for (int ix = 0; ix < 16; ix++) {
                    int screenX = x + ix; int screenY = y + iy;
                    if (screenX < 0 || screenX >= M_WIDTH || screenY < 0 || screenY >= M_HEIGHT) continue;
                    
                    int srcX = ix / 2; int srcY = iy / 2;
                    int i = (yOffset + srcY) * icon->width + srcX;
                    
                    if (icon->alpha[i] > 10) display.drawPixel(screenX, screenY, icon->pixels[i]);
                }
            }
        } else {
            for (int iy = 0; iy < renderHeight; iy++) {
                for (int ix = 0; ix < icon->width; ix++) {
                    int screenX = x + ix; int screenY = y + iy;
                    if (screenX < 0 || screenX >= M_WIDTH || screenY < 0 || screenY >= M_HEIGHT) continue;
                    
                    int i = (yOffset + iy) * icon->width + ix;
                    
                    if (icon->alpha[i] > 10) display.drawPixel(screenX, screenY, icon->pixels[i]);
                }
            }
        }
    }
};