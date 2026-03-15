#pragma once
#include "DisplayManager.h"

class App {
public:
    virtual ~App() {}
    
    // Zeichnet das Bild. True = hat sich geändert, False = statisch
    virtual bool draw(DisplayManager& display, bool force) = 0;

    // --- NEU FÜR AUTO-MODUS ---
    // Wird aufgerufen, wenn die App in den Vordergrund wechselt
    virtual void onActive() {} 
    
    // Gibt dem Auto-Modus das Signal: "Meine Show ist fertig, du darfst wechseln!"
    virtual bool isReadyToSwitch() { return false; }
};