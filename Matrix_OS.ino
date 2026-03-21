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
#include "ConfigManager.h"

AppMode currentApp = WORDCLOCK;
int brightness = 150; 

ConfigManager configManager;
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

MatrixNetworkManager network(currentApp, brightness, display, appSensors, configManager);
bool isBooting = true;
// FIX für den Boot-Screen
struct BootLogEntry { String text; uint16_t color; };
std::vector<BootLogEntry> bootLogs; 
int bootLogCounter = 1;

struct OverlayMessage { 
    OverlayType type; // <--- NEU: Text oder Animation?
    String text; 
    int durationSec; 
    String colorName; 
    int scrollSpeed; 
    bool isUrgent;
};

std::deque<OverlayMessage> overlayQueue;
bool isOverlayActive = false;
unsigned long overlayStartTime = 0;
unsigned long overlayEndTime = 0;
OverlayMessage currentOverlay;

int overlayScrollX = 0;
unsigned long overlayLastScrollStep = 0;
int overlayTextWidth = 0;
int overlayBoxWidth = 0;
bool overlayIsScrolling = false;
int overlayBoxX = 0;
int overlayBoxY = 0;

const int frameDelay = 10;
float fadeVal = 1.0;
const float fadeStep = 0.1; 
AppMode displayedApp = WORDCLOCK;
bool wasOverlayActive = false;

void queueOverlay(String msg, int durationSec, String colorName, int scrollSpeed) {
    Serial.print("Overlay Queued: "); Serial.println(msg);
    if (overlayQueue.size() < 5) overlayQueue.push_back({OVL_TEXT, msg, durationSec, colorName, scrollSpeed, false});
}

void forceOverlay(String msg, int durationSec, String colorName) {
    overlayQueue.clear();
    isOverlayActive = false;
    overlayQueue.push_back({OVL_TEXT, msg, durationSec, colorName, 0, true});
}

// --- NEU: Queue-Helper für Animationen ---
void queueAnimation(OverlayType animType, int durationSec) {
    Serial.println("Animation Queued");
    if (overlayQueue.size() < 5) overlayQueue.push_back({animType, "", durationSec, "", 0, false});
}

