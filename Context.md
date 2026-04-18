Architektur & Entwickler-Richtlinien
Diese Datei gibt den Kontext für den KI Prompt und sind immer strikt zu befolgen.

Hardware:
Wir entwickeln Software für einen ESP Controller mit der Arduino IDE.
Es ist ein MatrixPortal S3 mit einem ESP32-S3. Flash Size 8MB. PSRAM 2MB.
Partition Scheme "TinyUF2 8MB (2MB APP, 3,7MB FATFS)"
Board "Adafruit MatrixPortal ESP32-S3"
Wir verwenden OTA Updates.
Angeschlossen sind zwei 64x64 HUB75 LED Matrix Displays, durchgeschleift, querformat 128x64
Wir nutzen zur Ansteuerung eine Lib ESP32-HUB75-MatrixPanel-I2S-DMA.h die die Bildaten im SRAM benötigt, werden per DMA über den I2S Controller ausgegeben, double buffering

Das System nennen wir MatrixOS. Es ist ein schickes Infodisplay. Die Daten werden hauptsächlich über HomeAssistant per MQTT zugespielt

Wegen den begrenzten Embedded Ressourcen muss Speicher gespart werden und auch die verfügbare CPU Leistung ist begrenzt.
Alles, wass nicht zwingend im knappen SRAM (Heap) liegen muss, soll im PSRAM liegen, wenn die Performance es auch zulässt. Libs die kurzfristig SRAM benötigen wie SSL oder wo wir den Speicher nicht beeinflussen können, sollen lauffähig bleiben und keinen OOM erzeugen.
* **String-Verkettung:** Vermeide globale `String`-Verkettungen (`String a = b + c;`). Diese fragmentieren den internen RAM.
Statische Daten sollten idealerweise fest ins Programm compiliert werden und damit im Flash liegen, wenn möglich.
Wichtig ist, dass das Display nur dann neu gezeichnet wird, wenn es auch notwendig ist. Die Notwendigkeit für eine Aktualisierung kann sich dynamisch ändern, wenn auf einmal ein animiertes Icon dargestellt wird, in Echtzeit neue Daten empfangen werden.
Die Apps und Overlays müssen also immer wissen wann ein Refresh notwendig ist. Auch muss man nicht tausend Mal in der Sekunde irgendwas neu berechnen, wenn sich nichts ändert.


Das System soll nicht blockierend sein. Auf Multithreading auf unserer User Code Basis verzichten wir, weil das aus Erfahrung noch mehr Probleme macht. Tasks mit längerer Rechenzeit dürfen Hintergrundtasks wie das System, WLAN, Netzwerk etc. nicht in Schwierigkeiten bringen.

Bestandteile wie Webserver, MQTT laufen im Hintergrund und sind erreichbar.

Wir nutzen das Dateisystem zur Speicherung der Konfigurationen und Daten wie Icons.

* **RichText Engine:** DAS GESAMTE Text-Rendering sollte die `RichText` Engine verwenden. 
  * Programmiere Layout-Koordinaten niemals hart (hardcoded), wenn `richText.drawCentered` oder `richText.getTextWidth` diese dynamisch berechnen können.
* **Nicht-blockierendes `draw()`:** Apps werden extrem schnell aufgerufen (bis zu 100fps). Die `draw()`-Funktion DARF KEIN `delay()` enthalten.
* **State Machines (Zustandsmaschinen) & Early Exits:** * Nutze die `needsRedraw`-Logik. Wenn sich optisch nichts verändert hat, gib sofort `return false;` zurück, um CPU-Zyklen für den WLAN-Stack zu sparen.
  * Verwende `millis()` für Timer und Animationen (z. B. `if (now - stateTimer > 1000)`).
* **Z-Index:** Achte auf die richtige Zeichenreihenfolge. Zeichne zuerst Hintergründe, dann den Mittelgrund (Gitter/Netze), dann dynamische Vordergrundobjekte (Bälle/Spieler) und als Letztes Overlays (RichText).

