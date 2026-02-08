#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"

// Zugriff auf Overlay und Display
extern void forceOverlay(String msg, int durationSec, String colorName);
extern DisplayManager display; 

class WebManager {
private:
    WebServer server;
    File uploadFile; // Datei-Handle bleibt offen

public:
    WebManager() : server(80) {}

    void begin() {
        // 1. Root: Zeigt Dateiliste & Freien Speicher
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

        // 2. Upload Handler (OPTIMIERT & KORRIGIERT)
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
            forceOverlay("Upload Done", 3, "success"); 
            Serial.println("Web: Upload finished.");
            
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            
            // --- START: Datei öffnen ---
            if (upload.status == UPLOAD_FILE_START) {
                size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
                if (freeSpace < 10000) { 
                     Serial.println("Storage Full! Aborting.");
                     forceOverlay("Disk Full!", 5, "warn");
                     return; 
                }

                String filename = "/" + upload.filename;
                if(!filename.startsWith("/")) filename = "/" + filename;
                
                Serial.print("Web: Upload Start: "); Serial.println(filename);
                forceOverlay("Uploading...", 60, "warn"); 
                
                uploadFile = LittleFS.open(filename, "w");
                if(!uploadFile) Serial.println("Failed to open file for writing");

            } 
            // --- WRITE: Schreiben ---
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) {
                    uploadFile.write(upload.buf, upload.currentSize);
                }
            } 
            // --- END: Schließen ---
            else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) {
                    uploadFile.close();
                    Serial.print("Upload Size: "); Serial.println(upload.totalSize);
                }
            }
            // --- ABORT: Fehlerfall (HIER WAR DER FEHLER) ---
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