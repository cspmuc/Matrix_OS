#pragma once
#include <Arduino.h>
#include "secrets.h"

// --- WLAN & MQTT ---
const char* ssid     = "FBSTA";
const char* password = SECRET_PASS;
const char* mqtt_server   = "192.168.137.9"; // Home Assistant IP
const int   mqtt_port     = 1883;
const char* mqtt_user     = "mqttusr"; // Falls vorhanden
const char* mqtt_pass     = SECRET_MQTTPASS;
const char* ota_password  = SECRET_OTAPASS;

// --- ZEIT ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// --- DISPLAY ---
#define M_WIDTH  64   
#define M_HEIGHT 64
// --- Optik ---
#define GAMMA_VALUE 2.5  // Höherer Wert = dunklere Mitteltöne, 1.0 = Linear

enum AppMode { WORDCLOCK, SENSORS, OFF };

// Diese Variablen werden in Matrix_OS.ino definiert
extern AppMode currentApp;
extern int brightness; // 0 - 255
extern bool overlayActive;
extern String overlayMsg;
extern unsigned long overlayTimer;
extern const char* stundenNamen[];