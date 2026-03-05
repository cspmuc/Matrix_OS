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
        return cleanName;
    }

    void drawUploadStats(String filename, size_t current, bool isError = false) {
        if (!isError && (millis() - lastDrawTime < 100)) return;
        lastDrawTime = millis();
        display.clear();
        if (isError) {
            display.setTextColor(display.color565(255, 0, 0)); 
            display.printCentered("ERROR", 15);
            display.setTextColor(display.color565(255, 255, 255));
            display.printCentered("Write Failed!", 35);
        } else {
            display.setTextColor(display.color565(0, 200, 255)); 
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
        // --- 1. HAUPTSEITE (Datei-Browser) ---
        server.on("/", HTTP_GET, [this]() {
            String path = "/";
            if (server.hasArg("dir")) path = server.arg("dir");
            if (!path.startsWith("/")) path = "/" + path;
            if (!path.endsWith("/") && path.length() > 1) path += "/";

            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/html", ""); 
            
            // UTF-8 Meta Tag eingefügt
            String chunk = "<html><head><meta charset='utf-8'><title>Matrix OS</title></head><body style='font-family: Arial, sans-serif;'>";
            chunk += "<h1>Storage: " + path + "</h1>";
            
            size_t total = LittleFS.totalBytes();
            size_t used = LittleFS.usedBytes();
            chunk += "<p>Used: " + String(used) + " / " + String(total) + " Bytes</p>";
            
            // Format & Reboot Buttons (mit korrigierten Umlauten)
            chunk += "<div style='display: flex; gap: 10px; margin-bottom: 20px;'>";
            chunk += "<form method='POST' action='/format' onsubmit='return confirm(\"Alles löschen?\")'>";
            chunk += "<input type='submit' value='Formatieren (Alles löschen)' style='color:red; padding: 5px 10px;'></form>";
            
            chunk += "<form method='POST' action='/reboot' onsubmit='return confirm(\"System jetzt neu starten?\")'>";
            chunk += "<input type='submit' value='Reboot ESP32' style='color:darkorange; padding: 5px 10px; font-weight: bold;'></form>";
            chunk += "</div>";
            
            chunk += "<hr><form method='POST' action='/upload?dir=" + path + "' enctype='multipart/form-data'>";
            chunk += "<input type='file' name='upload'><input type='submit' value='Upload' style='padding: 5px 10px;'>";
            chunk += "</form><hr>";

            if (path != "/") {
                String parent = path.substring(0, path.length() - 1);
                int lastSlash = parent.lastIndexOf('/');
                if (lastSlash >= 0) parent = parent.substring(0, lastSlash + 1);
                else parent = "/";
                chunk += "<p><a href='/?dir=" + parent + "'>.. (Zurück)</a></p>";
            }

            chunk += "<table border='1' cellpadding='5' style='border-collapse: collapse; text-align: left;'><tr><th>Name</th><th>Size</th><th>Action</th></tr>";
            server.sendContent(chunk);

            File root = LittleFS.open(path);
            if (root && root.isDirectory()) {
                File file = root.openNextFile();
                while(file){
                    String fileName = String(file.name());
                    int lastSlash = fileName.lastIndexOf('/');
                    if(lastSlash >= 0) fileName = fileName.substring(lastSlash + 1);

                    String fullPath = path + fileName;
                    if(path == "/") fullPath = "/" + fileName;

                    if(file.isDirectory()) {
                        String line = "<tr><td><b><a href='/?dir=" + fullPath + "'>[" + fileName + "]</a></b></td>";
                        line += "<td>DIR</td>";
                        line += "<td>-</td></tr>";
                        server.sendContent(line);
                    } else {
                        String line = "<tr><td><a href='" + fullPath + "'>" + fileName + "</a></td>";
                        line += "<td>" + String(file.size()) + " B</td>";
                        line += "<td><a href='/editor?file=" + fullPath + "'>Edit</a> | <a href='/delete?name=" + fullPath + "' style='color:red;'>Delete</a></td></tr>";
                        server.sendContent(line); 
                    }
                    file = root.openNextFile();
                }
            }
            server.sendContent("</table></body></html>");
            server.sendContent(""); 
        });

        // --- 2. EDITOR (Anzeigen) ---
        server.on("/editor", HTTP_GET, [this]() {
            if (!server.hasArg("file")) {
                server.send(400, "text/plain", "Fehler: Keine Datei angegeben.");
                return;
            }
            String filename = server.arg("file");
            if (!LittleFS.exists(filename)) {
                server.send(404, "text/plain", "Fehler: Datei nicht gefunden.");
                return;
            }

            File f = LittleFS.open(filename, "r");
            String content = f.readString();
            f.close();

            // UTF-8 Meta Tag eingefügt
            String html = "<html><head><meta charset='utf-8'><title>Editor - Matrix OS</title></head><body style='font-family: Arial, sans-serif;'>";
            html += "<h2>Editing: " + filename + "</h2>";
            
            if (server.hasArg("saved")) {
                html += "<p style='color: green; font-weight: bold;'>Datei erfolgreich gespeichert!</p>";
            }

            html += "<form method='POST' action='/edit?file=" + filename + "'>";
            html += "<textarea name='content' rows='25' style='width: 100%; max-width: 800px; font-family: monospace; white-space: pre;'>" + content + "</textarea><br><br>";
            
            html += "<div style='display: flex; gap: 15px;'>";
            html += "<input type='submit' value='&#128190; Speichern' style='padding: 10px 20px; font-size: 16px; cursor: pointer;'>";
            html += "</form>";

            // "Änderungen" mit echtem "Ä"
            html += "<form method='POST' action='/reboot' onsubmit='return confirm(\"System jetzt neu starten um Änderungen zu übernehmen?\")'>";
            html += "<input type='submit' value='&#8635; Reboot ESP32' style='padding: 10px 20px; font-size: 16px; color: darkorange; cursor: pointer;'></form>";
            html += "</div>";

            html += "<br><br><a href='/?dir=/'>&larr; Zurück zum Datei-Browser</a>";
            html += "</body></html>";
            
            server.send(200, "text/html", html);
        });

        // --- 3. EDIT (Speichern) ---
        server.on("/edit", HTTP_POST, [this]() {
            if (!server.hasArg("file") || !server.hasArg("content")) {
                server.send(400, "text/plain", "Fehler: Fehlende Parameter.");
                return;
            }
            
            String filename = server.arg("file");
            String content = server.arg("content");
            
            File f = LittleFS.open(filename, "w"); 
            if (f) {
                f.print(content);
                f.close();
                forceOverlay("Saved!", 2, "success");
                
                server.sendHeader("Location", "/editor?file=" + filename + "&saved=1");
                server.send(303);
            } else {
                server.send(500, "text/plain", "Fehler: Datei konnte nicht geschrieben werden.");
            }
        });

        // --- 4. REBOOT ---
        server.on("/reboot", HTTP_POST, [this]() {
            // UTF-8 Meta Tag eingefügt
            String html = "<html><head><meta charset='utf-8'><meta http-equiv='refresh' content='7; url=/'></head>";
            html += "<body style='font-family: Arial, sans-serif; text-align: center; margin-top: 50px;'>";
            html += "<h2>System startet neu...</h2>";
            html += "<p>Bitte warten, du wirst in wenigen Sekunden automatisch weitergeleitet.</p>";
            html += "</body></html>";
            
            server.send(200, "text/html", html);
            
            delay(1000); 
            ESP.restart();
        });

        server.on("/format", HTTP_POST, [this]() {
            display.clear();
            display.setTextColor(display.color565(255, 0, 0));
            display.printCentered("FORMATTING...", 32);
            display.show();
            delay(100); 
            LittleFS.format();
            // UTF-8 Meta Tag und "Zurück"
            server.send(200, "text/html", "<html><head><meta charset='utf-8'></head><body>Formatiert! <a href='/'>Zurück</a></body></html>");
            forceOverlay("Format OK", 3, "success");
        });

        // --- UPLOAD ---
        server.on("/upload", HTTP_POST, [this]() {
            if (uploadError) server.send(507, "text/plain", "Error: Write Failed");
            else {
                String targetDir = "/";
                if(server.hasArg("dir")) targetDir = server.arg("dir");
                // HTML und "Zurück" aufgewertet
                server.send(200, "text/html", "<html><head><meta charset='utf-8'></head><body>Upload erfolgreich! <a href='/?dir=" + targetDir + "'>Zurück</a></body></html>");
                forceOverlay("Upload OK", 3, "success"); 
            }
        }, [this]() { 
            HTTPUpload& upload = server.upload();
            esp_task_wdt_reset();

            if (upload.status == UPLOAD_FILE_START) {
                uploadError = false; 
                
                String targetDir = "/";
                if (server.hasArg("dir")) targetDir = server.arg("dir");
                if (!targetDir.startsWith("/")) targetDir = "/" + targetDir;
                if (!targetDir.endsWith("/")) targetDir += "/";

                String filename = sanitizeFilename(upload.filename); 
                String fullPath = targetDir + filename;

                Serial.print("Web: Upload Start: "); Serial.println(fullPath);
                
                uploadFile = LittleFS.open(fullPath, "w");
                if (!uploadFile) { uploadError = true; return; }
                
                uploadBytesWritten = 0; 
                lastDrawTime = 0; 
                display.setBrightness(150);
                drawUploadStats(filename, 0);
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadError) return; 
                if (uploadFile) {
                    size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
                    
                    if (bytesWritten < upload.currentSize) {
                        Serial.println("Web: Write failed - Disk likely Full");
                        uploadError = true; 
                        uploadFile.close();
                        
                        String targetDir = "/";
                        if (server.hasArg("dir")) targetDir = server.arg("dir");
                        if (!targetDir.startsWith("/")) targetDir = "/" + targetDir;
                        if (!targetDir.endsWith("/")) targetDir += "/";
                        LittleFS.remove(targetDir + sanitizeFilename(upload.filename)); 
                        
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
                    if (!uploadError) drawUploadStats(upload.filename, uploadBytesWritten);
                }
            }
            else if (upload.status == UPLOAD_FILE_ABORTED) { 
                if (uploadFile) { 
                    uploadFile.close(); 
                    String targetDir = "/";
                    if (server.hasArg("dir")) targetDir = server.arg("dir");
                    if (!targetDir.startsWith("/")) targetDir = "/" + targetDir;
                    if (!targetDir.endsWith("/")) targetDir += "/";
                    LittleFS.remove(targetDir + sanitizeFilename(upload.filename));
                }
                uploadError = true;
                drawUploadStats("ABORTED", 0, true);
            }
        });

        server.on("/delete", HTTP_GET, [this]() {
            String redirectUrl = "/";
            if (server.hasArg("name")) {
                String filename = server.arg("name");
                if(!filename.startsWith("/")) filename = "/" + filename;
                
                if (LittleFS.exists(filename)) {
                    LittleFS.remove(filename);
                    forceOverlay("Deleted", 2, "info");
                    Serial.println("Web: Deleted " + filename);
                }

                int lastSlash = filename.lastIndexOf('/');
                if (lastSlash > 0) {
                    String parent = filename.substring(0, lastSlash);
                    redirectUrl = "/?dir=" + parent;
                }
            }
            server.sendHeader("Location", redirectUrl);
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