void status(const String& msg, uint16_t color = 0xFFFF) {
  Serial.print("[STATUS] "); Serial.println(msg);
if (isBooting) {
      char buf[64]; sprintf(buf, "%02d %s", bootLogCounter++, msg.c_str());
      bootLogs.push_back({String(buf), color});
if (bootLogs.size() > 8) bootLogs.erase(bootLogs.begin());
      
      display.clear(); 
      display.setAppFade(1.0);
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
        
        if (currentOverlay.type == OVL_TEXT) {
            Serial.println("Overlay Started: " + currentOverlay.text);
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
            overlayScrollX = overlayBoxX + overlayBoxWidth;
            overlayLastScrollStep = now;
        }
    }
    
    // ZEICHNEN
    if (isOverlayActive) {
        if (currentOverlay.type == OVL_TEXT) {
            // --- STANDARD TEXT OVERLAY ---
            String finalMsg = "{c:" + currentOverlay.colorName + "}" + currentOverlay.text;
            int boxH = 34;

            display.dimRect(overlayBoxX, overlayBoxY, overlayBoxWidth, boxH); 
            
            uint16_t borderColor = display.color565(80, 80, 80);
            if (currentOverlay.isUrgent) borderColor = display.color565(255, 100, 0);
            display.drawRect(overlayBoxX, overlayBoxY, overlayBoxWidth, boxH, borderColor);
            
            int textY = overlayBoxY + (boxH / 2) + 5;
            if (!overlayIsScrolling) { 
                richTextOverlay.drawCentered(display, textY, finalMsg, "Medium");
            } else {
                int speed = currentOverlay.scrollSpeed;
                if (speed < 1) speed = 30; 
                int stepDelay = 1000 / speed;
                if (now - overlayLastScrollStep >= stepDelay) {
                    overlayScrollX--;
                    overlayLastScrollStep = now;
                    int startX = overlayBoxX + overlayBoxWidth;
                    int totalDist = overlayBoxWidth + overlayTextWidth + 20; 
                    if (overlayScrollX < (startX - totalDist)) overlayScrollX = startX;
                }
                richTextOverlay.drawString(display, overlayScrollX, textY, finalMsg, "Medium");
            }
            
            int totalDur = currentOverlay.durationSec * 1000;
            long remaining = overlayEndTime - now;
            if (remaining > 0) {
                int barWidth = map(remaining, 0, totalDur, 0, overlayBoxWidth - 2);
                display.drawFastHLine(overlayBoxX + 1, overlayBoxY + boxH - 2, barWidth, display.color565(200, 200, 200));
            }
        } 
        else if (currentOverlay.type == OVL_ANIM_GHOST) {
            // --- NEU: GHOST EYES ANIMATION ---
            // Nur noch 1x abdunkeln für einen etwas helleren Hintergrund
            display.dimRect(0, 0, M_WIDTH, M_HEIGHT);
            
            unsigned long elapsed = now - overlayStartTime;
            int totalDur = currentOverlay.durationSec * 1000;
            
            // Smarter Fade-In / Fade-Out mit butterweichen 255 Stufen!
            int eyeBright = 255;
            if (elapsed < 300) {
                eyeBright = map(elapsed, 0, 300, 0, 255); // Einblenden in den ersten 300ms
            } else if (elapsed > totalDur - 300) {
                eyeBright = map(totalDur - elapsed, 0, 300, 0, 255); // Ausblenden in den letzten 300ms
            }
            
            // Sicherstellen, dass der Wert nicht überläuft, und RGB Farbe mischen
            eyeBright = constrain(eyeBright, 0, 255);
            uint16_t eyeColor = display.color565(eyeBright, eyeBright, eyeBright);
            
            // Etwas enger zusammen für ovale Augen
            int leftEyeX = M_WIDTH / 2 - 14; 
            int rightEyeX = M_WIDTH / 2 + 14;
            int eyeY = M_HEIGHT / 2 - 2;
            
            int pupilleOffsetX = 0;
            bool isBlinking = false;
            
            // Die Timeline (Das Drehbuch)
            if (elapsed >= 600 && elapsed < 1300) pupilleOffsetX = -4;      // Blick Links
            else if (elapsed >= 1300 && elapsed < 2000) pupilleOffsetX = 4; // Blick Rechts
            else if (elapsed >= 2000 && elapsed < 2200) isBlinking = true;  // Kurzes Blinzeln
            
            if (!isBlinking) {
                // Weiße Augäpfel als Ellipsen (Breite: 6, Höhe: 10)
                display.fillEllipse(leftEyeX, eyeY, 6, 10, eyeColor);
                display.fillEllipse(rightEyeX, eyeY, 6, 10, eyeColor);
                
                // Schwarze Pupillen (Bleiben Kreise, aber etwas kleiner)
                display.fillCircle(leftEyeX + pupilleOffsetX, eyeY, 2, 0x0000);
                display.fillCircle(rightEyeX + pupilleOffsetX, eyeY, 2, 0x0000);
            } else {
                // Zugekniffene Augen (einfache Striche, passend zur Breite der Ellipsen)
                display.drawFastHLine(leftEyeX - 5, eyeY, 11, eyeColor);
                display.drawFastHLine(rightEyeX - 5, eyeY, 11, eyeColor);
            }
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
      status("Load Config...", display.color565(255, 255, 0)); 
      configManager.begin();
      if (configManager.autoMode.enabled) currentApp = AUTO;
      brightness = configManager.system.startup_brightness; // <--- NEU: Helligkeit aus der Config setzen
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

        // --- 1. HILFSFUNKTIONEN FÜR APP-INSTANZEN ---
        auto getAppModeByName = [](String name) -> AppMode {
            name.toLowerCase();
            if (name == "wordclock") return WORDCLOCK;
            if (name == "sensors") return SENSORS;
            if (name == "testpattern") return TESTPATTERN;
            if (name == "ticker") return TICKER;
            if (name == "plasma") return PLASMA;
            return OFF;
        };

        auto getAppInstance = [](AppMode mode) -> App* {
            if (mode == WORDCLOCK) return &appWordClock;
            if (mode == SENSORS) return &appSensors;
            if (mode == TESTPATTERN) return &appTestPattern;
            if (mode == TICKER) return &appTicker;
            if (mode == PLASMA) return &appPlasma;
            return nullptr;
        };
        // ---------------------------------------------
       // 1. ZUERST die Variablen deklarieren!
        AppMode targetApp = currentApp; 
        static int autoAppIndex = 0;
        static unsigned long autoAppFallbackTimer = 0;

        // 2. DANN den ersten Start initialisieren!
        static bool firstRun = true;
        if (firstRun) {
            firstRun = false;
            autoAppFallbackTimer = millis();
            App* firstApp = getAppInstance(displayedApp);
            if (firstApp) firstApp->onActive(); // Weckt die Wortuhr auf
        }

// --- 2. AUTO-ROTATION LOGIK ---
        if (currentApp == AUTO) {
            if (configManager.autoMode.apps.empty()) {
                targetApp = WORDCLOCK; // Fallback, falls die Config leer ist
            } else {
                if (autoAppIndex >= configManager.autoMode.apps.size()) autoAppIndex = 0;
                targetApp = getAppModeByName(configManager.autoMode.apps[autoAppIndex]);
                
                App* runningApp = getAppInstance(displayedApp);
                unsigned long activeTime = millis() - autoAppFallbackTimer;
                
                // --- NEU: Der 1-Sekunden Prio-Scanner ---
                static unsigned long lastPrioScan = 0;
                static float currentDurationMultiplier = 1.0;
                
                if (millis() - lastPrioScan > 1000) {
                    lastPrioScan = millis();
                    int highestSystemPrio = 3;
                    
                    // Wir scannen nur die Apps, die auch in der Auto-Rotation sind!
                    for (const String& appName : configManager.autoMode.apps) {
                        App* app = getAppInstance(getAppModeByName(appName));
                        if (app) {
                            int p = app->getPriority();
                            if (p < highestSystemPrio) highestSystemPrio = p;
                        }
                    }
                    
                    // Multiplikator setzen (1 = 40%, 2 = 60%, 3 = 100%)
                    if (highestSystemPrio == 1) currentDurationMultiplier = 0.4;
                    else if (highestSystemPrio == 2) currentDurationMultiplier = 0.6;
                    else currentDurationMultiplier = 1.0;
                }
                // ----------------------------------------
                
                bool ready = false;
                // Faktor an die App übergeben!
                if (runningApp) ready = runningApp->isReadyToSwitch(currentDurationMultiplier); 
                
                // Fallback-Timer (60s) bekommt ebenfalls den Faktor
                if (activeTime > (60000 * currentDurationMultiplier)) ready = true; 
                
                // Wir wechseln nur, wenn die App fertig ist UND wir nicht gerade im Fade hängen
                if (ready && fadeVal >= 1.0 && displayedApp == targetApp) {
                    autoAppIndex++;
                    if (autoAppIndex >= configManager.autoMode.apps.size()) autoAppIndex = 0;
                    targetApp = getAppModeByName(configManager.autoMode.apps[autoAppIndex]);
                }
            }
        }
        // --------------------------------
        
        bool appChanged = false;
        bool isFading = false;
        
// --- 3. FADING LOGIK (inkl. Aufwecken der neuen App) ---
        if (displayedApp != targetApp) {
            fadeVal -= fadeStep;
            isFading = true; 
            if (fadeVal <= 0.0) { 
                fadeVal = 0.0;
                displayedApp = targetApp; 
                appChanged = true;
                
                // Timer resetten & neue App aufwecken!
                autoAppFallbackTimer = millis(); 
                App* newApp = getAppInstance(displayedApp);
                if (newApp) newApp->onActive(); 
            }
        } else { 
            if (fadeVal < 1.0) { 
                fadeVal += fadeStep;
                isFading = true; 
                if (fadeVal >= 1.0) {
                    fadeVal = 1.0; 
                    // WICHTIG: Wir lassen isFading für DIESEN Frame noch auf true!
                    // So wird das 100% Bild noch garantiert einmal gezeichnet.
                }
            } else {
                // Erst im Frame DANACH beenden wir den Fading-Zustand komplett.
                isFading = false;
            }
        }
        display.setAppFade(fadeVal);

        if (brightness > 0) {
             // LOGIK: Aufwachen erkennen
             bool justTurnedOn = wasDisplayOff;
             wasDisplayOff = false;

             bool overlayPending = !overlayQueue.empty();
             
             // --- NEU: isFading zur forceRedraw Logik hinzugefügt ---
             bool forceRedraw = appChanged || isOverlayActive ||
                                overlayPending || (wasOverlayActive && !isOverlayActive) || justTurnedOn || isFading;
             
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
                 display.setAppFade(1.0);
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