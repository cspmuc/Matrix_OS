#pragma once
#include <Arduino.h>
#include "App.h"
#include "WeatherRenderer.h"

class WeatherApp : public App {
private:
    WeatherRenderer renderer;
    int currentFrame = 0;
    unsigned long lastFrameTime = 0;
    const int frameDelay = 150; // Millisekunden pro Frame

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

    void setup() override {
        lastFrameTime = millis();
        lastDemoSwitch = millis();
    }

    void loop() override {
        // Animations-Taktgeber (8 Frames)
        if (millis() - lastFrameTime >= frameDelay) {
            currentFrame = (currentFrame + 1) % 8;
            lastFrameTime = millis();
        }

        // Demo-Modus: Alle 4 Sekunden umschalten
        if (currentCondition == "demo") {
            if (millis() - lastDemoSwitch >= 4000) {
                demoIndex = (demoIndex + 1) % 13;
                lastDemoSwitch = millis();
            }
        }
    }

    void draw(DisplayManager& display) override {
        String drawCondition = (currentCondition == "demo") ? conditions[demoIndex] : currentCondition;

        // Wir zeichnen das Icon zentriert in 24x24, damit du es gut bewerten kannst
        int iconSize = 24;
        int x = (M_WIDTH - iconSize) / 2;
        int y = (M_HEIGHT - iconSize) / 2;

        renderer.drawWeatherIcon(display, x, y, iconSize, drawCondition, currentFrame);

        // Im Demo-Modus blenden wir den Namen der Bedingung unten klein ein
        if (currentCondition == "demo") {
            display.drawText(drawCondition, 2, M_HEIGHT - 10, display.color565(100, 100, 100));
        }
    }

    // MQTT Steuerung (z.B. Payload: {"app":"weather", "cond":"rainy"} oder "demo")
    void setCondition(String cond) {
        currentCondition = cond;
        currentFrame = 0;
    }

    int getPriority() override {
        return 1; // Standard-Prio (läuft im Hintergrund, wenn keine Prio 2 App aktiv ist)
    }

    String getName() override {
        return "weather";
    }
};