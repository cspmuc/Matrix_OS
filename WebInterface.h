#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include "NetworkManager.h"

class WebInterface {
private:
    WebServer server;
    bool active = false;

public:
    WebInterface() : server(80) {}

    void begin() {
        if (!LittleFS.begin(true)) return;

        server.on("/", HTTP_GET, [this]() { handleFileList(); });
        server.on("/upload", HTTP_POST, [this]() { server.send(200); }, [this]() { handleFileUpload(); });
        
        server.on("/delete", HTTP_GET, [this]() {
            String path = server.arg("name");
            if (LittleFS.exists(path) && path != "/") LittleFS.remove(path);
            server.sendHeader("Location", "/");
            server.send(303);
        });
        
        server.onNotFound([this]() {
            if (!handleFileRead(server.uri())) server.send(404, "text/plain", "404: Not Found");
        });

        server.begin();
        active = true;
    }

    void loop() { if (active) server.handleClient(); }

private:
    void handleFileList() {
        String html = "<html><head><title>Matrix OS Admin</title><style>body{font-family:sans-serif;background:#222;color:#fff;padding:20px;}a{color:#0cf}table{width:100%;border-collapse:collapse}td{padding:5px;border-bottom:1px solid #444}</style></head><body><h2>File Manager</h2>";
        html += "<p>Used: " + String(LittleFS.usedBytes()) + " / " + String(LittleFS.totalBytes()) + "</p>";
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
            String fn = upload.filename; if (!fn.startsWith("/")) fn = "/" + fn;
            File f = LittleFS.open(fn, "w");
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            String fn = upload.filename; if (!fn.startsWith("/")) fn = "/" + fn;
            File f = LittleFS.open(fn, "a");
            if (f) f.write(upload.buf, upload.currentSize);
            f.close();
        } else if (upload.status == UPLOAD_FILE_END) {
            server.sendHeader("Location", "/"); server.send(303);
        }
    }

    bool handleFileRead(String path) {
        if (path.endsWith("/")) path += "index.htm";
        String ct = "text/plain";
        if (path.endsWith(".html")) ct = "text/html";
        else if (path.endsWith(".json")) ct = "application/json";
        else if (path.endsWith(".bmp")) ct = "image/bmp";
        if (LittleFS.exists(path)) {
            File file = LittleFS.open(path, "r");
            server.streamFile(file, ct);
            file.close();
            return true;
        }
        return false;
    }
};