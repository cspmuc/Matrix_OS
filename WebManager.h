#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"

// WICHTIG: Zugriff auf die neue Overlay-Funktion
extern void forceOverlay(String msg, int durationSec, String colorName);
extern DisplayManager display; 

class WebManager {
private:
    WebServer server;

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

        // 2. Upload Handler mit Overlay-Feedback
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
            
            // ERFOLG: Zeige "Done" in Grün für 3 Sekunden
            forceOverlay("Upload Done", 3, "success"); // "success" ist Grün (siehe RichText)
            Serial.println("Web: Upload finished.");
            
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            
            if (upload.status == UPLOAD_FILE_START) {
                // START: Speicher Check
                size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
                if (freeSpace < 10000) {
                     Serial.println("Storage Full! Aborting.");
                     forceOverlay("Disk Full!", 5, "warn");
                     return; 
                }

                String filename = "/" + upload.filename;
                if(!filename.startsWith("/")) filename = "/" + filename;
                
                Serial.print("Web: Upload Start: "); Serial.println(filename);
                
                // VISUELLE RÜCKMELDUNG:
                // Wir setzen eine lange Dauer (60s), damit der Text während des 
                // gesamten Uploads stehen bleibt. Am Ende überschreiben wir ihn mit "Done".
                forceOverlay("Uploading...", 60, "warn"); // "warn" ist Rot
                
                File f = LittleFS.open(filename, "w");
                if(!f) Serial.println("Failed to open file");

            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (upload.currentSize > 0) {
                    String filename = "/" + upload.filename;
                    if(!filename.startsWith("/")) filename = "/" + filename;
                    File f = LittleFS.open(filename, "a");
                    if(f) f.write(upload.buf, upload.currentSize);
                    f.close();
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