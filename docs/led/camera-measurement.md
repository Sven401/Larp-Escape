# USB-Kamera als LED-Messinstrument

Status, 18.10.2026: **Messwerkzeug-Prototyp; reale Bildaufnahme belegt,
vollständige automatische Geräteabnahme offen.** Quellbasis `fcb9121` plus
die Dateien dieses PRs. Kein Firmwareupload und keine neue Appinstallation.

## Messkette

1. Die echte Engine [LedOutput.cpp](../../firmware/led/src/LedOutput.cpp)
   exportiert auf dem Host ihre eingereichten RGB-Frames. Keine zweite
   Effektimplementierung als vermeintlich unabhängige Referenz.
2. `record` speichert verlustfreie PNGs, SHA-256, Kameraeinstellungen und
   monotone Empfangszeiten. Zeitstempel bezeichnen den Empfang am Rechner,
   **nicht** die Belichtung oder den tatsächlichen Firmwarestart.
3. `locate` findet helle Bildregionen in einem zeitlichen Maximum und erstellt
   ein beschriftetes Bild. Kandidat C0 ist **kein** Firmwareindex 0.
4. Nach unabhängig geprüfter Indexzuordnung liefert `extract` je LED und Bild
   RGB-Mittelwerte aus einer Kreisregion sowie den Anteil gesättigter Pixel.
5. `compare` vergleicht mit explizit vorgegebenem Startzeitversatz die zeitliche
   Helligkeitsform je LED. Ergebnis: Korrelation oder unbewertbar bei zu geringer
   Änderung. Keine automatische Gut-/Schlecht-Freigabe, kein Farb- oder
   absoluter Helligkeitsnachweis; Dunkel-/Stopnachweise sind noch zu ergänzen.

Für einen belastbaren Gesamtvergleich fehlen noch: installierter Firmwarehash,
geprüfte Zuordnung, Dunkel-/Farb-/Helligkeitskalibrierung, synchronisierte
Auftrags-/Antwortspur und validierte Fehlertoleranzen. Ein Transport-ACK oder
RMT-Abschluss ersetzt kein Kamerabild; siehe [Wire V2](ble-wire-v2.md).

## Wiederholen

Gemessene Umgebung: Windows, Python 3.12, DirectShow, OpenCV 4.13.0.
Abhängigkeiten in einer eigenen lokalen Umgebung installieren:

```powershell
python -m pip install -r scripts/requirements-led-camera.txt
python scripts/led_camera.py record --output private/camera/run-001 --seconds 6 --camera 0 --backend dshow --auto-exposure 0.25 --exposure -8 --auto-wb 0
python scripts/led_camera.py locate --capture private/camera/run-001 --output private/camera/candidates-001.json
python scripts/led_camera_reference.py ripple --output private/camera/ripple.csv
```

Zielverzeichnisse und Ergebnisdateien dürfen noch nicht existieren; übergeordnete
Ordner bei CSV-/JSON-Ausgaben vorher anlegen. Rohbilder bleiben unter dem
ignorierten `private/` oder außerhalb Git. Skripte steuern keine LEDs und öffnen
keinen seriellen Port. Kamera indexbasiert: nach Umstecken oder Gerätewechsel
Identität und Bildausschnitt erneut prüfen. Unter Windows kann die Sandbox
DirectShow sperren; dieser Lauf brauchte genehmigten Zugriff außerhalb davon.

Aufbau fixieren: Kamera und Box markieren, Umgebungslicht konstant halten,
Fokus fest einstellen, alle LEDs getrennt sichtbar halten. Bei Bewegung oder
geändertem Bildformat neu kalibrieren. Die Kameraeinstellungen sind
treiberabhängig: akzeptiertes `set()` allein bestätigt keine manuelle Belichtung.
Readback und Referenzaufnahme gemeinsam prüfen. Gesättigte LED-Kerne erlauben
keinen belastbaren Helligkeitsvergleich; Belichtung weiter reduzieren und
erneut prüfen. Kamera-RGB ist durch Gamma, Weißabgleich, Optik und Sensor geprägt.

Kalibrierdatei: `schema: 1`, `mapping_status: "verified"`, `image_size: [640,480]`,
`leds` mit **23** Einträgen `{ "index": 0, "x": 100, "y": 100, "radius": 3 }`,
geordnet von Index 0 bis 22. Die Beispielkoordinate ist kein gemessener Index.
Quelle und unabhängigen Prüfweg der Zuordnung zusätzlich in dieser Datei nennen.
Eine Kandidatenliste wird vom Extraktor ausdrücklich nicht als Zuordnung akzeptiert.

