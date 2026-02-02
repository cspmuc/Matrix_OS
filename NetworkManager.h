#pragma once
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "DisplayManager.h"
#include "SensorApp.h" 

// Status Signatur angepasst (const String&)
extern void status(const String& msg, uint16_t color);
extern volatile bool otaActive;
extern volatile int otaProgress;

class MatrixNetworkManager {
private:
    WiFiClient espClient;
    PubSubClient client;
    
    // Referenzen auf atomare Variablen
    std::atomic<AppMode>& currentAppRef;
    std::atomic<int>& brightnessRef;
    
    DisplayManager& displayRef;
    SensorApp& sensorAppRef; 

    static MatrixNetworkManager* instance;
    
    // Status-Flags
    bool otaInitialized = false;
    bool timeInitialized = false;
    bool timeSynced = false;
    bool mqttInitialized = false;
    
    unsigned long lastWifiCheck = 0;
    unsigned long lastMqttRetry = 0;
    unsigned long lastTimeCheck = 0;

    void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
        String t = String(topic);
        StaticJsonDocument<256> doc;
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
            // FIX: Explizite Konvertierung zu int für std::atomic
            brightnessRef = doc["val"].as<int>();
            publishState();
        }
        
        if (t == "matrix/cmd/app" && doc.containsKey("app")) {
            String newApp = doc["app"];
            if (newApp == "wordclock") currentAppRef = WORDCLOCK;
            else if (newApp == "sensors") currentAppRef = SENSORS;
            else if (newApp == "testpattern") currentAppRef = TESTPATTERN;
            else if (newApp == "ticker") currentAppRef = TICKER;
            else if (newApp == "plasma") currentAppRef = PLASMA; // <-- NEU
            else if (newApp == "off") currentAppRef = OFF;
            publishState(); 
        }     

        if (t == "matrix/cmd/overlay") {
             status(doc["msg"].as<String>(), displayRef.color565(255, 255, 255));
        }

        if (t == "matrix/sensor") {
            String temp = doc["temp"] | "--.-";
            String hum = doc["hum"] | "--";
            sensorAppRef.setData(temp, hum);
        }
    }

    static void mqttCallbackTrampoline(char* topic, byte* payload, unsigned int length) {
        if (instance) instance->handleMqttMessage(topic, payload, length);
    }

public:
    // Konstruktor nimmt jetzt atomic Referenzen
    MatrixNetworkManager(std::atomic<AppMode>& app, std::atomic<int>& bright, DisplayManager& disp, SensorApp& sensors) 
        : client(espClient), currentAppRef(app), brightnessRef(bright), displayRef(disp), sensorAppRef(sensors) {
        instance = this;
    }

    String getIp() { return WiFi.localIP().toString(); }
    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    bool isTimeSynced() { return timeSynced; }

    bool begin() {
        WiFi.setSleep(false);
        
        if (WiFi.status() == WL_CONNECTED) {
            return true;
        }
        
        status("Connecting...", displayRef.color565(255, 255, 255));
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        
        int r = 0;
        while (WiFi.status() != WL_CONNECTED && r < 4) { 
            delay(500); 
            r++; 
        }

        if (WiFi.status() == WL_CONNECTED) {
            WiFi.setSleep(false);
            status("WLAN OK", displayRef.color565(0, 255, 0));
            tryInitServices(); 
            return true;
        } else {
            status("No Connection", displayRef.color565(255, 0, 0));
            return false;
        }
    }

    void tryInitServices() {
        if (WiFi.status() != WL_CONNECTED) return;

        if (!otaInitialized) {
            setupOTA();
            otaInitialized = true;
        }
        
        if (!timeInitialized) {
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
            status("NTP Request...", displayRef.color565(0, 0, 255));
            timeInitialized = true; 
        }
        
        if (!mqttInitialized) { 
            client.setServer(mqtt_server, mqtt_port);
            client.setCallback(mqttCallbackTrampoline);
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
        ArduinoOTA.onError([](ota_error_t error) {
            otaActive = false;
        });
        ArduinoOTA.begin();
    }
    
    void checkTimeSync() {
        if (!timeInitialized || timeSynced) return; 
        struct tm ti;
        if (getLocalTime(&ti, 0)) {
            timeSynced = true;
            status("Time Synced!", displayRef.color565(0, 255, 0));
        }
    }

    void loop() {
        if (otaInitialized) {
            ArduinoOTA.handle();
        }

        unsigned long now = millis();

        // 1. WLAN Check
        if (WiFi.status() != WL_CONNECTED) {
            if (now - lastWifiCheck > 10000) { 
                lastWifiCheck = now;
                otaInitialized = false;
                mqttInitialized = false; 
                WiFi.disconnect();
                WiFi.reconnect();
                WiFi.setSleep(false);
            }
            return; 
        }

        // 2. Dienste
        tryInitServices();
        
        // 3. Zeit Check
        if (!timeSynced && (now - lastTimeCheck > 1000)) {
            lastTimeCheck = now;
            checkTimeSync();
        }

        // 4. MQTT
        if (!client.connected()) {
            if (now - lastMqttRetry > 5000) { 
                lastMqttRetry = now;
                if (client.connect("MatrixPortalS3", mqtt_user, mqtt_pass, "matrix/status", 0, true, "OFF")) {
                    client.subscribe("matrix/cmd/#");
                    client.subscribe("matrix/sensor");
                    publishState();
                }
            }
        } else {
            client.loop();
        }
    }

    void publishState() {
        if (!client.connected()) return;
        // .load() ist nicht zwingend bei atomic int read, aber explizit
        if (brightnessRef > 0) client.publish("matrix/status", "ON", true);
        else client.publish("matrix/status", "OFF", true);
        client.publish("matrix/status/brightness", String(brightnessRef.load()).c_str(), true);
        String appStr;
        switch(currentAppRef.load()) {
            case WORDCLOCK:   appStr = "wordclock"; break;
            case SENSORS:     appStr = "sensors"; break;
            case TESTPATTERN: appStr = "testpattern"; break;
            case TICKER:      appStr = "ticker"; break;
            case PLASMA:      appStr = "plasma"; break; // <--- DAS FEHLTE
            default:          appStr = "off"; break;
        }
        client.publish("matrix/status/app", appStr.c_str(), true);
    }
};

MatrixNetworkManager* MatrixNetworkManager::instance = nullptr;