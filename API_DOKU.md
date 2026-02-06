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
    * `wordclock` (Standard)
    * `sensors` (Wetter/Temp Anzeige)
    * `ticker` (Lauftext Demo)
    * `plasma` (Bunter Hintergrund)
    * `testpattern` (Pixel Test)
    * `off` (Display schwarz, aber WLAN aktiv)

### Helligkeit
* **Topic:** `matrix/cmd/brightness`
* **Payload (JSON):** `{"val": 150}` (Bereich: 0 - 255)

### Power (Schnellschalter)
* **Topic:** `matrix/cmd/power`
* **Payload (String):** `ON` oder `OFF`

### Sensor Daten senden (für SensorApp)
* **Topic:** `matrix/sensor`
* **Payload (JSON):** `{"temp": "22.5", "hum": "45"}`

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
* **Hinweis:** * Kurzer Text = Zentrierte Box.
    * Langer Text = Automatischer Lauftext (Banner).
    * Warteschlange: Max. 5 Nachrichten werden gepuffert.

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
* **Premium:** `gold`, `silver`
* **UI:** `highlight` (Orange), `warn` (Rot), `success` (Grün), `info` (Blau), `muted` (Grau)
* **Standard:** `white`, `red`, `green`, `blue`
* **Wetter:** `warm` (Gelb-Orange), `cold` (Eisblau)
* **Hex-Code:** `{c:#FF00FF}` (Benutzerdefiniert)

### Icons `{NAME}`
* **Wetter:** `sun`, `cloud`, `rain`, `snow`
* **Symbole:** `heart`, `star`, `check`, `smile`
* **Misc:** `music`, `phone`, `arrow_u` (Hoch), `arrow_d` (Runter)

### Schriftgrößen (Intern)
Wenn im Code Schriftgrößen verlangt werden:
* `"Small"` = Helvetica 10 (ca. 14px Zeilenhöhe)
* `"Medium"` = Helvetica 12 (ca. 16px Zeilenhöhe)
* `"Large"` = Helvetica 18 (ca. 24px Zeilenhöhe)


Name,Symbol,Unicode,Beschreibung
Wetter,,,
sun,☀,\u2600,Sonne (Black Sun with Rays)
cloud,☁,\u2601,Wolke
rain,☂,\u2602,Regenschirm
snow,☃,\u2603,Schneemann
UI & Status,,,
heart,♥,\u2665,Herz (Black Heart Suit)
star,★,\u2605,Stern (Black Star)
check,✓,\u2713,Häkchen
smile,☺,\u263A,Smiley (White Smiling Face)
Misc,,,
music,♫,\u266B,Musiknoten (Beamed Eighth Notes)
phone,☎,\u260E,Telefon (Black Telephone)
arrow_u,↑,\u2191,Pfeil nach oben
arrow_d,↓,\u2193,Pfeil nach unten

---

## 4. Beispiele
**Klingel-Benachrichtigung (Home Assistant):**
`{"msg": "{c:gold}Es klingelt! {u}Tür{u} öffnen? {smile}", "duration": 8}`

**Müll-Erinnerung:**
`{"msg": "Morgen: {c:blue}Papiertonne {c:white}rausstellen!", "color": "info"}`