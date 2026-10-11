# Rückweg vor einem möglichen BLE-Startfix

Stand: 11. Oktober 2026. Produktbasis:
`671be0b4f67254e640f36253baff780bacaed985`.
Für einen möglichen dritten, getrennt freizugebenden **App-only** Upload wird
die vorhandene Vollrücklesung nach dem BLE-Telemetrieversuch verwendet. Es
wurde für dieses Werkzeug keine neue Vollsicherung erstellt. Die zuvor
verwendeten Original- und Pre-Telemetrie-Sicherungen bleiben eigenständig.

## Exakter Rückweg

Private Dateien außerhalb des Produkt-Repositories:

```text
C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/COM3-20261011-PostBleStackTelemetry/
  after-ble-telemetry.bin
  after-ble-telemetry.manifest.json
  postflash-verification.json
```

Die bereits vorhandene `after-ble-telemetry.bin` wurde nach dem zweiten
Upload vollständig vom Chip zurückgelesen; der damalige Prüfbericht nennt
die Originalbox-MAC `a8:42:e3:cd:4b:18`, 4.194.304 Byte und den Hash
`ff1ac0bbd32c299006aae73045560e0a7db89a80344a6540ac9c8670fd31edb8`.
Das neu ergänzte Manifest bindet genau diese Datei, Chipkennung, Größe und
Hash. Der gleiche Hash ist fest im neuen
`scripts/verify_ble_startfix_rollback.py` hinterlegt. Die Sicherung enthält
die einzelne Factory-Partition und vollständig gelöschte `otadata`. Sie
führt zum Stand **nach** dem fehlerhaften BLE-Telemetrie-Start zurück.

Der Nutzer meldete eine bytegleiche weitere Rücklesung nach dem folgenden
Boot. Diese doppelte Kopie ist für den Rückweg nicht erforderlich; das
Werkzeug verwendet ausschließlich `after-ble-telemetry.bin`.

## Prüfung und ausdrücklicher Restore

Ohne Portangabe liest das Werkzeug nur Manifest und Sicherung:

```powershell
python scripts/verify_ble_startfix_rollback.py
```

Ein separater Live-Trockenlauf ruft `flash_id` auf. Das Öffnen des Ports kann
die Box zurücksetzen, schreibt aber keinen Flash:

```powershell
python scripts/verify_ble_startfix_rollback.py restore-pre-startfix --dry-run --port COM3 --esptool "C:/Users/seide/.platformio/packages/tool-esptoolpy/esptool.py" --python "C:/Users/seide/.platformio/penv/Scripts/python.exe"
```

Ein Vollrestore ist nur nach einer **eigenen ausdrücklichen Aufforderung**
des Nutzers mit dem gesonderten Bestätigungsparameter möglich:

```powershell
python scripts/verify_ble_startfix_rollback.py restore-pre-startfix --confirm-restore-pre-startfix --port COM3 --esptool "C:/Users/seide/.platformio/packages/tool-esptoolpy/esptool.py" --python "C:/Users/seide/.platformio/penv/Scripts/python.exe"
```

Vor dem einzigen `write_flash 0x0` werden das gepinnte Manifest, Dateigröße,
SHA-256 sowie Live-MAC und Flashgröße geprüft. Danach liest das Werkzeug
die vollen 4 MiB zurück und verlangt erneut passende Länge und SHA-256.
Es gibt keine automatische Wiederholung oder Rücksicherung. Ein korrekter
Readback belegt den Flashinhalt; Boot, BLE und angeschlossene Hardware sind
getrennt zu prüfen. Das Werkzeug ist in kein Build- oder Uploadziel
eingebunden.

## Prüfstand

Die echte bestehende Rücklesung wurde offline mit Länge und SHA-256 geprüft.
Sieben dateibasierte Fake-`esptool`-Tests bestanden, einschließlich falscher
Chipkennung/Flashgröße, falschem Image, fehlender Bestätigung, Dry-run und
vollständigem bzw. fehlerhaftem Readback. Der ausdrückliche COM3-Live-Dry-run
bestätigte MAC `a8:42:e3:cd:4b:18` und 4 MiB, meldete `no flash write or
readback performed`. Kein Upload und kein Restore wurden ausgeführt.
