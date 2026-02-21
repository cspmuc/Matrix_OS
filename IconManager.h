#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <list>
#include <vector>
#include <HTTPClient.h> 
#include <WiFiClientSecure.h> 
#include <PNGdec.h>     
#include <AnimatedGIF.h> 
#include "DisplayManager.h"
#include <esp_heap_caps.h> 

#ifndef SPIRAM_ALLOCATOR_DEFINED
#define SPIRAM_ALLOCATOR_DEFINED
struct SpiRamAllocator {
  void* allocate(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
  void deallocate(void* pointer) { heap_caps_free(pointer); }
  void* reallocate(void* ptr, size_t new_size) { return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM); }
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;
#endif

struct SheetDef { 
    String filePath; 
    int cols; 
    int tileW; 
    int tileH; 
    SheetDef() : cols(1), tileW(0), tileH(0) {} 
};

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
    uint16_t* delays; // NEU: PSRAM-Array für die individuellen Frame-Delays
    int width;        
    int height;       
    int totalHeight;  
    int frameCount;   
    int totalTime;    // NEU: Gesamtdauer der Animation in Millisekunden
    unsigned long lastUsed;
};

struct GifConvertContext {
    uint8_t* canvasBuffer; 
    int width, height, dispose, x, y, w, h, frameIndex; 
};

struct PngExtractContext {
    uint16_t* pixels; 
    uint8_t* alpha;   
    int targetX, targetY, targetW, targetH; 
    int sheetW;
    bool hasTransColor;
    uint32_t transColor; 
};

struct PngDownloadContext {
    File* fOut;
    int w;
    uint8_t* lineBuffer; 
    bool hasTransColor;
    uint32_t transColor;
};

class IconManager {
private:
    std::list<CachedIcon*> iconCache;      
    std::list<AnimatedIcon*> animCache;    
    std::vector<String> failedIcons;       

    const size_t MAX_CACHE_SIZE_STATIC = 20;
    const size_t MAX_CACHE_SIZE_ANIM = 10; 
    
    PNG png; 
    AnimatedGIF gif; 

    // --- Helper ---
    uint32_t read32(const uint8_t* data, int offset) {
        return data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
    }
    void write32(uint8_t* data, int offset, uint32_t val) {
        data[offset] = val & 0xFF; data[offset+1] = (val >> 8) & 0xFF;
        data[offset+2] = (val >> 16) & 0xFF; data[offset+3] = (val >> 24) & 0xFF;
    }
    void write16(uint8_t* data, int offset, uint16_t val) {
        data[offset] = val & 0xFF; data[offset+1] = (val >> 8) & 0xFF;
    }

