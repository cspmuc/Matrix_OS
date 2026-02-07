#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "NetworkManager.h"

class WebInterface {
private:
    WebServer server;
    bool active = false;
    bool fsMounted = false;
    File uploadFile; 

public:
    WebInterface() : server(80) {}

    void begin() {
        if (LittleFS.begin(true)) {
            fsMounted = true;
            Serial.println("WebInterface: LittleFS Mounted.");
        } else {
            Serial.println("WebInterface: LittleFS FAILED!");
            fsMounted = false;
        }

        server.on("/", HTTP_GET, [this]() {
            if (!fsMounted) return server.send(200, "text/html", "<h1>FS Fehler</h1>");
            handleFileList();
        });

        // UPLOAD HANDLER
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/plain", "OK"); 
        }, [this]() {
            if (fsMounted) handleFileUpload(); 
        });

        server.on("/delete", HTTP_GET, [this]() {
            if (!fsMounted) return server.send(500, "text/plain", "No FS");
            String path = server.arg("name");
            if (LittleFS.exists(path) && path != "/") LittleFS.remove(path);
            server.sendHeader("Location", "/");
            server.send(303);
        });
        
        server.onNotFound([this]() {
            if (!fsMounted) return server.send(404, "text/plain", "FS Error");
            if (!handleFileRead(server.uri())) server.send(404, "text/plain", "404");
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
        String html = "<html><head><title>Matrix OS</title><style>body{font-family:sans-serif;background:#222;color:#fff;padding:20px}a{color:#0cf}table{width:100%;border-collapse:collapse}td{padding:5px;border-bottom:1px solid #444}</style></head><body>";
        html += "<h2>Dateimanager</h2>";
        html += "<p>Speicher: " + String(LittleFS.usedBytes()) + " / " + String(LittleFS.totalBytes()) + " Bytes</p>";
        html += "<form method='POST' action='/upload' enctype='multipart/form-data'><input type='file' name='data'/><input type='submit' value='Upload'/></form>";
        html += "<table>";
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while(file){
            String n = file.name(); if(!n.startsWith("/")) n = "/"+n;
            html += "<tr><td><a href='" + n + "'>" + n + "</a></td><td>" + String(file.size()) + "</td><td><a href='/delete?name=" + n + "'>[DEL]</a></td></tr>";
            file = root.openNextFile();
        }
        html += "</table></body></html>";
        server.send(200, "text/html", html);
    }

    void handleFileUpload() {
        HTTPUpload& upload = server.upload();
        
        if (upload.status == UPLOAD_FILE_START) {
            // WICHTIG: Display stoppen!
            isFsBusy = true; 
            
            String filename = upload.filename;
            if (!filename.startsWith("/")) filename = "/" + filename;
            Serial.print("Upload Start: "); Serial.println(filename);
            
            uploadFile = LittleFS.open(filename, "w");
            
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (uploadFile) {
                uploadFile.write(upload.buf, upload.currentSize);
            }
            
        } else if (upload.status == UPLOAD_FILE_END) {
            if (uploadFile) {
                uploadFile.close();
                Serial.print("Upload Ende. Groesse: "); Serial.println(upload.totalSize);
            }
            
            // WICHTIG: Display wieder freigeben!
            isFsBusy = false; 
            
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
        if (LittleFS.exists(path)) {
            File file = LittleFS.open(path, "r");
            server.streamFile(file, contentType);
            file.close();
            return true;
        }
        return false;
    }
};