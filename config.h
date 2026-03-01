#pragma once
#include <Arduino.h>

// --- DISPLAY HARDWARE ---
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

// --- APP DEFINITIONEN ---
enum AppMode { WORDCLOCK, SENSORS, TESTPATTERN, TICKER, PLASMA, OFF };

extern AppMode currentApp;
extern int brightness;