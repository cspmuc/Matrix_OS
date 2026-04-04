#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "App.h"
#include "WeatherRenderer.h"
#include "RichText.h" // <--- Einbinden der RichText Engine!

struct WeatherData {
    String condition = "unknown";
    float temp = 0.0;     
    float tempMax = 0.0;  
    float precip = 0.0;   
    float wind = 0.0;     
    String day = "";      
};

class WeatherApp : public App {
private:
    WeatherRenderer renderer;
    RichText richText; // <--- Instanz für die Textausgabe
    int currentFrame = 0;
    unsigned long lastFrameTime = 0;
    const int frameDelay = 80; 

    WeatherData currentW;
    WeatherData forecasts[3];
    
    unsigned long dataTimestamp = 0;
    unsigned long dataValidityMs = 0;
    bool hasData = false;

    // Hilfsfunktion: Zentriert einen RichText-String perfekt in einer Spalte
    void drawColRichText(DisplayManager& display, int cx, int y, String text) {
        // "Small" ist ein guter Kompromiss für die 3 Spalten
        int w = richText.getTextWidth(display, text, "Small");
        richText.drawString(display, cx - (w / 2), y, text, "Small");
    }

public:
    WeatherApp() {}

    void setup() {
        lastFrameTime = millis();
    }

    void updateData(JsonDocument* doc) {
        dataValidityMs = ((*doc)["validity"] | 3600) * 1000; 

        if (doc->containsKey("current")) {
            currentW.condition = (*doc)["current"]["cond"] | "unknown";
            currentW.temp = (*doc)["current"]["temp"] | 0.0;
            currentW.precip = (*doc)["current"]["precip"] | 0.0;
            currentW.wind = (*doc)["current"]["wind"] | 0.0;
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

        if (!needsRedraw) return false;
        display.clear(); 

        if (!hasData || (millis() - dataTimestamp > dataValidityMs)) {
            // Auch hier nutzen wir direkt RichText
            richText.drawCentered(display, M_HEIGHT / 2 + 4, "{c:muted}No Weather!", "Small");
            return true;
        }

        int colWidth = M_WIDTH / 3; 
        int iconSize = 24;

        for (int i = 0; i < 3; i++) {
            int cx = (i * colWidth) + (colWidth / 2); 
            
            // 1. Wochentag (Grau)
            drawColRichText(display, cx, 12, "{c:#CCCCCC}" + forecasts[i].day);

            // 2. Animiertes Icon
            int iconX = cx - (iconSize / 2); 
            int iconY = 18;
            renderer.drawWeatherIcon(display, iconX, iconY, iconSize, forecasts[i].condition, currentFrame);

            // 3. Temperatur mit RichText (OHNE Leerzeichen, natürliche Schriftbreite!)
            String minT = String((int)round(forecasts[i].temp));
            String maxT = String((int)round(forecasts[i].tempMax));
            
            // Wir nutzen Hex-Codes für exakte Farben: 
            // Hellblau: #64C8FF, Grau: #888888, Kräftiges Gelb: #FFC800
            String richTempStr = "{c:#64C8FF}" + minT + "{c:#888888}|{c:#FFC800}" + maxT;
            
            drawColRichText(display, cx, 56, richTempStr);
        }

        return true; 
    }

    int getPriority() override { return 1; }
};