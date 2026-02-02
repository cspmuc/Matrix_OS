#pragma once
#include "App.h"
#include <time.h>

class WordClockApp : public App {
private:
    const char* stundenNamen[13] = {"Zwoelf", "Eins", "Zwei", "Drei", "Vier", "Fuenf", "Sechs", "Sieben", "Acht", "Neun", "Zehn", "Elf", "Zwoelf"};

public:
    void draw(DisplayManager& display) override {
        struct tm ti;
        if(!getLocalTime(&ti)) return;
        int h = ti.tm_hour, m = ti.tm_min, mR = (m / 5) * 5; 
        int s = h % 12, nextS = (s + 1) % 12;

        uint16_t dotCol = display.color565(255, 255, 255);
        
        // Ecken
        if (m % 5 >= 1) display.fillRect(0, 0, 2, 2, dotCol);
        if (m % 5 >= 2) display.fillRect(M_WIDTH - 2, 0, 2, 2, dotCol);
        if (m % 5 >= 3) display.fillRect(M_WIDTH - 2, M_HEIGHT - 2, 2, 2, dotCol);
        if (m % 5 >= 4) display.fillRect(0, M_HEIGHT - 2, 2, 2, dotCol);

        String z0 = "Es ist", z1 = "", z2 = "", z3 = "";
        String st[12]; 
        for(int i=0; i<12; i++) st[i] = stundenNamen[i+1]; // Mapping anpassen oder direkt Array nutzen

        // Logik wie gehabt...
        // Vereinfachter Zugriff auf stundenNamen Array von oben
        const char* s_curr = stundenNamen[(s==0)?12:s];
        const char* s_next = stundenNamen[(nextS==0)?12:nextS];

        if (mR == 0)       { z1 = "Punkt"; z2 = (s == 1) ? "Eins" : s_curr; z3 = "Uhr"; }
        else if (mR == 5)  { z1 = "fuenf"; z2 = "nach"; z3 = s_curr; }
        else if (mR == 10) { z1 = "zehn"; z2 = "nach"; z3 = s_curr; }
        else if (mR == 15) { z1 = "Viertel"; z2 = "nach"; z3 = s_curr; }
        else if (mR == 20) { z1 = "zwanzig"; z2 = "nach"; z3 = s_curr; }
        else if (mR == 25) { z1 = "fuenf vor"; z2 = "halb"; z3 = s_next; }
        else if (mR == 30) { z1 = "halb"; z2 = s_next; }
        else if (mR == 35) { z1 = "fuenf nach"; z2 = "halb"; z3 = s_next; }
        else if (mR == 40) { z1 = "zwanzig"; z2 = "vor"; z3 = s_next; } 
        else if (mR == 45) { z1 = "Drei-"; z2 = "viertel"; z3 = s_next; }
        else if (mR == 50) { z1 = "zehn"; z2 = "vor"; z3 = s_next; }
        else if (mR == 55) { z1 = "fuenf"; z2 = "vor"; z3 = s_next; }

        srand(h * 60 + mR);
        int gY = (rand() % 5) - 2;
        display.setTextSize(1);
        display.setTextColor(display.color565(255, 120, 0));
        int y = 10 + gY;
        
        display.printCentered(z0, y); 
        if (z1 != "") display.printCentered(z1, y + 12);
        if (z2 != "") display.printCentered(z2, y + 24);
        if (z3 != "") display.printCentered(z3, y + 36);
    }
};