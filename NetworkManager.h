#pragma once
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "DisplayManager.h"
#include "SensorApp.h" 

extern void status(const String& msg, uint16_t color);
extern void queueOverlay(String msg, int durationSec, String colorName, int scrollSpeed);

class MatrixNetworkManager {
private:
    WiFiClient espClient;
    PubSubClient client;
    
    AppMode& currentAppRef;
    int& brightnessRef;
    DisplayManager& displayRef;
    SensorApp& sensorAppRef; 

    static MatrixNetworkManager* instance;
    
    bool otaInitialized = false;
    bool timeInitialized = false;
    bool timeSynced = false;
    bool mqttInitialized = false;
    
    unsigned long lastWifiCheck = 0;
    unsigned long lastMqttRetry = 0;
    unsigned long lastTimeCheck = 0;

    void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
        String t = String(topic);
        DynamicJsonDocument doc(2560); 
        DeserializationError error = deserializeJson(doc, payload, length);
        if (error) {
             String msg = "";
             for (int i = 0; i < length; i++) msg += (char)payload[i];
             if (t == "matrix/cmd/power") {
                if (msg == "OFF") brightnessRef = 0;
                else if (msg == "ON" && brightnessRef == 0) brightnessRef = 150;
                publishState();
             }
             return;
        }
        if (t == "matrix/cmd/brightness" && doc.containsKey("val")) {
            brightnessRef = doc["val"].as<int>();
            publishState();
        }
        if (t == "matrix/cmd/app" && doc.containsKey("app")) {
            String newApp = doc["app"];
            if (newApp == "wordclock") currentAppRef = WORDCLOCK;
            else if (newApp == "sensors") currentAppRef = SENSORS;
            else if (newApp == "testpattern") currentAppRef = TESTPATTERN;
            else if (newApp == "ticker") currentAppRef = TICKER;
            else if (newApp == "plasma") currentAppRef = PLASMA;
            else if (newApp == "off") currentAppRef = OFF;
            publishState(); 
        }     
        if (t == "matrix/cmd/overlay") {
             String msg = doc["msg"] | "";
             int dur = doc["duration"] | 5;    
             String col = doc["color"] | "white";
             int speed = doc["speed"] | 30; 
             if (msg.length() > 0) queueOverlay(msg, dur, col, speed);
        }
        if (t == "matrix/cmd/sensor_page") {
            String id = doc["id"] | "default";
            String title = doc["title"] | "INFO";
            int ttl = doc["ttl"] | 60; 
            std::vector<SensorItem> items;
            JsonArray jsonItems = doc["items"].as<JsonArray>();
            for (JsonObject item : jsonItems) {
                SensorItem si;
                si.icon = item["icon"] | "";
                si.text = item["text"] | "--"; 
                si.color = item["color"] | "white";
                items.push_back(si);
            }
            if (!items.empty()) sensorAppRef.updatePage(id, title, ttl, items);
        }
    }

    static void mqttCallbackTrampoline(char* topic, byte* payload, unsigned int length) {
        if (instance) instance->handleMqttMessage(topic, payload, length);
    }

