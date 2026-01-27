#pragma once
#include <Arduino.h>
#include "secrets.h"

// --- WLAN & MQTT ---
const char* ssid     = "FBSTA";
const char* password = SECRET_PASS;
const char* mqtt_server   = "192.168.137.9";
const int   mqtt_port     = 1883;
const char* mqtt_user     = "mqttusr";
const char* mqtt_pass     = SECRET_MQTTPASS;
const char* ota_password  = SECRET_OTAPASS;

// --- ZEIT ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// --- DISPLAY & OPTIK ---
#define M_WIDTH  64   
#define M_HEIGHT 64
#define GAMMA_VALUE 2.2

// 1. Definition des Typs (Nur hier!)
enum AppMode { WORDCLOCK, SENSORS, OFF };

// 2. Bekanntgabe der Variablen (extern = "existiert woanders")
extern AppMode currentApp;
extern int brightness;
extern uint8_t gammaTable[256];
extern bool overlayActive;
extern String overlayMsg;
extern unsigned long overlayTimer;
extern const char* stundenNamen[];