## 4. Anweisungen für den KI-Assistenten
Wenn du (die KI) Code für dieses Projekt änderst oder generierst, MUsst du dich strikt an folgende Regeln halten:
1. **Kein Raten:** Basiere alle Änderungen AUSSCHLIESSLICH auf den letzten bereitgestellten Code-Dateien. Es kann auch sein, dass ich selbst Änderungen im Code durchführe die du noch nicht kennst und das muss zwingend beachtet werden. Erfinde keine externen Abhängigkeiten und gehe nicht von Standard-Arduino-Gewohnheiten aus, wenn diese die obige Speicherarchitektur verletzen.
2. **Komplette Dateien:** Wenn du Code-Korrekturen lieferst, gib die GESAMTE Datei komplett und bereit zum Copy & Paste zurück. Verwende keine Platzhalter wie `// ... restlicher Code`.
3. **Erst erklären:** Erkläre immer zuerst den spezifischen technischen Grund für eine Änderung (mit Bezug auf den ESP32-Speicher, die State Machine oder die UI-Logik), bevor du den Code ausgibst.
4. **Schritt für Schritt:** Refaktoriere nicht das gesamte Projekt auf einmal. Konzentriere dich nur auf die spezifische Datei oder App, die der Benutzer angefragt hat. Entferne nicht einfach etwas, was mit der aktuellen Änderung nicht zusammenhängt.

Das System besteht aus mehreren Apps. Daten sollen im Hintergrund empfangen werden, damit sie bei Anzeige der App aktuell sind.

## 5. Leitfaden: Eine neue App erstellen (Checkliste)

Wenn eine neue App zum Matrix OS hinzugefügt wird, müssen folgende Architektur-Schritte strikt eingehalten werden:

### 5.1. Die App-Klasse erstellen (`NeueApp.h`)
Erstelle eine neue Header-Datei, die von der Basisklasse `App` erbt. Sie muss mindestens diese vier Methoden überschreiben:
* `onActive()`: Wird exakt einmal aufgerufen, wenn die App in den Vordergrund wechselt. Hier werden Variablen zurückgesetzt, Timer gestartet oder eine MQTT-Statusmeldung über den Start der App gesendet.
* `isReadyToSwitch(float durationMultiplier)`: Für den Auto-Modus. Gibt `true` zurück, wenn die App fertig ist (z. B. nach Ablauf einer konfigurierten Zeit).
* `draw(DisplayManager& display, bool force)`: Die eigentliche Zeichen-Logik. Wird in der Hauptschleife ständig aufgerufen.
* `getPriority()`: Gibt die Systempriorität der App zurück (z. B. 10 für Spiele, die nicht unterbrochen werden dürfen, 3 für Standard-Apps).

### 5.2. MQTT-Integration (Steuerung & Status-Rückmeldung)
* **Daten & Befehle empfangen:** Wenn die App Befehle oder Daten via MQTT benötigt, erstelle eine öffentliche Funktion in der Klasse (z. B. `void processMqttMessage(String topic, String payload)`). Diese Funktion muss im zentralen MQTT-Callback (`NetworkManager.h` oder `Matrix_OS.ino`) aufgerufen werden, wenn das relevante Topic abonniert und empfangen wurde.
* **Status melden:** Die App (oder das Hauptsystem beim Umschalten) muss Rückmeldung an den MQTT-Broker geben.
  * Beim Wechseln in die App: Ein Status-Publish über die aktuelle App (z. B. `matrix/current_app -> "NeueApp"`).
  * Bei Ereignissen: Wenn sich app-interne Zustände ändern (z. B. ein abgeschlossenes Spiel oder Sensor-Werte), publisht die App dies asynchron über die globale MQTT-Instanz (z.B. über eine Referenz auf `NetworkManager`).

### 5.3. App im System registrieren (`Matrix_OS.ino`)
Die Klasse muss dem Hauptsystem an exakt fünf Stellen bekannt gemacht werden:
1. **Include:** `#include "NeueApp.h"` im Kopfbereich hinzufügen.
2. **Globale Instanz:** Ein globales App-Objekt anlegen (z. B. `NeueApp appNeue;`).
3. **AppMode Enum:** Einen neuen Identifikator für die App vergeben (z. B. `NEUEAPP`).
4. **Hilfsfunktionen:** Die App in die Lambda-Funktionen `getAppModeByName` und `getAppInstance` eintragen. Das ist zwingend nötig, damit der Auto-Modus, die Web-API und MQTT-Steuerbefehle die App finden und instanziieren können.
5. **Render-Loop:** Im `draw`-Block der Hauptschleife (`switch(displayedApp)`) den Aufruf hinzufügen: `case NEUEAPP: screenUpdated = appNeue.draw(display, forceRedraw); break;`.
Die Logik, wie Apps sich untereinander verhalten, ganz einfach an der WortUhr, der Sensor und WetterApp abschauen.

