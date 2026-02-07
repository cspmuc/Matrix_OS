#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "NetworkManager.h"

class WebInterface {
private:
    WebServer server;
    bool active = false;
    bool fsMounted = false;

public:
    WebInterface() : server(80) {}

    void begin() {
        // VERSUCH: LittleFS starten. 'true' bedeutet: Formatieren, wenn kaputt/leer!
        // Das verhindert den "Connection Refused" Fehler, weil der ESP nicht mehr in einer Bootschleife hängt.
        if (LittleFS.begin(true)) {
            fsMounted = true;
            Serial.println("WebInterface: LittleFS erfolgreich geladen.");
        } else {
            Serial.println("WebInterface: LittleFS FEHLGESCHLAGEN! (Partition Scheme prüfen?)");
            fsMounted = false;
        }

        // --- ROUTEN ---
        
        // Hauptseite
        server.on("/", HTTP_GET, [this]() {
            if (!fsMounted) {
                // Falls FS kaputt: Fehlerseite anzeigen statt Absturz
                server.send(200, "text/html", "<html><body><h1>Dateisystem Fehler</h1><p>LittleFS konnte nicht geladen werden.</p><p>Bitte in Arduino IDE pr&uuml;fen: <b>ESP32S3 Dev Module</b> & <b>Default 4MB with SPIFFS</b></p></body></html>");
                return;
            }
            handleFileList();
        });

        // Upload Endpunkt
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/plain", ""); 
        }, [this]() {
            if (fsMounted) handleFileUpload(); 
        });

        // Datei Löschen
        server.on("/delete", HTTP_GET, [this]() {
            if (!fsMounted) return server.send(500, "text/plain", "No FS");
            String path = server.arg("name");
            if (LittleFS.exists(path) && path != "/") LittleFS.remove(path);
            server.sendHeader("Location", "/");
            server.send(303);
        });
        
        // Dateien abrufen (für Bilder/JSON)
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
        
        html += "<h2>Matrix OS - File Manager</h2>";
        
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

    void handleFileUpload() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            File f = LittleFS.open(filename, "w");
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            File f = LittleFS.open(filename, "a");
            if (f) f.write(upload.buf, upload.currentSize);
            f.close();
        } else if (upload.status == UPLOAD_FILE_END) {
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