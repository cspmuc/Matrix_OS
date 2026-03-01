#pragma once
#include <WiFi.h>
#include <ESPmDNS.h>      // <--- NEU: Die mDNS Bibliothek
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ConfigManager.h" // <--- NEU: Einbinden des ConfigManagers
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
    ConfigManager& conf; // <--- NEU: Referenz auf unsere Konfiguration

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
        if (conf.network.use_static_ip) {
            IPAddress ip, gateway, subnet, dns;
            ip.fromString(conf.network.static_ip);
            gateway.fromString(conf.network.static_gateway);
            subnet.fromString(conf.network.static_subnet);
            dns.fromString(conf.network.static_dns);
            
            if (!WiFi.config(ip, gateway, subnet, dns)) {
                Serial.println("Network: Failed to configure Static IP");
            } else {
                Serial.println("Network: Static IP configured");
            }
        }
    }

public:
    MatrixNetworkManager(AppMode& app, int& bright, DisplayManager& disp, SensorApp& sensors, ConfigManager& config) 
        : client(espClient), currentAppRef(app), brightnessRef(bright), displayRef(disp), sensorAppRef(sensors), conf(config) {
        instance = this;
    }

    String getIp() { return WiFi.localIP().toString(); }
    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    bool isTimeSynced() { return timeSynced; }

    bool begin() {
        WiFi.setSleep(false); 
        
        if (WiFi.status() == WL_CONNECTED) return true;
        if (WiFi.status() != WL_CONNECTED) {
             
             WiFi.disconnect(true, true); 
             delay(100);
             
             WiFi.mode(WIFI_STA);
             
             // --- NEU: DHCP Hostname setzen (für den Router) ---
             WiFi.setHostname(conf.network.hostname.c_str()); 
             
             configureStaticIP();
             WiFi.begin(conf.network.wifi_ssid.c_str(), conf.network.wifi_pass.c_str());
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
            
            // --- NEU: mDNS Responder starten ---
            if (MDNS.begin(conf.network.hostname.c_str())) {
                Serial.print("mDNS responder started: ");
                Serial.print(conf.network.hostname);
                Serial.println(".local");
            }
            
            otaInitialized = true;
        }
        if (!timeInitialized) {
            configTime(conf.time.gmt_offset_sec, conf.time.daylight_offset_sec, conf.time.ntp_server.c_str());
            timeInitialized = true; 
        }
        if (!mqttInitialized) { 
            client.setServer(conf.mqtt.server.c_str(), conf.mqtt.port);
            client.setCallback(mqttCallbackTrampoline);
            
            client.setBufferSize(4096); 
            client.setKeepAlive(30); 
            mqttInitialized = true;
        }
    }

    void setupOTA() {
        ArduinoOTA.setHostname(conf.network.hostname.c_str());
        ArduinoOTA.setPassword(conf.system.ota_password.c_str());

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
                
                // Beim Reconnect wieder Hostname und ggf. IP setzen
                WiFi.setHostname(conf.network.hostname.c_str()); 
                configureStaticIP(); 
                WiFi.begin(conf.network.wifi_ssid.c_str(), conf.network.wifi_pass.c_str()); 
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
                    if (testClient.connect(conf.mqtt.server.c_str(), conf.mqtt.port, 200)) {
                        serverReachable = true;
                        testClient.stop();
                    }
                }

                if (serverReachable) {
                    if (client.connect(conf.network.hostname.c_str(), conf.mqtt.user.c_str(), conf.mqtt.pass.c_str(), "matrix/status", 0, true, "OFF")) {
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