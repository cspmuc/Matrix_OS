#include <ArduinoJson.h>
#include <math.h>
#include <vector>
#include <deque>
#include "config.h"

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
#include "IconManager.h" 

AppMode currentApp = WORDCLOCK;
int brightness = 150; 

DisplayManager display;
IconManager iconManager;
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
// FIX für den Boot-Screen
struct BootLogEntry { String text; uint16_t color; };
std::vector<BootLogEntry> bootLogs; 
int bootLogCounter = 1;
// NEU: bool isUrgent am Ende der Struktur
struct OverlayMessage { String text; int durationSec; String colorName; int scrollSpeed; bool isUrgent; };
std::deque<OverlayMessage> overlayQueue;
bool isOverlayActive = false;
unsigned long overlayStartTime = 0;
unsigned long overlayEndTime = 0;
OverlayMessage currentOverlay;

// --- NEU: Globale Variablen für flüssiges Integer-Stepping ---
int overlayScrollX = 0;
unsigned long overlayLastScrollStep = 0;
int overlayTextWidth = 0;
int overlayBoxWidth = 0;
bool overlayIsScrolling = false;
int overlayBoxX = 0;
int overlayBoxY = 0;
// -------------------------------------------------------------

// TUNING: 10ms = 100 FPS für flüssigere Animationen
const int frameDelay = 10;

float fadeVal = 1.0;
const float fadeStep = 0.05; 
AppMode displayedApp = WORDCLOCK;
bool wasOverlayActive = false;

void queueOverlay(String msg, int durationSec, String colorName, int scrollSpeed) {
    Serial.print("Overlay Queued: "); Serial.println(msg);
    // NEU: false für normale Nachrichten
    if (overlayQueue.size() < 5) overlayQueue.push_back({msg, durationSec, colorName, scrollSpeed, false});
}

void forceOverlay(String msg, int durationSec, String colorName) {
    overlayQueue.clear();
    isOverlayActive = false; 
    // NEU: true für Urgent Nachrichten
    overlayQueue.push_back({msg, durationSec, colorName, 0, true});
}

void status(const String& msg, uint16_t color = 0xFFFF) {
  Serial.print("[STATUS] "); Serial.println(msg);
if (isBooting) {
      char buf[64]; sprintf(buf, "%02d %s", bootLogCounter++, msg.c_str());
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
      delay(50);
}
}

void processAndDrawOverlay(DisplayManager& display) {
    unsigned long now = millis();
    if (isOverlayActive) { 
        if (now > overlayEndTime) isOverlayActive = false;
    }
    
    // START EINES NEUEN OVERLAYS (Initialisierung)
    if (!isOverlayActive && !overlayQueue.empty()) {
        currentOverlay = overlayQueue.front();
        overlayQueue.pop_front();
        isOverlayActive = true; 
        overlayStartTime = now; 
        overlayEndTime = now + (currentOverlay.durationSec * 1000);
        Serial.println("Overlay Started: " + currentOverlay.text);

        // --- Layout Berechnung (Nur 1x beim Start) ---
        String finalMsg = "{c:" + currentOverlay.colorName + "}" + currentOverlay.text;
        overlayTextWidth = richTextOverlay.getTextWidth(display, finalMsg, "Medium");
        
        int boxH = 34;
        overlayBoxWidth = 104; 
        overlayIsScrolling = false;
        
        if (overlayTextWidth > (overlayBoxWidth - 10)) { 
            overlayBoxWidth = M_WIDTH; 
            overlayIsScrolling = true; 
        }
        
        overlayBoxX = (M_WIDTH - overlayBoxWidth) / 2;
        overlayBoxY = (M_HEIGHT - boxH) / 2;

        // Startposition für Scrolltext (Rechts vom Rahmen)
        overlayScrollX = overlayBoxX + overlayBoxWidth; 
        overlayLastScrollStep = now;
    }
    
    // ZEICHNEN
    if (isOverlayActive) {
        String finalMsg = "{c:" + currentOverlay.colorName + "}" + currentOverlay.text;
        int boxH = 34;

        display.dimRect(overlayBoxX, overlayBoxY, overlayBoxWidth, boxH); 
        
        // Rahmenfarbe basierend auf Dringlichkeit
        uint16_t borderColor = display.color565(80, 80, 80); // Standard Grau
        if (currentOverlay.isUrgent) {
            borderColor = display.color565(255, 100, 0); // Dunkelorange
        }
        display.drawRect(overlayBoxX, overlayBoxY, overlayBoxWidth, boxH, borderColor);
        
        int textY = overlayBoxY + (boxH / 2) + 5;

        if (!overlayIsScrolling) { 
            // Statischer Text (Zentriert)
            richTextOverlay.drawCentered(display, textY, finalMsg, "Medium");
        } else {
            // SCROLLING (Integer Stepping Logic)
            int speed = currentOverlay.scrollSpeed;
            if (speed < 1) speed = 30; 
            
            // Verzögerung pro Pixel berechnen (z.B. 30px/s -> 33ms pro Schritt)
            int stepDelay = 1000 / speed;
            
            // Zeit prüfen und Pixel weiterschalten
            if (now - overlayLastScrollStep >= stepDelay) {
                overlayScrollX--; 
                overlayLastScrollStep = now;
                
                // Reset Logik: Wenn Text ganz links raus ist
                int startX = overlayBoxX + overlayBoxWidth;
                int totalDist = overlayBoxWidth + overlayTextWidth + 20; // +Buffer
                
                if (overlayScrollX < (startX - totalDist)) {
                    overlayScrollX = startX;
                }
            }
            
            // Zeichnen an der exakten Integer-Position (Kein Aliasing mehr!)
            richTextOverlay.drawString(display, overlayScrollX, textY, finalMsg, "Medium");
        }
        
        // Fortschrittsbalken
        int totalDur = currentOverlay.durationSec * 1000;
        long remaining = overlayEndTime - now;
        if (remaining > 0) {
            int barWidth = map(remaining, 0, totalDur, 0, overlayBoxWidth - 2);
            display.drawFastHLine(overlayBoxX + 1, overlayBoxY + boxH - 2, barWidth, display.color565(200, 200, 200));
        }
    }
}

