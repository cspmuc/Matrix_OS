#pragma once
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_task_wdt.h> 
#include "config.h"

extern void forceOverlay(String msg, int durationSec, String colorName);
extern DisplayManager display; 

// --- NEU: Empfängt den Reset-Befehl aus der HTML und reicht ihn an PongApp weiter ---
extern bool pong_end_trigger;

// --- PONG HTML IM FLASH SPEICHER ---
const char PONG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, orientation=landscape">
  <title>Matrix Pong</title>
  <style>
    body { background: #111; color: #eee; font-family: Arial, sans-serif; text-align: center; margin: 0; overflow: hidden; user-select: none; -webkit-user-select: none; touch-action: none; }
    .btn { display: block; border: 2px solid #555; color: white; font-weight: bold; border-radius: 15px; width: 100%; margin: 0; touch-action: none; box-sizing: border-box; }
    .btn:active { opacity: 0.7; }
    
    #setup { padding-top: 5vh; display: flex; flex-direction: column; align-items: center; justify-content: center; height: 100vh; }
    .btn-join { height: 15vh; width: 90%; font-size: 24px; background: #333; margin: 10px 0; }
    .btn-join-p1 { border-color: cyan; color: cyan; }
    .btn-join-p2 { border-color: magenta; color: magenta; }
    .btn-join-split { border-color: gold; color: gold; margin-top: 30px; }
    
    /* Layout 1: Einzelnes Handy (Links Controls, Rechts Up/Down) */
    #game-single { display: none; height: 100vh; flex-direction: row; justify-content: space-between; align-items: stretch; padding: 10px; box-sizing: border-box; gap: 15px; }
    .controls-left { width: 35%; display: flex; flex-direction: column; justify-content: space-around; }
    .controls-right { width: 60%; display: flex; flex-direction: column; justify-content: space-between; gap: 15px; }
    
    /* Layout 2: Split Screen (Querformat) */
    #game-split { display: none; height: 100vh; flex-direction: row; justify-content: space-between; align-items: stretch; padding: 10px; box-sizing: border-box; gap: 10px; }
    .split-side { width: 35%; display: flex; flex-direction: column; justify-content: space-between; gap: 15px; }
    .split-center { width: 25%; display: flex; flex-direction: column; justify-content: space-around; }
    
    /* System & Movement Buttons */
    .btn-ctrl { background: #222; font-size: 18px; flex: 1; margin: 5px 0; display: flex; align-items: center; justify-content: center; }
    .btn-start { border-color: #00ff00; color: #00ff00; }
    .btn-end { border-color: #ff3333; color: #ff3333; }
    .btn-fs { border-color: #ffff00; color: #ffff00; }
    
    .btn-move { flex: 1; background: #333; font-size: 36px; display: flex; align-items: center; justify-content: center; }
  </style>
  <script>
    let player = 0;
    
    function send(cmd, p) { 
        fetch('/pong_ctrl?cmd=' + cmd + '&p=' + p).catch(e => console.log(e)); 
    }
    
    // --- FULL SCREEN API ---
    function toggleFS() {
        let doc = window.document;
        let docEl = doc.documentElement;
        let requestFS = docEl.requestFullscreen || docEl.mozRequestFullScreen || docEl.webkitRequestFullScreen || docEl.msRequestFullscreen;
        let cancelFS = doc.exitFullscreen || doc.mozCancelFullScreen || doc.webkitExitFullscreen || doc.msExitFullscreen;
        
        if(!doc.fullscreenElement && !doc.mozFullScreenElement && !doc.webkitFullscreenElement && !doc.msFullscreenElement) {
            requestFS.call(docEl).catch(err => console.log(err));
        } else {
            cancelFS.call(doc);
        }
    }
    
    function joinSingle(p) {
      player = p;
      send('join', p);
      document.getElementById('setup').style.display = 'none';
      document.getElementById('game-single').style.display = 'flex';
      let col = (p == 1) ? 'cyan' : 'magenta';
      document.getElementById('btn-up').style.borderColor = col;
      document.getElementById('btn-up').style.color = col;
      document.getElementById('btn-down').style.borderColor = col;
      document.getElementById('btn-down').style.color = col;
    }
    
    function joinSplit() {
      send('join', 1);
      setTimeout(function() { send('join', 2); }, 150);
      document.getElementById('setup').style.display = 'none';
      document.getElementById('game-split').style.display = 'flex';
    }
    
    function setupBtn(id, cmdDown, cmdUp, targetPlayer) {
        let btn = document.getElementById(id);
        if(!btn) return;
        btn.addEventListener('touchstart', function(e) { e.preventDefault(); send(cmdDown, targetPlayer()); });
        btn.addEventListener('touchend', function(e) { e.preventDefault(); send(cmdUp, targetPlayer()); });
        btn.addEventListener('mousedown', function(e) { e.preventDefault(); send(cmdDown, targetPlayer()); });
        btn.addEventListener('mouseup', function(e) { e.preventDefault(); send(cmdUp, targetPlayer()); });
    }
    
    window.onload = function() {
        // Setup für Single-Screen
        setupBtn('btn-up', 'up', 'stop', () => player);
        setupBtn('btn-down', 'down', 'stop', () => player);
        
        // Setup für Split-Screen
        setupBtn('btn-split-up-p1', 'up', 'stop', () => 1);
        setupBtn('btn-split-down-p1', 'down', 'stop', () => 1);
        setupBtn('btn-split-up-p2', 'up', 'stop', () => 2);
        setupBtn('btn-split-down-p2', 'down', 'stop', () => 2);
    };
  </script>
</head>
<body>
  <div id="setup">
    <h2 style="margin-top: 0;">Matrix Pong</h2>
    <p style="color:#aaa; font-size:14px; margin-bottom: 20px;">&#128241; Bitte Gerät quer halten!</p>
    <button class="btn btn-join btn-join-p1" onclick="joinSingle(1)">P1 (CYAN)</button>
    <button class="btn btn-join btn-join-p2" onclick="joinSingle(2)">P2 (MAGENTA)</button>
    <button class="btn btn-join btn-join-split" onclick="joinSplit()">&#128241; MULTITOUCH</button>
  </div>
  
  <div id="game-single">
    <div class="controls-left">
        <button class="btn btn-ctrl btn-start" onclick="send('start', player)">START</button>
        <button class="btn btn-ctrl btn-end" onclick="send('end', player)">END GAME</button>
        <button class="btn btn-ctrl btn-fs" onclick="toggleFS()">FULL SCR</button>
    </div>
    <div class="controls-right">
        <button id="btn-up" class="btn btn-move">&#9650;</button>
        <button id="btn-down" class="btn btn-move">&#9660;</button>
    </div>
  </div>
  
  <div id="game-split">
    <div class="split-side">
        <button id="btn-split-up-p1" class="btn btn-move" style="border-color:cyan; color:cyan;">&#9650;</button>
        <button id="btn-split-down-p1" class="btn btn-move" style="border-color:cyan; color:cyan;">&#9660;</button>
    </div>
    <div class="split-center">
        <button class="btn btn-ctrl btn-start" onclick="send('start', 1)">START</button>
        <button class="btn btn-ctrl btn-end" onclick="send('end', 1)">END GAME</button>
        <button class="btn btn-ctrl btn-fs" onclick="toggleFS()">FULL SCR</button>
    </div>
    <div class="split-side">
        <button id="btn-split-up-p2" class="btn btn-move" style="border-color:magenta; color:magenta;">&#9650;</button>
        <button id="btn-split-down-p2" class="btn btn-move" style="border-color:magenta; color:magenta;">&#9660;</button>
    </div>
  </div>
</body>
</html>
)rawliteral";

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
        server.on("/", HTTP_GET, [this]() {
            String path = "/";
            if (server.hasArg("dir")) path = server.arg("dir");
            if (!path.startsWith("/")) path = "/" + path;
            if (!path.endsWith("/") && path.length() > 1) path += "/";

            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/html", ""); 
            
            server.sendContent(F("<html><head><meta charset='utf-8'><title>Matrix OS</title></head><body style='font-family: Arial, sans-serif;'>"));
            server.sendContent(F("<h1>Storage: ")); server.sendContent(path);
            server.sendContent(F("</h1><p>Used: ")); server.sendContent(String(LittleFS.usedBytes()));
            server.sendContent(F(" / ")); server.sendContent(String(LittleFS.totalBytes())); server.sendContent(F(" Bytes</p>"));
            
            server.sendContent(F("<div style='display: flex; gap: 10px; margin-bottom: 20px;'>"));
            server.sendContent(F("<form method='POST' action='/format' onsubmit='return confirm(\"Alles löschen?\")'><input type='submit' value='Formatieren (Alles löschen)' style='color:red; padding: 5px 10px;'></form>"));
            server.sendContent(F("<form method='POST' action='/reboot' onsubmit='return confirm(\"System jetzt neu starten?\")'><input type='submit' value='Reboot ESP32' style='color:darkorange; padding: 5px 10px; font-weight: bold;'></form>"));
            server.sendContent(F("</div><hr><form method='POST' action='/upload?dir="));
            server.sendContent(path);
            server.sendContent(F("' enctype='multipart/form-data'><input type='file' name='upload'><input type='submit' value='Upload' style='padding: 5px 10px;'></form><hr>"));

            if (path != "/") {
                String parent = path.substring(0, path.length() - 1);
                int lastSlash = parent.lastIndexOf('/');
                if (lastSlash >= 0) parent = parent.substring(0, lastSlash + 1);
                else parent = "/";
                server.sendContent(F("<p><a href='/?dir=")); server.sendContent(parent); server.sendContent(F("'>.. (Zurück)</a></p>"));
            }

            server.sendContent(F("<table border='1' cellpadding='5' style='border-collapse: collapse; text-align: left;'><tr><th>Name</th><th>Size</th><th>Action</th></tr>"));

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
                        server.sendContent(F("<tr><td><b><a href='/?dir=")); server.sendContent(fullPath);
                        server.sendContent(F("'>[")); server.sendContent(fileName); server.sendContent(F("]</a></b></td><td>DIR</td><td>-</td></tr>"));
                    } else {
                        server.sendContent(F("<tr><td><a href='")); server.sendContent(fullPath); server.sendContent(F("'>"));
                        server.sendContent(fileName); server.sendContent(F("</a></td><td>"));
                        server.sendContent(String(file.size()));
                        server.sendContent(F(" B</td><td><a href='/editor?file=")); server.sendContent(fullPath);
                        server.sendContent(F("'>Edit</a> | <a href='/delete?name=")); server.sendContent(fullPath);
                        server.sendContent(F("' style='color:red;'>Delete</a></td></tr>"));
                    }
                    file = root.openNextFile();
                    yield(); 
                }
            }
            server.sendContent(F("</table></body></html>"));
            server.sendContent(""); 
        });

        server.on("/editor", HTTP_GET, [this]() {
            if (!server.hasArg("file")) { server.send(400, "text/plain", "Fehler: Keine Datei"); return; }
            String filename = server.arg("file");
            if (!LittleFS.exists(filename)) { server.send(404, "text/plain", "Fehler: Datei nicht gefunden."); return; }

            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/html", "");

            server.sendContent(F("<html><head><meta charset='utf-8'><title>Editor - Matrix OS</title></head><body style='font-family: Arial, sans-serif;'>"));
            server.sendContent(F("<h2>Editing: ")); server.sendContent(filename); server.sendContent(F("</h2>"));
            
            if (server.hasArg("saved")) server.sendContent(F("<p style='color: green; font-weight: bold;'>Datei gespeichert!</p>"));

            server.sendContent(F("<form method='POST' action='/edit?file=")); server.sendContent(filename); server.sendContent(F("'>"));
            server.sendContent(F("<textarea name='content' rows='25' style='width: 100%; max-width: 800px; font-family: monospace; white-space: pre;'>"));

            File f = LittleFS.open(filename, "r");
            if (f) {
                while(f.available()) {
                    server.sendContent(f.readStringUntil('\n') + "\n");
                    yield(); 
                }
                f.close();
            }

            server.sendContent(F("</textarea><br><br>"));
            server.sendContent(F("<div style='display: flex; gap: 15px;'><input type='submit' value='&#128190; Speichern' style='padding: 10px 20px; cursor: pointer;'></form>"));
            server.sendContent(F("<form method='POST' action='/reboot'><input type='submit' value='&#8635; Reboot' style='padding: 10px 20px; color: darkorange; cursor: pointer;'></form></div>"));
            server.sendContent(F("<br><br><a href='/?dir=/'>&larr; Zurück</a></body></html>"));
            server.sendContent("");
        });

        server.on("/edit", HTTP_POST, [this]() {
            if (!server.hasArg("file") || !server.hasArg("content")) { server.send(400, "text/plain", "Fehler: Fehlende Parameter."); return; }
            String filename = server.arg("file");
            String content = server.arg("content");
            File f = LittleFS.open(filename, "w"); 
            if (f) {
                f.print(content); f.close();
                forceOverlay("Saved!", 2, "success");
                server.sendHeader("Location", "/editor?file=" + filename + "&saved=1");
                server.send(303);
            } else server.send(500, "text/plain", "Fehler: Datei konnte nicht geschrieben werden.");
        });

        server.on("/reboot", HTTP_POST, [this]() {
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/html", "");
            server.sendContent(F("<html><head><meta charset='utf-8'><meta http-equiv='refresh' content='7; url=/'></head>"));
            server.sendContent(F("<body style='font-family: Arial, sans-serif; text-align: center; margin-top: 50px;'>"));
            server.sendContent(F("<h2>System startet neu...</h2><p>Bitte warten...</p></body></html>"));
            server.sendContent("");
            delay(1000); 
            ESP.restart();
        });

        server.on("/format", HTTP_POST, [this]() {
            display.clear(); display.setTextColor(display.color565(255, 0, 0));
            display.printCentered("FORMATTING...", 32); display.show(); delay(100); 
            LittleFS.format();
            server.send(200, "text/html", "<html><head><meta charset='utf-8'></head><body>Formatiert! <a href='/'>Zurück</a></body></html>");
            forceOverlay("Format OK", 3, "success");
        });

        server.on("/upload", HTTP_POST, [this]() {
            if (uploadError) server.send(507, "text/plain", "Error: Write Failed");
            else {
                String targetDir = "/";
                if(server.hasArg("dir")) targetDir = server.arg("dir");
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
                uploadFile = LittleFS.open(fullPath, "w");
                if (!uploadFile) { uploadError = true; return; }
                
                uploadBytesWritten = 0; lastDrawTime = 0; 
                display.setBrightness(150);
                drawUploadStats(filename, 0);
            } 
            else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadError) return; 
                if (uploadFile) {
                    size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
                    if (bytesWritten < upload.currentSize) {
                        uploadError = true; uploadFile.close();
                        String targetDir = "/";
                        if (server.hasArg("dir")) targetDir = server.arg("dir");
                        if (!targetDir.startsWith("/")) targetDir = "/" + targetDir;
                        if (!targetDir.endsWith("/")) targetDir += "/";
                        LittleFS.remove(targetDir + sanitizeFilename(upload.filename)); 
                        drawUploadStats("ERROR", 0, true); return;
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
                uploadError = true; drawUploadStats("ABORTED", 0, true);
            }
        });

        server.on("/delete", HTTP_GET, [this]() {
            String redirectUrl = "/";
            if (server.hasArg("name")) {
                String filename = server.arg("name");
                if(!filename.startsWith("/")) filename = "/" + filename;
                if (LittleFS.exists(filename)) { LittleFS.remove(filename); forceOverlay("Deleted", 2, "info"); }
                int lastSlash = filename.lastIndexOf('/');
                if (lastSlash > 0) {
                    String parent = filename.substring(0, lastSlash);
                    redirectUrl = "/?dir=" + parent;
                }
            }
            server.sendHeader("Location", redirectUrl); server.send(303);
        });

        server.on("/pong", HTTP_GET, [this]() {
            server.send_P(200, "text/html", PONG_HTML);
        });

        server.on("/pong_ctrl", HTTP_GET, [this]() {
            if (server.hasArg("cmd") && server.hasArg("p")) {
                String cmd = server.arg("cmd");
                int p = server.arg("p").toInt();

                if (cmd == "join") {
                    if (p == 1) pong_p1_ready = true;
                    if (p == 2) pong_p2_ready = true;
                } 
                else if (cmd == "start") {
                    pong_start_trigger = true;
                }
                // --- NEU: Verarbeitet den neuen End Game Button ---
                else if (cmd == "end") {
                    pong_end_trigger = true;
                }
                else if (cmd == "up") {
                    if (p == 1) pong_p1_dir = -1;
                    if (p == 2) pong_p2_dir = -1;
                }
                else if (cmd == "down") {
                    if (p == 1) pong_p1_dir = 1;
                    if (p == 2) pong_p2_dir = 1;
                }
                else if (cmd == "stop") {
                    if (p == 1) pong_p1_dir = 0;
                    if (p == 2) pong_p2_dir = 0;
                }
            }
            server.send(200, "text/plain", "OK");
        });

        server.onNotFound([this]() {
            String path = server.uri();
            if (LittleFS.exists(path)) {
                File file = LittleFS.open(path, "r");
                server.streamFile(file, "application/octet-stream"); file.close();
            } else server.send(404, "text/plain", "File not found");
        });

        server.begin();
    }

    void handle() {
        server.handleClient();
    }
};