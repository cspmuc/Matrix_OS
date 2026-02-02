#include <ArduinoJson.h>
#include <math.h>
#include <vector>
#include <atomic> 
#include "config.h"

// Includes
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "WordClockApp.h"
#include "TestPatternApp.h"
#include "SensorApp.h"
#include "TickerApp.h"
#include "PlasmaApp.h" // NEU: Plasma App Include

// Globale Steuerung (Thread-Sicher!)
std::atomic<AppMode> currentApp(WORDCLOCK);
std::atomic<int> brightness(150); 

// Objekte instanziieren
DisplayManager display;

WordClockApp appWordClock;
TestPatternApp appTestPattern;
SensorApp appSensors;
TickerApp appTicker;
PlasmaApp appPlasma; // NEU: Plasma App Instanz

MatrixNetworkManager network(currentApp, brightness, display, appSensors);

SemaphoreHandle_t overlayMutex; 
volatile bool isBooting = true;

struct BootLogEntry {
    String text;
    uint16_t color;
};

std::vector<BootLogEntry> bootLogs; 
int bootLogCounter = 1;

volatile bool otaActive = false;
volatile int otaProgress = 0;

volatile bool overlayActive = false;
String overlayMsg = "";
unsigned long overlayTimer = 0;

// Konstanten für den Loop
const int frameDelay = 16; 
const int fadeDurationMs = 500;
const float fadeStep = 1.0 / ((float)fadeDurationMs / (float)frameDelay);

// Status Funktion (Speicherschonend)
void status(const String& msg, uint16_t color = 0xFFFF) {
  if (overlayMutex && xSemaphoreTake(overlayMutex, portMAX_DELAY) == pdTRUE) {
    if (isBooting) {
      char buf[64];
      sprintf(buf, "%02d %s", bootLogCounter++, msg.c_str());
      bootLogs.push_back({String(buf), color});
      if (bootLogs.size() > 8) bootLogs.erase(bootLogs.begin());
    } else {
      overlayMsg = msg;
      overlayActive = true;
      overlayTimer = millis() + 4000;
    }
    xSemaphoreGive(overlayMutex);
  }
}

void drawBootLog() {
  display.setTextSize(1);
  display.setFont(NULL); 
  
  int y = 0;
  for (const auto& entry : bootLogs) {
    display.setTextColor(display.color565(100, 100, 100)); 
    display.setCursor(2, y); 
    display.print(entry.text.substring(0, 3));

    display.setTextColor(entry.color);
    display.setCursor(20, y);
    display.print(entry.text.substring(3));
    
    y += 8; 
  }
}

void drawOTA(int progress) {
  display.setTextColor(display.color565(255, 255, 0));
  display.printCentered("SYSTEM UPDATE", 15);
  display.drawRect(14, 35, 100, 12, display.color565(100, 100, 100));
  int w = map(progress, 0, 100, 0, 96);
  display.fillRect(16, 37, w, 8, display.color565(0, 255, 0));
  String p = String(progress) + "%";
  display.printCentered(p, 50);
}

void drawOverlay(String text) {
  display.fillRect(0, 18, M_WIDTH, 28, display.color565(0, 0, 100)); 
  display.drawRect(0, 18, M_WIDTH, 28, display.color565(255, 255, 255)); 
  display.setTextColor(display.color565(255, 255, 255));
  display.printCentered(text, 28);
}

TaskHandle_t NetworkTask;
TaskHandle_t DisplayTask;

