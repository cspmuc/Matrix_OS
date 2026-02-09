#pragma once
#include <Arduino.h>
#include "secrets.h"
#include <FS.h>
#include <LittleFS.h>

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

// --- DISPLAY ---
#define M_WIDTH  128   
#define M_HEIGHT 64
#define GAMMA_VALUE 2.2 

// Pin Definitionen (ESP32-S3 Matrix Portal)
#define R1_PIN 42
#define G1_PIN 41
#define B1_PIN 40
#define R2_PIN 38
#define G2_PIN 39
#define B2_PIN 37
#define A_PIN 45
#define B_PIN 36
#define C_PIN 48
#define D_PIN 35
#define E_PIN 21
#define CLK_PIN 2
#define LAT_PIN 47
#define OE_PIN  14

// App Definitionen
enum AppMode { WORDCLOCK, SENSORS, TESTPATTERN, TICKER, PLASMA, OFF };

// CLEANUP: Keine Atomics mehr nötig
extern AppMode currentApp;
extern int brightness;