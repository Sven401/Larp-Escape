# USB-Kamera als LED-Messinstrument

Für frühes gestalterisches Feedback gibt es zusätzlich die
[interaktive Entwicklung mit seriellen Liveparametern](interactive-development.md).
Die folgende vollständige Mess-/Kalibrierroute bleibt für ihre eigenen
Abnahmekriterien maßgeblich; sie ist keine Vorbedingung jeder Gestaltungsprobe.

Stand: 18.10.2026. Implementierter Laborprüfweg für die 23 LEDs und die drei
Produkt-Effekte. [Gerätebefunde und Grenzen](camera-results-2026-10-18.md).
Quellbasis: `fcb9121` plus der Kamera-PR. Die ursprüngliche reine
Aufnahmevorbereitung steht in der Historie bei `fa878d9`.

Ergänzung für den vierten Effekt: [Resonanzkreis-Prüfweg mit synthetischen
Wake-Eingängen](resonance-camera-measurement.md) und
[getrennter Kamerabefund](resonance-camera-results-2026-10-18.md).

## Aufbau und Zuständigkeit

`larp-led-camera-lab` ist eine getrennte USB-Prüffirmware. Sie verwendet die
**echte** [Effektengine](../../firmware/led/src/LedOutput.cpp) und den tatsächlichen
[RMT-Adapter](../../firmware/led/src/RmtLedBus.cpp): GPIO15, 23 Pixel, GRB und
maximal 32/255 pro Farbkanal. Sie enthält keinen BLE-Zugang und keine
Audio-/Matrix-/NFC-Steuerung. Die Produktfirmware bekommt keine zusätzlichen
USB-Steuerbefehle. Ein Laborergebnis ersetzt keine Abnahme des kombinierten
App-/Produktfirmware-Paars, elektrischer Signale oder Versorgung.

Kamera und Box fixieren. Alle 23 Lichtflächen müssen getrennt sichtbar sein.
Bei Bewegung, anderer Kamera, geändertem Bildformat oder geänderter Optik neu
kalibrieren. Native Kameraindizes können sich nach Umstecken ändern.

## Wiederholen

Gemessene Umgebung: Windows, Python 3.12, DirectShow, OpenCV 4.13.0.
Abhängigkeiten in einer eigenen lokalen Python-Umgebung installieren:

```powershell
python -m pip install -r scripts/requirements-led-camera.txt
python -m platformio run -d firmware/led -e larp-led-camera-lab -j 2
```

Ein Geräteupload benötigt den konkreten Auftrag und einen vorher geprüften,
chipgebundenen Rückweg. Vorher **aktuellen** vollständigen Flash sichern und am
Chip verifizieren; Partitionstabelle und Anwendungsgrenze abgleichen. Nur die
Anwendung ab `0x10000` schreiben und verifizieren. Nach dem Versuch bisherige
Anwendung wiederherstellen und vollständigen 4-MiB-Flash gegen die frische
Sicherung verifizieren. Firmwaredatei, ELF, Quellstand und private Installations-
/Restoreprotokolle erhalten. [Historischer Rückwegbeleg](install-2026-10-15.md)
ist keine Bestätigung des heute angeschlossenen Chips.

Nach beauftragter Installation der Laborfirmware führt ein Befehl den ganzen
Messlauf aus; das Zielverzeichnis darf noch nicht existieren:

```powershell
python scripts/led_camera_suite.py --port COM3 --firmware .pio/led-product/larp-led-camera-lab/firmware.bin --output private/camera/run-001
```

Die Suite startet Übersicht, Regionssuche, Kalibrierung sowie Funkeln,
Lichtimpuls und Ripple **nacheinander**. Sie speichert Quellfingerabdrücke,
Kalibrierung, Einzelurteile und `suite.json`; ein negatives Urteil liefert
Exitcode 1. Keine Geräteuploads, automatischen Wiederholungen oder Überschreibungen.
Keine Builds oder Bildtests parallel zur Kameramessung ausführen. Unter Windows
benötigte dieser Aufbau genehmigten Zugriff außerhalb der Sandbox für Kamera,
seriellen Port und lokale Buildwerkzeuge.

Bei vorhandenem unverändertem Aufbau kann ein einzelner Effekt mit dem
passenden Regionslayout und der passenden Kalibrierung wiederholt werden:

```powershell
python scripts/led_camera_lab.py ripple --port COM3 --firmware .pio/led-product/larp-led-camera-lab/firmware.bin --rois private/camera/run-001/rois.json --output private/camera/ripple-002
python scripts/led_camera_assess.py assess --capture private/camera/ripple-002 --calibration private/camera/run-001/calibration.json --output private/camera/ripple-002-result.json
```

