#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector> 
#include "App.h"
#include "WeatherRenderer.h"
#include "RichText.h" 

struct HourlyForecast {
    String time;
    String cond;
    float temp;
    int precipProb;
    float precip; // <-- NEU: Hier speichern wir jetzt auch die Regenmenge
};

struct WeatherData {
    String condition = "unknown";
    float temp = 0.0;     
    float tempMax = 0.0;  
    float precip = 0.0;   
    float wind = 0.0;     
    String day = "";      
    
    int windDir = 0;        
    float windGust = 0.0;   
    int precipProb = 0;     

    std::vector<HourlyForecast> hourly; 
};

struct LocalSensorData {
    float ltemp = 0.0; 
    float humidity = 0.0;
    float pm25 = 0.0;
    int voc = 0;
};

class WeatherApp : public App {
private:
    WeatherRenderer renderer;
    RichText richText; 
    int currentFrame = 0;
    unsigned long lastFrameTime = 0;
    const int frameDelay = 80; 

    WeatherData currentW;
    WeatherData forecasts[3];
    LocalSensorData localSensors; 
    
    unsigned long dataTimestamp = 0;
    unsigned long dataValidityMs = 0;
    bool hasData = false;

    unsigned long lastInfoToggle = 0;
    int infoPage = 0; 
    bool cycleComplete = false;
    float currentMultiplier = 1.0; 
    const int BASE_SWITCH_DELAY = 12000; 

    void drawColRichText(DisplayManager& display, int cx, int y, String text) {
        int w = richText.getTextWidth(display, text, "Small");
        richText.drawString(display, cx - (w / 2), y, text, "Small");
    }

public:
    WeatherApp() {}

    void setup() {
        lastFrameTime = millis();
        lastInfoToggle = millis(); 
    }

    void onActive() override {
        cycleComplete = false;
        infoPage = 0; 
        lastInfoToggle = millis();
        currentMultiplier = 1.0;
        
        renderer.shuffleAnimations();
    }

    bool isReadyToSwitch(float durationMultiplier = 1.0) override {
        currentMultiplier = durationMultiplier; 
        return cycleComplete;
    }

    void updateData(JsonDocument* doc) {
        dataValidityMs = ((*doc)["validity"] | 3600) * 1000; 

        if (doc->containsKey("current")) {
            currentW.condition = (*doc)["current"]["cond"] | "unknown";
            currentW.temp = (*doc)["current"]["temp"] | 0.0;
            currentW.precip = (*doc)["current"]["precip"] | 0.0;
            currentW.wind = (*doc)["current"]["wind"] | 0.0;
            currentW.windDir = (*doc)["current"]["wind_dir"] | 0;
            currentW.windGust = (*doc)["current"]["wind_gust"] | 0.0;
        }

        if (doc->containsKey("forecasts") && (*doc)["forecasts"].is<JsonArray>()) {
            JsonArray arr = (*doc)["forecasts"].as<JsonArray>();
            for (int i = 0; i < 3 && i < arr.size(); i++) {
                forecasts[i].day = arr[i]["day"].as<String>();
                forecasts[i].condition = arr[i]["cond"].as<String>();
                forecasts[i].temp = arr[i]["tmin"] | 0.0;
                forecasts[i].tempMax = arr[i]["tmax"] | 0.0;
                forecasts[i].precip = arr[i]["precip"] | 0.0;
                forecasts[i].wind = arr[i]["wind"] | 0.0;
                forecasts[i].windDir = arr[i]["wind_dir"] | 0;
                forecasts[i].windGust = arr[i]["wind_gust"] | 0.0;
                forecasts[i].precipProb = arr[i]["precip_prob"] | 0;
            }
        }

        // Parsen der stündlichen Daten
        if (doc->containsKey("hourly") && (*doc)["hourly"].is<JsonArray>()) {
            currentW.hourly.clear();
            JsonArray arr = (*doc)["hourly"].as<JsonArray>();
            for (int i = 0; i < arr.size(); i++) {
                HourlyForecast hf;
                hf.time = arr[i]["time_str"] | "--:--";
                hf.cond = arr[i]["cond"] | "unknown";
                hf.temp = arr[i]["temp"] | 0.0;
                hf.precipProb = arr[i]["precip_prob"] | 0;
                hf.precip = arr[i]["precip"] | 0.0; // <-- NEU: Menge einlesen
                currentW.hourly.push_back(hf);
            }
        }

        if (doc->containsKey("local")) {
            localSensors.ltemp = (*doc)["local"]["ltemp"] | 0.0;
            localSensors.humidity = (*doc)["local"]["humidity"] | 0.0;
            localSensors.pm25 = (*doc)["local"]["pm25"] | 0.0;
            localSensors.voc = (*doc)["local"]["voc"] | 0;
        }
        
        dataTimestamp = millis();
        hasData = true;
    }

