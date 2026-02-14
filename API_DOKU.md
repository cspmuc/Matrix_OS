# Matrix OS - System & API Dokumentation

Diese Dokumentation beschreibt die Funktionsweise, Konfiguration und Steuerung der Matrix OS Firmware.

## Inhaltsverzeichnis
1. [RichText Engine & Icons](#1-richtext-engine--icons)
2. [Dateisystem & Konfiguration](#2-dateisystem--konfiguration)
3. [MQTT Schnittstelle](#3-mqtt-schnittstelle)
4. [Web Interface](#4-web-interface)
5. [Ordnerstruktur](#5-ordnerstruktur)

---

## 1. RichText Engine & Icons

Das Herzstück der Anzeige ist die `RichText` Engine. Sie ermöglicht es, formatierten Text, Farben und Icons aus verschiedenen Quellen (Lokal, Schriftart, Internet) zu mischen. Diese Tags funktionieren überall: im **Lauftext (Ticker)**, in **Overlays** und auf **Sensor-Seiten**.

### Icon Syntax (Neues System)
Das System nutzt Präfixe, um genau zu steuern, woher ein Icon geladen wird.

| Tag | Typ | Beschreibung | Beispiel |
| :--- | :--- | :--- | :--- |
| `{ti:name}` | **T**ext **I**con | Vektor-Icon aus der installierten Schriftart (Monochrom, skalierbar). | `{ti:sun}`, `{ti:wifi}` |
| `{ic:name}` | **Ic**on Sheet | Lädt ein Bitmap-Icon aus einem lokalen Sprite-Sheet (z.B. `dotto1.bmp`), definiert in `catalog.json`. | `{ic:smile}`, `{ic:ghost}` |
| `{ln:ID}` | **L**ametric **N**umber | Lädt ein Icon anhand der ID **direkt aus der LaMetric Cloud**. Wird beim ersten Aufruf heruntergeladen und lokal gespeichert. | `{ln:2356}`, `{ln:627}` |
| `{lt:name}` | **L**ametric **T**ag | Nutzt einen Alias aus der `catalog.json`, der auf eine LaMetric ID verweist. | `{lt:wetter}`, `{lt:youtube}` |

* **Legacy Support:** Tags ohne Präfix (z.B. `{sun}`) werden standardmäßig als `{ti:sun}` (Text Icon) interpretiert.
* **Layout:** Nach jedem Icon (egal welcher Typ) wird automatisch **1 Pixel Abstand** eingefügt.
* **Skalierung:** LaMetric Icons (original 8x8 Pixel) werden automatisch pixel-perfekt auf 16x16 hochskaliert, um zur Schrifthöhe zu passen.
* **Fehlerbehandlung:** Kann ein Online-Icon nicht geladen werden (z.B. ID falsch oder kein WLAN), wird ein rotes "X" gezeichnet und das Icon auf eine Blacklist gesetzt, um das System nicht zu verlangsamen.

### Textformatierung
| Tag | Funktion |
| :--- | :--- |
| `{b}` | **Fett** (Umschalter an/aus) |
| `{u}` | _Unterstrichen_ (Umschalter an/aus) |

### Farben `{c:WERT}`
Setzt die Textfarbe für alle folgenden Zeichen.

**Vordefinierte Namen:**
* **Neon:** `pink`, `cyan`, `lime`, `purple`, `orange`, `magenta`
* **Pastell:** `rose`, `sky`, `mint`, `lavender`, `peach`, `lemon`
* **Basis:** `white`, `red`, `green`, `blue`
* **UI:** `highlight` (Orange), `warn` (Rot), `success` (Grün), `info` (Blau), `muted` (Dunkelgrau)
* **Metall:** `gold`, `silver`
* **Temperatur:** `warm`, `cold`

**Benutzerdefiniert:**
* `{c:#FF00FF}` (Hex-Code für exakte Farben)

---

## 2. Dateisystem & Konfiguration

Die Datei `/catalog.json` im Flash-Speicher steuert die Zuordnung von Namen zu Icons.

### Aufbau der `catalog.json`
```json
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
```
* **sheets:** Definiert große Bitmap-Dateien (Sprite Sheets), die lokal gespeichert sind.
* **icons:** Verknüpft einen Namen (für `{ic:name}`) mit einer Position in einem Sheet.
* **aliases:** Verknüpft einen Namen (für `{lt:name}`) mit einer LaMetric-ID (Zahl), die dann online geladen wird.

---

## 3. MQTT Schnittstelle

Das Gerät verbindet sich mit dem konfigurierten MQTT Broker.
**Basis Topic:** `matrix/cmd/...`

### Apps steuern
* **Topic:** `matrix/cmd/app`
* **Payload:** `{"app": "APPNAME"}`
* **Verfügbare Apps:**
    * `wordclock` (Wortuhr)
    * `sensors` (Dashboard)
    * `ticker` (Lauftext Demo)
    * `plasma` (Grafik Demo)
    * `testpattern` (Pixel Test)
    * `off` (Display aus, System läuft weiter)

### Helligkeit
* **Topic:** `matrix/cmd/brightness`
* **Payload:** `{"val": 150}` (0-255)

### Power (Schnellschalter)
* **Topic:** `matrix/cmd/power`
* **Payload:** `ON` oder `OFF` (Schaltet Helligkeit auf 150 oder 0)

### Overlay Nachricht (Popup)
Zeigt eine Nachricht über der aktuellen App an.
* **Topic:** `matrix/cmd/overlay`
* **Payload:**
    ```json
    {
      "msg": "Hallo {lt:wetter}", // Text inkl. Tags
      "duration": 5,              // Dauer in Sekunden
      "color": "gold",            // Rahmenfarbe
      "speed": 30,                // Scrollgeschwindigkeit
      "urgent": false             // true = Roter Alarm-Rahmen, löscht Queue
    }
    ```

### Sensor Dashboard
Sendet Daten an die `SensorApp`. Das Display rotiert automatisch alle 5 Sekunden durch alle empfangenen Seiten.
* **Topic:** `matrix/cmd/sensor_page`
* **Payload:**
    ```json
    {
      "id": "raum1",            // Eindeutige ID der Seite
      "title": "WOHNZIMMER",    // Überschrift
      "ttl": 60,                // Timeout in Sek. (Löscht Seite bei Inaktivität)
      "items": [                // 1 bis 4 Elemente
        { "icon": "ti:sun", "text": "22°C", "color": "white" },
        { "icon": "ln:872", "text": "45%",  "color": "blue" }
      ]
    }
    ```
* **Layouts:** Das System wählt das Layout automatisch anhand der Anzahl der Items (Einzeln, Liste oder Grid).

### Status (Rückkanal)
Das System sendet Statusänderungen an:
* `matrix/status` -> `ON`/`OFF`
* `matrix/status/app` -> Aktueller App-Name
* `matrix/status/brightness` -> Aktueller Helligkeitswert

---

## 4. Web Interface

Erreichbar unter `http://[IP-ADRESSE]/`

* **Dateimanager:**
    * Anzeige aller Dateien und Ordner.
    * Navigation in Unterordner (z.B. `/icons/`) ist möglich.
    * Hochladen von Dateien (z.B. neue `catalog.json` oder `.bmp` Sheets).
    * Löschen von Dateien.
    * **Auto-Cleanup:** Fehlgeschlagene Uploads (z.B. wenn der Speicher vollläuft) werden automatisch wieder gelöscht, um keine korrupten Dateien zu hinterlassen.
* **Formatierung:** Button zum kompletten Löschen des internen Speichers (Vorsicht!).

---

## 5. Ordnerstruktur

Das Dateisystem (LittleFS) ist wie folgt aufgebaut:

* `/` (Root)
    * `config.h`, `secrets.h` (Optional, falls config file basierend)
    * `catalog.json` (Zentrale Datenbank für Icon-Mapping und Aliases)
    * `dotto1.bmp` (Beispiel für ein lokales Icon Sheet)
* `/icons/`
    * Hier werden automatisch heruntergeladene LaMetric Icons gespeichert.
    * Benennung: `[ID].bmp` (z.B. `2356.bmp`).
    * Format: 32-Bit BMP (inkl. Alpha-Kanal).