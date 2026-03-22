# Matrix OS - System & API Dokumentation

Diese Dokumentation beschreibt die Funktionsweise, Konfiguration und Steuerung der Matrix OS Firmware.

## Inhaltsverzeichnis
1. RichText Engine & Icons
2. Dateisystem & Konfiguration
3. MQTT Schnittstelle
4. Web Interface
5. Ordnerstruktur

---

## 1. RichText Engine & Icons

Das Herzstück der Anzeige ist die RichText Engine. Sie ermöglicht es, formatierten Text, Farben und Icons aus verschiedenen Quellen (Lokal, Schriftart, Internet) zu mischen. Diese Tags funktionieren überall: im Lauftext (Ticker), in Overlays und auf Sensor-Seiten.

### Icon Syntax (Neues System)
Das System nutzt Präfixe, um genau zu steuern, woher ein Icon geladen wird.

| Tag | Typ | Beschreibung | Beispiel |
| :--- | :--- | :--- | :--- |
| {ti:name} | Text Icon | Vektor-Icon aus der installierten Schriftart (Monochrom, skalierbar). | {ti:sun}, {ti:wifi} |
| {ic:name} | Icon Sheet | Lädt ein Bitmap-Icon aus einem lokalen Sprite-Sheet (z.B. dotto1.bmp), definiert in catalog.json. | {ic:smile}, {ic:ghost} |
| {ln:ID} | Lametric Number | Lädt ein Icon anhand der ID direkt aus der LaMetric Cloud. Wird beim ersten Aufruf heruntergeladen und lokal gespeichert. | {ln:2356}, {ln:627} |
| {lt:name} | Lametric Tag | Nutzt einen Alias aus der catalog.json, der auf eine LaMetric ID verweist. | {lt:wetter}, {lt:youtube} |

* Legacy Support: Tags ohne Präfix (z.B. {sun}) werden standardmäßig als {ti:sun} (Text Icon) interpretiert.
* Layout: Nach jedem Icon (egal welcher Typ) wird automatisch 1 Pixel Abstand eingefügt.
* Skalierung: LaMetric Icons (original 8x8 Pixel) werden automatisch pixel-perfekt auf 16x16 hochskaliert, um zur Schrifthöhe zu passen.
* Fehlerbehandlung: Kann ein Online-Icon nicht geladen werden (z.B. ID falsch oder kein WLAN), wird ein rotes "X" gezeichnet und das Icon auf eine Blacklist gesetzt, um das System nicht zu verlangsamen.

### Textformatierung
| Tag | Funktion |
| :--- | :--- |
| {b} | Fett (Umschalter an/aus) |
| {u} | Unterstrichen (Umschalter an/aus) |

### Farben {c:WERT}
Setzt die Textfarbe für alle folgenden Zeichen.

Vordefinierte Namen:
* Neon: pink, cyan, lime, purple, orange, magenta
* Pastell: rose, sky, mint, lavender, peach, lemon
* Basis: white, red, green, blue
* UI: highlight (Orange), warn (Rot), success (Grün), info (Blau), muted (Dunkelgrau)
* Metall: gold, silver
* Temperatur: warm, cold