void networkTaskFunction(void * pvParameters) {
  
  // --- STRICT BOOT SEQUENCE ---
  while (true) {
      
      // 1. WLAN VERBINDUNG
      status("Connect WiFi...", display.color565(255, 255, 255));
      
      while(!network.isConnected()) {
          if (network.begin()) {
             String ip = network.getIp();
             status("IP: " + ip, display.color565(0, 255, 0));
             delay(2500); 
             break; 
          } else {
             status("WiFi Retry 5s...", display.color565(255, 0, 0));
             delay(5000); 
          }
      }

      // 2. ZEIT SYNCHRONISATION
      status("Wait for Time...", display.color565(255, 165, 0)); 
      
      network.tryInitServices(); 
      
      bool timeSuccess = false;
      while(!timeSuccess) {
          network.loop(); 
          
          if (!network.isConnected()) {
              status("WiFi Lost!", display.color565(255, 0, 0));
              delay(1000);
              break; // Zurück zu Schritt 1
          }

          if (network.isTimeSynced()) {
              timeSuccess = true;
          }
          
          delay(100); 
      }

      if (timeSuccess) {
          break; // Alles OK
      }
  }

  // --- FINISH ---
  status("Start in 3s", display.color565(255, 255, 255)); 

  delay(3000);

  if (xSemaphoreTake(overlayMutex, portMAX_DELAY) == pdTRUE) {
    isBooting = false;
    xSemaphoreGive(overlayMutex);
  }
  
  for(;;) {
    network.loop();
    vTaskDelay((otaActive ? 1 : 10) / portTICK_PERIOD_MS);
  }
}

void displayTaskFunction(void * pvParameters) {
  // Lokale Kopie vom atomaren Wert
  AppMode displayedApp = currentApp.load();
  float fadeVal = 1.0;

  // Präzises Timing Setup
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(frameDelay); // 16ms
  
  for(;;) {
      unsigned long now = millis();
      
      // Atomaren Wert laden
      int currentBright = brightness.load();
      display.setBrightness(currentBright);
      
      AppMode targetApp = currentApp.load();

      if (displayedApp != targetApp) {
          fadeVal -= fadeStep; 
          if (fadeVal <= 0.0) {
              fadeVal = 0.0;
              displayedApp = targetApp; 
          }
      } else {
          if (fadeVal < 1.0) {
              fadeVal += fadeStep;
              if (fadeVal > 1.0) fadeVal = 1.0;
          }
      }
      display.setFade(fadeVal);

      bool localBooting = false;
      bool localOta = false;
      int localOtaProg = 0;

      if (xSemaphoreTake(overlayMutex, 5 / portTICK_PERIOD_MS) == pdTRUE) {
        localBooting = isBooting;
        localOta = otaActive;
        localOtaProg = otaProgress;
        
        display.clear(); 

        if (localOta) {
           display.setFade(1.0); 
           drawOTA(localOtaProg);
           xSemaphoreGive(overlayMutex);
        }
        else if (localBooting) {
           display.setFade(1.0);
           drawBootLog(); 
           xSemaphoreGive(overlayMutex); 
        } 
        else {
           xSemaphoreGive(overlayMutex); 
           
           if (currentBright > 0) {
             switch(displayedApp) {
               case WORDCLOCK:   appWordClock.draw(display);   break;
               case SENSORS:     appSensors.draw(display);     break;
               case TESTPATTERN: appTestPattern.draw(display); break;
               case TICKER:      appTicker.draw(display);      break;
               case PLASMA:      appPlasma.draw(display);      break; // NEU: Plasma Case
               case OFF:         display.clear(); break;
             }
             
             if (xSemaphoreTake(overlayMutex, 5 / portTICK_PERIOD_MS) == pdTRUE) {
                if (overlayActive) {
                  display.setFade(1.0); 
                  if (now > overlayTimer) overlayActive = false;
                  else drawOverlay(overlayMsg);
                }
                xSemaphoreGive(overlayMutex);
             }
           }
        }
      } 
      
      display.show();
      
      // Präzises Warten
      vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void setup() {
  Serial.begin(115200);
  overlayMutex = xSemaphoreCreateMutex();

  if (!display.begin()) {
    Serial.println("Display Init Failed!");
    while(1);
  }
  
  status("Boot..."); 

  xTaskCreatePinnedToCore(networkTaskFunction, "NetworkTask", 10000, NULL, 0, &NetworkTask, 0);
  xTaskCreatePinnedToCore(displayTaskFunction, "DisplayTask", 10000, NULL, 10, &DisplayTask, 1);
}

void loop() {
  vTaskDelay(10000 / portTICK_PERIOD_MS);
}