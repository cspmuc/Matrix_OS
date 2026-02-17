#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <map>
#include <list>
#include <vector>
#include <HTTPClient.h> 
#include <WiFiClientSecure.h> 
#include <PNGdec.h>     
#include <AnimatedGIF.h> 
#include "DisplayManager.h"
#include <esp_heap_caps.h> 
#include <esp_task_wdt.h> 

struct IconDef { String sheetName; int index; };
struct SheetDef { String filePath; int cols; int tileW; int tileH; };

struct CachedIcon { 
    String name; 
    uint16_t* pixels; 
    uint8_t* alpha; 
    unsigned long lastUsed; 
    int width; 
    int height; 
};

struct AnimatedIcon {
    String name;
    uint16_t* pixels; 
    uint8_t* alpha;   
    int width;        
    int height;       
    int totalHeight;  
    int frameCount;   
    int delayMs;      
    unsigned long lastUsed;
};

struct GifConvertContext {
    uint8_t* canvasBuffer; 
    int width;
    int height; 
    int dispose;           
    int x, y, w, h;        
    int frameIndex; 
};

class IconManager {
private:
    std::map<String, IconDef> iconCatalog;
    std::map<String, SheetDef> sheetCatalog;
    std::map<String, String> aliasMap; 
    
    std::list<CachedIcon*> iconCache;      
    std::list<AnimatedIcon*> animCache;    
    std::vector<String> failedIcons;       

    const size_t MAX_CACHE_SIZE_STATIC = 50;
    const size_t MAX_CACHE_SIZE_ANIM = 10; 
    
    PNG png; 
    AnimatedGIF gif; 
    
    // Temporäres File-Handle für Streaming-Operationen (falls benötigt)
    static File staticGifFile; 

