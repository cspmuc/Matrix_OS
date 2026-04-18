#pragma once
#include <WiFi.h>
#include <ESPmDNS.h>      
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ConfigManager.h" 
#include "DisplayManager.h"
#include "SensorApp.h" 
#include <time.h> 
#include <esp_heap_caps.h> 
#include "WeatherApp.h" 

extern void status(const String& msg, uint16_t color);
extern void queueOverlay(String msg, int durationSec, String colorName, int scrollSpeed);
extern void forceOverlay(String msg, int durationSec, String colorName);
extern void queueAnimation(OverlayType animType, int durationSec); 
extern WeatherApp weatherApp;

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
    ConfigManager& conf; 

    static MatrixNetworkManager* instance;
    
    bool otaInitialized = false;
    bool timeInitialized = false;
    bool timeSynced = false;
    bool mqttInitialized = false;
    
    unsigned long lastWifiCheck = 0;
    unsigned long lastMqttRetry = 0;
    unsigned long lastTimeCheck = 0;
    int lastSavedBrightness = 150; 

    void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
        String t = String(topic);
        
        SpiRamJsonDocument* doc = new SpiRamJsonDocument(8192);
        DeserializationError error = deserializeJson(*doc, payload, length);
        
        if (error) {
             String msg = "";
             for (int i = 0; i < length; i++) msg += (char)payload[i];
             if (t == "matrix/cmd/power") {
                if (msg == "OFF") {
                    if (brightnessRef > 0) lastSavedBrightness = brightnessRef; 
                    brightnessRef = 0;
                }
                else if (msg == "ON" && brightnessRef == 0) {
                    brightnessRef = (lastSavedBrightness > 0) ? lastSavedBrightness : conf.system.startup_brightness;
                }
                publishState();
             }
             delete doc;
             return;
        }
        
        if (t == "matrix/cmd/brightness" && doc->containsKey("val")) {
            brightnessRef = (*doc)["val"].as<int>();
            if (brightnessRef > 0) lastSavedBrightness = brightnessRef; 
            publishState();
        }

        if (t == "matrix/cmd/app" && doc->containsKey("app")) {
            String newApp = (*doc)["app"];
            if (newApp == "wordclock") currentAppRef = WORDCLOCK;
            else if (newApp == "sensors") currentAppRef = SENSORS;
            else if (newApp == "testpattern") currentAppRef = TESTPATTERN;
            else if (newApp == "ticker") currentAppRef = TICKER;
            else if (newApp == "plasma") currentAppRef = PLASMA;
            else if (newApp == "weather") currentAppRef = WEATHER; 
            else if (newApp == "pong") currentAppRef = PONG;   
            else if (newApp == "off") currentAppRef = OFF;
            else if (newApp == "auto") currentAppRef = AUTO; 
            publishState(); 
            
            // --- NEU: Overlay beim App-Wechsel (10 Sekunden) ---
            queueOverlay("Modus: " + newApp, 10, "cyan", 30);
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
        if (t == "matrix/cmd/animation" && doc->containsKey("anim")) {
            String anim = (*doc)["anim"];
            int dur = (*doc)["duration"] | 3; 
            if (anim == "ghost_eyes") {
                queueAnimation(OVL_ANIM_GHOST, dur);
            }
        }
        if (t == "matrix/cmd/sensor_page") {
            String id = (*doc)["id"] | "default";
            String title = (*doc)["title"] | "INFO";
            int ttl = (*doc)["ttl"] | 60; 
            int prio = (*doc)["priority"] | 3; 
            
            std::vector<SensorItem> items;
            JsonArray jsonItems = (*doc)["items"].as<JsonArray>();
            for (JsonObject item : jsonItems) {
                SensorItem si;
                si.icon = item["icon"] | "";
                si.text = item["text"] | "--"; 
                si.color = item["color"] | "white";
                items.push_back(si);
            }
            if (!items.empty()) sensorAppRef.updatePage(id, title, ttl, prio, items);
        }
        if (t == "matrix/data/weather") { 
            weatherApp.updateData(doc);
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
            
            if (MDNS.begin(conf.network.hostname.c_str())) {
                Serial.print("mDNS responder started: ");
                Serial.print(conf.network.hostname);
                Serial.println(".local");
            }
            
            otaInitialized = true;
        }
        if (!timeInitialized) {
            configTzTime(conf.time.timezone.c_str(), conf.time.ntp_server.c_str());
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
            displayRef.setAppFade(1.0);
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
            static int lastP = -1; 
            
            if (p != lastP) {
                lastP = p;
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
            // --- GEÄNDERT: Die Time-Synced-Meldung erscheint nun lautlos im Hintergrund ---
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
                        client.subscribe("matrix/data/#"); 
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
            case WEATHER:     appStr = "weather"; break; 
            case PONG:        appStr = "pong"; break;   
            case AUTO:        appStr = "auto"; break; 
            default:          appStr = "off"; break;
        }
        client.publish("matrix/status/app", appStr.c_str(), true);
    }
};

MatrixNetworkManager* MatrixNetworkManager::instance = nullptr;