#pragma once
#include "config.h"

extern void status(const String& msg, uint16_t color);
extern DisplayManager display; 

class StorageManager {
public:
    bool begin() {
        Serial.println("Storage: Mounting LittleFS...");
        
        for (int i = 0; i < 3; i++) {
            if (LittleFS.begin(false)) {
                Serial.println("Storage: Mounted successfully.");
                status("FS Mounted", display.color565(0, 255, 0));
                listDir("/");
                return true;
            }
            
            Serial.print("Storage: Mount attempt "); 
            Serial.print(i + 1); 
            Serial.println(" failed. Retrying...");
            delay(500); // Kurz warten und Hardware beruhigen lassen
        }

        // Erst wenn 3x Mounten fehlschlug, gehen wir von einem leeren/korrupten FS aus.
        Serial.println("Storage: Mount failed completely. Formatting...");
        status("Formatting...", display.color565(255, 0, 0));
        
        // Formatieren (formatOnFail = true)
        if (LittleFS.begin(true)) {
            Serial.println("Storage: Formatted & Mounted.");
            status("FS Formatted", display.color565(0, 255, 0));
            
            // Wichtig: Nach Formatierung eine Info-Datei schreiben, damit wir wissen, dass es geklappt hat
            File f = LittleFS.open("/info.txt", "w");
            if(f) { f.print("Formatted by MatrixOS"); f.close(); }
            
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
        if(!root) return;
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