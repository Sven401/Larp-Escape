# Vollhardwareprüfung der ursprünglichen Reichstagsbox

Stand: 10. Oktober 2026. Produktbasis: `7f2a3e4`, Arbeitsbranch
`codex/reichstagsbox-hardware`. Der angeschlossene ESP32 ist ein ESP32-D0WD-V3
mit 4 MB Flash an COM3. COM-Nummern sind nicht dauerhaft; vor weiteren
Geräteaktionen erneut prüfen.

## Schutz des Originalstands

Die vollständige Original-Firmware wurde **vor dem ersten Upload** unter
`C:\Users\seide\AppData\Local\LarpEscape\ESP32-Backups\COM3-20261010-Reichstagsbox\original-flash.bin`
gesichert: 4.194.304 Bytes, SHA-256
`a502ea122c9c81e02637f593a2c8c03ddf5c2963fa5f4b878eb291e271c3f020`.
Das private `original-flash.manifest.json` im selben Ordner bindet die Sicherung
an Flashgröße und Chipkennung. Beide Dateien bleiben außerhalb von Git und
OneDrive. Die Sicherung des früheren nackten ESP32 in [usb-probe.md](usb-probe.md)
gehört zu einem anderen Gerät.

**Keine automatische Rücksicherung:** Mehrere Prüfungen laufen nacheinander auf
der geladenen Testfirmware. `capture` und `inspect` lesen nur und enthalten
keinen Rücksicherungsschritt. Der Originalstand wird erst nach einer klaren
Aufforderung des Nutzers zurückgeschrieben. Dazu ist der separate Befehl
`restore-original` mit `--confirm-restore-original` erforderlich. Er prüft vor
dem Schreiben Dateigröße, SHA-256, Chipkennung und Flashgröße. Ein Fehler stoppt
den Vorgang vor `write_flash`.

## Wiederholbarer Lauf

Die normale Spiel-Firmware wird mit `pio run -e az-delivery-devkit-v4` gebaut
und nach ausdrücklicher Uploadfreigabe mit
`pio run -e az-delivery-devkit-v4 -t upload --upload-port COM3` geschrieben.
Der Upload schreibt Bootloader, Partitionstabelle und App; das private Vollbackup
ist daher der Rückweg. Nach einem Test bleibt die Testfirmware auf der Box.

Mit der Python-Umgebung von PlatformIO einen frischen seriellen Lauf erfassen:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" scripts/reichstagsbox_hardware.py capture --port COM3 --seconds 45 --output "$env:LOCALAPPDATA\LarpEscape\ESP32-Backups\COM3-20261010-Reichstagsbox\lauf-001.txt"
```

Der Befehl startet den ESP32 einmal neu, zeichnet maximal 512 KiB auf und
wertet Start- und Laufzeitmarker aus. Jede Erfassung benötigt einen neuen
Dateinamen. Rohlogs können Karten-IDs enthalten und gehören nicht in Git.
`inspect <Logdatei>` prüft einen gespeicherten Mitschnitt ohne Gerätezugriff.

Für einen **ausdrücklich beauftragten** Restore gilt:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" scripts/reichstagsbox_hardware.py restore-original --port COM3 --manifest "$env:LOCALAPPDATA\LarpEscape\ESP32-Backups\COM3-20261010-Reichstagsbox\original-flash.manifest.json" --esptool "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" --confirm-restore-original
```

Dieser Befehl ist kein Teil der Testfolge. Nach einer Rücksicherung sind
erneut Flashschreib-Hashes und der Start der Originalfirmware zu prüfen.

## Bisher gemessener Stand

Die alte Firmware meldete beide MCP23017 an 0x20/0x21, PN532-Firmware 1.6
und eine 9×9-Matrix mit lauter High-Werten im Ruhezustand. Der aktuelle
Produkt-Build war erfolgreich: RAM 55.148 Bytes, Flash 1.229.077 von
1.310.720 Bytes (93,8 %). Der Upload an COM3 bestätigte die Hashes aller
vier geschriebenen Bereiche. Im 45-Sekunden-Lauf erreichte die neue Firmware
`Runtime ready`, erkannte beide MCP23017 und den PN532 und lieferte vier
10-Sekunden-Berichte ohne verpasste LED-Frames oder Queueverluste. Gemessen:
maximale Loop-Ausführungszeit 39.161 µs, minimale freie LED-Taskstackreserve
3.192 Bytes, maximale LED-Renderdauer 2.760 µs. FastLED meldete einen
generischen Clockless-Ausgabepfad; dessen Verhalten unter Last bleibt offen.
Der neue `capture`-Befehl wurde anschließend an derselben Firmware ausgeführt:
ein Neustart, vier Berichte, `PASS`, 0 verpasste LED-Frames und 0 Queueverluste;
maximale Loop-Ausführungszeit 39.214 µs, LED-Stackreserve mindestens
3.224 Bytes, maximale LED-Renderdauer 3.089 µs. Die aktuelle Firmware bleibt
auf der Box; ein Restore wurde nicht ausgelöst.

Diese Beobachtung beweist weder eine korrekte Matrixbetätigung oder Karten-UID,
noch hörbares Audio, sichtbare LED-Farben oder ESP-NOW-Zustellung. Für diese
Ende-zu-Ende-Prüfungen sind passende Eingaben, Sensoren oder ein Empfänger
erforderlich. Ohne sie bleibt der jeweilige Teil ausdrücklich ungeprüft.
