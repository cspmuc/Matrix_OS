#pragma once
#include "DisplayManager.h"

class App {
public:
    virtual ~App() {}
    
    // Zeichnet das Bild. True = hat sich geändert, False = statisch
    virtual bool draw(DisplayManager& display, bool force) = 0;

    // Wird aufgerufen, wenn die App in den Vordergrund wechselt
    virtual void onActive() {} 
    
    // NEU: Gibt dem Auto-Modus das Signal zum Wechseln. 
    // Empfängt einen Multiplikator (z.B. 0.4 für 40% der Zeit)
    virtual bool isReadyToSwitch(float durationMultiplier = 1.0) { return false; }

    // NEU: Globale Prio-Abfrage. Jede Standard-App hat Prio 3 (Normal).
    virtual int getPriority() { return 3; }
};