#include <ArduinoJson.h>
#include <math.h>
#include <vector>
#include <deque>
#include <atomic> 
#include "config.h"

// Includes
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "WordClockApp.h"
#include "TestPatternApp.h"
#include "SensorApp.h"
#include "TickerApp.h"
#include "PlasmaApp.h"
#include "RichText.h"
#include "StorageManager.h" 
#include "WebManager.h"     

// Globale Steuerung
std::atomic<AppMode> currentApp(WORDCLOCK);
std::atomic<int> brightness(150); 

// Objekte
DisplayManager display;
RichText richTextOverlay; 

WordClockApp appWordClock;
TestPatternApp appTestPattern;
SensorApp appSensors;
TickerApp appTicker;
PlasmaApp appPlasma;
StorageManager storage;
WebManager webServer;       

MatrixNetworkManager network(currentApp, brightness, display, appSensors);

SemaphoreHandle_t overlayMutex; 
volatile bool isBooting = true;
// isSystemUploading brauchen wir nicht mehr -> gelöscht

// --- BOOT LOGIC ---
struct BootLogEntry { String text; uint16_t color; };
std::vector<BootLogEntry> bootLogs; 
int bootLogCounter = 1;

// --- OTA STATUS ---
volatile bool otaActive = false;
volatile int otaProgress = 0;

// --- OVERLAY SYSTEM (Queue) ---
struct OverlayMessage {
    String text;
    int durationSec;
    String colorName;
    int scrollSpeed;
};

std::deque<OverlayMessage> overlayQueue;
bool isOverlayActive = false;
unsigned long overlayStartTime = 0;
unsigned long overlayEndTime = 0;
OverlayMessage currentOverlay;

const int frameDelay = 16; 
const int fadeDurationMs = 500;
const float fadeStep = 1.0 / ((float)fadeDurationMs / (float)frameDelay);

// --- FUNKTIONEN ---

void queueOverlay(String msg, int durationSec, String colorName, int scrollSpeed) {
    if (xSemaphoreTake(overlayMutex, portMAX_DELAY) == pdTRUE) {
        if (overlayQueue.size() < 5) {
            overlayQueue.push_back({msg, durationSec, colorName, scrollSpeed});
        }
        xSemaphoreGive(overlayMutex);
    }
}

void forceOverlay(String msg, int durationSec, String colorName) {
    if (xSemaphoreTake(overlayMutex, portMAX_DELAY) == pdTRUE) {
        overlayQueue.clear();
        isOverlayActive = false;
        overlayQueue.push_back({msg, durationSec, colorName, 0});
        xSemaphoreGive(overlayMutex);
    }
}

