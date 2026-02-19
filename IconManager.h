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
    int width;        
    int height;       
    int totalHeight;  
    int frameCount;   
    int delayMs;      
    unsigned long lastUsed;
};

// --- Context Strukturen ---
struct GifConvertContext {
    uint8_t* canvasBuffer; 
    int width, height, dispose, x, y, w, h, frameIndex; 
};

struct PngExtractContext {
    uint16_t* pixels; // Zielbuffer 565
    uint8_t* alpha;   // Zielbuffer Alpha
    int targetX, targetY, targetW, targetH; 
    int sheetW;
    // Transparenz Infos
    bool hasTransColor;
    uint32_t transColor; // Für RGB Support
};

struct PngDownloadContext {
    File* fOut;
    int w;
    uint8_t* lineBuffer; 
};

class IconManager {
private:
    std::map<String, IconDef> iconCatalog;
    std::map<String, SheetDef> sheetCatalog;
    std::map<String, String> aliasMap; 
    
    std::list<CachedIcon*> iconCache;      
    std::list<AnimatedIcon*> animCache;    
    std::vector<String> failedIcons;       

    // OPTIMIERUNG 1: Cache erhöht (PSRAM Nutzung)
    const size_t MAX_CACHE_SIZE_STATIC = 0;
    const size_t MAX_CACHE_SIZE_ANIM = 10; 
    
    PNG png; 
    AnimatedGIF gif; 
    
