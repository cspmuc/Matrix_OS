#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

#ifndef SPIRAM_ALLOCATOR_DEFINED
#define SPIRAM_ALLOCATOR_DEFINED
struct SpiRamAllocator {
  void* allocate(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }
  void deallocate(void* pointer) { heap_caps_free(pointer); }
  void* reallocate(void* ptr, size_t new_size) { return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM); }
};
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;
#endif

// --- 1. Die Datenstrukturen mit sicheren Standardwerten ---
struct NetworkConfig {
    String hostname = "MatrixOS";
    String wifi_ssid = "DEIN_WLAN";
    String wifi_pass = "DEIN_PASSWORT";
    bool use_static_ip = false;
    String static_ip = "192.168.178.100";
    String static_subnet = "255.255.255.0";
    String static_gateway = "192.168.178.1";
    String static_dns = "192.168.178.1";
};

struct MqttConfig {
    String server = "192.168.178.50";
    int port = 1883;
    String user = "";
    String pass = "";
};

struct TimeConfig {
    String ntp_server = "pool.ntp.org";
    long gmt_offset_sec = 3600;
    int daylight_offset_sec = 3600;
};

struct SystemConfig {
    String ota_password = "admin";
};

// --- 2. Die Manager Klasse ---
class ConfigManager {
public:
    NetworkConfig network;
    MqttConfig mqtt;
    TimeConfig time;
    SystemConfig system;

    void begin() {
        if (!LittleFS.exists("/config.json")) {
            Serial.println("ConfigManager: /config.json existiert nicht. Nutze Standardwerte.");
            return;
        }

        File file = LittleFS.open("/config.json", "r");
        if (!file) {
            Serial.println("ConfigManager: Fehler beim Öffnen der /config.json.");
            return;
        }

        // Wir nutzen unseren bewährten PSRAM Allocator, damit der RAM geschont wird!
        SpiRamJsonDocument* doc = new SpiRamJsonDocument(4096);
        DeserializationError error = deserializeJson(*doc, file);
        file.close();

        if (error) {
            Serial.print("ConfigManager: JSON Parsing fehlgeschlagen: ");
            Serial.println(error.c_str());
            delete doc;
            return;
        }

        // --- 3. Werte überschreiben (Fallback-Logik durch den '|' Operator) ---
        
        if (doc->containsKey("network")) {
            JsonObject net = (*doc)["network"];
            network.hostname       = net["hostname"] | network.hostname;
            network.wifi_ssid      = net["wifi_ssid"] | network.wifi_ssid;
            network.wifi_pass      = net["wifi_pass"] | network.wifi_pass;
            network.use_static_ip  = net["use_static_ip"] | network.use_static_ip;
            network.static_ip      = net["static_ip"] | network.static_ip;
            network.static_subnet  = net["static_subnet"] | network.static_subnet;
            network.static_gateway = net["static_gateway"] | network.static_gateway;
            network.static_dns     = net["static_dns"] | network.static_dns;
        }

        if (doc->containsKey("mqtt")) {
            JsonObject m = (*doc)["mqtt"];
            mqtt.server = m["server"] | mqtt.server;
            mqtt.port   = m["port"] | mqtt.port;
            mqtt.user   = m["user"] | mqtt.user;
            mqtt.pass   = m["pass"] | mqtt.pass;
        }

        if (doc->containsKey("time")) {
            JsonObject t = (*doc)["time"];
            time.ntp_server          = t["ntp_server"] | time.ntp_server;
            time.gmt_offset_sec      = t["gmt_offset_sec"] | time.gmt_offset_sec;
            time.daylight_offset_sec = t["daylight_offset_sec"] | time.daylight_offset_sec;
        }

        if (doc->containsKey("system")) {
            JsonObject sys = (*doc)["system"];
            system.ota_password = sys["ota_password"] | system.ota_password;
        }

        delete doc; // JSON im PSRAM sofort wieder zerstören
        Serial.println("ConfigManager: config.json erfolgreich in den RAM geladen.");
    }
};