#pragma once
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "DisplayManager.h"
#include "SensorApp.h" 
#include <time.h> 
#include <esp_heap_caps.h> 

extern void status(const String& msg, uint16_t color);
extern void queueOverlay(String msg, int durationSec, String colorName, int scrollSpeed);
extern void forceOverlay(String msg, int durationSec, String colorName);

// Eigener Allocator für ArduinoJson, der den PSRAM zwingend nutzt
#ifndef SPIRAM_ALLOCATOR_DEFINED
#define SPIRAM_ALLOCATOR_DEFINED
struct SpiRamAllocator {
  void* allocate(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
  void deallocate(void* pointer) { heap_caps_free(pointer); }
  void* reallocate(void* ptr, size_t new_size) { return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM); }
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;
#endif

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
        
        SpiRamJsonDocument* doc = new SpiRamJsonDocument(8192);
        DeserializationError error = deserializeJson(*doc, payload, length);
        
        if (error) {
             String msg = "";
             for (int i = 0; i < length; i++) msg += (char)payload[i];
             if (t == "matrix/cmd/power") {
                if (msg == "OFF") brightnessRef = 0;
                else if (msg == "ON" && brightnessRef == 0) brightnessRef = 150;
                publishState();
             }
             delete doc;
             return;
        }
        
        if (t == "matrix/cmd/brightness" && doc->containsKey("val")) {
            brightnessRef = (*doc)["val"].as<int>();
            publishState();
        }
        if (t == "matrix/cmd/app" && doc->containsKey("app")) {
            String newApp = (*doc)["app"];
            if (newApp == "wordclock") currentAppRef = WORDCLOCK;
            else if (newApp == "sensors") currentAppRef = SENSORS;
            else if (newApp == "testpattern") currentAppRef = TESTPATTERN;
            else if (newApp == "ticker") currentAppRef = TICKER;
            else if (newApp == "plasma") currentAppRef = PLASMA;
            else if (newApp == "off") currentAppRef = OFF;
            publishState(); 
        }     
        if (t == "matrix/cmd/overlay") {
             String msg = (*doc)["msg"] | "";
             int dur = (*doc)["duration"] | 5;    
             String col = (*doc)["color"] | "white";
             int speed = (*doc)["speed"] | 30; 
             bool urgent = (*doc)["urgent"] | false;
             
             Serial.print("MQTT Overlay: "); Serial.println(msg);
             
             if (msg.length() > 0) {
                 if (urgent) {
                     forceOverlay(msg, dur, col);
                 } else {
                     queueOverlay(msg, dur, col, speed);
                 }
             }
        }
        if (t == "matrix/cmd/sensor_page") {
            String id = (*doc)["id"] | "default";
            String title = (*doc)["title"] | "INFO";
            int ttl = (*doc)["ttl"] | 60; 
            std::vector<SensorItem> items;
            JsonArray jsonItems = (*doc)["items"].as<JsonArray>();
            for (JsonObject item : jsonItems) {
                SensorItem si;
                si.icon = item["icon"] | "";
                si.text = item["text"] | "--"; 
                si.color = item["color"] | "white";
                items.push_back(si);
            }
            if (!items.empty()) sensorAppRef.updatePage(id, title, ttl, items);
        }
        delete doc;
    }

    static void mqttCallbackTrampoline(char* topic, byte* payload, unsigned int length) {
        if (instance) instance->handleMqttMessage(topic, payload, length);
    }

    void configureStaticIP() {
        if (USE_STATIC_IP) {
            IPAddress ip, gateway, subnet, dns;
            ip.fromString(STATIC_IP);
            gateway.fromString(STATIC_GATEWAY);
            subnet.fromString(STATIC_SUBNET);
            dns.fromString(STATIC_DNS);
            
            if (!WiFi.config(ip, gateway, subnet, dns)) {
                Serial.println("Network: Failed to configure Static IP");
            } else {
                Serial.println("Network: Static IP configured");
            }
        }
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
             configureStaticIP(); // IP anwenden bevor die Verbindung startet
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
            
            client.setBufferSize(4096); 
            client.setKeepAlive(30); 
            mqttInitialized = true;
        }
    }

    void setupOTA() {
        ArduinoOTA.setHostname("Wortuhr-Matrix-OS");
        ArduinoOTA.setPassword(ota_password);

        ArduinoOTA.onStart([this]() { 
            displayRef.setFade(1.0);
            displayRef.clear();
            displayRef.setTextColor(displayRef.color565(255, 255, 0)); 
            displayRef.printCentered("UPDATE", 32);
            displayRef.show(); 
        });

        ArduinoOTA.onEnd([this]() { 
            displayRef.clear();
            displayRef.setTextColor(displayRef.color565(0, 255, 0)); 
            displayRef.printCentered("SUCCESS!", 32);
            displayRef.show();
            delay(2000); 
        });

        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            int p = (progress / (total / 100));
            if (p % 2 == 0) {
                displayRef.clear();
                displayRef.setTextColor(displayRef.color565(255, 255, 0));
                displayRef.printCentered("UPDATING...", 17);
                int w = map(p, 0, 100, 0, 100);
                displayRef.drawRect(14, 28, 102, 12, displayRef.color565(100, 100, 100)); 
                displayRef.fillRect(15, 29, w, 10, displayRef.color565(0, 255, 0));       
                String s = String(p) + "%";
                displayRef.printCentered(s, 57);
                displayRef.show();
            }
        });

        ArduinoOTA.onError([this](ota_error_t error) { 
            displayRef.clear();
            displayRef.setTextColor(displayRef.color565(255, 0, 0)); 
            displayRef.printCentered("OTA ERROR!", 25);
            displayRef.show();
            delay(3000); 
        });

        ArduinoOTA.begin();
    }
    
    void checkTimeSync() {
        if (!timeInitialized || timeSynced) return; 
        
        time_t now = time(nullptr);
        if (now > 1600000000) { 
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
                configureStaticIP(); // Beim Reconnect die IP Konfig wieder anwenden
                WiFi.begin(ssid, password); 
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
                bool serverReachable = false;
                {
                    WiFiClient testClient;
                    if (testClient.connect(mqtt_server, mqtt_port, 200)) {
                        serverReachable = true;
                        testClient.stop();
                    }
                }

                if (serverReachable) {
                    if (client.connect("MatrixPortalS3", mqtt_user, mqtt_pass, "matrix/status", 0, true, "OFF")) {
                        client.subscribe("matrix/cmd/#");
                        publishState();
                        Serial.println("MQTT: Connected");
                    }
                } else {
                    Serial.println("MQTT: Server not reachable (TCP Fail) - Skipping blocking connect");
                }
                lastMqttRetry = millis(); 
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