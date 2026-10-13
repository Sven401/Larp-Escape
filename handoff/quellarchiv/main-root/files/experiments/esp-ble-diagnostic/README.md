# Eigenständiger BLE-Transportprüfstand

Kein v2-Spielserver. Kein GPIO, NFC, Audio, Spielsteuerung, Peergrant oder Profiltransfer. Der Arduino-BLE-Dienst startet echten Funk und zählt tatsächlich empfangene Diagnoseanfragen. Keine Authentifizierung: ausschließlich für den lokalen Hardwareprüfstand.

Service `86ddc5b0-3878-4ff7-9f55-17c62aeb0001`; Status READ `...0002`, Command WRITE mit ATT-Antwort `...0003`, Ergebnis READ `...0004`. Alle Werte passen bei MTU 23 ohne Fragmentierung. Kein Notify nötig. Eigene Diagnoseversion 1, nicht Firmware-v2-Framing.

| Wert | Binärvertrag, alle Zahlen little-endian |
| --- | --- |
| Status 16 Bytes | Version1, Marker0x49, Bootnonce32, millis32, Heap32, Diagnoseflag16=1 |
| Request 8 Bytes | Version1, Opcode8, Request32, Reserved16=0 |
| Result 16 Bytes | Version1, Status8 (1 erfolgreich/2 abgewiesen), Request32, Bootnonce32, Processed32, Reason16 |

Opcode1 Ping; andere Opcodes abweisen, keine Seiteneffekte auf Spielhardware. Reason1 ungültiger Opcode, 2 ungültiges Format, 3 nicht steigende Request-ID, 4 Zähler ausgeschöpft. Wohlgeformte neue Anfragen erhöhen Processed auch bei abgewiesenem Opcode; Duplikate/Formatfehler nicht. Verbindungswechsel setzt die Requestfolge und den gespeicherten Antwortwert zurück; Processed bleibt bis zum Neustart erhalten. Bootnonce ist eine Sitzungskorrelation, keine dauerhaft verifizierte Geräteidentität.

Hostprüfung in `src/host.cpp` prüft Korrelation, Ablehnung, doppelte IDs, kurze/lange Nachrichten, Version/Reserved, Reconnect und Überlauf. Aktive Assertions sind Buildvoraussetzung.

```powershell
platformio run --project-dir experiments/esp-ble-diagnostic -e host-diagnostic -e esp32-ble-diagnostic
./experiments/esp-ble-diagnostic/.pio/build/host-diagnostic/program.exe
```

Vor dem Upload aktive Partition/OTA-Auswahl lesen und sichern. Nicht den gewöhnlichen PlatformIO-Upload verwenden: nur das geprüfte `firmware.bin` ab 0x10000 schreiben, falls die reale Partitionstabelle genau den dafür geprüften Appslot zeigt. Bootloader, Partitionstabelle, OTA und NVS nicht überschreiben. Die BLE-Bibliothek kann bei Initialisierung eigene NVS-Daten anlegen; der Flashupload selbst betrifft nur app0.

Für diesen ESP: app0 ab 0x10000, Länge 0x140000; OTA seq1 mit geprüfter CRC wählt Slot0. [ESP-IDF-OTA-Quelle](https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/system/ota.html). Originalpartition liegt lokal ignoriert unter `.local-backup/20261010-original-app0.bin`. Kein vollständiges Gerätebackup, keine NVS-/Bootloader-Sicherung. Rückweg mit demselben esptool 4.5.1: nur dieses unveränderte Backup wieder ab 0x10000 schreiben. Prüfsumme vor jedem Restore mit dem lokalen Werkbanknachweis vergleichen.

Android-Diagnoseclient: Status lesen, Ping senden/korrelierte Antwort prüfen, Opcode127 senden/Ablehnung samt erhöhtem Verarbeitungszähler prüfen. GATT-Callbacks müssen zur aktuellen Verbindung gehören. Danach trennen und dasselbe Ziel neu verbinden; keine erfolgreiche Hostprüfung als Hardwarebeleg zählen.

Quellen sind eigene Experimente im Produkt, Messbelege liegen in der getrennten KI-Werkbank. Binärbackup und Buildartefakte bleiben ignoriert. Der fremde Firmwareworktree und die originale GM-App bleiben unverändert.