Benutzerdefiniert:
* {c:#FF00FF} (Hex-Code für exakte Farben)

---

## 2. Dateisystem & Konfiguration

Die Konfiguration des Systems erfolgt über JSON-Dateien im Flash-Speicher.

### System-Konfiguration (config.json)
Steuert Netzwerk, MQTT, Systemeinstellungen und den Auto-Modus.
{
  "system": {
    "ota_password": "otaflash",
    "startup_brightness": 150
  },
  "auto": {
    "enabled": true,
    "wordclock_duration_sec": 20,
    "apps": ["wordclock", "sensors", "plasma"]
  }
}
(Hinweis: Netzwerk, MQTT und Zeit-Einstellungen sind ebenfalls in dieser Datei möglich, siehe ConfigManager-Code).

### Icon Katalog (catalog.json)
Die Datei /catalog.json steuert die Zuordnung von Namen zu lokalen Sheets oder LaMetric-IDs.
{
  "sheets": {
    "dotto": { 
      "file": "/dotto1.bmp", 
      "cols": 16, 
      "rows": 64 
    }
  },
  "icons": {
    "sun":     { "sheet": "dotto", "index": 0 },
    "rain":    { "sheet": "dotto", "index": 15 }
  },
  "aliases": {
    "wetter": 2356,
    "crypto": 4567,
    "alarm": 1234
  }
}

---

## 3. MQTT Schnittstelle

Das Gerät verbindet sich mit dem konfigurierten MQTT Broker.
Basis Topic: matrix/cmd/...

### Apps steuern
* Topic: matrix/cmd/app
* Payload: {"app": "APPNAME"}
* Verfügbare Apps:
    * wordclock (Wortuhr)
    * sensors (Dashboard)
    * ticker (Lauftext Demo)
    * plasma (Grafik Demo)
    * testpattern (Pixel Test)
    * off (Display aus, System läuft weiter)
    * auto (Startet die automatische App-Rotation)

### Helligkeit
* Topic: matrix/cmd/brightness
* Payload: {"val": 150} (0-255)

### Power (Schnellschalter)
* Topic: matrix/cmd/power
* Payload: ON oder OFF 
(Tipp: Das System merkt sich beim Ausschalten die letzte Helligkeit und stellt diese beim nächsten ON wieder her. Ist keine bekannt, wird der startup_brightness Wert aus der config.json genutzt).

### Overlay Nachricht (Popup)
Zeigt eine Nachricht über der aktuellen App an.
* Topic: matrix/cmd/overlay
* Payload:
    {
      "msg": "Hallo {lt:wetter}",
      "duration": 5,
      "color": "gold",
      "speed": 30,
      "urgent": false
    }

### Animationen (Vollbild-Effekte)
Zeigt eine Vollbild-Animation über dem abgedunkelten aktuellen Bild an.
* Topic: matrix/cmd/animation
* Payload:
    {
      "anim": "ghost_eyes",
      "duration": 3
    }

### Sensor Dashboard & Prioritäten-System
Sendet Daten an die SensorApp. Das Display rotiert automatisch durch alle empfangenen Seiten.
* Topic: matrix/cmd/sensor_page
* Payload:
    {
      "id": "raum1",
      "title": "WOHNZIMMER",
      "ttl": 60,
      "priority": 2,
      "items": [
        { "icon": "ti:sun", "text": "22°C", "color": "white" },
        { "icon": "ln:872", "text": "45%",  "color": "blue" }
      ]
    }
* Layouts: Das System wählt das Layout automatisch anhand der Anzahl der Items (Einzeln, Liste oder Grid).

Das Prioritäten-System:
* Prio 3 (Normal): Die Seite wird regulär (z.B. 7 Sek.) angezeigt. Punkt-Indikator: Weiß.
* Prio 2 (Wichtig): Die Seite wird 50% länger angezeigt. Punkt-Indikator: Gelb. Laufende passive Apps (wie die Uhr) reduzieren ihre Anzeigedauer auf 60%, um schneller Platz zu machen.
* Prio 1 (Alarm): Die Seite wird 100% länger angezeigt (doppelte Zeit). Punkt-Indikator: Rot. Laufende passive Apps reduzieren ihre Anzeigedauer auf 40%.

### Status (Rückkanal)
Das System sendet Statusänderungen an:
* matrix/status -> ON/OFF
* matrix/status/app -> Aktueller App-Name (z.B. "auto")
* matrix/status/brightness -> Aktueller Helligkeitswert

---

## 4. Web Interface

Erreichbar unter http://[IP-ADRESSE]/ oder http://[HOSTNAME].local/

* Dateimanager:
    * Anzeige aller Dateien und Ordner.
    * Navigation in Unterordner (z.B. /icons/) ist möglich.
    * Hochladen von Dateien (z.B. neue catalog.json oder .bmp Sheets).
    * Löschen von Dateien.
    * Auto-Cleanup: Fehlgeschlagene Uploads (z.B. wenn der Speicher vollläuft) werden automatisch wieder gelöscht, um keine korrupten Dateien zu hinterlassen.
* Formatierung: Button zum kompletten Löschen des internen Speichers (Vorsicht!).
* Neustart: Button zum Durchführen eines sauberen Reboots des ESP32.

---

## 5. Ordnerstruktur

Das Dateisystem (LittleFS) ist wie folgt aufgebaut:

* / (Root)
    * config.json (System-Einstellungen)
    * catalog.json (Zentrale Datenbank für Icon-Mapping und Aliases)
    * dotto1.bmp (Beispiel für ein lokales Icon Sheet)
* /icons/
    * Hier werden automatisch heruntergeladene statische LaMetric Icons gespeichert.
    * Benennung: [ID].bmp (z.B. 2356.bmp).
    * Format: 32-Bit BMP (inkl. Alpha-Kanal).
* /iconsan/
    * Speicher für animierte Icons.
    * Enthält jeweils eine .bmp (Sprite Sheet) und eine .dly (Timing-Informationen) Datei.