void setup() {
  Serial.begin(115200);
  
  if (!display.begin()) while(1);
  
  status("Check Storage...", display.color565(255, 255, 255));
  bool fsMounted = storage.begin();
  
  if (!fsMounted) { 
      status("Storage Fail!", display.color565(255, 0, 0)); 
      delay(1000);
} else { 
      status("Load Icons...", display.color565(255, 255, 0)); 
      iconManager.begin();
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
  status("I:" + network.getIp(), display.color565(0, 255, 0)); 
  delay(1000);
  status("Start WebSrv...", display.color565(200, 200, 255)); 
  webServer.begin(); 
  
  status("Wait for Time...", display.color565(255, 165, 0)); 
  network.tryInitServices();
  
  int ntpRetryCount = 0;
  while(!network.isTimeSynced()) { 
      network.loop(); 
      delay(50); 
      if(ntpRetryCount++ > 40) break;
} 
  
  status("Start in 3s", display.color565(0, 255, 0));
  delay(3000); 
  isBooting = false;
  bootLogs.clear();
  bootLogs.shrink_to_fit();
}

unsigned long lastDebugTick = 0;

// MERKER: War das Display aus?
static bool wasDisplayOff = false;

void loop() {
    unsigned long now = millis();
    // Debug Ausgabe
    if (now - lastDebugTick > 2000) {
        Serial.print("Tick: ");
        Serial.print(now / 1000);
        Serial.print(" | IP: "); Serial.print(WiFi.localIP()); 
        Serial.print(" | Heap: "); Serial.println(ESP.getFreeHeap());
        lastDebugTick = now;
}

    network.loop(); 
    webServer.handle();
    
    static unsigned long lastFrameTime = 0;
    if (now - lastFrameTime >= frameDelay) {
        lastFrameTime = now;
        
        display.setBrightness(brightness);
        AppMode targetApp = currentApp; 
        bool appChanged = false;
        
        if (displayedApp != targetApp) {
            fadeVal -= fadeStep;
            if (fadeVal <= 0.0) { 
                fadeVal = 0.0;
                displayedApp = targetApp; 
                appChanged = true;
            }
        } else { 
            if (fadeVal < 1.0) { 
                fadeVal += fadeStep;
                if (fadeVal > 1.0) fadeVal = 1.0; 
            } 
        }
        display.setFade(fadeVal);
        if (brightness > 0) {
             // LOGIK: Aufwachen erkennen
             bool justTurnedOn = wasDisplayOff;
             wasDisplayOff = false;

             bool overlayPending = !overlayQueue.empty();
             // FORCE REDRAW auch bei justTurnedOn!
             bool forceRedraw = appChanged || isOverlayActive || overlayPending || (wasOverlayActive && !isOverlayActive) || justTurnedOn;
             bool screenUpdated = false;
             switch(displayedApp) {
               case WORDCLOCK:   screenUpdated = appWordClock.draw(display, forceRedraw);
               break;
               case SENSORS:     screenUpdated = appSensors.draw(display, forceRedraw); break;
               case TESTPATTERN: screenUpdated = appTestPattern.draw(display, forceRedraw); break;
               case TICKER:      screenUpdated = appTicker.draw(display, forceRedraw); break;
               case PLASMA:      screenUpdated = appPlasma.draw(display, forceRedraw); break;
               case OFF:         display.clear(); screenUpdated = true; break;
             }
             
             if (isOverlayActive || overlayPending) { 
                 processAndDrawOverlay(display);
                 if (isOverlayActive) screenUpdated = true;
             }
             
             if (screenUpdated) display.show();
             wasOverlayActive = isOverlayActive;
        } else { 
             // LOGIK: Display ist AUS -> Merker setzen
             wasDisplayOff = true;
             display.clear();
             display.show(); 
        }
    }
    delay(1); 
}