    // OPTIMIERUNG 3: Inline Static Definition (Sauberer C++ Stil)
    inline static File staticGifFile; 

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
                int idx = x * 4;
                anim->pixels[y * w + x] = color565(lineBuffer[idx+2], lineBuffer[idx+1], lineBuffer[idx]);
                anim->alpha[y * w + x] = lineBuffer[idx+3];
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
                int idx = x * 4;
                newIcon->pixels[y * width + x] = color565(lineBuffer[idx+2], lineBuffer[idx+1], lineBuffer[idx]);
                newIcon->alpha[y * width + x] = lineBuffer[idx+3];
            }
        }
        free(lineBuffer); f.close(); return newIcon;
    }
    
    // --- PNG Sheet Loader ---
    static int pngSheetDrawCallback(PNGDRAW *pDraw) {
        PngExtractContext* ctx = (PngExtractContext*)pDraw->pUser;
        int y = pDraw->y; 

        if (y < ctx->targetY || y >= ctx->targetY + ctx->targetH) return 1;

        uint8_t* src = (uint8_t*)pDraw->pPixels;
        uint8_t* pPalette = (uint8_t*)pDraw->pPalette;
        int pixelType = pDraw->iPixelType; 
        
        int rowInTile = y - ctx->targetY;
        
        for (int x = 0; x < pDraw->iWidth; x++) {
            if (x < ctx->targetX || x >= ctx->targetX + ctx->targetW) continue;
            
            int colInTile = x - ctx->targetX;
            int targetIndex = rowInTile * ctx->targetW + colInTile;
            
            uint8_t r=0, g=0, b=0, a=255;

            // Type 3: Palette (Indexiert)
            if (pixelType == 3 && pPalette) { 
                uint8_t idx = src[x];
                if (ctx->hasTransColor && idx == (uint8_t)ctx->transColor) { 
                    a = 0; 
                } else {
                    r = pPalette[idx*3]; g = pPalette[idx*3+1]; b = pPalette[idx*3+2];
                }
            } 
            // Type 2: RGB (Truecolor)
            else if (pixelType == 2) { 
                int idx = x * 3;
                r = src[idx]; g = src[idx+1]; b = src[idx+2];
                if (ctx->hasTransColor) {
                    uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
                    if(rgb == ctx->transColor) a = 0;
                }
            }
            // Type 6: RGBA (Truecolor Alpha)
            else if (pixelType == 6) { 
                int idx = x * 4;
                r = src[idx]; g = src[idx+1]; b = src[idx+2]; a = src[idx+3];
            }
            
            ctx->pixels[targetIndex] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            ctx->alpha[targetIndex] = a;
        }
        return 1;
    }

    CachedIcon* loadPngIconFromSheet(const SheetDef& sheet, int index) {
        File f = LittleFS.open(sheet.filePath, "r");
        if (!f) return nullptr;

        if (png.open(sheet.filePath.c_str(), myOpen, myClose, myRead, mySeek, pngSheetDrawCallback) != PNG_SUCCESS) {
            f.close(); return nullptr;
        }

        int imgW = png.getWidth();
        
        int cols = (sheet.cols > 0) ? sheet.cols : 1;
        int tileW = (sheet.tileW > 0) ? sheet.tileW : (imgW / cols);
        int tileH = (sheet.tileH > 0) ? sheet.tileH : tileW; 

        int col = index % cols;
        int row = index / cols;
        int startX = col * tileW;
        int startY = row * tileH;

        CachedIcon* newIcon = new CachedIcon();
        newIcon->width = tileW;
        newIcon->height = tileH;
        newIcon->pixels = (uint16_t*)heap_caps_malloc(tileW * tileH * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        newIcon->alpha = (uint8_t*)heap_caps_malloc(tileW * tileH * sizeof(uint8_t), MALLOC_CAP_SPIRAM);

        if (!newIcon->pixels || !newIcon->alpha) {
            delete newIcon; png.close(); f.close(); return nullptr;
        }

        PngExtractContext ctx;
        ctx.pixels = newIcon->pixels;
        ctx.alpha = newIcon->alpha;
        ctx.targetX = startX;
        ctx.targetY = startY;
        ctx.targetW = tileW;
        ctx.targetH = tileH;
        ctx.sheetW = imgW;
        
        ctx.hasTransColor = false;
        int transColor = png.getTransparentColor(); 
        if(transColor != -1) { 
            ctx.hasTransColor = true; 
            ctx.transColor = (uint32_t)transColor; 
        }

        png.decode((void*)&ctx, 0);
        png.close();
        f.close();
        return newIcon;
    }

    CachedIcon* loadIconFromSheet(String sheetName, int index) {
        if (sheetCatalog.find(sheetName) == sheetCatalog.end()) return nullptr;
        SheetDef& sheet = sheetCatalog[sheetName];
        
        // OPTIMIERUNG 2: Case-Insensitive Prüfung
        String lowerPath = sheet.filePath;
        lowerPath.toLowerCase();
        if (lowerPath.endsWith(".png")) {
            return loadPngIconFromSheet(sheet, index);
        }

        // BMP Logic
        if (!LittleFS.exists(sheet.filePath)) return nullptr;
        File f = LittleFS.open(sheet.filePath, "r");
        uint8_t header[54];
        f.read(header, 54);
        uint32_t dataOffset = read32(header, 10);
        int32_t width = read32(header, 18);
        int32_t height = read32(header, 22);
        bool flipY = (height > 0);
        if (height < 0) height = -height;
        
        int tileW = (sheet.tileW > 0) ? sheet.tileW : (width / sheet.cols);
        int tileH = (sheet.tileH > 0) ? sheet.tileH : tileW;
        
        CachedIcon* newIcon = new CachedIcon();
        newIcon->width = tileW;
        newIcon->height = tileH;
        size_t numPixels = tileW * tileH;
        newIcon->pixels = (uint16_t*)heap_caps_malloc(numPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        newIcon->alpha = (uint8_t*)heap_caps_malloc(numPixels * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
        
        int col = index % sheet.cols;
        int row = index / sheet.cols;
        int startX = col * tileW;
        int startY = row * tileH; 
        
        size_t lineSize = tileW * 4; 
        uint8_t* lineBuffer = (uint8_t*)malloc(lineSize);
        
        for (int y = 0; y < tileH; y++) {
            int srcY_Visual = startY + y;
            int bmpRow = flipY ? (height - 1 - srcY_Visual) : srcY_Visual;
            size_t filePos = dataOffset + ((size_t)bmpRow * width * 4) + ((size_t)startX * 4);
            f.seek(filePos);
            f.read(lineBuffer, lineSize); 
            for (int x = 0; x < tileW; x++) {
                newIcon->pixels[y * tileW + x] = color565(lineBuffer[x*4+2], lineBuffer[x*4+1], lineBuffer[x*4]);
                newIcon->alpha[y * tileW + x] = lineBuffer[x*4+3];
            }
        }
        free(lineBuffer); f.close(); return newIcon;
    }

    // --- Standard Callbacks (File I/O) ---
    static void* myOpen(const char *filename, int32_t *size) {
        File f = LittleFS.open(filename, "r");
        if (f && size) *size = f.size();
        return new File(f);
    }
    static void myClose(void *handle) { File* f = (File*)handle; if(f) f->close(); delete f; }
    static int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
        File* f = (File*)handle->fHandle; return f->read(buffer, length);
    }
    static int32_t mySeek(PNGFILE *handle, int32_t position) {
        File* f = (File*)handle->fHandle; return f->seek(position);
    }
    
    // Callback für Download (PNG -> BMP Konvertierung)
    static int pngDownloadDraw(PNGDRAW *pDraw) {
        PngDownloadContext* ctx = (PngDownloadContext*)pDraw->pUser;
        uint8_t* src = (uint8_t*)pDraw->pPixels;
        uint8_t* pPalette = (uint8_t*)pDraw->pPalette;
        int pixelType = pDraw->iPixelType; 
        
        for (int x = 0; x < pDraw->iWidth; x++) {
            uint8_t r=0, g=0, b=0, a=255;

            if (pixelType == 3 && pPalette) { // Palette
                uint8_t idx = src[x];
                r = pPalette[idx*3]; g = pPalette[idx*3+1]; b = pPalette[idx*3+2];
            } 
            else if (pixelType == 2) { // RGB
                int idx = x * 3;
                r = src[idx]; g = src[idx+1]; b = src[idx+2];
            }
            else if (pixelType == 6) { // RGBA
                int idx = x * 4;
                r = src[idx]; g = src[idx+1]; b = src[idx+2]; a = src[idx+3];
            }
            
            // Speichern als BGR + A (für BMP)
            int targetIdx = x * 4;
            ctx->lineBuffer[targetIdx]   = b;
            ctx->lineBuffer[targetIdx+1] = g;
            ctx->lineBuffer[targetIdx+2] = r;
            ctx->lineBuffer[targetIdx+3] = a;
        }
        ctx->fOut->write(ctx->lineBuffer, ctx->w * 4);
        return 1;
    }

    // --- GIF Zeug ---
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
            
            if (pDraw->ucHasTransparency && s[x] == pDraw->ucTransparent) continue; 

            uint16_t palIdx = s[x] * 3;
            uint8_t r = pPalette[palIdx];
            uint8_t g = pPalette[palIdx+1];
            uint8_t b = pPalette[palIdx+2];
            
            int idx = lineOffset + (x_abs * 4);
            d[idx] = b; d[idx+1] = g; d[idx+2] = r; d[idx+3] = 255; 
        }
    }

    // --- Download Logic ---
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
            if (millis() - start > 3000) break;
            if (len > 0 && total >= len) break; 
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
            WiFiClientSecure client; client.setInsecure();
            HTTPClient http; http.begin(client, url);
            http.setUserAgent("Mozilla/5.0 (ESP32)"); http.setTimeout(5000); 
            
            if (http.GET() == HTTP_CODE_OK) {
                File f = LittleFS.open("/temp_dl.dat", "w");
                if (f) {
                    if (downloadFile(http, f)) {
                        f.close();
                        File fRead = LittleFS.open("/temp_dl.dat", "r");
                        size_t fSize = fRead.size();
                        uint8_t* gifRamBuffer = (uint8_t*)malloc(fSize);
                        
                        if (gifRamBuffer && fSize > 50) {
                            fRead.read(gifRamBuffer, fSize); fRead.close();
                            esp_task_wdt_reset();
                            
                            if (gif.open(gifRamBuffer, fSize, GIFDrawCallback)) {
                                GIFINFO info;
                                if (gif.getInfo(&info) && info.iFrameCount > 0) {
                                    int w = gif.getCanvasWidth();
                                    int h = gif.getCanvasHeight();
                                    int frames = info.iFrameCount;
                                    int totalH = h * frames;
                                    size_t stripSize = w * totalH * 4;
                                    size_t canvasSize = w * h * 4;
                                    
                                    uint8_t* stripBuffer = (uint8_t*)heap_caps_malloc(stripSize, MALLOC_CAP_SPIRAM);
                                    uint8_t* canvasBuffer = (uint8_t*)heap_caps_malloc(canvasSize, MALLOC_CAP_SPIRAM);
                                    
                                    if (stripBuffer && canvasBuffer) {
                                        memset(stripBuffer, 0, stripSize); 
                                        memset(canvasBuffer, 0, canvasSize); 
                                        GifConvertContext ctx = {canvasBuffer, w, h, 0, 0, 0, 0, 0}; 
                                        int prevDispose = 2;
                                        
                                        for (int i=0; i<frames; i++) {
                                            ctx.frameIndex = i;
                                            if (prevDispose == 2) memset(canvasBuffer, 0, canvasSize); 
                                            gif.playFrame(false, NULL, &ctx); 
                                            memcpy(stripBuffer + (i*canvasSize), canvasBuffer, canvasSize);
                                            prevDispose = ctx.dispose;
                                            esp_task_wdt_reset();
                                        }
                                        gif.close();
                                        File fOut = LittleFS.open(outName, "w");
                                        writeBmpHeader(fOut, w, totalH);
                                        for (int y = totalH - 1; y >= 0; y--) fOut.write(stripBuffer + (y * w * 4), w * 4);
                                        fOut.close();
                                        free(stripBuffer); free(canvasBuffer);
                                        success = true;
                                        Serial.println("[ICON] GIF Converted: " + outName);
                                    } else { if(stripBuffer) free(stripBuffer); if(canvasBuffer) free(canvasBuffer); gif.close(); }
                                }
                            }
                            if(gifRamBuffer) free(gifRamBuffer);
                        } else if(fRead) fRead.close();
                    } else { f.close(); }
                } 
                LittleFS.remove("/temp_dl.dat");
            } 
            client.stop(); http.end();
        }

        if (!success) {
            String url = "https://developer.lametric.com/content/apps/icon_thumbs/" + id + ".png";
            Serial.println("[ICON] Downloading PNG: " + url);
            WiFiClientSecure client; client.setInsecure();
            HTTPClient http; http.begin(client, url);
            http.setUserAgent("Mozilla/5.0 (ESP32)"); http.setTimeout(3000);
            
            if (http.GET() == HTTP_CODE_OK) {
                 File f = LittleFS.open("/temp_dl.dat", "w");
                 if (f && downloadFile(http, f)) {
                     f.close();
                     PngDownloadContext ctx;
                     File fOut = LittleFS.open(outName, "w");
                     if(fOut) {
                         ctx.fOut = &fOut;
                         if (png.open("/temp_dl.dat", myOpen, myClose, myRead, mySeek, pngDownloadDraw) == PNG_SUCCESS) {
                             ctx.w = png.getWidth();
                             int h = png.getHeight();
                             uint8_t header[54] = {0};
                             header[0] = 'B'; header[1] = 'M';
                             uint32_t dataSize = ctx.w * h * 4;
                             write32(header, 2, dataSize + 54);
                             write32(header, 10, 54);
                             write32(header, 14, 40);
                             write32(header, 18, ctx.w);
                             uint32_t negH = (uint32_t)(-h); 
                             write32(header, 22, negH); 
                             write16(header, 26, 1);
                             write16(header, 28, 32); 
                             fOut.write(header, 54);
                             
                             ctx.lineBuffer = (uint8_t*)malloc(ctx.w * 4);
                             if (ctx.lineBuffer) {
                                 png.decode((void*)&ctx, 0); 
                                 free(ctx.lineBuffer);
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
            client.stop(); http.end();
        }
        return success;
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
            SheetDef def; 
            def.filePath = kv.value()["file"].as<String>(); 
            def.cols = kv.value()["cols"] | 1; 
            def.tileW = 0; def.tileH = 0;
            sheetCatalog[kv.key().c_str()] = def;
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
                 } else failedIcons.push_back(name);
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
            } else return nullptr;
        }
        anim = loadAnimFromFS(path, id);
        
        if (anim) {
            if (animCache.size() >= MAX_CACHE_SIZE_ANIM) {
                AnimatedIcon* old = animCache.back(); animCache.pop_back();
                free(old->pixels); free(old->alpha); delete old;
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

        int frameDuration = anim->delayMs; 
        if (frameDuration < 50) frameDuration = 500; 
        int totalTime = anim->frameCount * frameDuration;
        int currentFrameIdx = (millis() % totalTime) / frameDuration;
        if (currentFrameIdx >= anim->frameCount) currentFrameIdx = 0; 

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