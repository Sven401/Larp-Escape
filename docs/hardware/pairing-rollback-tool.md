# Rückweg-Werkzeug für die Originalbox vor BLE-Kopplung

Stand: 11. Oktober 2026. Basis: Integrations-Commit `53693cba485d702c471b8e9aba562df8bbfaaa3b`.
Werkzeug: `scripts/verify_pairing_rollback.py`. Es ist auf die **heutige**
Vollsicherung der Originalbox festgelegt, nicht auf den früheren Originalstand
oder den separaten ESP-NOW-Empfänger.

## Datei- und Gerätebindung

Das Werkzeug erwartet `before-pairing-flash.manifest.json` und
`before-pairing-flash.bin` im privaten Ordner
`C:/Users/seide/AppData/Local/LarpEscape/ESP32-Backups/COM3-20261011-PrePairing`.
Manifest und Image müssen MAC `a8:42:e3:cd:4b:18`, 4.194.304 Byte und
SHA-256 `170b9392dd1970ce53c8d2d16fbcc78c6e590b625576494375626193322075a9`
ergeben. Diese Werte sind zusätzlich im Werkzeug verankert; ein gemeinsam
geändertes Manifest und Image wird damit abgewiesen.

Der Standardaufruf liest ausschließlich lokale Dateien:

```powershell
python scripts/verify_pairing_rollback.py
```

Eine **ausdrücklich** angegebene Schnittstelle fügt `esptool flash_id` hinzu.
Dieser Aufruf kann den ESP32 zurücksetzen. Vor Verwendung die aktuelle
COM-Zuordnung prüfen; COM3 allein identifiziert kein Gerät:

```powershell
python scripts/verify_pairing_rollback.py verify --port COM3 --esptool "C:/Users/seide/.platformio/packages/tool-esptoolpy/esptool.py"
```

## Getrennter Restore zum heutigen Stand

Ein Probelauf prüft Image und Gerätekennung, schreibt aber keinen Flash:

```powershell
python scripts/verify_pairing_rollback.py restore-current --dry-run --port COM3 --esptool "C:/Users/seide/.platformio/packages/tool-esptoolpy/esptool.py"
```

Nur nach einer eigenen ausdrücklichen Restore-Aufforderung darf der folgende,
separate Befehl in Betracht kommen:

```powershell
python scripts/verify_pairing_rollback.py restore-current --confirm-restore-current --port COM3 --esptool "C:/Users/seide/.platformio/packages/tool-esptoolpy/esptool.py"
```

Er prüft Datei und Chip **vor** dem Schreibaufruf, schreibt das komplette
4-MiB-Abbild ab Offset `0x0`, liest danach denselben Bereich vollständig zurück
und vergleicht Länge sowie SHA-256. Jeder Fehler beendet den Aufruf; es gibt
keinen automatischen zweiten Schreibversuch. Ein bestandener Readback-Vergleich
belegt den Flashinhalt, aber noch keinen erfolgreichen Boot oder die Funktion
der angeschlossenen Baugruppen. Die temporäre Readback-Datei wird nach der
Prüfung entfernt. Falls der reguläre Tempordner nicht nutzbar ist, kann
`LARP_ROLLBACK_TEMP_DIR` auf einen geeigneten privaten Pfad gesetzt werden.

Das Werkzeug führt keinen Upload und keine Migration zur Single-App-Karte aus.
Es wird von keinem Build-, Test- oder Uploadziel automatisch gestartet. Die
alte 2×OTA-Tabelle und die damalige `otadata` werden durch den Vollrestore
mit dem gesicherten Stand wiederhergestellt; ein separates Factory-Image
hat selbst keinen OTA-Rollback. Nach einem tatsächlich beauftragten Restore
müssen Geräteidentität, Readback, Bootauswahl und Start der rückgesicherten
Firmware getrennt protokolliert werden.

## Prüfstand

Der Standardaufruf hat die private Sicherung lokal verifiziert. Sieben
Python-Tests mit einem rein dateibasierten Fake-`esptool` prüfen Offline-Modus,
Länge/Hash, falsche MAC/Flashgröße, fehlende Bestätigung, Dry-run, volles
Readback und abweichenden Readback. Weder Flash-Schreib- noch Geräteaufruf
wurde für diese Werkzeugprüfung ausgeführt. Produkt-Upload und Rücksicherung
bleiben offen.
