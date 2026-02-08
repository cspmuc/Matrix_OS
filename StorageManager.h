#pragma once
#include "config.h"

// Externe Status-Funktion nutzen
extern void status(const String& msg, uint16_t color);
extern DisplayManager display; // Für Farben

class StorageManager {
public:
    bool begin() {
        Serial.println("Storage: Mounting LittleFS...");
        
        // Versuch 1: Normal mounten
        if (LittleFS.begin(false)) {
            Serial.println("Storage: Mounted successfully.");
            status("FS Mounted", display.color565(0, 255, 0));
            listDir("/");
            return true;
        }

        // Wenn wir hier sind, ist das FS korrupt oder leer
        Serial.println("Storage: Mount failed. Formatting...");
        status("Formatting...", display.color565(255, 0, 0));
        
        // Das kann 30-60 Sekunden dauern!
        // Parameter true = formatOnFail
        if (LittleFS.begin(true)) {
            Serial.println("Storage: Formatted & Mounted.");
            status("FS Formatted", display.color565(0, 255, 0));
            delay(1000); 
            return true;
        }

        Serial.println("Storage: CRITICAL FAILURE");
        status("FS Error!", display.color565(255, 0, 0));
        return false;
    }

    void listDir(const char * dirname){
        Serial.printf("Listing directory: %s\n", dirname);
        File root = LittleFS.open(dirname);
        if(!root){
            Serial.println("Failed to open directory");
            return;
        }
        File file = root.openNextFile();
        while(file){
            if(file.isDirectory()){
                Serial.print("  DIR : "); Serial.println(file.name());
            } else {
                Serial.print("  FILE: "); Serial.print(file.name());
                Serial.print("  SIZE: "); Serial.println(file.size());
            }
            file = root.openNextFile();
        }
    }
};