```powershell
python scripts/led_camera.py extract --capture private/camera/run-001 --calibration private/camera/calibration.json --output private/camera/samples.csv
python scripts/led_camera.py compare --samples private/camera/samples.csv --reference private/camera/ripple.csv --offset-ms 1000 --output private/camera/comparison.json
```

`1000` ist ausschließlich ein Syntaxbeispiel: Der Versatz muss aus einer
unabhängigen Zeitzuordnung stammen. Positive Werte bedeuten Start nach Beginn
der Kameraaufnahme. Keine nachträgliche freie Phasenoptimierung als Zeitnachweis.
Die Referenz simuliert 6 s, 1-ms-Service und sofortigen Busabschluss. Vergleich
verwendet gehaltene Frames bis 6000 ms; echte Scheduling-/USB-/Kameralatenz ist
separat zu berücksichtigen. Bei 30 Bildern/s lässt sich der 33-ms-Firmwaretakt
nicht lückenlos beobachten; PWM, kurze Aussetzer und elektrische Signale brauchen
andere Messmittel. `max_gap_ms` ist ein Empfangsabstand, keine sichere Anzahl
verlorener Sensorbilder.

## Beobachteter Kameralauf

Quelle: vom Nutzer gestartetes Funkeln; private Rohbelege unter
`private/led-camera-probe/` im Hauptcheckout. Keine Firmwareidentität aus dem
sichtbaren Muster abgeleitet.

| Lauf | Befund |
| --- | --- |
| Erste Aufnahme, 640×480 | 124 Bilder in rund 5 s; 24,80 empfangene Bilder/s; größter Abstand 40,65 ms. |
| Angeforderte manuelle Belichtung −8, automatischer Weißabgleich aus | 90 Bilder in rund 3 s; 30,03 Bilder/s; größter Abstand 33,64 ms. Belichtung −8 und Auto-WB 0 zurückgelesen; Auto-Exposure bleibt −1 trotz akzeptiertem Auftrag: manuelle Automatikabschaltung nicht bestätigt. |
| Regionensuche im zweiten Lauf | 23 helle Kandidaten mit Schwelle 160 auf dem Minimum der drei Kanalmaxima, Flächenfilter 2–150 Pixel. Das ist eine Kandidatenzählung, noch kein Vollständigkeits-/Indexnachweis. Helle Kerne bleiben teilweise gesättigt. |

## Prüfungen und nächster Abnahmeschritt

```powershell
python -m unittest discover -s scripts/tests -p test_led_camera.py
python scripts/led_camera_reference.py twinkle --output private/camera/twinkle.csv
python scripts/led_camera_reference.py burst --output private/camera/burst.csv
python scripts/led_camera_reference.py ripple --output private/camera/ripple.csv
```

Drei Hosttests prüfen: ungültige Zuordnung/Format/Grenzen; Kanalreihenfolge,
Sättigung und manipulierte Rohbilder; positive/invertierte/dunkle Messkurven
und fehlende Zeitüberlappung. Alle drei Referenzexporte lokal kompiliert und
ausgeführt. Das ersetzt keinen Vergleich einer realen Messung mit Sollframes.

Nächster konkreter Schritt: Belichtung ohne Clipping festlegen, anschließend
eine einzeln adressierbare Index-/Farbsequenz mit Dunkelpausen und korrelierter
Startmarkierung vorbereiten. Damit 23 Positionen sowie Kamerareaktion eindeutig
kalibrieren und an einem getrennten Wiederholungslauf prüfen. Der aktuelle
Produktvertrag bietet nur Funkeln/Lichtimpuls/Ripple/Stop, keine Einzelpixel.
Ein zusätzlicher begrenzter Kalibrierpfad ist deshalb erst zu implementieren
und zu prüfen; ein Geräteupload benötigt den gesonderten Auftrag und den
chipgebundenen Rückweg aus der Hardwaredokumentation. Danach kontrollierte
Effektstarts und Fehlerbilder (falscher Index, fehlende LED, falsches Timing,
fehlendes Aus) verwenden, um Toleranzen zu validieren. Erst dann automatische
Abnahmeurteile freischalten.
