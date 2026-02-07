#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "NetworkManager.h"

class WebInterface {
private:
    WebServer server;
    bool active = false;
    bool fsMounted = false;
    File uploadFile; // HIER: Globales Handle für den Upload

public:
    WebInterface() : server(80) {}

    void begin() {
        // LittleFS starten. 'true' formatiert bei Fehler.
        if (LittleFS.begin(true)) {
            fsMounted = true;
            Serial.println("WebInterface: LittleFS Mounted.");
        } else {
            Serial.println("WebInterface: LittleFS FAILED!");
            fsMounted = false;
        }

        // --- ROUTEN ---
        
        // 1. Hauptseite
        server.on("/", HTTP_GET, [this]() {
            if (!fsMounted) {
                server.send(200, "text/html", "<html><body><h1>Dateisystem Status</h1><p>LittleFS wird formatiert oder Fehler.</p><p>Bitte warten oder Reset druecken.</p></body></html>");
                return;
            }
            handleFileList();
        });

        // 2. Upload (Der wichtige Teil)
        server.on("/upload", HTTP_POST, [this]() {
            // Sende OK ans Frontend, wenn fertig
            server.send(200, "text/plain", "OK"); 
        }, [this]() {
            // Hier werden die Datenpakete verarbeitet
            if (fsMounted) handleFileUpload(); 
        });

        // 3. Löschen
        server.on("/delete", HTTP_GET, [this]() {
            if (!fsMounted) return server.send(500, "text/plain", "No FS");
            String path = server.arg("name");
            if (LittleFS.exists(path) && path != "/") LittleFS.remove(path);
            server.sendHeader("Location", "/");
            server.send(303);
        });
        
        // 4. Download / Ansicht
        server.onNotFound([this]() {
            if (!fsMounted) { server.send(404, "text/plain", "FS Error"); return; }
            if (!handleFileRead(server.uri())) {
                server.send(404, "text/plain", "404: Not Found");
            }
        });

        server.begin();
        active = true;
        Serial.println("WebInterface: Server gestartet.");
    }

    void loop() {
        if (active) server.handleClient();
    }

private:
    void handleFileList() {
        String html = "<html><head><title>Matrix OS</title>";
        html += "<style>body{font-family:sans-serif; background:#222; color:#fff; padding:20px;}";
        html += "a{color:#0cf; text-decoration:none;} table{width:100%; border-collapse:collapse; margin-top:20px;}";
        html += "td{padding:8px; border-bottom:1px solid #444;} .btn{background:#0cf; color:#000; padding:5px 10px; border:none; cursor:pointer;}</style></head><body>";
        
        html += "<h2>File Manager</h2>";
        
        size_t total = LittleFS.totalBytes();
        size_t used = LittleFS.usedBytes();
        html += "<p>Speicher: " + String(used) + " / " + String(total) + " Bytes</p>";

        html += "<form method='POST' action='/upload' enctype='multipart/form-data'>";
        html += "<input type='file' name='data' /> <input class='btn' type='submit' value='Upload' />";
        html += "</form>";

        html += "<table><tr><th>Name</th><th>Size</th><th>Action</th></tr>";
        
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while(file){
            String fileName = file.name();
            if(!fileName.startsWith("/")) fileName = "/" + fileName;
            
            html += "<tr>";
            html += "<td><a href='" + fileName + "'>" + fileName + "</a></td>";
            html += "<td>" + String(file.size()) + "</td>";
            html += "<td><a href='/delete?name=" + fileName + "' style='color:#f55'>[L&ouml;schen]</a></td>";
            html += "</tr>";
            file = root.openNextFile();
        }
        html += "</table></body></html>";
        server.send(200, "text/html", html);
    }

    // --- OPTIMIERTER UPLOAD ---
    void handleFileUpload() {
        HTTPUpload& upload = server.upload();
        
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            Serial.print("Upload Start: "); Serial.println(filename);
            
            // Datei EINMAL öffnen (Write Mode "w" überschreibt existierende)
            // Wichtig: Wir schließen sie hier NICHT sofort wieder!
            uploadFile = LittleFS.open(filename, "w");
            
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            // Nur schreiben, wenn Datei offen ist
            if (uploadFile) {
                uploadFile.write(upload.buf, upload.currentSize);
            }
            
        } else if (upload.status == UPLOAD_FILE_END) {
            // Erst ganz am Ende schließen
            if (uploadFile) {
                uploadFile.close();
                Serial.print("Upload Ende. Groesse: "); Serial.println(upload.totalSize);
            }
            // Weiterleitung zurück zur Hauptseite
            server.sendHeader("Location", "/");
            server.send(303);
        }
    }

    bool handleFileRead(String path) {
        if (path.endsWith("/")) path += "index.htm";
        String contentType = "text/plain";
        if (path.endsWith(".html")) contentType = "text/html";
        else if (path.endsWith(".json")) contentType = "application/json";
        else if (path.endsWith(".bmp")) contentType = "image/bmp";
        else if (path.endsWith(".png")) contentType = "image/png";

        if (LittleFS.exists(path)) {
            File file = LittleFS.open(path, "r");
            server.streamFile(file, contentType);
            file.close();
            return true;
        }
        return false;
    }
};