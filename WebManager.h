#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_task_wdt.h> 
#include "config.h"

extern void forceOverlay(String msg, int durationSec, String colorName);
extern DisplayManager display; 

// WICHTIG: Puffer auf 1024 reduziert! 
// 4096 blockiert den Flash zu lange -> WLAN Paketverlust -> Abbruch
#define UPLOAD_BUFFER_SIZE 1024 

class WebManager {
private:
    WebServer server;
    File uploadFile;
    
    uint8_t buffer[UPLOAD_BUFFER_SIZE];
    size_t bufferPos = 0;

    void flushBuffer() {
        if (uploadFile && bufferPos > 0) {
            uploadFile.write(buffer, bufferPos);
            bufferPos = 0;
            // Dem Netzwerk-Stack Zeit geben, ACKs zu senden
            yield(); 
        }
    }

public:
    WebManager() : server(80) {}

    void begin() {
        // 1. Root Page
        server.on("/", HTTP_GET, [this]() {
            String html = "<html><head><title>Matrix OS</title></head><body>";
            html += "<h1>Matrix OS Storage</h1>";
            
            size_t total = LittleFS.totalBytes();
            size_t used = LittleFS.usedBytes();
            html += "<p>Used: " + String(used) + " / " + String(total) + " Bytes</p>";
            
            html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
            html += "<input type='file' name='upload'><input type='submit' value='Upload'>";
            html += "</form><hr>";

            html += "<table border='1'><tr><th>Name</th><th>Size</th><th>Action</th></tr>";
            File root = LittleFS.open("/");
            File file = root.openNextFile();
            while(file){
                if(!file.isDirectory()) {
                    html += "<tr><td><a href='" + String(file.name()) + "'>" + String(file.name()) + "</a></td>";
                    html += "<td>" + String(file.size()) + " B</td>";
                    html += "<td><a href='/delete?name=" + String(file.name()) + "'>Delete</a></td></tr>";
                }
                file = root.openNextFile();
            }
            html += "</table></body></html>";
            server.send(200, "text/html", html);
        });

        // 2. Upload Handler
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
            forceOverlay("Upload Done", 3, "success"); 
            Serial.println("Web: Upload finished.");
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            
            // Watchdog füttern
            esp_task_wdt_reset();

            if (upload.status == UPLOAD_FILE_START) {
                bufferPos = 0;
                size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
                if (freeSpace < 10000) { 
                     forceOverlay("Disk Full!", 5, "warn");
                     return; 
                }

                String filename = "/" + upload.filename;
                if(!filename.startsWith("/")) filename = "/" + filename;
                
                Serial.print("Web: Upload Start: "); Serial.println(filename);
                forceOverlay("Uploading...", 60, "warn"); 
                
                // Datei im "Write" Modus öffnen
                uploadFile = LittleFS.open(filename, "w");
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) {
                    size_t bytesToProcess = upload.currentSize;
                    size_t incomingIndex = 0;
                    
                    while (bytesToProcess > 0) {
                        size_t spaceLeft = UPLOAD_BUFFER_SIZE - bufferPos;
                        size_t chunk = (bytesToProcess < spaceLeft) ? bytesToProcess : spaceLeft;
                        
                        memcpy(buffer + bufferPos, upload.buf + incomingIndex, chunk);
                        
                        bufferPos += chunk;
                        bytesToProcess -= chunk;
                        incomingIndex += chunk;
                        
                        if (bufferPos >= UPLOAD_BUFFER_SIZE) {
                            flushBuffer();
                        }
                    }
                }
            } 
            else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) {
                    flushBuffer(); 
                    uploadFile.close();
                    Serial.print("Upload Size: "); Serial.println(upload.totalSize);
                }
            }
            else if (upload.status == UPLOAD_FILE_ABORTED) { 
                if (uploadFile) {
                    uploadFile.close();
                    Serial.println("Web: Upload Aborted");
                    forceOverlay("Aborted", 3, "warn");
                    LittleFS.remove("/" + upload.filename); 
                }
            }
        });

        // 3. Delete Handler
        server.on("/delete", HTTP_GET, [this]() {
            if (server.hasArg("name")) {
                String filename = server.arg("name");
                if(!filename.startsWith("/")) filename = "/" + filename;
                if (LittleFS.exists(filename)) {
                    LittleFS.remove(filename);
                    forceOverlay("Deleted", 2, "info");
                }
            }
            server.sendHeader("Location", "/");
            server.send(303);
        });

        server.onNotFound([this]() {
            String path = server.uri();
            if (LittleFS.exists(path)) {
                File file = LittleFS.open(path, "r");
                server.streamFile(file, "application/octet-stream");
                file.close();
            } else {
                server.send(404, "text/plain", "File not found");
            }
        });

        server.begin();
    }

    void handle() {
        server.handleClient();
    }
};