#pragma once
#include <Arduino.h>
#include "App.h"
#include "WeatherRenderer.h"

class WeatherApp : public App {
private:
    WeatherRenderer renderer;
    int currentFrame = 0;
    unsigned long lastFrameTime = 0;
    
    // NEU: Da wir 16 Frames haben, senken wir das Delay für eine butterweiche Animation
    const int frameDelay = 80; // Millisekunden pro Frame

    // Demo-Modus Variablen
    unsigned long lastDemoSwitch = 0;
    int demoIndex = 0;
    String conditions[13] = {
        "sunny", "clear-night", "cloudy", "partlycloudy", "partlycloudy-night",
        "rainy", "pouring", "snowy", "lightning", "lightning-rain", 
        "windy", "fog", "exceptional"
    };

    String currentCondition = "demo"; // Startet im Demo-Modus

public:
    WeatherApp() {}

    // Setup wird einmalig beim Start von der Matrix_OS.ino aufgerufen
    void setup() {
        lastFrameTime = millis();
        lastDemoSwitch = millis();
    }

    // Die überschriebene draw-Funktion der App.h
    bool draw(DisplayManager& display, bool force) override {
        bool needsRedraw = force;

        // Animations-Taktgeber (NEU: 16 Frames!)
        if (millis() - lastFrameTime >= frameDelay) {
            currentFrame = (currentFrame + 1) % 16; // <--- Umschlagpunkt jetzt bei 16
            lastFrameTime = millis();
            needsRedraw = true; 
        }

        // Demo-Modus: Alle 5 Sekunden umschalten, damit der Loop voll durchläuft
        if (currentCondition == "demo") {
            if (millis() - lastDemoSwitch >= 5000) {
                demoIndex = (demoIndex + 1) % 13;
                lastDemoSwitch = millis();
                currentFrame = 0;
                needsRedraw = true;
            }
        }

        // CPU schonen, wenn nichts gezeichnet werden muss
        if (!needsRedraw) return false;

        // Display für das neue Frame leeren
        display.clear(); 

        String drawCondition = (currentCondition == "demo") ? conditions[demoIndex] : currentCondition;

        // Wir zeichnen das Icon zentriert in 24x24
        int iconSize = 24;
        int x = (M_WIDTH - iconSize) / 2;
        int y = (M_HEIGHT - iconSize) / 2;

        renderer.drawWeatherIcon(display, x, y, iconSize, drawCondition, currentFrame);

        // Im Demo-Modus blenden wir den Namen der Bedingung unten klein ein
        if (currentCondition == "demo") {
            display.drawString(2, M_HEIGHT - 10, drawCondition, display.color565(100, 100, 100));
        }

        return true; 
    }

    // MQTT Steuerung
    void setCondition(String cond) {
        currentCondition = cond;
        currentFrame = 0;
    }

    int getPriority() override {
        return 1; // Standard-Prio
    }
};