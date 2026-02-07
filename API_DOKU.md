# Matrix OS - API & Parameter Dokumentation

## Fonts
https://github.com/olikraus/u8g2/wiki/fntlistall

Diese Datei dient als Schnellanleitung für die MQTT-Steuerung und die Text-Formatierung.

---

## 1. MQTT Befehle
**Basis Topic:** `matrix/cmd/...`

### Apps umschalten
* **Topic:** `matrix/cmd/app`
* **Payload (JSON):** `{"app": "APPNAME"}`
* **Mögliche Apps:**
    * `wordclock` (Standard, Wortuhr)
    * `sensors` (Dashboard für Sensor-Seiten)
    * `ticker` (Lauftext Demo)
    * `plasma` (Bunter Hintergrund)
    * `testpattern` (Pixel Test)
    * `off` (Display schwarz, WLAN bleibt aktiv)

### Helligkeit
* **Topic:** `matrix/cmd/brightness`
* **Payload (JSON):** `{"val": 150}` (Bereich: 0 - 255)

### Power (Schnellschalter)
* **Topic:** `matrix/cmd/power`
* **Payload (String):** `ON` oder `OFF`

### Sensor Dashboard (SensorApp)
Das System unterstützt mehrere Seiten, die automatisch rotieren.
* **Topic:** `matrix/cmd/sensor_page`
* **Payload (JSON):**
    ```json
    {
      "id": "wohnzimmer",       // Eindeutige ID (Überschreibt existierende Seite)
      "title": "RAUMKLIMA",     // Überschrift der Seite
      "ttl": 60,                // Lebensdauer in Sekunden (Seite wird gelöscht wenn keine Updates kommen)
      "items": [                // Liste der Werte (1 bis 4 Stück)
        { "icon": "sun",  "text": "22.5°C", "color": "white" },
        { "icon": "rain", "text": "45%",    "color": "blue" }
      ]
    }
    ```
* **Layout-Automatik:**
    * **1 Item:** Große Darstellung (Big Mode).
    * **2 Items:** Liste untereinander (List Mode).
    * **3-4 Items:** Raster (Grid Mode).
* **Rotation:** Sende Nachrichten mit *unterschiedlichen* `id`s (z.B. "innen", "aussen"), um mehrere Seiten anzulegen. Das Display wechselt alle 5 Sekunden durch.

### Overlay / Benachrichtigung
Zeigt eine Nachricht über der aktuellen App an (mit "Glass-Effekt").
* **Topic:** `matrix/cmd/overlay`
* **Payload (JSON):**
    ```json
    {
      "msg": "Hallo {heart}",    // Text mit RichText Tags
      "duration": 5,             // (Optional) Anzeigedauer in Sekunden (Standard: 5)
      "color": "gold",           // (Optional) Farbe des Rahmens/Textes (Standard: white)
      "speed": 30                // (Optional) Scroll-Speed in Pixel/Sekunde (Standard: 30)
    }
    ```
* **Hinweis:**
    * Kurzer Text = Zentrierte Box.
    * Langer Text = Automatischer Lauftext (Banner).

---

## 2. MQTT Status (Rückkanal)
Das System sendet Status-Updates, wenn sich etwas ändert.

* `matrix/status` -> `ON` / `OFF`
* `matrix/status/app` -> Aktueller App-Name
* `matrix/status/brightness` -> Aktueller Wert

---

## 3. RichText Engine (Formatierung)
Diese Tags können in **allen** Texten (Overlay, SensorApp, Ticker) verwendet werden.

### Formatierung
* `{b}` = **Fett** (Umschalter an/aus)
* `{u}` = _Unterstrichen_ (Umschalter an/aus)

### Farben `{c:NAME}`

**Neon (Knallig):**
* `pink`, `cyan`, `lime`, `purple`, `orange`, `magenta`

**Pastell (Soft):**
* `rose`, `sky`, `mint`, `lavender`, `peach`, `lemon`

**Standard & UI:**
* `white`, `red`, `green`, `blue`
* `highlight` (Orange), `warn` (Rot), `success` (Grün), `info` (Blau), `muted` (Dunkelgrau)
* `gold`, `silver` (Premium-Look)
* `warm` (Gelb-Orange), `cold` (Eisblau)

**Benutzerdefiniert:**
* `{c:#FF00FF}` (Hex-Code)

### Icons `{NAME}`
* **Wetter:** `sun`, `cloud`, `rain`, `snow`
* **Symbole:** `heart`, `star`, `check`, `smile`
* **Misc:** `music`, `phone`, `arrow_u` (Hoch), `arrow_d` (Runter)

---

## 4. Beispiele

**Home Assistant Automation (Sensor Page):**
```yaml
action: mqtt.publish
data:
  topic: matrix/cmd/sensor_page
  payload: >-
    {
      "id": "klima",
      "title": "WOHNZIMMER",
      "ttl": 120,
      "items": [
        { "icon": "sun", "text": "{{ states('sensor.temp') }}°C", "color": "white" },
        { "icon": "cloud", "text": "{{ states('sensor.co2') }} ppm", "color": "{% if states('sensor.co2')|int > 1000 %}warn{% else %}mint{% endif %}" }
      ]
    }