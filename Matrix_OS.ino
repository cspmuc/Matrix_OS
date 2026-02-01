#include <Adafruit_Protomatter.h>
#include <WiFi.h>              
#include <PubSubClient.h>      
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <math.h>
#include "config.h"

// Globale Variablen
AppMode currentApp = WORDCLOCK;
int brightness = 150;
uint8_t gammaTable[256];
bool overlayActive = false;
String overlayMsg = "";
unsigned long overlayTimer = 0;
const char* stundenNamen[] = {"Zwoelf", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf", "Zwoelf"};

uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
uint8_t addrPins[] = {45, 36, 48, 35, 21};
uint8_t clockPin = 2, latchPin = 47, oePin = 14;

Adafruit_Protomatter matrix(M_WIDTH, 3, 1, rgbPins, 5, addrPins, clockPin, latchPin, oePin, true);

unsigned long lastDrawTime = 0;
const int frameDelay = 33; 

WiFiClient espClient;
PubSubClient client(espClient);

// Status-Funktion VOR den Includes definieren, damit sie überall bekannt ist
void status(String msg, uint16_t color = 0xFFFF) {
  static int bootLine = 0;
  matrix.setCursor(2, (bootLine % 8) * 8 + 2);
  matrix.setTextColor(color);
  matrix.println(msg);
  matrix.show();
  bootLine++;
}

#include "network_logic.h"
#include "apps.h"

TaskHandle_t NetworkTask;
TaskHandle_t DisplayTask;

void networkTaskFunction(void * pvParameters) {
  initNetwork(); // Core 0 Initialisierung für OTA Stabilität
  for(;;) {
    networkLoop();
    ArduinoOTA.handle(); 
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void displayTaskFunction(void * pvParameters) {
  for(;;) {
    unsigned long now = millis();
    if (now - lastDrawTime >= frameDelay) {
      lastDrawTime = now;
      if (brightness > 0 && currentApp != OFF) {
        switch(currentApp) {
          case WORDCLOCK:   drawWordClock();   break;
          case SENSORS:     drawSensors();     break;
          case TESTPATTERN: drawTestPattern(); break;
          case OFF:         matrix.fillScreen(0); break;
        }
        if(overlayActive) {
          if (now > overlayTimer) overlayActive = false;
          else drawOverlay();
        }
      } else {
        matrix.fillScreen(0);
      }
      matrix.show();
    }
    vTaskDelay(1 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240);
  for (int i = 0; i < 256; i++) {
    if (i == 0) gammaTable[i] = 0;
    else {
      float scaled = pow((float)i / 255.0, GAMMA_VALUE);
      gammaTable[i] = (uint8_t)(12 + scaled * (255.0 - 12) + 0.5);
    }
  }
  if (matrix.begin() != PROTOMATTER_OK) for(;;);
  matrix.fillScreen(0);
  status("BOOTING Matrix OS...", matrix.color565(0, 255, 255));
  
  xTaskCreatePinnedToCore(networkTaskFunction, "NetworkTask", 10000, NULL, 1, &NetworkTask, 0);
  xTaskCreatePinnedToCore(displayTaskFunction, "DisplayTask", 10000, NULL, 24, &DisplayTask, 1);
}

void loop() {
  vTaskDelay(10000 / portTICK_PERIOD_MS);
}