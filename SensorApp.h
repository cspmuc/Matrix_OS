#pragma once
#include "App.h"

class SensorApp : public App {
private:
    String currentTemp = "--.-";
    String currentHum = "--";

public:
    void draw(DisplayManager& display) override {
        // Schönes Design für Sensordaten
        display.setTextColor(display.color565(0, 255, 0)); // Grün
        display.printCentered("WOHNZIMMER", 5);
        
        display.setTextColor(display.color565(255, 255, 255)); // Weiß
        String t = currentTemp + " C";
        display.printCentered(t, 25); 
        
        display.setTextColor(display.color565(0, 0, 255)); // Blau
        String h = currentHum + " %";
        display.printCentered(h, 45);
    }

    // WICHTIG: const String& spart Speicher (keine Kopie)
    void setData(const String& temp, const String& hum) {
        currentTemp = temp;
        currentHum = hum;
    }
};