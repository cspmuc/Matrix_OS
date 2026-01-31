#include <Adafruit_Protomatter.h>
#include <WiFi.h>              
#include <PubSubClient.h>      
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <math.h>
#include "config.h"

// 1. Definition der globalen Variablen (Nur hier!)
AppMode currentApp = WORDCLOCK;
int brightness = 150;
uint8_t gammaTable[256];
bool overlayActive = false;
String overlayMsg = "";
unsigned long overlayTimer = 0;
const char* stundenNamen[] = {"Zwoelf", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf", "Zwoelf"};

// 2. Hardware-Pins
uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
uint8_t addrPins[] = {45, 36, 48, 35, 21};
uint8_t clockPin = 2, latchPin = 47, oePin = 14;

Adafruit_Protomatter matrix(M_WIDTH, 3, 1, rgbPins, 5, addrPins, clockPin, latchPin, oePin, true);

unsigned long lastDrawTime = 0;
const int targetFPS = 25; // 25 Bilder pro Sekunde reichen
const int frameDelay = 1000 / targetFPS;
WiFiClient espClient;
PubSubClient client(espClient);

// 3. Header laden (Jetzt oben!)
#include "network_logic.h"
#include "apps.h"

int bootLine = 0;
void status(String msg, uint16_t color) {
  matrix.setCursor(2, bootLine * 8 + 2);
  matrix.setTextColor(color);
  matrix.println(msg);
  matrix.show();
  bootLine++;
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240); // Maximale CPU-Power erzwingen

for (int i = 0; i < 256; i++) {
    if (i == 0) {
      gammaTable[i] = 0;
    } else {
      // Offset von 12 sorgt dafür, dass die LEDs auch bei Slider 1% noch "zünden"
      float scaled = pow((float)i / 255.0, GAMMA_VALUE);
      gammaTable[i] = (uint8_t)(12 + scaled * (255.0 - 12) + 0.5);
    }
  }

  if (matrix.begin() != PROTOMATTER_OK) for(;;);
  matrix.fillScreen(0);
  status("BOOTING...", matrix.color565(0, 255, 255));
  initNetwork();
  delay(1000);
  matrix.fillScreen(0);
}

void loop() {
  networkLoop();
  
  static unsigned long lastFrame = 0;
  if (millis() - lastFrame > 100) {
    if (brightness > 0 && currentApp != OFF) {
      switch(currentApp) {
        case WORDCLOCK:   drawWordClock();   break;
        case SENSORS:     drawSensors();     break;
        case TESTPATTERN: drawTestPattern(); break; // NEU
        case OFF:         matrix.fillScreen(0); break;
      }
      if(overlayActive) drawOverlay();
    } else {
      matrix.fillScreen(0);
    }
    matrix.show();
    lastFrame = millis();
  }

  if(overlayActive && millis() > overlayTimer) overlayActive = false;
}