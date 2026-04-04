#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "App.h"
#include "WeatherRenderer.h"

// --- Datenstruktur für einen einzelnen Wetter-Datensatz ---
struct WeatherData {
    String condition = "unknown";
    float temp = 0.0;     // Aktuelle Temp oder Min-Temp bei Vorhersage
    float tempMax = 0.0;  // Max-Temp (nur Vorhersage)
    float precip = 0.0;   // Niederschlagsmenge
    float wind = 0.0;     // Windgeschwindigkeit
    String day = "";      // "MO", "DI" etc. (nur Vorhersage)
};

class WeatherApp : public App {
private:
    WeatherRenderer renderer;
    int currentFrame = 0;
    unsigned long lastFrameTime = 0;
    const int frameDelay = 80; // 16 Frames, butterweich

    // --- Datenspeicher ---
    WeatherData currentW;
    WeatherData forecasts[3];
    
    unsigned long dataTimestamp = 0;
    unsigned long dataValidityMs = 0;
    bool hasData = false;

    // Hilfsfunktion: Zentriert Text perfekt in einer bestimmten Spalte (X-Koordinate)
    void drawColText(DisplayManager& display, int cx, int y, String text, uint16_t color) {
        int w = display.getTextWidth(text);
        display.drawString(cx - (w / 2), y, text, color);
    }

public:
    WeatherApp() {}

    void setup() {
        lastFrameTime = millis();
    }

    // --- JSON auswerten (Wird vom NetworkManager aufgerufen) ---
    void updateData(JsonDocument* doc) {
        // Gültigkeit in Millisekunden umrechnen (Standard: 1 Stunde)
        dataValidityMs = ((*doc)["validity"] | 3600) * 1000; 

        // 1. Aktuelles Wetter parsen
        if (doc->containsKey("current")) {
            currentW.condition = (*doc)["current"]["cond"] | "unknown";
            currentW.temp = (*doc)["current"]["temp"] | 0.0;
            currentW.precip = (*doc)["current"]["precip"] | 0.0;
            currentW.wind = (*doc)["current"]["wind"] | 0.0;
        }

        // 2. Vorhersagen parsen (Array mit 3 Einträgen)
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

        // Animations-Timer
        if (millis() - lastFrameTime >= frameDelay) {
            currentFrame = (currentFrame + 1) % 16;
            lastFrameTime = millis();
            needsRedraw = true; 
        }

        if (!needsRedraw) return false;
        display.clear(); 

        // --- Timeout / Fehlen der Daten abfangen ---
        if (!hasData || (millis() - dataTimestamp > dataValidityMs)) {
            // NEU: Kürzerer und knackigerer Text!
            display.drawCenteredString(M_HEIGHT / 2 + 4, "No Weather!", display.color565(150, 150, 150));
            return true;
        }

        // --- Das 3-Spalten Layout (Heute, Morgen, Übermorgen) ---
        int colWidth = M_WIDTH / 3; // 128 / 3 = ca. 42 Pixel pro Spalte
        int iconSize = 24;

        for (int i = 0; i < 3; i++) {
            // Die exakte X-Mitte der jeweiligen Spalte berechnen (21, 63, 105)
            int cx = (i * colWidth) + (colWidth / 2); 
            
            // 1. Wochentag (Oben, y=12)
            drawColText(display, cx, 12, forecasts[i].day, display.color565(200, 200, 200));

            // 2. Animiertes Icon (Mitte, y=18)
            int iconX = cx - (iconSize / 2); // Um die Spaltenmitte zentrieren
            int iconY = 18;
            renderer.drawWeatherIcon(display, iconX, iconY, iconSize, forecasts[i].condition, currentFrame);

            // 3. Temperatur (Unten, y=56)
            // Rundet die Floats auf ganze Zahlen und baut den String: z.B. "12/18"
            String tempStr = String((int)round(forecasts[i].temp)) + "/" + String((int)round(forecasts[i].tempMax));
            drawColText(display, cx, 56, tempStr, display.color565(100, 200, 255));
        }

        return true; 
    }

    int getPriority() override {
        return 1; // Standard-Priorität
    }
    
};