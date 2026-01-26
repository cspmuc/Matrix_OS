#include <Adafruit_Protomatter.h>
#include <WiFi.h>              
#include <PubSubClient.h>      
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "config.h"
#include <math.h> // Für die pow() Funktion nötig

// --- Hardware-Pins ---
uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
uint8_t addrPins[] = {45, 36, 48, 35, 21};
uint8_t clockPin = 2, latchPin = 47, oePin = 14;

uint8_t gammaTable[256]; // Speicherplatz für die berechneten Werte

Adafruit_Protomatter matrix(M_WIDTH, 4, 1, rgbPins, 5, addrPins, clockPin, latchPin, oePin, true);

// --- Globale Variablen ---
AppMode currentApp = WORDCLOCK;
int brightness = 150; // Startwert (0-255)
bool overlayActive = false;
String overlayMsg = "";
unsigned long overlayTimer = 0;
int bootLine = 0;
const char* stundenNamen[] = {"Zwoelf", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf", "Zwoelf"};

WiFiClient espClient;
PubSubClient client(espClient);

// Logik-Header laden
#include "network_logic.h"
#include "apps.h"

// Definition ohne Standardwert (da dieser im Header steht)
void status(String msg, uint16_t color) {
  matrix.setCursor(2, bootLine * 8 + 2);
  matrix.setTextColor(color);
  matrix.println(msg);
  matrix.show();
  bootLine++;
  if(bootLine > 7) { delay(1000); matrix.fillScreen(0); bootLine = 0; }
}

void setup() {
    Serial.begin(115200);

    // Gamma-Tabelle einmalig berechnen
    for (int i = 0; i < 256; i++) {
        gammaTable[i] = (uint8_t)(pow((float)i / 255.0, GAMMA_VALUE) * 255.0 + 0.5);
    }

    if (matrix.begin() != PROTOMATTER_OK) {
        for(;;);
    }
    initNetwork();
}

void loop() {
  networkLoop();
  


  static unsigned long lastFrame = 0;
  if (millis() - lastFrame > 100) {
    matrix.fillScreen(0);

    switch(currentApp) {
      case WORDCLOCK: drawWordClock(); break;
      case SENSORS:   drawSensors();   break;
      case OFF:       break;
    }

    if(overlayActive) drawOverlay();
    
    matrix.show();
    lastFrame = millis();
  }

  if(overlayActive && millis() > overlayTimer) overlayActive = false;
}