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
extern volatile bool otaActive;
extern volatile int otaProgress;

class MatrixNetworkManager {
private:
    WiFiClient espClient;
    PubSubClient client;
    
    // CLEANUP: Keine Atomic Referenzen mehr
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
        // (Logik bleibt gleich, aber Zugriff auf Refs ist einfacher)
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
            // (Sensor Code bleibt gleich)
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
    // CLEANUP: Konstruktor Signatur angepasst (kein std::atomic)
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
        ArduinoOTA.onStart([]() { otaActive = true; otaProgress = 0; });
        ArduinoOTA.onEnd([]() { otaActive = false; });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            otaProgress = (progress / (total / 100));
        });
        ArduinoOTA.onError([](ota_error_t error) { otaActive = false; });
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
        
        // CLEANUP: .load() entfernt
        client.publish("matrix/status/brightness", String(brightnessRef).c_str(), true);
        String appStr;
        switch(currentAppRef) { // CLEANUP: .load() entfernt
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