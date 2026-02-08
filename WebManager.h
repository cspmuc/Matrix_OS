#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"

// Zugriff auf Display Status
extern void status(const String& msg, uint16_t color);
extern DisplayManager display; 

class WebManager {
private:
    WebServer server;
    bool isUploading = false;

public:
    WebManager() : server(80) {}

    void begin() {
        // 1. Root: Zeigt Dateiliste & Freien Speicher
        server.on("/", HTTP_GET, [this]() {
            String html = "<html><head><title>Matrix OS</title></head><body>";
            html += "<h1>Storage Manager</h1>";
            
            // Speicher Info
            size_t total = LittleFS.totalBytes();
            size_t used = LittleFS.usedBytes();
            html += "<p>Used: " + String(used) + " / " + String(total) + " Bytes</p>";
            
            // Upload Form
            html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
            html += "<input type='file' name='upload'><input type='submit' value='Upload'>";
            html += "</form><hr>";

            // File List
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

        // 2. Upload Handler MIT CHECKS
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
            // Upload fertig -> Status reset
            status("Upload Done", display.color565(0, 255, 0));
            delay(1000); // Kurz zeigen
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            
            if (upload.status == UPLOAD_FILE_START) {
                // START: Checken ob Platz ist (grobe Schätzung)
                size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
                
                // Wir reservieren 10KB Sicherheitspuffer
                if (freeSpace < 10000) {
                     Serial.println("Storage Full! Aborting.");
                     return; // Einfach abbrechen, Datei wird nicht geöffnet
                }

                String filename = "/" + upload.filename;
                if(!filename.startsWith("/")) filename = "/" + filename;
                
                Serial.print("Upload Start: "); Serial.println(filename);
                status("Uploading...", display.color565(255, 0, 0)); // Roter Warntext auf Matrix
                
                File f = LittleFS.open(filename, "w");
                if(!f) Serial.println("Failed to open file for writing");

            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (upload.currentSize > 0) {
                    String filename = "/" + upload.filename;
                    if(!filename.startsWith("/")) filename = "/" + filename;
                    File f = LittleFS.open(filename, "a");
                    if(f) f.write(upload.buf, upload.currentSize);
                    f.close();
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                Serial.print("Upload Size: "); Serial.println(upload.totalSize);
            }
        });

        // 3. Delete Handler
        server.on("/delete", HTTP_GET, [this]() {
            if (server.hasArg("name")) {
                String filename = server.arg("name");
                if(!filename.startsWith("/")) filename = "/" + filename;
                if (LittleFS.exists(filename)) LittleFS.remove(filename);
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