    // --- Helper ---
    uint32_t read32(const uint8_t* data, int offset) {
        return data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
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

    // --- Loading Logic ---
    
    // Lädt eine Animation vom Dateisystem in den PSRAM
    AnimatedIcon* loadAnimFromFS(String filename, String name) {
        if (!LittleFS.exists(filename)) return nullptr;
        File f = LittleFS.open(filename, "r");
        if (!f) return nullptr;

        uint8_t header[54];
        if (f.read(header, 54) != 54) { f.close(); return nullptr; }
        
        uint32_t dataOffset = read32(header, 10);
        int32_t w = read32(header, 18);
        int32_t h = read32(header, 22);
        bool flipY = true;
        if (h < 0) { h = -h; flipY = false; }

        if (w <= 0 || h <= 0) { f.close(); return nullptr; }

        int frames = 1;
        int frameH = h;
        if (h > w && (h % w == 0)) { frames = h / w; frameH = w; }
        else if (h > 8 && w == 8) { frames = h / 8; frameH = 8; }

        AnimatedIcon* anim = new AnimatedIcon();
        anim->name = name;
        anim->width = w;
        anim->height = frameH;
        anim->totalHeight = h;
        anim->frameCount = frames;
        anim->delayMs = 100;
        anim->lastUsed = millis();

        size_t numPixels = w * h;
        
        // Speicher zwingend im PSRAM reservieren
        anim->pixels = (uint16_t*)heap_caps_malloc(numPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        anim->alpha = (uint8_t*)heap_caps_malloc(numPixels * sizeof(uint8_t), MALLOC_CAP_SPIRAM);

        if (!anim->pixels || !anim->alpha) { 
            if(anim->pixels) free(anim->pixels); 
            if(anim->alpha) free(anim->alpha); 
            delete anim; f.close(); return nullptr; 
        }

        size_t lineSize = w * 4;
        uint8_t* lineBuffer = (uint8_t*)malloc(lineSize);
        if (!lineBuffer) { 
            free(anim->pixels); free(anim->alpha); delete anim; f.close(); return nullptr; 
        }

        for (int y = 0; y < h; y++) {
            if (y % 16 == 0) esp_task_wdt_reset(); 
            int srcY_Visual = y;
            int bmpRow = flipY ? (h - 1 - srcY_Visual) : srcY_Visual;
            size_t filePos = dataOffset + ((size_t)bmpRow * w * 4);
            f.seek(filePos);
            f.read(lineBuffer, lineSize);

            for (int x = 0; x < w; x++) {
                int targetIdx = y * w + x;
                int bufIdx = x * 4;
                
                // Konvertierung von 32-Bit (File) zu 16-Bit (Display)
                uint8_t b = lineBuffer[bufIdx];
                uint8_t g = lineBuffer[bufIdx+1];
                uint8_t r = lineBuffer[bufIdx+2];
                uint8_t a = lineBuffer[bufIdx+3];
                
                anim->pixels[targetIdx] = color565(r, g, b);
                anim->alpha[targetIdx] = a;
            }
        }
        free(lineBuffer); f.close(); return anim;
    }

    CachedIcon* loadBmpFile(String filename) {
        if (!LittleFS.exists(filename)) return nullptr;
        File f = LittleFS.open(filename, "r");
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

        if (!newIcon->pixels || !newIcon->alpha) { delete newIcon; f.close(); return nullptr; }

        size_t lineSize = width * 4;
        uint8_t* lineBuffer = (uint8_t*)malloc(lineSize);
        if(!lineBuffer) { free(newIcon->pixels); free(newIcon->alpha); delete newIcon; f.close(); return nullptr; }

        for (int y = 0; y < height; y++) {
             if (y % 16 == 0) esp_task_wdt_reset();
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
        free(lineBuffer); f.close(); return newIcon;
    }
    
    CachedIcon* loadIconFromSheet(String sheetName, int index) {
        if (sheetCatalog.find(sheetName) == sheetCatalog.end()) return nullptr;
        SheetDef& sheet = sheetCatalog[sheetName];
        if (!LittleFS.exists(sheet.filePath)) return nullptr;
        File f = LittleFS.open(sheet.filePath, "r");
        uint8_t header[54];
        f.read(header, 54);
        uint32_t dataOffset = read32(header, 10);
        int32_t width = read32(header, 18);
        int32_t height = read32(header, 22);
        bool flipY = (height > 0);
        if (height < 0) height = -height;
        if (sheet.tileW == 0) { sheet.tileW = width / sheet.cols; sheet.tileH = sheet.tileW; }
        CachedIcon* newIcon = new CachedIcon();
        newIcon->width = sheet.tileW;
        newIcon->height = sheet.tileH;
        size_t numPixels = sheet.tileW * sheet.tileH;
        newIcon->pixels = (uint16_t*)heap_caps_malloc(numPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        newIcon->alpha = (uint8_t*)heap_caps_malloc(numPixels * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        int col = index % sheet.cols;
        int row = index / sheet.cols;
        int startX = col * sheet.tileW;
        int startY = row * sheet.tileH; 
        size_t lineSize = sheet.tileW * 4; 
        uint8_t* lineBuffer = (uint8_t*)malloc(lineSize);
        for (int y = 0; y < sheet.tileH; y++) {
            int srcY_Visual = startY + y;
            int bmpRow = flipY ? (height - 1 - srcY_Visual) : srcY_Visual;
            size_t filePos = dataOffset + ((size_t)bmpRow * width * 4) + ((size_t)startX * 4);
            f.seek(filePos);
            f.read(lineBuffer, lineSize); 
            for (int x = 0; x < sheet.tileW; x++) {
                newIcon->pixels[y * sheet.tileW + x] = color565(lineBuffer[x*4+2], lineBuffer[x*4+1], lineBuffer[x*4]);
                newIcon->alpha[y * sheet.tileW + x] = lineBuffer[x*4+3];
            }
        }
        free(lineBuffer); f.close(); return newIcon;
    }

    // --- GIF Callbacks (Minimal) ---
    static void* GIFOpen(const char *filename, int32_t *size) { return (void*)1; }
    static void GIFClose(void *handle) { }
    static int32_t GIFRead(GIFFILE *handle, uint8_t *buffer, int32_t length) { return 0; }
    static int32_t GIFSeek(GIFFILE *handle, int32_t position) { return 0; }

    static void GIFDrawCallback(GIFDRAW *pDraw) {
        GifConvertContext* ctx = (GifConvertContext*)pDraw->pUser;
        ctx->dispose = pDraw->ucDisposalMethod;
        ctx->x = pDraw->iX;
        ctx->y = pDraw->iY;
        ctx->w = pDraw->iWidth;
        ctx->h = pDraw->iHeight;

        uint8_t *s = pDraw->pPixels;
        uint8_t *pPalette = (uint8_t*)pDraw->pPalette;
        if (!pPalette) return; 

        uint8_t* d = ctx->canvasBuffer;
        int y_abs = pDraw->iY + pDraw->y;
        if (y_abs >= ctx->height) return;

        uint32_t lineOffset = y_abs * ctx->width * 4;

        for (int x = 0; x < pDraw->iWidth; x++) {
            int x_abs = pDraw->iX + x;
            if (x_abs >= ctx->width) continue;
            
            // Transparenz behandeln: Pixel überspringen
            if (pDraw->ucHasTransparency && s[x] == pDraw->ucTransparent) continue; 

            uint16_t palIdx = s[x] * 3;
            uint8_t r = pPalette[palIdx];
            uint8_t g = pPalette[palIdx+1];
            uint8_t b = pPalette[palIdx+2];
            
            int idx = lineOffset + (x_abs * 4);
            d[idx] = b; d[idx+1] = g; d[idx+2] = r; d[idx+3] = 255; 
        }
    }

    // Manueller Download Loop für bessere Kontrolle
    bool downloadFile(HTTPClient& http, File& f) {
        int len = http.getSize();
        int total = 0;
        uint8_t buff[512] = { 0 };
        WiFiClient * stream = http.getStreamPtr();
        unsigned long start = millis();

        while (http.connected() || (stream && stream->available())) {
            size_t size = stream ? stream->available() : 0;
            if (size) {
                int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                f.write(buff, c);
                total += c;
                start = millis(); 
            } else { delay(10); }
            
            if (millis() - start > 3000) break; // 3s Timeout bei Stille
            if (len > 0 && total >= len) break; // Fertig
            esp_task_wdt_reset();
        }
        return total > 0;
    }

    bool downloadAndConvert(String id, String targetFolder, bool forceAnim) {
        if (WiFi.status() != WL_CONNECTED) return false;

        LittleFS.remove("/temp_dl.dat");
        bool success = false;
        String outName = targetFolder + id + ".bmp";

        if (forceAnim) {
            String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".gif";
            Serial.println("[ICON] Downloading GIF: " + url);

            WiFiClientSecure client;
            client.setInsecure();
            HTTPClient http;
            http.begin(client, url);
            http.setUserAgent("Mozilla/5.0 (ESP32)");
            http.setReuse(false);
            http.setTimeout(5000); 
            
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                File f = LittleFS.open("/temp_dl.dat", "w");
                if (f) {
                    if (downloadFile(http, f)) {
                        f.close();
                        // Decodierung im RAM für Stabilität
                        File fRead = LittleFS.open("/temp_dl.dat", "r");
                        size_t fSize = fRead.size();
                        uint8_t* gifRamBuffer = nullptr;
                        
                        if (fSize > 50) {
                            gifRamBuffer = (uint8_t*)malloc(fSize);
                            if (gifRamBuffer) {
                                fRead.read(gifRamBuffer, fSize);
                                fRead.close();
                                esp_task_wdt_reset();
                                
                                if (gif.open(gifRamBuffer, fSize, GIFDrawCallback)) {
                                    GIFINFO info;
                                    int frames = 0, w = 0, h = 0;
                                    if (gif.getInfo(&info)) {
                                        frames = info.iFrameCount;
                                        w = gif.getCanvasWidth();
                                        h = gif.getCanvasHeight();
                                    }
                                    
                                    if (frames > 0 && w > 0 && h > 0) {
                                        int totalH = h * frames;
                                        size_t stripSize = w * totalH * 4;
                                        size_t canvasSize = w * h * 4;
                                        
                                        uint8_t* stripBuffer = (uint8_t*)heap_caps_malloc(stripSize, MALLOC_CAP_SPIRAM);
                                        uint8_t* canvasBuffer = (uint8_t*)heap_caps_malloc(canvasSize, MALLOC_CAP_SPIRAM);
                                        
                                        if (stripBuffer && canvasBuffer) {
                                            // Puffer initial transparent füllen
                                            memset(stripBuffer, 0, stripSize); 
                                            memset(canvasBuffer, 0, canvasSize); 
                                            
                                            GifConvertContext ctx;
                                            ctx.canvasBuffer = canvasBuffer; 
                                            ctx.width = w; ctx.height = h;
                                            ctx.dispose = 0;
                                            
                                            int prevDispose = 2; 
                                            int prevX=0, prevY=0, prevW=w, prevH=h;
                                            
                                            for (int i=0; i<frames; i++) {
                                                ctx.frameIndex = i;
                                                
                                                // Disposal Logic: Restore Background -> Transparent
                                                if (prevDispose == 2) { 
                                                    for(int dy=prevY; dy<prevY+prevH; dy++) {
                                                        if(dy>=h) break;
                                                        for(int dx=prevX; dx<prevX+prevW; dx++) {
                                                            if(dx>=w) break;
                                                            int idx = (dy*w + dx)*4;
                                                            canvasBuffer[idx] = 0; 
                                                            canvasBuffer[idx+1] = 0;
                                                            canvasBuffer[idx+2] = 0; 
                                                            canvasBuffer[idx+3] = 0;
                                                        }
                                                    }
                                                }

                                                gif.playFrame(false, NULL, &ctx); 
                                                
                                                size_t offset = i * canvasSize;
                                                memcpy(stripBuffer + offset, canvasBuffer, canvasSize);

                                                prevDispose = ctx.dispose;
                                                prevX = ctx.x; prevY = ctx.y; 
                                                prevW = ctx.w; prevH = ctx.h;
                                                esp_task_wdt_reset();
                                            }
                                            gif.close();

                                            // BMP Speichern
                                            File fOut = LittleFS.open(outName, "w");
                                            writeBmpHeader(fOut, w, totalH);
                                            size_t lineSize = w * 4;
                                            for (int y = totalH - 1; y >= 0; y--) {
                                                uint8_t* srcRow = stripBuffer + (y * lineSize);
                                                fOut.write(srcRow, lineSize);
                                            }
                                            fOut.close();
                                            free(stripBuffer); free(canvasBuffer);
                                            success = true;
                                            Serial.println("[ICON] Processed & Saved: " + outName);
                                        } else {
                                            if(stripBuffer) free(stripBuffer);
                                            if(canvasBuffer) free(canvasBuffer);
                                            gif.close();
                                        }
                                    } 
                                } 
                                if(gifRamBuffer) free(gifRamBuffer);
                            } 
                        } 
                        if(fRead) fRead.close();
                    } else { f.close(); }
                } 
                LittleFS.remove("/temp_dl.dat");
            } 
            client.stop(); // Wichtig: SSL Session schließen
            http.end();
        }

        // 2. Fallback PNG
        if (!success) {
            String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".png";
            Serial.println("[ICON] Downloading PNG: " + id);
            
            WiFiClientSecure client;
            client.setInsecure(); 
            HTTPClient http;
            http.begin(client, url);
            http.setUserAgent("Mozilla/5.0 (ESP32)");
            http.setReuse(false);
            http.setTimeout(3000);
            
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                 File f = LittleFS.open("/temp_dl.dat", "w");
                 if (f && downloadFile(http, f)) {
                     f.close();
                     int rc = png.open("/temp_dl.dat", myOpen, myClose, myRead, mySeek, myDraw);
                     if (rc == PNG_SUCCESS) {
                        int w = png.getWidth();
                        int h = png.getHeight();
                        // PNG Puffer ebenfalls im PSRAM
                        uint8_t* rgbaBuffer = (uint8_t*)heap_caps_malloc(w * h * 4, MALLOC_CAP_SPIRAM);
                        if (rgbaBuffer) {
                            png.decode(rgbaBuffer, 0); 
                            png.close();
                            File fOut = LittleFS.open(outName, "w");
                            writeBmpHeader(fOut, w, h);
                            size_t lineSize = w * 4;
                            uint8_t* lineOut = (uint8_t*)malloc(lineSize);
                            for (int y = h - 1; y >= 0; y--) {
                                uint8_t* srcRow = rgbaBuffer + (y * lineSize);
                                for(int x=0; x<w; x++) {
                                    int i = x * 4;
                                    lineOut[i]   = srcRow[i+2]; 
                                    lineOut[i+1] = srcRow[i+1]; 
                                    lineOut[i+2] = srcRow[i];   
                                    lineOut[i+3] = srcRow[i+3]; 
                                }
                                fOut.write(lineOut, lineSize);
                            }
                            free(lineOut);
                            fOut.close();
                            free(rgbaBuffer);
                            success = true;
                            Serial.println("[ICON] Saved as BMP: " + outName);
                        } else {
                            Serial.println("[ICON] PNG OOM in PSRAM");
                            png.close();
                        }
                    }
                 } else if (f) f.close();
                 LittleFS.remove("/temp_dl.dat");
            }
            client.stop(); 
            http.end();
        }
        return success;
    }

    // --- PNG Callbacks ---
    static void* myOpen(const char *filename, int32_t *size) {
        File f = LittleFS.open(filename, "r");
        if (size) *size = f.size();
        return new File(f);
    }
    static void myClose(void *handle) { File* f = (File*)handle; f->close(); delete f; }
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
    
    void begin() {
        gif.begin(GIF_PALETTE_RGB888); 
        
        if (!LittleFS.exists("/catalog.json")) return; 
        File f = LittleFS.open("/catalog.json", "r");
        DynamicJsonDocument* doc = new DynamicJsonDocument(8192); 
        deserializeJson(*doc, f); f.close();

        JsonObject sheets = (*doc)["sheets"];
        for (JsonPair kv : sheets) {
            String key = kv.key().c_str(); JsonObject val = kv.value();
            SheetDef def; 
            def.filePath = val["file"].as<String>(); 
            def.cols = val["cols"] | 1; 
            sheetCatalog[key] = def;
        }
        JsonObject icons = (*doc)["icons"];
        for (JsonPair kv : icons) {
            IconDef def; def.sheetName = kv.value()["sheet"].as<String>(); def.index = kv.value()["index"] | 0; 
            iconCatalog[kv.key().c_str()] = def;
        }
        JsonObject aliases = (*doc)["aliases"];
        for (JsonPair kv : aliases) aliasMap[kv.key().c_str()] = kv.value().as<String>();
        delete doc; 

        if (!LittleFS.exists("/icons")) LittleFS.mkdir("/icons");
        if (!LittleFS.exists("/iconsan")) LittleFS.mkdir("/iconsan");
    }
    
    String resolveAlias(String tag) {
        if (aliasMap.count(tag)) return aliasMap[tag];
        return "";
    }

    CachedIcon* getIcon(String name) {
        for (auto it = iconCache.begin(); it != iconCache.end(); ++it) {
            if ((*it)->name == name) {
                (*it)->lastUsed = millis();
                if (it != iconCache.begin()) iconCache.splice(iconCache.begin(), iconCache, it);
                return *it;
            }
        }
        for(const String& bad : failedIcons) if (bad == name) return nullptr;

        CachedIcon* newIcon = nullptr;
        if (iconCatalog.count(name)) {
            newIcon = loadIconFromSheet(iconCatalog[name].sheetName, iconCatalog[name].index);
        } else if (LittleFS.exists("/icons/" + name + ".bmp")) {
             newIcon = loadBmpFile("/icons/" + name + ".bmp");
        } else {
             bool isNumeric = true;
             for(unsigned int i=0; i<name.length(); i++) if(!isDigit(name[i])) isNumeric = false;
             if (isNumeric && name.length() > 0) {
                 if (downloadAndConvert(name, "/icons/", false)) { 
                     newIcon = loadBmpFile("/icons/" + name + ".bmp");
                 } else {
                     failedIcons.push_back(name);
                 }
             } else failedIcons.push_back(name);
        }
        
        if (newIcon) {
            newIcon->name = name; 
            newIcon->lastUsed = millis();
            if (iconCache.size() >= MAX_CACHE_SIZE_STATIC) {
                CachedIcon* old = iconCache.back(); iconCache.pop_back(); 
                free(old->pixels); free(old->alpha); delete old;
            }
            iconCache.push_front(newIcon);
        }
        return newIcon;
    }

    AnimatedIcon* getAnimatedIcon(String id) {
        for (auto it = animCache.begin(); it != animCache.end(); ++it) {
            if ((*it)->name == id) {
                (*it)->lastUsed = millis();
                return *it;
            }
        }
        
        for(const String& bad : failedIcons) if (bad == id) return nullptr;

        AnimatedIcon* anim = nullptr;
        String path = "/iconsan/" + id + ".bmp";

        if (!LittleFS.exists(path)) {
            bool isNumeric = true;
            for(unsigned int i=0; i<id.length(); i++) if(!isDigit(id[i])) isNumeric = false;
            
            if (isNumeric && id.length() > 0) {
                if (!downloadAndConvert(id, "/iconsan/", true)) {
                    failedIcons.push_back(id);
                    return nullptr;
                }
            } else {
                return nullptr;
            }
        }

        anim = loadAnimFromFS(path, id);
        
        if (anim) {
            if (animCache.size() >= MAX_CACHE_SIZE_ANIM) {
                AnimatedIcon* old = animCache.back(); animCache.pop_back();
                free(old->pixels); free(old->alpha); delete old;
            }
            animCache.push_front(anim);
        } else {
            failedIcons.push_back(id);
        }
        
        return anim;
    }

    void drawIcon(DisplayManager& display, int x, int y, String name, bool scaleTo16 = false) {
        CachedIcon* icon = getIcon(name); 
        if (!icon) return; 
        
        bool doUpscale = scaleTo16 && (icon->width == 8) && (icon->height == 8);
        for (int iy = 0; iy < (doUpscale ? 16 : icon->height); iy++) {
            for (int ix = 0; ix < (doUpscale ? 16 : icon->width); ix++) {
                int screenX = x + ix; int screenY = y + iy;
                if (screenX < 0 || screenX >= M_WIDTH || screenY < 0 || screenY >= M_HEIGHT) continue;
                
                int srcX = doUpscale ? (ix/2) : ix;
                int srcY = doUpscale ? (iy/2) : iy;
                int i = srcY * icon->width + srcX;
                
                if (icon->alpha[i] > 10) display.drawPixel(screenX, screenY, icon->pixels[i]);
            }
        }
    }

    void drawAnimatedIcon(DisplayManager& display, int x, int y, String id) {
        AnimatedIcon* anim = getAnimatedIcon(id);

        if (!anim) {
            // Roter Punkt bei Fehler (oder Platzhalter)
            display.drawPixel(x, y, display.color565(255, 0, 0));
            return;
        }

        int frameDuration = anim->delayMs; 
        if (frameDuration < 50) frameDuration = 500; 
        
        int totalTime = anim->frameCount * frameDuration;
        int currentFrameIdx = (millis() % totalTime) / frameDuration;
        if (currentFrameIdx >= anim->frameCount) currentFrameIdx = 0; 

        int pixelsPerFrame = anim->width * anim->height;
        int startPixelIdx = currentFrameIdx * pixelsPerFrame;

        bool doUpscale = (anim->width == 8 && anim->height == 8);
        
        int drawW = doUpscale ? 16 : anim->width;
        int drawH = doUpscale ? 16 : anim->height;

        for (int iy = 0; iy < drawH; iy++) {
            for (int ix = 0; ix < drawW; ix++) {
                int screenX = x + ix; int screenY = y + iy;
                if (screenX < 0 || screenX >= M_WIDTH || screenY < 0 || screenY >= M_HEIGHT) continue;
                
                int srcX = doUpscale ? (ix/2) : ix;
                int srcY = doUpscale ? (iy/2) : iy;
                
                int i = startPixelIdx + (srcY * anim->width + srcX);

                if (anim->alpha[i] > 10) display.drawPixel(screenX, screenY, anim->pixels[i]);
            }
        }
    }

    int getAnimWidth(String id) {
        AnimatedIcon* anim = getAnimatedIcon(id);
        if (anim) {
            return (anim->width == 8) ? 16 : anim->width; 
        }
        return 16; 
    }
    
    int getIconWidth(String name) {
        CachedIcon* i = getIcon(name);
        return i ? i->width : 16; 
    }
    int getIconHeight(String name) {
        CachedIcon* i = getIcon(name);
        return i ? i->height : 16;
    }
};

File IconManager::staticGifFile;