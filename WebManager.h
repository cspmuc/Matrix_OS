#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_task_wdt.h> 
#include "config.h"

extern void forceOverlay(String msg, int durationSec, String colorName);
extern DisplayManager display; 

// Zugriff auf die globale Pause-Variable (in Matrix_OS.ino definiert)
extern volatile bool isSystemUploading; 

// 512 Byte: Kleinere Häppchen = Kürzere Blockade = Stabiles WLAN
#define UPLOAD_BUFFER_SIZE 512 

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
            
            // Watchdog streicheln, da Schreiben dauern kann
            esp_task_wdt_reset();
            
            // WICHTIG: Gibt dem WLAN-Stack Zeit für ACKs
            delay(1); 
        }
    }

    String sanitizeFilename(String filename) {
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
        
        String cleanName = "";
        for (char c : filename) {
            if (isalnum(c) || c == '.' || c == '_' || c == '-') {
                cleanName += c;
            } else {
                cleanName += '_'; 
            }
        }
        
        if (cleanName.length() > 30) {
            String ext = "";
            int dotIndex = cleanName.lastIndexOf('.');
            if (dotIndex > 0) ext = cleanName.substring(dotIndex);
            cleanName = cleanName.substring(0, 30 - ext.length()) + ext;
        }
        
        if (!cleanName.startsWith("/")) cleanName = "/" + cleanName;
        return cleanName;
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
            
            // Format-Button hinzufügen (Vorsicht!)
            html += "<form method='POST' action='/format' onsubmit='return confirm(\"Alles loeschen?\")'>";
            html += "<input type='submit' value='Formatieren (Alles loeschen)' style='color:red'></form>";
            
            html += "<hr><form method='POST' action='/upload' enctype='multipart/form-data'>";
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

        // 2. Format Handler (NEU)
        server.on("/format", HTTP_POST, [this]() {
            // Display Pause
            isSystemUploading = true;
            delay(100);
            
            forceOverlay("Formatting...", 10, "warn");
            LittleFS.format();
            
            isSystemUploading = false;
            server.send(200, "text/html", "Formatiert! <a href='/'>Zurueck</a>");
        });

        // 3. Upload Handler
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
            isSystemUploading = false; 
            forceOverlay("Upload Done", 3, "success"); 
            Serial.println("Web: Upload finished.");
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            esp_task_wdt_reset();

            if (upload.status == UPLOAD_FILE_START) {
                isSystemUploading = true;
                delay(100); 
                
                bufferPos = 0;
                String filename = sanitizeFilename(upload.filename);
                Serial.print("Web: Upload Start: "); Serial.println(filename);
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
                        if (bufferPos >= UPLOAD_BUFFER_SIZE) flushBuffer();
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
                isSystemUploading = false; 
                if (uploadFile) {
                    uploadFile.close();
                    Serial.println("Web: Upload Aborted");
                    LittleFS.remove("/" + upload.filename); 
                }
            }
        });

        // 4. Delete Handler
        server.on("/delete", HTTP_GET, [this]() {
            if (server.hasArg("name")) {
                String filename = server.arg("name");
                if(!filename.startsWith("/")) filename = "/" + filename;
                if (LittleFS.exists(filename)) {
                    isSystemUploading = true;
                    delay(100); 
                    
                    LittleFS.remove(filename);
                    
                    delay(10); 
                    isSystemUploading = false; 
                    
                    forceOverlay("Deleted", 2, "info");
                    Serial.println("Web: Deleted " + filename);
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