#pragma once
#include "App.h"
#include "RichText.h" 

class SensorApp : public App {
private:
    RichText richText;
    String currentTemp = "--.-";
    String currentHum = "--";

public:
    void draw(DisplayManager& display) override {
        
        // --- Header (Small) ---
        // "Small" ist Helvetica 10.
        // Wir aktivieren Grün ({c:success}) und Unterstreichung ({u}).
        richText.drawCentered(display, 11, "{c:success}{u}WO{u}hnzim{b}mer", "Small");
        
        // --- Temperatur (Large) ---
        // "Large" ist Helvetica 18.
        // {c:warm} = Gelbe Sonne. {sun} = Icon. {c:white} = Weißer Text. {b} = Fett.
        String tString = "{c:warm}{sun} {c:white}{b}" + currentTemp + "° {b}heute";
        richText.drawCentered(display, 40, tString, "Large");
        
        // --- Feuchte (Medium) ---
        // "Medium" ist Helvetica 12.
        // {c:warn} = Rotes Herz. {c:info} = Blauer Text.
        // Hinweis: Hier KEIN {b}, also normaler Font (Regular).
        String hString = "{c:warn}{heart} {c:info}" + currentHum + "{b}%";
        richText.drawCentered(display, 62, hString, "Medium");
    }

    void setData(const String& temp, const String& hum) {
        currentTemp = temp;
        currentHum = hum;
    }
};