#pragma once
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

extern WiFiClient espClient;
extern PubSubClient client;

void publishState() {
    if (brightness > 0) client.publish("matrix/status", "ON", true);
    else client.publish("matrix/status", "OFF", true);
    client.publish("matrix/status/brightness", String(brightness).c_str(), true);
    String appStr = (currentApp == WORDCLOCK) ? "wordclock" : (currentApp == SENSORS) ? "sensors" : (currentApp == TESTPATTERN) ? "testpattern" : "off";
    client.publish("matrix/status/app", appStr.c_str(), true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String t = String(topic);
    String msg = "";
    for (int i = 0; i < length; i++) msg += (char)payload[i];
    if (t == "matrix/cmd/power") {
        if (msg == "OFF") brightness = 0;
        else if (msg == "ON" && brightness == 0) brightness = 150;
        publishState();
    }
    if (t == "matrix/cmd/brightness") {
        StaticJsonDocument<128> doc;
        if (!deserializeJson(doc, payload, length) && doc.containsKey("val")) {
            brightness = doc["val"];
            publishState();
        }
    }
    if (t == "matrix/cmd/app") {
        StaticJsonDocument<128> doc;
        if (!deserializeJson(doc, payload, length) && doc.containsKey("app")) {
            String newApp = doc["app"];
            if (newApp == "wordclock") currentApp = WORDCLOCK;
            else if (newApp == "sensors") currentApp = SENSORS;
            else if (newApp == "testpattern") currentApp = TESTPATTERN;
            else if (newApp == "off") currentApp = OFF;
            publishState(); 
        }
    }
    if (t == "matrix/cmd/overlay") {
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, payload, length)) {
            overlayMsg = doc["msg"].as<String>();
            overlayActive = true;
            overlayTimer = millis() + (doc["sec"].as<int>() * 1000);
        }
    }
}

void initNetwork() {
    WiFi.begin(ssid, password);
    int r = 0;
    while (WiFi.status() != WL_CONNECTED && r < 20) { delay(500); r++; }
    ArduinoOTA.setHostname("Wortuhr-Matrix-OS");
    ArduinoOTA.setPassword(ota_password);
    ArduinoOTA.begin();
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);
}

void networkLoop() {
    if (!client.connected()) {
        if (client.connect("MatrixPortalS3", mqtt_user, mqtt_pass, "matrix/status", 0, true, "OFF")) {
            client.subscribe("matrix/cmd/#");
            publishState();
        }
    }
    client.loop();
}