#pragma once
#include <Arduino.h>
#include "secrets.h"
#include <FS.h>
#include <LittleFS.h>

const char* ssid     = CONFIG_WIFISSID;
const char* password = SECRET_PASS;

const char* mqtt_server   = CONFIG_MQTTSERVER;
const int   mqtt_port     = CONFIG_MQTTPORT;
const char* mqtt_user     = SECRET_MQTTUSER;
const char* mqtt_pass     = SECRET_MQTTPASS;

const char* ota_password  = SECRET_OTAPASS;

// --- ZEIT ---
const char* ntpServer = CONFIG_NTPSERVER;
const long  gmtOffset_sec = CONFIG_GMTOFFSETSEC;
const int   daylightOffset_sec = CONFIG_DAYLIGHTOFFSET;

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

extern AppMode currentApp;
extern int brightness;