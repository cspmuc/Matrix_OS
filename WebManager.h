#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "config.h"

class WebManager {
private:
    WebServer server;

public:
    WebManager() : server(80) {}

    void begin() {
        // 1. Root: Zeigt Dateiliste & Upload Formular
        server.on("/", HTTP_GET, [this]() {
            String html = "<html><head><title>Matrix OS File Manager</title></head><body>";
            html += "<h1>Matrix OS Storage</h1>";
            
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

        // 2. Upload Handler
        server.on("/upload", HTTP_POST, [this]() {
            server.send(200, "text/html", "Upload success! <a href='/'>Back</a>");
        }, [this]() { // File Upload Handler
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                String filename = "/" + upload.filename;
                if(!filename.startsWith("/")) filename = "/" + filename;
                File f = LittleFS.open(filename, "w");
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                String filename = "/" + upload.filename;
                if(!filename.startsWith("/")) filename = "/" + filename;
                File f = LittleFS.open(filename, "a");
                if(f) f.write(upload.buf, upload.currentSize);
                f.close();
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

        // 4. File Download Handler (Fallback für alle Dateien)
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
        Serial.println("Web: Server started on Port 80");
    }

    void handle() {
        server.handleClient();
    }
};