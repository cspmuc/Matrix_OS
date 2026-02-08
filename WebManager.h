#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_task_wdt.h> 
#include "config.h"

extern void forceOverlay(String msg, int durationSec, String colorName);
extern DisplayManager display; 

class WebManager {
private:
    WebServer server;
    File uploadFile;
    
    // KEIN Puffer-Array mehr! Wir nutzen den RAM des ESP effizienter.

    String sanitizeFilename(String filename) {
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
        
        // Performance: Speicher vorab reservieren
        String cleanName = "";
        cleanName.reserve(filename.length() + 1); 
        
        for (char c : filename) {
            if (isalnum(c) || c == '.' || c == '_' || c == '-') cleanName += c;
            else cleanName += '_'; 
        }
        // Limitierung auf 30 Zeichen für LittleFS Sicherheit
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

        // 2. Format Handler
        server.on("/format", HTTP_POST, [this]() {
            forceOverlay("Formatting...", 10, "warn");
            delay(100); // Kurze Pause vorm Hammer
            LittleFS.format();
            server.send(200, "text/html", "Formatiert! <a href='/'>Zurueck</a>");
        });

        // 3. Upload Handler (DIRECT STREAM MODE)
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
            forceOverlay("Upload Done", 3, "success"); 
            Serial.println("Web: Upload finished.");
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            esp_task_wdt_reset();

            if (upload.status == UPLOAD_FILE_START) {
                String filename = sanitizeFilename(upload.filename);
                Serial.print("Web: Upload Start: "); Serial.println(filename);
                
                // Datei öffnen
                uploadFile = LittleFS.open(filename, "w");
                
                // Overlay Info (Display läuft weiter!)
                forceOverlay("Uploading...", 60, "warn"); 
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) {
                    // DIREKTES SCHREIBEN: Wir schreiben genau das Paket, das reinkommt.
                    // Das sind meist ~1400 Bytes. Das geht schnell genug für das Display.
                    uploadFile.write(upload.buf, upload.currentSize);
                    
                    // WICHTIG: Das hier erlaubt dem Display-Task, 
                    // zwischen den Paketen kurz ein Frame zu zeichnen.
                    yield(); 
                }
            } 
            else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) {
                    uploadFile.close();
                    Serial.print("Upload Size: "); Serial.println(upload.totalSize);
                }
            }
            else if (upload.status == UPLOAD_FILE_ABORTED) { 
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
                    LittleFS.remove(filename);
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