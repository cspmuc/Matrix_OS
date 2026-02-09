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
    
    size_t uploadBytesWritten = 0;
    unsigned long lastDrawTime = 0;
    bool uploadError = false;

    String sanitizeFilename(String filename) {
        int lastSlash = filename.lastIndexOf('/');
        if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
        
        String cleanName = "";
        cleanName.reserve(filename.length() + 1); 
        
        for (char c : filename) {
            if (isalnum(c) || c == '.' || c == '_' || c == '-') cleanName += c;
            else cleanName += '_'; 
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

    void drawUploadStats(String filename, size_t current, bool isError = false) {
        // Performance: Nur alle 100ms zeichnen (außer bei Fehler)
        if (!isError && (millis() - lastDrawTime < 100)) return;
        lastDrawTime = millis();

        display.clear();
        
        if (isError) {
            display.setTextColor(display.color565(255, 0, 0)); // Rot
            display.printCentered("ERROR", 15);
            display.setTextColor(display.color565(255, 255, 255));
            display.printCentered("Write Failed!", 35);
        } else {
            display.setTextColor(display.color565(0, 200, 255)); // Cyan
            display.printCentered("UPLOADING", 15);
            
            String shortName = filename;
            if (shortName.length() > 16) shortName = "..." + shortName.substring(shortName.length()-13);
            display.setTextColor(display.color565(150, 150, 150)); 
            display.printCentered(shortName, 35);

            display.setTextColor(display.color565(255, 255, 255)); 
            String sizeStr;
            if (current < 1024) sizeStr = String(current) + " B";
            else sizeStr = String(current / 1024) + " KB";
            display.printCentered(sizeStr, 55);
        }
        display.show();
    }

public:
    WebManager() : server(80) {}

    void begin() {
        // 1. Root Page
        server.on("/", HTTP_GET, [this]() {
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/html", ""); 
            
            String chunk = "<html><head><title>Matrix OS</title></head><body>";
            chunk += "<h1>Matrix OS Storage</h1>";
            
            size_t total = LittleFS.totalBytes();
            size_t used = LittleFS.usedBytes();
            chunk += "<p>Used: " + String(used) + " / " + String(total) + " Bytes</p>";
            
            chunk += "<form method='POST' action='/format' onsubmit='return confirm(\"Alles loeschen?\")'>";
            chunk += "<input type='submit' value='Formatieren (Alles loeschen)' style='color:red'></form>";
            
            chunk += "<hr><form method='POST' action='/upload' enctype='multipart/form-data'>";
            chunk += "<input type='file' name='upload'><input type='submit' value='Upload'>";
            chunk += "</form><hr>";

            chunk += "<table border='1'><tr><th>Name</th><th>Size</th><th>Action</th></tr>";
            server.sendContent(chunk);

            File root = LittleFS.open("/");
            File file = root.openNextFile();
            while(file){
                if(!file.isDirectory()) {
                    String line = "<tr><td><a href='" + String(file.name()) + "'>" + String(file.name()) + "</a></td>";
                    line += "<td>" + String(file.size()) + " B</td>";
                    line += "<td><a href='/delete?name=" + String(file.name()) + "'>Delete</a></td></tr>";
                    server.sendContent(line); 
                }
                file = root.openNextFile();
            }
            server.sendContent("</table></body></html>");
            server.sendContent(""); 
        });

        // 2. Format Handler
        server.on("/format", HTTP_POST, [this]() {
            display.clear();
            display.setTextColor(display.color565(255, 0, 0));
            display.printCentered("FORMATTING...", 32);
            display.show();
            delay(100); 
            LittleFS.format();
            server.send(200, "text/html", "Formatiert! <a href='/'>Zurueck</a>");
            forceOverlay("Format OK", 3, "success");
        });

        // 3. Upload Handler
        server.on("/upload", HTTP_POST, [this]() {
            if (uploadError) {
                server.send(507, "text/plain", "Error: Write Failed (Disk Full?)");
            } else {
                server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
                forceOverlay("Upload OK", 3, "success"); 
            }
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            esp_task_wdt_reset();

            if (upload.status == UPLOAD_FILE_START) {
                uploadError = false; 
                String filename = sanitizeFilename(upload.filename);
                Serial.print("Web: Upload Start: "); Serial.println(filename);
                
                uploadFile = LittleFS.open(filename, "w");
                if (!uploadFile) {
                    uploadError = true;
                    return;
                }
                
                uploadBytesWritten = 0; 
                lastDrawTime = 0; 
                display.setBrightness(150);
                drawUploadStats(filename, 0);
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadError) return; // Wenn Fehler, verwerfen wir den Rest

                if (uploadFile) {
                    size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
                    
                    // WRITE CHECK: Hat das Schreiben geklappt?
                    if (bytesWritten < upload.currentSize) {
                        Serial.println("Web: Write failed - Disk likely Full");
                        uploadError = true;
                        uploadFile.close();
                        LittleFS.remove("/" + sanitizeFilename(upload.filename)); // Kaputte Datei löschen
                        drawUploadStats("ERROR", 0, true);
                        return;
                    }

                    uploadBytesWritten += bytesWritten;
                    drawUploadStats(upload.filename, uploadBytesWritten);
                    delay(1); 
                }
            } 
            else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) {
                    uploadFile.close();
                    if (!uploadError) {
                        Serial.print("Web: Upload Size: "); Serial.println(uploadBytesWritten);
                        lastDrawTime = 0; 
                        drawUploadStats(upload.filename, uploadBytesWritten);
                    }
                }
            }
            else if (upload.status == UPLOAD_FILE_ABORTED) { 
                if (uploadFile) {
                    uploadFile.close();
                    LittleFS.remove("/" + upload.filename); 
                }
                uploadError = true;
                drawUploadStats("ABORTED", 0, true);
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