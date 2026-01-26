#pragma once
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

extern WiFiClient espClient;
extern PubSubClient client;

// Prototyp für Status-Anzeige auf dem Display
void status(String msg, uint16_t color = 0xFFFF);

// Sendet den aktuellen Zustand an Home Assistant
void publishState() {
    // 1. Der Power-Status (ON oder OFF)
    if (brightness > 0) {
        client.publish("matrix/status", "ON", true);
    } else {
        client.publish("matrix/status", "OFF", true);
    }

    // 2. Die Helligkeit als nackte Zahl (0-255)
    client.publish("matrix/status/brightness", String(brightness).c_str(), true);

    // 3. Die aktuelle App
    String appStr = (currentApp == WORDCLOCK) ? "wordclock" : "sensors";
    client.publish("matrix/status/app", appStr.c_str(), true);
}

// Verarbeitet eingehende MQTT-Befehle
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String t = String(topic);
    String msg = "";
    for (int i = 0; i < length; i++) msg += (char)payload[i];

    // FALL A: Power-Befehl (Klartext)
    if (t == "matrix/cmd/power") {
        if (msg == "OFF") {
            brightness = 0;
        } else if (msg == "ON" && brightness == 0) {
            brightness = 150; // Standardwert beim Einschalten
        }
        publishState();
    }

    // FALL B: Helligkeits-Befehl (JSON)
    if (t == "matrix/cmd/brightness") {
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error && doc.containsKey("val")) {
            brightness = doc["val"];
            publishState();
        }
    }

    // FALL C: App-Wechsel (JSON)
    if (t == "matrix/cmd/app") {
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error && doc.containsKey("app")) {
            String newApp = doc["app"];
            if (newApp == "wordclock") currentApp = WORDCLOCK;
            else if (newApp == "sensors") currentApp = SENSORS;
            else if (newApp == "off") currentApp = OFF;
            publishState();
        }
    }
}

void initNetwork() {
    WiFi.begin(ssid, password);
    int r = 0;
    while (WiFi.status() != WL_CONNECTED && r < 20) {
        delay(500);
        r++;
    }
    
    if(WiFi.status() == WL_CONNECTED) {
        status("WiFi OK", 0x07E0);
    }

    ArduinoOTA.setHostname("Wortuhr-Matrix-OS");
    ArduinoOTA.setPassword(ota_password);
    ArduinoOTA.begin();

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);
}

void networkLoop() {
    if (!client.connected()) {
        // Last Will: Wenn die Uhr offline geht, meldet der Broker "OFF"
        if (client.connect("MatrixPortalS3", mqtt_user, mqtt_pass, "matrix/status", 0, true, "OFF")) {
            client.subscribe("matrix/cmd/#");
            publishState(); // Status beim Verbinden melden
        }
    }
    client.loop();
    ArduinoOTA.handle();
}