Standard: 1280×720, MJPG-Kameraformat, angeforderte 30 Bilder/s, Belichtung −7,
Verstärkung 0, automatische Belichtung/Weißabgleich aus, Gegenlichtkorrektur 0,
Schärfung 0 und Gamma 100. Angeforderte, akzeptierte und **nach allen Änderungen**
zurückgelesene Werte werden protokolliert und beim Vergleich abgeglichen.
DirectShow setzt beim Ändern der Bildrate den Stream neu auf; FourCC muss deshalb
zuletzt gesetzt werden. Der Getter für Auto-Exposure ist dort nicht implementiert
und liefert −1; daraus allein keinen Automatikzustand ableiten.
[OpenCV-4.13-Quelle](https://github.com/opencv/opencv/blob/4.13.0/modules/videoio/src/cap_dshow.cpp).
Belichtung −7 entspricht im Windows-Vertrag 1/128 s; die Zeitunsicherheit enthält
zusätzlich zur Abtastung eine halbe Belichtungsdauer als Schutzabstand.
[Microsoft CameraControl](https://learn.microsoft.com/en-us/windows/win32/api/strmif/ne-strmif-cameracontrolproperty).

## Messkette und Daten

1. `pilot` zeichnet einen Lichtimpuls mit absichtlich auf 5 Bilder/s reduzierter
   **Speicherung** der Gesamtbilder auf. Die Kamera wird durchgehend gelesen.
   `led_camera_rois.py` findet 23 Aufnahmebereiche. Lokale Reflexfragmente werden
   zusammengefasst; daraus folgt noch **keine** Firmwareindexzuordnung.
2. Für die Messung werden dekodierte Kamerapixel in 23 Kacheln verlustfrei erhalten.
   Feste kreisförmige Blenden aus den Nachbarabständen verhindern die doppelte
   Abbildung benachbarter LEDs. Außerhalb der Aufnahmefläche steht Schwarz.
   Regelmäßige vollständige Kontextbilder erhalten die räumliche Herkunft.
   So blockiert die Speicherung großer Vollbilder nicht die Messung.
   Die Kamera selbst liefert MJPG; die PNGs sind kein Sensor-RAW und fügen den
   dekodierten RGB-Pixeln innerhalb der Aufnahmefläche keine weiteren Verluste hinzu.
3. Die Kalibrierung zeigt drei optische Zeitmarker, dann jeden Index 0–22
   einzeln mit Weiß 16 und in umgekehrter Reihenfolge mit Weiß 8. Die Gegenfolge
   darf die zuvor gefundenen Koordinaten nicht anpassen: Sie bestätigt Lage,
   ausreichenden Eigenpegel und geringe Fremdreaktion.
4. Weiß und Blau mit R=G=B/4 werden bei 1/2/4/8/16/24/32 gemessen. Eine feste
   Messfläche je LED wird nur aus Kalibrierdaten gewählt: ausreichendes Signal,
   mindestens fünf nicht gesättigte Pixel und Eigenlicht statt Nachbarlicht.
   Überstrahlte Kerne werden ausgeschlossen; die Flächen bleiben bei späteren
   Effekttests unverändert. Alle drei Grundfarben müssen in ihrem tatsächlichen
   Kamera-RGB-Kanal dominieren, damit ein Kanaltausch nicht eingelernt wird.
5. Jeder Effektlauf hat eigene dedizierte optische Zeitmarker mit unterschiedlich
   langen Phasen und eine
   Dunkelpause **vor** dem Effekt. Die Auswertung verwendet nur diese Marker zur
   Zeitzuordnung, niemals eine nachträgliche freie Phasenanpassung an den Effekt.
   Ansprechkurven und Messflächen stammen aus der getrennten Kalibrierung.
6. Die Firmware meldet jeden eingereichten RGB-Frame erst nach TX_DONE mit
   Laufkennung, Schritt, Sequenz sowie Submit-/Complete-Millisekunden. Das ist
   die Sollspur am tatsächlichen Ausgabeadapter; die Kamera liefert separat die
   sichtbare Wirkung. Ein USB-Schreib-ACK ist keine Lichtbeobachtung.

Bildempfang und USB-Empfang verwenden dieselbe monotone Hostzeit. Die tatsächlichen
TX_DONE-Abstände aus der Geräteuhr ersetzen die durch USB-Abfragen gerasterten
Empfangsabstände; ein konstanter Host-Anker wird durch die optischen Marker
kompensiert. Zeitmarker werden je LED zwischen tatsächlich empfangenen Bildern
eingegrenzt. Unterschiedliche Phasen verhindern eine Kopplung an das 30-Hz-Raster.
Die Schnittmenge der Markerintervalle einschließlich Belichtungs-/Millisekunden-
Schutzabstand bestimmt die geschätzte Unsicherheit; widersprüchliche Marker
werden verworfen. Es gibt keine Optimierung anhand des getesteten Effekts.
Kameraempfang ist
kein Sensor-Belichtungszeitstempel. Pixel-RGB sind Kameracodes, keine lineare
Radiometrie. Die stückweise interpolierten Kurven berücksichtigen das gemessene
Ansprechverhalten beider tatsächlichen Effektfarbfamilien, ohne gleiche Zahlen
von Firmware und Kamera zu unterstellen.

## Urteil und Grenzen

Voraussetzungen: vollständige Firmwarefolge, unveränderte Bild-/Ereignisdateien,
passende Firmwarebinärdatei und Kalibrierung, gleiches Regionslayout und gleiche
protokollierte Kameraeinstellungen. Maximal 80 ms Bildempfangslücke und 60 ms
**geschätzte** Zeitunsicherheit. Fehlerhafte oder unvollständige Aufnahmen werden
verworfen. Laufzeit-Imagegröße und Image-MD5 müssen zur Binärdatei passen; deren
SHA-256 bindet das Artefakt im Messmanifest. MD5 dient dem Abgleich mit der
ESP-Imagefunktion, nicht der Authentifizierung eines untrusted Geräts.

Nach festem Drei-Bild-Mittel müssen pro LED Helligkeitskorrelation ≥0,90,
mittlerer RGB-Absolutfehler ≤12 % und dessen 95. Perzentil ≤25 % der Weiß-32-
Referenz gelten. Sichtbare Spitze ≥10 %, finales Dunkel ≤5 %, keine gesättigten
Pixel innerhalb der festen Messfläche. Zeitmarker fließen nicht in das
Effekturteil ein. Diese Kriterien sind technische Laborkriterien, keine
absolute photometrische Kalibrierung oder vollständige Fehlerabdeckung.

Bei 30 Bildern/s bleibt ein 33-ms-Firmwaretakt nicht lückenlos beobachtbar.
PWM, elektrische Flanken, kurze Aussetzer und Gesamtspielbetrieb benötigen
andere bzw. zusätzliche Nachweise. Unterstützt sind die drei aktuellen Effekte;
andere Farbfamilien werden nicht stillschweigend durch diese Kalibrierung bewertet.

Der USB-Host sendet alle 200 ms ein sitzungsgebundenes Lebenszeichen. Nach
1500 ms ohne Lebenszeichen oder 90 s fordert die Laborfirmware einen frischen
Schwarzframe an. Auch bei Stop und regulärem Abschluss wird dessen Übertragung
abgewartet. Hardwarestörungen können das angeforderte Aus verhindern; ein
fehlerhafter Transfer ist kein bestätigtes Dunkel.

## Softwareprüfung

```powershell
python firmware/led/tests/run_camera_tests.py
python -m unittest discover -s scripts/tests -p 'test_led_camera*.py'
python firmware/led/tests/run_output_tests.py
```

Die tatsächliche USB-Steuerschleife wird mit verzögertem Bus geprüft: vollständige
Sequenz, Verbindungswächter, Stop bei direkter/Engine-Übertragung und natürliche
Laufzeit aller drei Effekte. Unabhängige optische Fixtures prüfen Indexzuordnung,
Kameraverzögerung, unveränderte Pixel der Aufnahmeflächen und Ablehnung fehlender
LEDs, falscher Farbe, ausbleibenden Aus und tatsächlich verschobener Effekte.
Reine Kameraverzögerung und unterschiedliche Zeilen-/LED-Abtastzeiten werden anhand
der eigenen Marker kompensiert; ein gesonderter Test erhält die Geräteintervalle
trotz gerastertem USB-Empfang. Der CI-Job
prüft diese Tests und Referenzexporte; der Firmwarejob baut auch den Laborzweig.

Die ursprünglichen Werkzeuge `record`, `locate`, `extract`, `compare` und
`led_camera_reference.py` bleiben für freie Aufnahmen und CSV-Analysen verfügbar.
Ihr einfacher Korrelationsbericht ist ausdrücklich kein automatisches
Geräteabnahmeurteil; dafür dient der oben beschriebene kalibrierte Laborprüfweg.
