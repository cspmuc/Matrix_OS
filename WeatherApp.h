#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "App.h"
#include "WeatherRenderer.h"
#include "RichText.h" 

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

class WeatherApp : public App {
private:
    WeatherRenderer renderer;
    RichText richText; 
    int currentFrame = 0;
    unsigned long lastFrameTime = 0;
    const int frameDelay = 80; 

    WeatherData currentW;
    WeatherData forecasts[3];
    
    unsigned long dataTimestamp = 0;
    unsigned long dataValidityMs = 0;
    bool hasData = false;

    unsigned long lastInfoToggle = 0;
    bool showTemp = true; 

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

        // --- NEU: Anzeigezeit auf 10 Sekunden (10000 ms) erhöht ---
        if (millis() - lastInfoToggle >= 10000) {
            showTemp = !showTemp;
            lastInfoToggle = millis();
            needsRedraw = true;
        }

        if (!needsRedraw) return false;
        display.clear(); 

        if (!hasData || (millis() - dataTimestamp > dataValidityMs)) {
            richText.drawCentered(display, M_HEIGHT / 2 + 4, "{c:muted}No Weather!", "Small");
            return true;
        }

        int colWidth = M_WIDTH / 3; 
        int iconSize = 24;

        for (int i = 0; i < 3; i++) {
            int cx = (i * colWidth) + (colWidth / 2); 
            
            // Wochentag
            drawColRichText(display, cx, 11, "{c:#CCCCCC}" + forecasts[i].day);

            // Icon
            int iconX = cx - (iconSize / 2); 
            int iconY = 16;
            renderer.drawWeatherIcon(display, iconX, iconY, iconSize, forecasts[i].condition, currentFrame);

            // Info-Bereich
            if (showTemp) {
                // --- NEU: Kompakter Temperatur-String ohne Leerzeichen ---
                String minT = String((int)round(forecasts[i].temp));
                String maxT = String((int)round(forecasts[i].tempMax));
                String richTempStr = "{c:#64C8FF}" + minT + "{c:#888888}|{c:#FFC800}" + maxT;
                drawColRichText(display, cx, 57, richTempStr);
            } else {
                // --- NEU: Wahrscheinlichkeit ohne Icon ---
                String probStr = "{c:#64C8FF}" + String(forecasts[i].precipProb) + "%";
                drawColRichText(display, cx, 51, probStr);

                // --- NEU: Menge 1 Pixel tiefer (y=64) ---
                String mmStr;
                if (forecasts[i].precip < 0.1) {
                    mmStr = "0mm";
                } else {
                    mmStr = String(forecasts[i].precip, 1) + "mm";
                }
                drawColRichText(display, cx, 64, "{c:#AAAAAA}" + mmStr);
            }
        }

        return true; 
    }

    int getPriority() override { return 1; }
};