    void writeBmpHeader(File& f, int w, int h) {
        uint8_t header[54] = {0};
        header[0] = 'B'; header[1] = 'M';
        uint32_t dataSize = w * abs(h) * 4;
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

    // --- Laderoutinen ---
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
        anim->name = name; anim->width = w; anim->height = frameH;
        anim->totalHeight = h; anim->frameCount = frames;
        anim->lastUsed = millis();

        size_t numPixels = w * h;
        anim->pixels = (uint16_t*)heap_caps_malloc(numPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        anim->alpha = (uint8_t*)heap_caps_malloc(numPixels * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        // NEU: PSRAM Allokation für die Frame-Delays
        anim->delays = (uint16_t*)heap_caps_malloc(frames * sizeof(uint16_t), MALLOC_CAP_SPIRAM);

        if (!anim->pixels || !anim->alpha || !anim->delays) { 
            if(anim->pixels) heap_caps_free(anim->pixels); 
            if(anim->alpha) heap_caps_free(anim->alpha); 
            if(anim->delays) heap_caps_free(anim->delays);
            delete anim; f.close(); return nullptr; 
        }

        // --- NEU: DLY Datei laden ---
        String dlyFilename = filename;
        dlyFilename.replace(".bmp", ".dly");
        int calculatedTotalTime = 0;

        File fDly = LittleFS.open(dlyFilename, "r");
        if (fDly) {
            for (int i = 0; i < frames; i++) {
                uint16_t d = 100;
                if (fDly.available() >= 2) fDly.read((uint8_t*)&d, 2);
                anim->delays[i] = d;
                calculatedTotalTime += d;
            }
            fDly.close();
        } else {
            // Fallback für alte Animationen ohne .dly Datei
            for (int i = 0; i < frames; i++) {
                anim->delays[i] = 100;
                calculatedTotalTime += 100;
            }
        }
        anim->totalTime = calculatedTotalTime;

        size_t lineSize = w * 4;
        uint8_t* lineBuffer = (uint8_t*)heap_caps_malloc(lineSize, MALLOC_CAP_SPIRAM);
        if (!lineBuffer) { 
            heap_caps_free(anim->pixels); heap_caps_free(anim->alpha); heap_caps_free(anim->delays);
            delete anim; f.close(); return nullptr; 
        }

        for (int y = 0; y < h; y++) {
            int bmpRow = flipY ? (h - 1 - y) : y;
            f.seek(dataOffset + ((size_t)bmpRow * w * 4));
            f.read(lineBuffer, lineSize);
            for (int x = 0; x < w; x++) {
                int idx = x * 4;
                anim->pixels[y * w + x] = color565(lineBuffer[idx+2], lineBuffer[idx+1], lineBuffer[idx]);
                anim->alpha[y * w + x] = lineBuffer[idx+3];
            }
        }
        heap_caps_free(lineBuffer); f.close(); return anim;
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
        newIcon->width = width; newIcon->height = height;
        size_t numPixels = width * height;
        
        newIcon->pixels = (uint16_t*)heap_caps_malloc(numPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        newIcon->alpha = (uint8_t*)heap_caps_malloc(numPixels * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        if (!newIcon->pixels || !newIcon->alpha) { delete newIcon; f.close(); return nullptr; }

        size_t lineSize = width * 4;
        uint8_t* lineBuffer = (uint8_t*)heap_caps_malloc(lineSize, MALLOC_CAP_SPIRAM);
        if(!lineBuffer) { heap_caps_free(newIcon->pixels); heap_caps_free(newIcon->alpha); delete newIcon; f.close(); return nullptr; }

        for (int y = 0; y < height; y++) {
            int bmpRow = flipY ? (height - 1 - y) : y;
            f.seek(dataOffset + ((size_t)bmpRow * width * 4));
            f.read(lineBuffer, lineSize);
            for (int x = 0; x < width; x++) {
                int idx = x * 4;
                newIcon->pixels[y * width + x] = color565(lineBuffer[idx+2], lineBuffer[idx+1], lineBuffer[idx]);
                newIcon->alpha[y * width + x] = lineBuffer[idx+3];
            }
        }
        heap_caps_free(lineBuffer); f.close(); return newIcon;
    }
    
    // --- PNG und Sprite Sheet Decoder ---
    static int pngSheetDrawCallback(PNGDRAW *pDraw) {
        if (pDraw->y % 32 == 0) yield(); 

        PngExtractContext* ctx = (PngExtractContext*)pDraw->pUser;
        int y = pDraw->y; 

        if (y >= ctx->targetY + ctx->targetH) return 0; 
        if (y < ctx->targetY) return 1; 

        uint8_t* src = (uint8_t*)pDraw->pPixels;
        uint8_t* pPalette = (uint8_t*)pDraw->pPalette;
        int pixelType = pDraw->iPixelType; 
        int rowInTile = y - ctx->targetY;
        
        for (int x = 0; x < pDraw->iWidth; x++) {
            if (x < ctx->targetX || x >= ctx->targetX + ctx->targetW) continue;
            int colInTile = x - ctx->targetX;
            int targetIndex = rowInTile * ctx->targetW + colInTile;
            
            uint8_t r=0, g=0, b=0, a=255;

            if (pixelType == 3 && pPalette) { 
                uint8_t idx = src[x];
                if (ctx->hasTransColor && idx == (uint8_t)ctx->transColor) { a = 0; } 
                else { r = pPalette[idx*3]; g = pPalette[idx*3+1]; b = pPalette[idx*3+2]; }
            } else if (pixelType == 2) { 
                int idx = x * 3;
                r = src[idx]; g = src[idx+1]; b = src[idx+2];
                if (ctx->hasTransColor) {
                    uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
                    if(rgb == ctx->transColor) a = 0;
                }
            } else if (pixelType == 6) { 
                int idx = x * 4;
                r = src[idx]; g = src[idx+1]; b = src[idx+2]; a = src[idx+3];
            }
            
            ctx->pixels[targetIndex] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            ctx->alpha[targetIndex] = a;
        }
        return 1;
    }

    CachedIcon* loadPngIconFromSheet(const SheetDef& sheet, int index) {
        if (!LittleFS.exists(sheet.filePath)) return nullptr;
        if (png.open(sheet.filePath.c_str(), myOpen, myClose, myRead, mySeek, pngSheetDrawCallback) != PNG_SUCCESS) return nullptr;

        int imgW = png.getWidth();
        int cols = (sheet.cols > 0) ? sheet.cols : 1;
        int tileW = (sheet.tileW > 0) ? sheet.tileW : (imgW / cols);
        int tileH = (sheet.tileH > 0) ? sheet.tileH : tileW; 

        CachedIcon* newIcon = new CachedIcon();
        newIcon->width = tileW; newIcon->height = tileH;
        newIcon->pixels = (uint16_t*)heap_caps_malloc(tileW * tileH * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        newIcon->alpha = (uint8_t*)heap_caps_malloc(tileW * tileH * sizeof(uint8_t), MALLOC_CAP_SPIRAM);

        if (!newIcon->pixels || !newIcon->alpha) { delete newIcon; png.close(); return nullptr; }

        PngExtractContext ctx;
        ctx.pixels = newIcon->pixels; ctx.alpha = newIcon->alpha;
        ctx.targetX = (index % cols) * tileW; ctx.targetY = (index / cols) * tileH;
        ctx.targetW = tileW; ctx.targetH = tileH; ctx.sheetW = imgW;
        
        int transColor = png.getTransparentColor(); 
        ctx.hasTransColor = (transColor != -1);
        ctx.transColor = (uint32_t)transColor;

        png.decode((void*)&ctx, 0);
        png.close();
        return newIcon;
    }

    CachedIcon* loadIconFromSheet(const SheetDef& sheet, int index) {
        String lowerPath = sheet.filePath;
        lowerPath.toLowerCase();
        if (lowerPath.endsWith(".png")) return loadPngIconFromSheet(sheet, index);

        if (!LittleFS.exists(sheet.filePath)) return nullptr;
        File f = LittleFS.open(sheet.filePath, "r");
        uint8_t header[54]; f.read(header, 54);
        uint32_t dataOffset = read32(header, 10);
        int32_t width = read32(header, 18);
        int32_t height = read32(header, 22);
        bool flipY = (height > 0);
        if (height < 0) height = -height;
        
        int tileW = (sheet.tileW > 0) ? sheet.tileW : (width / sheet.cols);
        int tileH = (sheet.tileH > 0) ? sheet.tileH : tileW;
        
        CachedIcon* newIcon = new CachedIcon();
        newIcon->width = tileW; newIcon->height = tileH;
        newIcon->pixels = (uint16_t*)heap_caps_malloc(tileW * tileH * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        newIcon->alpha = (uint8_t*)heap_caps_malloc(tileW * tileH * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        
        int startX = (index % sheet.cols) * tileW;
        int startY = (index / sheet.cols) * tileH; 
        
        size_t lineSize = tileW * 4; 
        uint8_t* lineBuffer = (uint8_t*)heap_caps_malloc(lineSize, MALLOC_CAP_SPIRAM);
        
        for (int y = 0; y < tileH; y++) {
            int bmpRow = flipY ? (height - 1 - (startY + y)) : (startY + y);
            f.seek(dataOffset + ((size_t)bmpRow * width * 4) + ((size_t)startX * 4));
            f.read(lineBuffer, lineSize); 
            for (int x = 0; x < tileW; x++) {
                newIcon->pixels[y * tileW + x] = color565(lineBuffer[x*4+2], lineBuffer[x*4+1], lineBuffer[x*4]);
                newIcon->alpha[y * tileW + x] = lineBuffer[x*4+3];
            }
        }
        heap_caps_free(lineBuffer); f.close(); return newIcon;
    }

    // --- Standard Callbacks (File I/O) ---
    static void* myOpen(const char *filename, int32_t *size) {
        File* f = new File(LittleFS.open(filename, "r"));
        if (*f) { if (size) *size = f->size(); return (void*)f; }
        delete f; return nullptr;
    }
    static void myClose(void *handle) { File* f = (File*)handle; if(f) { f->close(); delete f; } }
    static int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
        File* f = (File*)handle->fHandle; return f ? f->read(buffer, length) : 0;
    }
    static int32_t mySeek(PNGFILE *handle, int32_t position) {
        File* f = (File*)handle->fHandle; return f ? f->seek(position) : 0;
    }
    
    // --- Download PNG & GIF Konvertierung ---
    static int pngDownloadDraw(PNGDRAW *pDraw) {
        PngDownloadContext* ctx = (PngDownloadContext*)pDraw->pUser;
        uint8_t* src = (uint8_t*)pDraw->pPixels;
        uint8_t* pPalette = (uint8_t*)pDraw->pPalette;
        int pixelType = pDraw->iPixelType; 
        
        for (int x = 0; x < pDraw->iWidth; x++) {
            uint8_t r=0, g=0, b=0, a=255;
            if (pixelType == 3 && pPalette) { 
                uint8_t idx = src[x];
                if (ctx->hasTransColor && idx == (uint8_t)ctx->transColor) a = 0;
                else { r = pPalette[idx*3]; g = pPalette[idx*3+1]; b = pPalette[idx*3+2]; }
            } else if (pixelType == 2) { 
                int idx = x * 3; r = src[idx]; g = src[idx+1]; b = src[idx+2];
                if (ctx->hasTransColor) {
                    uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
                    if(rgb == ctx->transColor) a = 0;
                }
            } else if (pixelType == 6) { 
                int idx = x * 4; r = src[idx]; g = src[idx+1]; b = src[idx+2]; a = src[idx+3];
            }
            
            int targetIdx = x * 4;
            ctx->lineBuffer[targetIdx] = b; ctx->lineBuffer[targetIdx+1] = g;
            ctx->lineBuffer[targetIdx+2] = r; ctx->lineBuffer[targetIdx+3] = a;
        }
        ctx->fOut->write(ctx->lineBuffer, ctx->w * 4);
        return 1;
    }

    static void* GIFOpen(const char *filename, int32_t *size) { return (void*)1; }
    static void GIFClose(void *handle) { }
    static int32_t GIFRead(GIFFILE *handle, uint8_t *buffer, int32_t length) { return 0; }
    static int32_t GIFSeek(GIFFILE *handle, int32_t position) { return 0; }

    static void GIFDrawCallback(GIFDRAW *pDraw) {
        GifConvertContext* ctx = (GifConvertContext*)pDraw->pUser;
        ctx->dispose = pDraw->ucDisposalMethod; ctx->x = pDraw->iX;
        ctx->y = pDraw->iY; ctx->w = pDraw->iWidth; ctx->h = pDraw->iHeight;

        uint8_t *s = pDraw->pPixels;
        uint8_t *pPalette = (uint8_t*)pDraw->pPalette;
        if (!pPalette) return; 

        int y_abs = pDraw->iY + pDraw->y;
        if (y_abs >= ctx->height) return;

        uint32_t lineOffset = y_abs * ctx->width * 4;

        for (int x = 0; x < pDraw->iWidth; x++) {
            int x_abs = pDraw->iX + x;
            if (x_abs >= ctx->width || (pDraw->ucHasTransparency && s[x] == pDraw->ucTransparent)) continue;
            uint16_t palIdx = s[x] * 3;
            int idx = lineOffset + (x_abs * 4);
            ctx->canvasBuffer[idx] = pPalette[palIdx+2];   // B
            ctx->canvasBuffer[idx+1] = pPalette[palIdx+1]; // G
            ctx->canvasBuffer[idx+2] = pPalette[palIdx];   // R
            ctx->canvasBuffer[idx+3] = 255;                // A
        }
    }

    bool downloadFile(String url, File& fOut) {
        for (int redirects = 0; redirects < 3; redirects++) {
            WiFiClientSecure client;
            client.setInsecure();
            
            HTTPClient http;
            http.begin(client, url);
            http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
            
            const char* headerKeys[] = {"Location"};
            http.collectHeaders(headerKeys, 1);
            http.setTimeout(5000); 
            
            int httpCode = http.GET();
            
            if (httpCode == HTTP_CODE_OK) {
                int written = http.writeToStream(&fOut);
                http.end();
                return written > 0;
            } 
            else if (httpCode == 301 || httpCode == 302 || httpCode == 307 || httpCode == 308) {
                String newUrl = http.header("Location");
                http.end();
                if (newUrl == "") return false;
                if (newUrl.startsWith("/")) newUrl = "https://developer.lametric.com" + newUrl;
                url = newUrl;
                Serial.println("[ICON] Redirect: " + url);
                continue; 
            } 
            else {
                Serial.printf("[ICON] HTTP Error %d for %s\n", httpCode, url.c_str());
                http.end();
                return false;
            }
        }
        return false;
    }

    bool downloadAndConvert(String id, String targetFolder, bool forceAnim) {
        if (WiFi.status() != WL_CONNECTED) return false;
        LittleFS.remove("/temp_dl.dat");
        bool success = false;
        String outName = targetFolder + id + ".bmp";
        String dlyName = targetFolder + id + ".dly"; // NEU: Delay File

        if (forceAnim) {
            String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".gif";
            Serial.println("[ICON] Downloading GIF: " + url);
            File f = LittleFS.open("/temp_dl.dat", "w");
            if (f && downloadFile(url, f)) {
                f.close();
                File fRead = LittleFS.open("/temp_dl.dat", "r");
                size_t fSize = fRead.size();
                uint8_t* gifRamBuffer = (uint8_t*)heap_caps_malloc(fSize, MALLOC_CAP_SPIRAM);
                
                if (gifRamBuffer && fSize > 50) {
                    fRead.read(gifRamBuffer, fSize); fRead.close();
                    if (gif.open(gifRamBuffer, fSize, GIFDrawCallback)) {
                        GIFINFO info;
                        if (gif.getInfo(&info) && info.iFrameCount > 0) {
                            int w = gif.getCanvasWidth(), h = gif.getCanvasHeight();
                            int frames = info.iFrameCount;
                            int totalH = h * frames;
                            size_t stripSize = w * totalH * 4, canvasSize = w * h * 4;
                            
                            uint8_t* stripBuffer = (uint8_t*)heap_caps_malloc(stripSize, MALLOC_CAP_SPIRAM);
                            uint8_t* canvasBuffer = (uint8_t*)heap_caps_malloc(canvasSize, MALLOC_CAP_SPIRAM);
                            
                            if (stripBuffer && canvasBuffer) {
                                memset(stripBuffer, 0, stripSize); 
                                GifConvertContext ctx = {canvasBuffer, w, h, 0, 0, 0, 0, 0}; 
                                int prevDispose = 2;
                                
                                // NEU: Schreibe Delay-Werte parallel
                                File fDly = LittleFS.open(dlyName, "w");
                                
                                for (int i=0; i<frames; i++) {
                                    if (i % 10 == 0) yield(); 
                                    ctx.frameIndex = i;
                                    if (prevDispose == 2) memset(canvasBuffer, 0, canvasSize); 
                                    
                                    // Hole Delay von der GIF Lib (in Millisekunden)
                                    int frameDelayMs = 0;
                                    gif.playFrame(false, &frameDelayMs, &ctx); 
                                    if (frameDelayMs < 20) frameDelayMs = 100; // Fallback
                                    
                                    if (fDly) {
                                        uint16_t d = (uint16_t)frameDelayMs;
                                        fDly.write((uint8_t*)&d, 2);
                                    }
                                    
                                    memcpy(stripBuffer + (i*canvasSize), canvasBuffer, canvasSize);
                                    prevDispose = ctx.dispose;
                                }
                                if (fDly) fDly.close();
                                gif.close();
                                
                                File fOut = LittleFS.open(outName, "w");
                                writeBmpHeader(fOut, w, totalH);
                                for (int y = totalH - 1; y >= 0; y--) fOut.write(stripBuffer + (y * w * 4), w * 4);
                                fOut.close();
                                success = true;
                                Serial.println("[ICON] GIF Converted: " + outName);
                                heap_caps_free(stripBuffer); heap_caps_free(canvasBuffer);
                            }
                        }
                    }
                    heap_caps_free(gifRamBuffer);
                } else if(fRead) fRead.close();
            } else if (f) f.close();
            LittleFS.remove("/temp_dl.dat");
        }

        if (!success && !forceAnim) {
            String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".png";
            Serial.println("[ICON] Downloading PNG: " + url);
            File f = LittleFS.open("/temp_dl.dat", "w");
            if (f && downloadFile(url, f)) {
                 f.close();
                 File fOut = LittleFS.open(outName, "w");
                 if(fOut) {
                     PngDownloadContext ctx; ctx.fOut = &fOut;
                     if (png.open("/temp_dl.dat", myOpen, myClose, myRead, mySeek, pngDownloadDraw) == PNG_SUCCESS) {
                         ctx.w = png.getWidth();
                         int tColor = png.getTransparentColor();
                         ctx.hasTransColor = (tColor != -1); ctx.transColor = (uint32_t)tColor;

                         int h = png.getHeight();
                         writeBmpHeader(fOut, ctx.w, -h); 
                         
                         ctx.lineBuffer = (uint8_t*)heap_caps_malloc(ctx.w * 4, MALLOC_CAP_SPIRAM);
                         if (ctx.lineBuffer) {
                             png.decode((void*)&ctx, 0); 
                             heap_caps_free(ctx.lineBuffer);
                             success = true;
                             Serial.println("[ICON] PNG Saved: " + outName);
                         }
                         png.close();
                     }
                     fOut.close();
                     if(!success) LittleFS.remove(outName); 
                 }
            } else if (f) f.close();
            LittleFS.remove("/temp_dl.dat");
        }
        return success;
    }

public:
    IconManager() {}
    
    void begin() {
        gif.begin(GIF_PALETTE_RGB888); 
        if (!LittleFS.exists("/icons")) LittleFS.mkdir("/icons");
        if (!LittleFS.exists("/iconsan")) LittleFS.mkdir("/iconsan");
    }
    
    String resolveAlias(String tag) {
        String result = "";
        if (LittleFS.exists("/catalog.json")) {
            File f = LittleFS.open("/catalog.json", "r");
            if (f) {
                SpiRamJsonDocument* doc = new SpiRamJsonDocument(8192); 
                if (!deserializeJson(*doc, f)) {
                    if (doc->containsKey("aliases") && (*doc)["aliases"].containsKey(tag)) {
                        result = (*doc)["aliases"][tag].as<String>();
                    }
                }
                delete doc; f.close();
            }
        }
        return result;
    }

    CachedIcon* getIcon(String name) {
        if (name.startsWith("ln:")) name = name.substring(3);
        else if (name.startsWith("la:")) name = name.substring(3);
        else if (name.startsWith("ic:")) name = name.substring(3);

        for (auto it = iconCache.begin(); it != iconCache.end(); ++it) {
            if ((*it)->name == name) {
                (*it)->lastUsed = millis();
                if (it != iconCache.begin()) iconCache.splice(iconCache.begin(), iconCache, it);
                return *it;
            }
        }
        for(const String& bad : failedIcons) if (bad == name) return nullptr;

        CachedIcon* newIcon = nullptr;
        bool foundInCatalog = false;

        if (LittleFS.exists("/catalog.json")) {
            File f = LittleFS.open("/catalog.json", "r");
            if (f) {
                SpiRamJsonDocument* doc = new SpiRamJsonDocument(8192);
                if (!deserializeJson(*doc, f)) {
                    if (doc->containsKey("icons") && (*doc)["icons"].containsKey(name)) {
                        String sheetName = (*doc)["icons"][name]["sheet"].as<String>();
                        int sheetIndex = (*doc)["icons"][name]["index"].as<int>();
                        
                        if (doc->containsKey("sheets") && (*doc)["sheets"].containsKey(sheetName)) {
                            SheetDef def;
                            def.filePath = (*doc)["sheets"][sheetName]["file"].as<String>();
                            def.cols = (*doc)["sheets"][sheetName]["cols"] | 1;
                            foundInCatalog = true;
                            
                            delete doc; doc = nullptr; f.close();
                            newIcon = loadIconFromSheet(def, sheetIndex);
                        }
                    }
                }
                if (doc) delete doc;
                if (f) f.close();
            }
        }

        if (!foundInCatalog) {
             if (LittleFS.exists("/icons/" + name + ".bmp")) {
                  newIcon = loadBmpFile("/icons/" + name + ".bmp");
             } else {
                  bool isNumeric = true;
                  for(unsigned int i=0; i<name.length(); i++) if(!isDigit(name[i])) isNumeric = false;
                  
                  if (isNumeric && name.length() > 0) {
                      if (downloadAndConvert(name, "/icons/", false)) { 
                          newIcon = loadBmpFile("/icons/" + name + ".bmp");
                      } else failedIcons.push_back(name);
                  } else failedIcons.push_back(name);
             }
        }
        
        if (newIcon) {
            newIcon->name = name; 
            newIcon->lastUsed = millis();
            while (iconCache.size() >= MAX_CACHE_SIZE_STATIC && !iconCache.empty()) {
                CachedIcon* old = iconCache.back(); iconCache.pop_back(); 
                if(old->pixels) heap_caps_free(old->pixels); 
                if(old->alpha) heap_caps_free(old->alpha); 
                delete old;
            }
            iconCache.push_front(newIcon);
        }
        return newIcon;
    }

    AnimatedIcon* getAnimatedIcon(String id) {
        if (id.startsWith("la:")) id = id.substring(3);
        else if (id.startsWith("ln:")) id = id.substring(3);

        for (auto it = animCache.begin(); it != animCache.end(); ++it) {
            if ((*it)->name == id) { (*it)->lastUsed = millis(); return *it; }
        }
        for(const String& bad : failedIcons) if (bad == id) return nullptr;

        AnimatedIcon* anim = nullptr;
        String path = "/iconsan/" + id + ".bmp";

        if (!LittleFS.exists(path)) {
            bool isNumeric = true;
            for(unsigned int i=0; i<id.length(); i++) if(!isDigit(id[i])) isNumeric = false;
            if (isNumeric && id.length() > 0) {
                if (!downloadAndConvert(id, "/iconsan/", true)) { failedIcons.push_back(id); return nullptr; }
            } else { failedIcons.push_back(id); return nullptr; }
        }
        
        anim = loadAnimFromFS(path, id);
        
        if (anim) {
            while (animCache.size() >= MAX_CACHE_SIZE_ANIM && !animCache.empty()) {
                AnimatedIcon* old = animCache.back(); animCache.pop_back();
                if(old->pixels) heap_caps_free(old->pixels); 
                if(old->alpha) heap_caps_free(old->alpha); 
                if(old->delays) heap_caps_free(old->delays); // NEU: PSRAM Freigabe
                delete old;
            }
            animCache.push_front(anim);
        } else failedIcons.push_back(id);
        
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
        if (!anim) { display.drawPixel(x, y, display.color565(255, 0, 0)); return; }

        int currentFrameIdx = 0;
        
        // --- NEU: Dynamische Frame-Auswahl anhand summierter Delays ---
        if (anim->totalTime > 0) {
            unsigned long timeInCycle = millis() % anim->totalTime;
            unsigned long accumulatedTime = 0;
            for (int i = 0; i < anim->frameCount; i++) {
                accumulatedTime += anim->delays[i];
                if (timeInCycle < accumulatedTime) {
                    currentFrameIdx = i;
                    break;
                }
            }
        }

        int pixelsPerFrame = anim->width * anim->height;
        int startPixelIdx = currentFrameIdx * pixelsPerFrame;
        bool doUpscale = (anim->width == 8 && anim->height == 8);
        
        for (int iy = 0; iy < (doUpscale?16:anim->height); iy++) {
            for (int ix = 0; ix < (doUpscale?16:anim->width); ix++) {
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
        if (anim) return (anim->width == 8) ? 16 : anim->width; 
        return 16; 
    }
    
    int getIconWidth(String name) {
        CachedIcon* i = getIcon(name);
        return i ? (i->width == 8 ? 16 : i->width) : 16; 
    }
    int getIconHeight(String name) {
        CachedIcon* i = getIcon(name);
        return i ? (i->height == 8 ? 16 : i->height) : 16; 
    }
};