#include <ArduinoJson.h>
#include <math.h>
#include <vector>
#include <deque>
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

AppMode currentApp = WORDCLOCK;
int brightness = 150; 

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

bool isBooting = true;

struct BootLogEntry { String text; uint16_t color; };
std::vector<BootLogEntry> bootLogs; 
int bootLogCounter = 1;

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
float fadeVal = 1.0;
const float fadeStep = 0.05; 
AppMode displayedApp = WORDCLOCK;

void queueOverlay(String msg, int durationSec, String colorName, int scrollSpeed) {
    if (overlayQueue.size() < 5) {
        overlayQueue.push_back({msg, durationSec, colorName, scrollSpeed});
    }
}

void forceOverlay(String msg, int durationSec, String colorName) {
    overlayQueue.clear();
    isOverlayActive = false;
    overlayQueue.push_back({msg, durationSec, colorName, 0});
}

void status(const String& msg, uint16_t color = 0xFFFF) {
  Serial.print("[STATUS] "); Serial.println(msg);
  if (isBooting) {
      char buf[64];
      sprintf(buf, "%02d %s", bootLogCounter++, msg.c_str());
      bootLogs.push_back({String(buf), color});
      if (bootLogs.size() > 8) bootLogs.erase(bootLogs.begin());
      display.clear();
      display.setFade(1.0);
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
      display.show();
  }
}

void processAndDrawOverlay(DisplayManager& display) {
    unsigned long now = millis();
    if (isOverlayActive) {
        if (now > overlayEndTime) isOverlayActive = false;
    }
    if (!isOverlayActive && !overlayQueue.empty()) {
        currentOverlay = overlayQueue.front();
        overlayQueue.pop_front();
        isOverlayActive = true;
        overlayStartTime = now;
        overlayEndTime = now + (currentOverlay.durationSec * 1000);
    }
    if (isOverlayActive) {
        String finalMsg = "{c:" + currentOverlay.colorName + "}" + currentOverlay.text;
        int textW = richTextOverlay.getTextWidth(display, finalMsg, "Medium");
        int boxH = 34; int boxW = 104; 
        bool isScrolling = false;
        if (textW > (boxW - 10)) { boxW = M_WIDTH; isScrolling = true; }
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

void setup() {
  Serial.begin(115200);
  delay(1000); 

  if (!display.begin()) while(1);
  
  status("Check Storage...", display.color565(255, 255, 255));
  bool fsMounted = storage.begin();
  if (!fsMounted) {
     status("Storage Fail!", display.color565(255, 0, 0));
     delay(1000); 
  }

  status("Connect WiFi...", display.color565(255, 255, 255));
  network.begin(); 
  
  int retryCounter = 0;
  while(!network.isConnected()) {
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

  status("IP: " + network.getIp(), display.color565(0, 255, 0));
  delay(1000);

  if (fsMounted) {
      status("Start WebSrv...", display.color565(200, 200, 255));
      webServer.begin(); 
  }

  status("Wait for Time...", display.color565(255, 165, 0));
  network.tryInitServices();
  
  int ntpRetryCount = 0;
  while(!network.isTimeSynced()) {
      network.loop(); 
      delay(50);
      if(ntpRetryCount++ > 100) break; 
  }

  status("System Ready", display.color565(0, 255, 0));
  delay(1000);
  isBooting = false;
}

void loop() {
    unsigned long now = millis();

    network.loop();
    webServer.handle(); 

    static unsigned long lastFrameTime = 0;
    if (now - lastFrameTime >= frameDelay) {
        lastFrameTime = now;

        display.setBrightness(brightness);
        
        AppMode targetApp = currentApp;
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

        display.clear();
       
        if (brightness > 0) {
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
        display.show();
    }
}