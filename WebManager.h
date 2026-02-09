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
    
    // NEU: Variablen für stabilen Upload-Balken
    size_t uploadBytesWritten = 0;
    int lastUploadPercent = -1;

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

    // Optimierte Zeichenfunktion (Anti-Flicker)
    void drawUploadProgress(String filename, size_t current, size_t total) {
        int percent = 0;
        if (total > 0) {
            percent = (int)((current * 100) / total);
            if (percent > 100) percent = 100;
        }

        // TRICK: Nur zeichnen, wenn sich Prozent ändert! 
        // Das verhindert das Flickern komplett.
        if (percent == lastUploadPercent && current > 0) return; 
        lastUploadPercent = percent;

        display.clear();
        display.setTextColor(display.color565(0, 200, 255)); // Cyan
        display.printCentered("FILE UPLOAD", 15);
        
        // Dateiname gekürzt anzeigen
        String shortName = filename;
        if (shortName.length() > 16) shortName = "..." + shortName.substring(shortName.length()-13);
        
        display.setTextColor(display.color565(150, 150, 150));
        display.printCentered(shortName, 55);

        // Balken zeichnen
        if (total > 0) {
            int w = map(percent, 0, 100, 0, 100);
            display.drawRect(14, 30, 102, 12, display.color565(80, 80, 80)); // Rahmen
            display.fillRect(15, 31, w, 10, display.color565(0, 200, 255)); // Füllung
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
            forceOverlay("Format Done", 3, "success");
        });

        // 3. Upload Handler (Stabilisiert)
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
            
            // Overlay wird erst angezeigt, wenn der Loop wieder läuft
            forceOverlay("Upload Complete", 4, "success"); 
            Serial.println("Web: Upload finished.");
            
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            esp_task_wdt_reset();

            if (upload.status == UPLOAD_FILE_START) {
                String filename = sanitizeFilename(upload.filename);
                Serial.print("Web: Upload Start: "); Serial.println(filename);
                
                uploadFile = LittleFS.open(filename, "w");
                
                // Reset Zähler
                uploadBytesWritten = 0; 
                lastUploadPercent = -1;
                
                display.setBrightness(150);
                drawUploadProgress(filename, 0, 100);
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) {
                    uploadFile.write(upload.buf, upload.currentSize);
                    
                    // Manuell zählen statt upload.index nutzen (das fehlte in der Lib)
                    uploadBytesWritten += upload.currentSize; 
                    
                    drawUploadProgress(upload.filename, uploadBytesWritten, upload.totalSize);
                    
                    delay(1); 
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
                    LittleFS.remove("/" + upload.filename); 
                }
                display.clear();
                display.setTextColor(display.color565(255, 0, 0));
                display.printCentered("UPLOAD FAILED", 32);
                display.show();
                delay(2000);
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