public:
    MatrixNetworkManager(AppMode& app, int& bright, DisplayManager& disp, SensorApp& sensors) 
        : client(espClient), currentAppRef(app), brightnessRef(bright), displayRef(disp), sensorAppRef(sensors) {
        instance = this;
    }

    String getIp() { return WiFi.localIP().toString(); }
    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    bool isTimeSynced() { return timeSynced; }

    bool begin() {
        WiFi.setSleep(false);
        if (WiFi.status() == WL_CONNECTED) return true;
        if (WiFi.status() != WL_CONNECTED) {
             WiFi.mode(WIFI_STA);
             WiFi.begin(ssid, password);
        }
        if (WiFi.status() == WL_CONNECTED) {
            tryInitServices(); 
            return true;
        } 
        return false;
    }

    void tryInitServices() {
        if (WiFi.status() != WL_CONNECTED) return;

        if (!otaInitialized) {
            setupOTA();
            otaInitialized = true;
        }
        if (!timeInitialized) {
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
            timeInitialized = true; 
        }
        if (!mqttInitialized) { 
            client.setServer(mqtt_server, mqtt_port);
            client.setCallback(mqttCallbackTrampoline);
            client.setBufferSize(2048); 
            mqttInitialized = true;
        }
    }

    void setupOTA() {
        ArduinoOTA.setHostname("Wortuhr-Matrix-OS");
        ArduinoOTA.setPassword(ota_password);

        ArduinoOTA.onStart([this]() { 
            displayRef.setBrightness(150); // Sicherstellen dass man was sieht
            displayRef.setFade(1.0);
            displayRef.clear();
            
            displayRef.setTextColor(displayRef.color565(255, 255, 0)); // Gelb
            displayRef.printCentered("SYSTEM UPDATE", 20);
            displayRef.printCentered("BITTE WARTEN...", 40);
            
            displayRef.show(); // Zeigt das Bild sofort an
        });

        // 2. ENDE: Erfolgsmeldung + Delay vor Reboot
        ArduinoOTA.onEnd([this]() { 
            displayRef.clear();
            displayRef.setTextColor(displayRef.color565(0, 255, 0)); // Grün
            displayRef.printCentered("UPDATE SUCCESS!", 32);
            displayRef.show();
            
            // Hier das gewünschte Delay, damit man es lesen kann
            delay(2000); 
        });

        // 3. PROGRESS: Balken aktualisieren (Optional, aber schön)
        // ArduinoOTA ruft das oft genug auf, dass es animiert wirkt.
        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            int p = (progress / (total / 100));
            
            // Nur alle 2% zeichnen, spart etwas Zeit, sieht aber flüssig aus
            if (p % 2 == 0) {
                displayRef.clear();
                displayRef.setTextColor(displayRef.color565(255, 255, 0));
                displayRef.printCentered("UPDATING...", 15);
                
                // Balken zeichnen
                int w = map(p, 0, 100, 0, 100);
                displayRef.drawRect(14, 35, 102, 12, displayRef.color565(100, 100, 100)); // Rahmen
                displayRef.fillRect(15, 36, w, 10, displayRef.color565(0, 255, 0));       // Füllung
                
                // Prozentzahl
                String s = String(p) + "%";
                displayRef.printCentered(s, 55);
                
                displayRef.show();
            }
        });

        // 4. ERROR: Fehlermeldung
        ArduinoOTA.onError([this](ota_error_t error) { 
            displayRef.clear();
            displayRef.setTextColor(displayRef.color565(255, 0, 0)); // Rot
            displayRef.printCentered("UPDATE ERROR!", 25);
            
            if (error == OTA_AUTH_ERROR) displayRef.printCentered("Auth Failed", 45);
            else if (error == OTA_BEGIN_ERROR) displayRef.printCentered("Begin Failed", 45);
            else if (error == OTA_CONNECT_ERROR) displayRef.printCentered("Connect Failed", 45);
            else if (error == OTA_RECEIVE_ERROR) displayRef.printCentered("Receive Failed", 45);
            else if (error == OTA_END_ERROR) displayRef.printCentered("End Failed", 45);
            
            displayRef.show();
            delay(3000); // Zeit zum Lesen lassen
        });

        ArduinoOTA.begin();
    }
    
    void checkTimeSync() {
        if (!timeInitialized || timeSynced) return; 
        struct tm ti;
        if (getLocalTime(&ti, 0)) {
            timeSynced = true;
            queueOverlay("Time Synced", 3, "success", 0); 
            Serial.println("Network: NTP Time Synchronized successfully.");
        }
    }

    void loop() {
        if (otaInitialized) ArduinoOTA.handle();

        unsigned long now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            if (now - lastWifiCheck > 10000) { 
                lastWifiCheck = now;
                otaInitialized = false;
                mqttInitialized = false; 
                WiFi.disconnect();
                WiFi.reconnect();
            }
            return; 
        }

        tryInitServices();
        
        if (!timeSynced && (now - lastTimeCheck > 1000)) {
            lastTimeCheck = now;
            checkTimeSync();
        }

        if (!client.connected()) {
            if (now - lastMqttRetry > 5000) { 
                lastMqttRetry = now;
                if (client.connect("MatrixPortalS3", mqtt_user, mqtt_pass, "matrix/status", 0, true, "OFF")) {
                    client.subscribe("matrix/cmd/#");
                    publishState();
                }
            }
        } else {
            client.loop();
        }
    }

    void publishState() {
        if (!client.connected()) return;
        if (brightnessRef > 0) client.publish("matrix/status", "ON", true);
        else client.publish("matrix/status", "OFF", true);
        client.publish("matrix/status/brightness", String(brightnessRef).c_str(), true);
        String appStr;
        switch(currentAppRef) {
            case WORDCLOCK:   appStr = "wordclock"; break;
            case SENSORS:     appStr = "sensors"; break;
            case TESTPATTERN: appStr = "testpattern"; break;
            case TICKER:      appStr = "ticker"; break;
            case PLASMA:      appStr = "plasma"; break;
            default:          appStr = "off"; break;
        }
        client.publish("matrix/status/app", appStr.c_str(), true);
    }
};

MatrixNetworkManager* MatrixNetworkManager::instance = nullptr;