void status(const String& msg, uint16_t color = 0xFFFF) {
  Serial.print("[STATUS] ");
  Serial.println(msg);

  if (overlayMutex && xSemaphoreTake(overlayMutex, portMAX_DELAY) == pdTRUE) {
    if (isBooting) {
      char buf[64];
      sprintf(buf, "%02d %s", bootLogCounter++, msg.c_str());
      bootLogs.push_back({String(buf), color});
      if (bootLogs.size() > 8) bootLogs.erase(bootLogs.begin());
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

void processAndDrawOverlay(DisplayManager& display) {
    unsigned long now = millis();
    if (xSemaphoreTake(overlayMutex, 0) == pdTRUE) { 
        if (isOverlayActive) {
            if (now > overlayEndTime) {
                isOverlayActive = false;
            }
        }
        if (!isOverlayActive && !overlayQueue.empty()) {
            currentOverlay = overlayQueue.front();
            overlayQueue.pop_front();
            isOverlayActive = true;
            overlayStartTime = now;
            overlayEndTime = now + (currentOverlay.durationSec * 1000);
        }
        xSemaphoreGive(overlayMutex);
    }

    if (isOverlayActive) {
        String finalMsg = "{c:" + currentOverlay.colorName + "}" + currentOverlay.text;
        int textW = richTextOverlay.getTextWidth(display, finalMsg, "Medium");
        int boxH = 34;
        int boxW = 104; 
        bool isScrolling = false;
        if (textW > (boxW - 10)) {
            boxW = M_WIDTH;
            isScrolling = true;
        }

        int boxX = (M_WIDTH - boxW) / 2;
        int boxY = (M_HEIGHT - boxH) / 2;

        display.dimRect(boxX, boxY, boxW, boxH); 
        display.drawRect(boxX, boxY, boxW, boxH, display.color565(80, 80, 80));
        int textY = boxY + (boxH / 2) + 5;
        if (!isScrolling) {
            richTextOverlay.drawCentered(display, textY, finalMsg, "Medium");
        } else {
            float speedPPS = (float)currentOverlay.scrollSpeed;
            long timePassedMs = now - overlayStartTime;
            float pixelsMoved = ((float)timePassedMs / 1000.0f) * speedPPS;
            int startX = boxX + boxW;
            int totalDist = boxW + textW + 20;
            int currentScrollX = startX - ((int)pixelsMoved % totalDist);
            richTextOverlay.drawString(display, currentScrollX, textY, finalMsg, "Medium");
        }
        
        int totalDur = currentOverlay.durationSec * 1000;
        long remaining = overlayEndTime - now;
        if (remaining > 0) {
            int barWidth = map(remaining, 0, totalDur, 0, boxW - 2);
            display.drawFastHLine(boxX + 1, boxY + boxH - 2, barWidth, display.color565(200, 200, 200));
        }
    }
}

// --- TASKS ---

TaskHandle_t NetworkTask;
TaskHandle_t DisplayTask;

void networkTaskFunction(void * pvParameters) {
  Serial.println("Boot: Checking Filesystem...");
  status("Check Storage...", display.color565(255, 255, 255));
  
  bool fsMounted = storage.begin();
  if (!fsMounted) {
     Serial.println("Boot: FS Failed! Continuing in Safe Mode...");
     status("Storage Fail!", display.color565(255, 0, 0));
     delay(2000); 
  }

  Serial.println("Boot: Connecting to WiFi...");
  status("Connect WiFi...", display.color565(255, 255, 255));
  
  network.begin(); 
  int retryCounter = 0;

  while(!network.isConnected()) {
      Serial.print(".");
      delay(500);
      retryCounter++;
      if (retryCounter > 20) {
          status("Retry WiFi...", display.color565(255, 100, 0));
          WiFi.disconnect();
          delay(100);
          network.begin(); 
          retryCounter = 0; 
      }
  }
  Serial.println();

  String ip = network.getIp();
  status("IP: " + ip, display.color565(0, 255, 0));
  delay(3000);

  if (fsMounted) {
      status("Start WebSrv...", display.color565(200, 200, 255));
      webServer.begin(); 
  } else {
      Serial.println("Boot: Skipping WebServer (Safe Mode)");
  }
  delay(1000);

  status("Wait for Time...", display.color565(255, 165, 0));
  network.tryInitServices();
  bool timeSuccess = false;
  int ntpRetryCount = 0;
  
  while(!timeSuccess) {
      if (fsMounted) webServer.handle();
      network.loop();
      if (network.isTimeSynced()) timeSuccess = true;
      if (!network.isConnected()) {
          status("WiFi Lost!", display.color565(255, 0, 0));
          delay(1000);
      }
      delay(100);
      if(ntpRetryCount++ > 200) break;
  }

  status("Start in 3s", display.color565(255, 255, 255));
  delay(3000);

  if (xSemaphoreTake(overlayMutex, portMAX_DELAY) == pdTRUE) {
    isBooting = false;
    xSemaphoreGive(overlayMutex);
  }
  
  // --- ENDLOS LOOP ---
  for(;;) {
    network.loop();
    if (fsMounted) webServer.handle();
    vTaskDelay((otaActive ? 1 : 10) / portTICK_PERIOD_MS);
  }
}

void displayTaskFunction(void * pvParameters) {
  AppMode displayedApp = currentApp.load();
  float fadeVal = 1.0;

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(frameDelay);

  for(;;) {
      // --- NORMALE SCHLEIFE (Kein Upload-Stop mehr nötig) ---
      
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
        xSemaphoreGive(overlayMutex); 
      }

      display.clear();

      if (localOta) {
           display.setFade(1.0);
           drawOTA(localOtaProg);
      }
      else if (localBooting) {
           display.setFade(1.0);
           if (xSemaphoreTake(overlayMutex, 5)) {
              drawBootLog();
              xSemaphoreGive(overlayMutex);
           }
      } 
      else {
           if (currentBright > 0) {
             switch(displayedApp) {
               case WORDCLOCK:   appWordClock.draw(display);   break;
               case SENSORS:     appSensors.draw(display);     break;
               case TESTPATTERN: appTestPattern.draw(display); break;
               case TICKER:      appTicker.draw(display);      break;
               case PLASMA:      appPlasma.draw(display);      break;
               case OFF:         /* Clear */                   break;
             }
             processAndDrawOverlay(display);
           }
      } 
      
      display.show();
      vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void setup() {
  Serial.begin(115200);
  delay(4000); 

  overlayMutex = xSemaphoreCreateMutex();
  if (!display.begin()) {
    while(1);
  }
  
  status("Boot...");
  // Prio 4 ist gut für Network, damit Display (Prio 10) Vorfahrt hat, aber Network nicht verhungert
  xTaskCreatePinnedToCore(networkTaskFunction, "NetworkTask", 16000, NULL, 4, &NetworkTask, 0);
  xTaskCreatePinnedToCore(displayTaskFunction, "DisplayTask", 10000, NULL, 10, &DisplayTask, 1);
}

void loop() {
  vTaskDelay(10000 / portTICK_PERIOD_MS);
}