    bool draw(DisplayManager& display, bool force) override {
        bool needsRedraw = force;

        if (millis() - lastFrameTime >= frameDelay) {
            currentFrame = (currentFrame + 1) % 16;
            lastFrameTime = millis();
            needsRedraw = true; 
        }

        if (!hasData || (millis() - dataTimestamp > dataValidityMs)) {
            if (millis() - lastInfoToggle > 3000) cycleComplete = true; 
            if (!needsRedraw) return false;
            display.clear();
            richText.drawCentered(display, M_HEIGHT / 2 + 4, "{c:muted}No Weather!", "Small");
            return true;
        }

        unsigned long switchDelay = BASE_SWITCH_DELAY * currentMultiplier;

        if (millis() - lastInfoToggle >= switchDelay) {
            if (cycleComplete && currentApp == AUTO) {
                // Warten auf Fade
            } else {
                lastInfoToggle = millis();
                infoPage++;
                
                renderer.shuffleAnimations();
                
                if (infoPage >= 5) {
                    cycleComplete = true; 
                    if (currentApp == AUTO) infoPage = 4; 
                    else infoPage = 0; 
                }
                needsRedraw = true;
            }
        }

        if (!needsRedraw) return false;
        display.clear(); 

        // ==========================================
        // SEITE 1: Aktuelles Wetter & Lokale Sensoren
        // ==========================================
        if (infoPage == 0) {
            richText.drawCentered(display, 11, "{c:#FFC800}Feldmoching", "Small");
            display.drawLine(66, 15, 66, 62, display.color565(60, 60, 60));

            // --- LINKE SEITE ---
            int iconSize = 30;
            int iconX = 32 - (iconSize / 2); 
            int iconY = 15; 
            renderer.drawWeatherIcon(display, iconX, iconY, iconSize, currentW.condition, currentFrame);

            // Temperatur
            String tempText = "{c:white}" + String(localSensors.ltemp, 1) + "°C";
            int tW = richText.getTextWidth(display, tempText, "Small");
            int totalTempW = 16 + 2 + tW; 
            int tX = 32 - (totalTempW / 2);

            renderer.drawThermometer(display, tX, 49, 16, currentFrame, localSensors.ltemp);
            richText.drawString(display, tX + 18, 61, tempText, "Small");

            // --- RECHTE SEITE ---
            int tx = 71; 
            
            // Feuchtigkeit
            renderer.drawHumidity(display, tx, 14, 14, currentFrame, localSensors.humidity);
            richText.drawString(display, tx + 16, 26, "{c:white}" + String(localSensors.humidity, 0) + "%", "Small");
            
            // PM2.5
            renderer.drawPM25(display, tx, 31, 14, currentFrame, localSensors.pm25);
            richText.drawString(display, tx + 16, 43, "{c:white}" + String(localSensors.pm25, 1), "Small");
            
            // VOC
            renderer.drawVOC(display, tx, 48, 14, currentFrame, localSensors.voc);
            richText.drawString(display, tx + 16, 60, "{c:white}" + String(localSensors.voc), "Small");
            
            return true;
        }

        // ==========================================
        // SEITE 2: Stündliche Vorhersage (+2h, +4h, +8h)
        // ==========================================
        if (infoPage == 1) {
            int colWidth = M_WIDTH / 3;
            
            // --- NEU: Dynamischer Toggle-Logik ---
            // Wir berechnen die Zeit, die wir uns schon auf dieser Seite befinden
            unsigned long timeOnPage = millis() - lastInfoToggle; 
            // Alle 2000ms wechseln wir den Zustand (Modulo 2 gibt uns 0 oder 1)
            bool showAmount = (timeOnPage / 2000) % 2 != 0; 

            for (int i = 0; i < 3 && i < currentW.hourly.size(); i++) {
                int cx = (i * colWidth) + (colWidth / 2);
                
                // 1. Uhrzeit (oben)
                drawColRichText(display, cx, 11, "{c:#CCCCCC}" + currentW.hourly[i].time.substring(0, 5));
                
                // 2. Wetter-Icon (mittig)
                int iconSize = 22; 
                int iconX = cx - (iconSize / 2);
                int iconY = 18;
                renderer.drawWeatherIcon(display, iconX, iconY, iconSize, currentW.hourly[i].cond, currentFrame);
                
                // 3. Temperatur (darunter)
                String tStr = "{c:#FFC800}" + String((int)round(currentW.hourly[i].temp)) + "C";
                drawColRichText(display, cx, 50, tStr);
                
                // 4. Regeninfo (Ganz unten mit Toggle)
                String pStr;
                if (!showAmount) {
                    // Zeige Wahrscheinlichkeit (z.B. "30%")
                    pStr = "{c:#64C8FF}" + String(currentW.hourly[i].precipProb) + "%";
                } else {
                    // Zeige Menge (z.B. "1.2mm" oder "0mm")
                    if (currentW.hourly[i].precip < 0.1) {
                        pStr = "{c:#AAAAAA}0mm";
                    } else {
                        pStr = "{c:#AAAAAA}" + String(currentW.hourly[i].precip, 1) + "mm";
                    }
                }
                drawColRichText(display, cx, 63, pStr);
            }
            return true;
        }

        // ==========================================
        // SEITEN 3 bis 5: Die 3-Tages-Vorhersage
        // ==========================================
        int colWidth = M_WIDTH / 3; 

        for (int i = 0; i < 3; i++) {
            int cx = (i * colWidth) + (colWidth / 2); 
            
            drawColRichText(display, cx, 11, "{c:#CCCCCC}" + forecasts[i].day);

            if (infoPage == 4) {
                // WIND
                renderer.drawWindRose(display, cx, 28, 11, forecasts[i].windDir, currentFrame);

                String wSpeed = String((int)round(forecasts[i].wind));
                String wGust = String((int)round(forecasts[i].windGust));
                String richWindStr = "{c:#64C8FF}" + wSpeed + "{c:#888888}|{c:#FFC800}" + wGust;
                drawColRichText(display, cx, 57, richWindStr);

            } else {
                // TEMPERATUR & REGEN
                int iconSize = 24;
                int iconX = cx - (iconSize / 2); 
                int iconY = 16;
                renderer.drawWeatherIcon(display, iconX, iconY, iconSize, forecasts[i].condition, currentFrame);

                if (infoPage == 2) {
                    String minT = String((int)round(forecasts[i].temp));
                    String maxT = String((int)round(forecasts[i].tempMax));
                    String richTempStr = "{c:#64C8FF}" + minT + "{c:#888888}|{c:#FFC800}" + maxT;
                    drawColRichText(display, cx, 57, richTempStr);
                } else if (infoPage == 3) {
                    String probStr = "{c:#64C8FF}" + String(forecasts[i].precipProb) + "%";
                    drawColRichText(display, cx, 51, probStr);

                    String mmStr;
                    if (forecasts[i].precip < 0.1) mmStr = "0mm";
                    else mmStr = String(forecasts[i].precip, 1) + "mm";
                    
                    drawColRichText(display, cx, 64, "{c:#AAAAAA}" + mmStr);
                }
            }
        }

        return true; 
    }

    int getPriority() override { return 3; }
};