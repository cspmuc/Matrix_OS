#pragma once
#include "DisplayManager.h"

class App {
public:
    virtual ~App() {}
    // Rückgabe true: Bild wurde aktualisiert -> Display.show() aufrufen
    // Rückgabe false: Bild ist statisch -> Keine Änderung am Display nötig
    // Parameter force: Erzwingt ein Neuzeichnen (z.B. wenn Overlay verschwindet)
    virtual bool draw(DisplayManager& display, bool force) = 0;
};