#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "App.h"
#include "WeatherRenderer.h"
#include "RichText.h" 

// KEINE fehlerhaften "extern" Zeilen mehr hier oben! Das OS kennt sie aus der config.h!

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
    const int BASE_SWITCH_DELAY = 12000; // Standard: 12 Sekunden

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

        // ==========================================
        // 1. FEHLENDE DATEN: Nach 3 Sekunden abbrechen
        // ==========================================
        if (!hasData || (millis() - dataTimestamp > dataValidityMs)) {
            if (millis() - lastInfoToggle > 3000) {
                cycleComplete = true; // Dem OS nach 3s den Wechsel erlauben!
            }
            if (!needsRedraw) return false;
            display.clear();
            richText.drawCentered(display, M_HEIGHT / 2 + 4, "{c:muted}No Weather!", "Small");
            return true;
        }

        // ==========================================
        // 2. NORMALE SEITEN-LOGIK
        // ==========================================
        unsigned long switchDelay = BASE_SWITCH_DELAY * currentMultiplier;

        if (millis() - lastInfoToggle >= switchDelay) {
            if (cycleComplete && currentApp == AUTO) {
                // AUTO-MODUS: Wir frieren auf der letzten Seite ein und warten aufs Fade-Out
            } else {
                lastInfoToggle = millis();
                infoPage++;
                
                if (infoPage >= 4) {
                    cycleComplete = true; 
                    if (currentApp == AUTO) {
                        infoPage = 3; 
                    } else {
                        infoPage = 0; 
                    }
                }
                needsRedraw = true;
            }
        }

        if (!needsRedraw) return false;
        display.clear(); 

        // ==========================================
        // SEITE 1 (infoPage == 0): Aktuelles Wetter & Lokale Sensoren
        // ==========================================
        if (infoPage == 0) {
            richText.drawCentered(display, 11, "{c:#FFC800}Feldmoching", "Small");
            display.drawLine(66, 15, 66, 62, display.color565(60, 60, 60));

            int iconSize = 30;
            int iconX = 32 - (iconSize / 2); 
            int iconY = 15; 
            renderer.drawWeatherIcon(display, iconX, iconY, iconSize, currentW.condition, currentFrame);

            String tempStr = "{ln:2056} {c:white}" + String(localSensors.ltemp, 1) + "°C";
            drawColRichText(display, 32, 61, tempStr);

            int tx = 71; 
            richText.drawString(display, tx, 26, "{ln:53330} {c:white}" + String(localSensors.humidity, 0) + "%", "Small");
            richText.drawString(display, tx, 43, "{ln:65316} {c:white}" + String(localSensors.pm25, 1), "Small");
            richText.drawString(display, tx, 60, "{la:37364} {c:white}" + String(localSensors.voc), "Small");
            
            return true;
        }

        // ==========================================
        // SEITEN 2 bis 4: Die 3-Tages-Vorhersage
        // ==========================================
        int colWidth = M_WIDTH / 3; 

        for (int i = 0; i < 3; i++) {
            int cx = (i * colWidth) + (colWidth / 2); 
            
            drawColRichText(display, cx, 11, "{c:#CCCCCC}" + forecasts[i].day);

            if (infoPage == 3) {
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

                if (infoPage == 1) {
                    String minT = String((int)round(forecasts[i].temp));
                    String maxT = String((int)round(forecasts[i].tempMax));
                    String richTempStr = "{c:#64C8FF}" + minT + "{c:#888888}|{c:#FFC800}" + maxT;
                    drawColRichText(display, cx, 57, richTempStr);
                } else if (infoPage == 2) {
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

    // Prio 3 = Normal / Niedrige Priorität
    int getPriority() override { return 3; }
};