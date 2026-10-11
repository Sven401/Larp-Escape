# Rückweg vor einem möglichen BLE-Fehlertelemetrie-Upload

Stand: 11. Oktober 2026. Produktbasis:
`95acedf17e7218bcaf0463d39b6e0651f3d9779c`. Das separate Werkzeug
`scripts/verify_ble_telemetry_rollback.py` ist an die jetzt gebootete
Originalbox **vor** einem möglichen weiteren BLE-Telemetrie-Upload gebunden.
Es ersetzt oder verändert weder die frühere Originalstandsicherung noch den
ersten Rückweg vor der BLE-Kopplung.

## Gesicherter Stand

Private, nicht im Produkt abgelegte Dateien:

```text
C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/COM3-20261011-PreBleStackTelemetry/
  before-ble-telemetry.bin
  before-ble-telemetry.manifest.json
```

Das Manifest bindet das 4.194.304-Byte-Vollimage an MAC
`a8:42:e3:cd:4b:18` und SHA-256
`155c425805863d9d0a4b9464116c17962ad04ffc6fb8956768c2be0411fce1ec`.
Der Toolcode enthält dieselben erwarteten Werte. Ein abweichendes Image oder
Manifest wird vor jedem Gerätezugriff abgewiesen. Die Prüfung des aktuellen
Ports erfolgt nur, wenn `--port` ausdrücklich angegeben ist.

Rein lokale Prüfung:

```powershell
python scripts/verify_ble_telemetry_rollback.py
```

Live-Trockenlauf mit `flash_id`, ohne Flash-Schreiben. Das Öffnen von COM3 kann
die Box zurücksetzen. Die COM-Nummer allein identifiziert keine Box:

```powershell
python scripts/verify_ble_telemetry_rollback.py restore-pre-telemetry --dry-run --port COM3 --esptool "C:/Users/seide/.platformio/packages/tool-esptoolpy/esptool.py" --python "C:/Users/seide/.platformio/penv/Scripts/python.exe"
```

Der Stand vom 11. Oktober 2026: lokale Imageprüfung erfolgreich; derselbe
Live-Trockenlauf bestätigte MAC und 4 MiB Flash und meldete ausdrücklich
`no flash write or readback performed`. Es wurde kein Upload oder Restore
ausgeführt. Sieben Offline-Tests verwenden ausschließlich einen Fake-`esptool`
und decken falsche MAC/Flashgröße, Länge/Hash, fehlende Bestätigung,
Trockenlauf, vollständigen Fake-Readback und Readback-Abweichung ab.

## Gesondert beauftragter Vollrestore

Nur wenn der Nutzer später ausdrücklich die Rücksicherung **dieses**
Pre-Telemetrie-Stands verlangt, ist der getrennte Befehl zulässig:

```powershell
python scripts/verify_ble_telemetry_rollback.py restore-pre-telemetry --confirm-restore-pre-telemetry --port COM3 --esptool "C:/Users/seide/.platformio/packages/tool-esptoolpy/esptool.py" --python "C:/Users/seide/.platformio/penv/Scripts/python.exe"
```

Er prüft Image und Live-Chipkennung vor einem einzigen `write_flash` des
vollständigen 4-MiB-Abbilds bei `0x0`. Danach liest er 4 MiB zurück und
vergleicht Länge und SHA-256. Fehler beenden den Ablauf; es gibt keinen
automatischen weiteren Schreibversuch. Ein Readback belegt den gespeicherten
Flashinhalt, noch keinen erfolgreichen Boot oder angeschlossene Funktionen.
Geräteidentität, Schreib-/Readbackbelege und Start der rückgesicherten Firmware
sind nach einer tatsächlich beauftragten Rücksicherung getrennt festzuhalten.
Das Werkzeug ist in kein Build- oder Uploadziel eingebunden und startet
keinen Restore automatisch.
