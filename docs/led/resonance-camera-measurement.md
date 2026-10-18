# Resonanzkreis mit USB-Kamera prüfen

Stand: 18. Oktober 2026. Ergänzung des [bestehenden Kameramesswegs](camera-measurement.md)
und des [Produkt-Prüfplans](resonance-test-preparation.md).

Die isolierte Umgebung `larp-led-camera-lab` verwendet unverändert `LedOutput`
und den SDK-RMT-Adapter. Nur diese Laborumgebung gibt synthetische Wake-
Beobachtungen über `updateWake` vor; sie liest keinen MCP und startet kein BLE.
Die Produktvarianten erhalten keinen USB-Steuerzugang. Vor temporärer
Installation gelten Chipidentifikation, vollständiger verifizierter Flash-
Rückweg, Größen-/Partitionsabgleich und anschließende Wiederherstellung.

## Reihenfolge

1. Laborbuild und Hostprüfungen nach dem vorhandenen Kameramessweg ausführen.
2. Mit `pilot` aktuelle Aufnahmeflächen bestimmen und mit `calibrate` alle
   23 Indizes durch Hin-/Rückfolge unabhängig verifizieren. Kameraposition,
   Bildformat und Einstellungen während der gesamten Messung erhalten.
3. `resonance-colors` aufnehmen. Diese direkte, vom Effekt getrennte Farbreihe
   enthält die Stufen 1/2/4/8/16/24/32 der Familie `(B/6, 2B/3, B)` samt
   Schwarzreferenzen. Der neue Effekt darf nicht mit der alten Weiß-/Cool-
   Farbfamilie bewertet werden.
4. `resonance` prüft alle 16 Masken jeweils 3200 ms: keine, einzelne,
   Zweier-/Dreierkombinationen und alle vier Eingänge. `resonance-transitions`
   enthält Loslassen/Wiederkehr jedes Eingangs, ungültigen Lesestand und
   ungültige Maske, Neustart, Stop bei gemeinsamem Licht und beim Atmen,
   Eingangswechsel nach Stop sowie Wechsel zu Ripple und Funkeln.
5. Für getrennte Abschaltnachweise denselben Übergangslauf zusätzlich mit
   `--stop-after 6` beziehungsweise `--watchdog-after 5` aufnehmen. Letzteres
   unterbricht ausschließlich die USB-KEEP-Nachrichten; es prüft den
   Labor-Watchdog, keinen realen BLE-Verbindungsabbruch.

Beispiel nach bereitgestellter Laborfirmware und aktueller Kalibrierung:

```powershell
python scripts/led_camera_lab.py resonance-colors --port COM3 --firmware private/run/lab.bin --rois private/run/rois.json --output private/run/colors
python scripts/led_camera_resonance.py colors --capture private/run/colors --calibration private/run/calibration.json --output private/run/resonance-calibration.json
python scripts/led_camera_lab.py resonance --port COM3 --firmware private/run/lab.bin --rois private/run/rois.json --output private/run/masks
python scripts/led_camera_resonance.py assess --capture private/run/masks --calibration private/run/resonance-calibration.json --output private/run/masks-result.json
```

`resonance-transitions` wird entsprechend aufgenommen und bewertet. Ausgaben
benötigen eigene neue Pfade. Abhängigkeiten bleiben
`scripts/requirements-led-camera.txt`; der Firmwarebuild nutzt die unveränderte
PlatformIO-Umgebung des bisherigen Kameralabors.

## Nachweiskette und Kriterien

`W`-Ereignisse enthalten vorgegebene Gültigkeit, Rohmaske, Effekt und Neustart
mit Gerätezeit. `F`-Ereignisse enthalten tatsächlich bestätigte RMT-Frames;
die Szenariozuordnung wird bereits bei Übergabe des Frames festgehalten.
Quell- und Firmwarefingerabdrücke, Hashes der Bilder und beider Ereignisspuren
bleiben in der Aufnahme erhalten. Das Auswertungsskript prüft die vorgegebene
Szenarioreihenfolge und die feste Zuordnung 0–6/7–12/13–18/19–22 unabhängig
von der Ursprungssuche der Engine.

Unabhängige optische Marker bestimmen den Zeitversatz pro Aufnahme und LED;
der getestete Effekt wird dafür nicht angepasst. Verlustfrei gespeicherte
Messpixel bleiben in allen Läufen gleich. Die empirische Farbreferenz wird
auf die ausgegebenen RGB-Werte interpoliert. Die bisherigen optischen Grenzen
gelten: Korrelation mindestens 0,90, mittlerer normierter RGB-Fehler höchstens
0,12, 95. Perzentil höchstens 0,25, finaler Dunkelrest höchstens 0,05 der
Weiß-32-Referenz, keine gesättigten Messpixel, maximal 80 ms Bildempfangslücke.
Vor jeder Aufnahme stehen die Grenzwerte fest; fehlende Belege sind kein Erfolg.

Die Kamera liefert empfangene Bildzeitpunkte und RGB-Codes, keine Luxwerte,
elektrische Messung oder präzise Sensor-/BLE-Latenz. Eine bestandene Labor-
prüfung ersetzt weder reale Wake-Betätigungen noch das passende Produktpaar
oder den gemeinsamen Spielversuch. Der [Ergebnisbericht](resonance-camera-results-2026-10-18.md)
trennt diese Umfänge.
