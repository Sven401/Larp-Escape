# ESP-NOW-Apppaar: Installation am 18. Oktober 2026

Status: Installation vom Nutzer ausdrücklich beauftragt. Gemeinsamer
Sendetest am Handy bleibt ein eigener beobachteter Prüfschritt.
Quellstand der korrigierten Box: `0b13dfe0372d80601f88178524c3e07175cd8b36`.
App und Empfänger sind bytegleich mit dem vorbereiteten Stand `2dd4661`.

## Rückweg und Zuordnung

Beide ESPs vor dem Schreiben erneut anhand ihrer Chip-MAC identifiziert,
4 MiB vollständig ausgelesen und auf dem jeweiligen Chip verifiziert:

| Gerät | USB / Chip-MAC | SHA-256 der frischen vollständigen Sicherung |
|---|---|---|
| Box | COM3 / `a8:42:e3:cd:4b:18` | `47e73b5295f44335eaeb98a84ca0fdcf8df63459f211d5bd483d3bb16678acda` |
| Empfänger | COM4 / `a8:42:e3:ce:de:d8` | `7b70fd8fb7686d38c9a0a8e3f0b7832169c3debf97917e185050a7aa9dbc2716` |

Box-Partitionstabelle und kompletter Schlüsselbereich stimmen mit dem
chipgebundenen Rückweg `V2-NFC-20261017-784984d` überein. Nur die jeweilige
Anwendung ab `0x10000` ersetzt; keine neue Partitionstabelle installiert.
Die Empfängeranwendung passt in dessen vorhandene App-Partition von `0x140000`
Byte, die Boxanwendung in deren `0x300000` Byte.

Vorhandene Handy-APK und private App-Dateien gesichert. Signaturvergleich
vor dem Update erfolgreich. Installation mit Datenerhalt; installierte APK
danach erneut ausgelesen und bytegleich mit dem geplanten Artefakt geprüft.
Alle acht vorherigen App-Dateien blieben beim Update bytegleich erhalten.
Kein Löschen von App-Daten oder erneuter Schlüsselimport.

Private Rohbelege und Rückwege liegen im Hauptcheckout unter
`private/espnow-app/install-20261018/{box,receiver,phone}/`.
Keine Flashabbilder oder Zugangsdaten im Repository versioniert.

## Bei der Geräteprüfung gefundene Korrekturen

- CI am Vorbereitungshead `f5c1d4a`: älterer `espnow-probe` baute beide
  Einstiegsquellen und scheiterte an doppeltem `app_main`. Commit `12445a3`
  grenzt dessen Quellen ausdrücklich auf `main.cpp` ein. Beide Probevarianten
  danach lokal gemeinsam erfolgreich gebaut; Empfänger-Binärdatei unverändert.
- Erster Start der neuen Box: wiederholtes `abort()` mit ausdrücklicher
  SDK-Meldung, dass WLAN-Modemschlaf bei gleichzeitigem Bluetooth nötig ist.
  Commit `0b13dfe` verwendet `WIFI_PS_MIN_MODEM` statt `WIFI_PS_NONE`.
  Das entspricht der [Espressif-Dokumentation für ESP-IDF 4.4](https://docs.espressif.com/projects/esp-idf/en/v4.4.3/esp32/api-guides/wifi.html).
  Boxvariante erneut erfolgreich gebaut. Der fehlgeschlagene Bootbeleg ist
  separat unter `box/failed-attempt-01/` erhalten.

## Installierte Artefakte

Korrigiertes Paar unter `outputs/espnow-app-20261018-coex/`, dessen Manifest
bindet Quellstand und Dateien. Der frühere Ausgabeordner bleibt als historischer
Vorbereitungsstand erhalten und enthält die Boxversion vor der Korrektur.

| Artefakt | SHA-256 |
|---|---|
| Handy-App | `a75ddadbe776207d915c24f25c484390662d4d34d458f80f879ca45660f0a0b1` |
| Box | `ed12c11424e78c3f11ed53d119b9ec33bfd04c0dfe50d4a433b39f1512897354` |
| Empfänger | `b23b87e8ee6d27f8c32a00fc1a086b810176b46f2bc54944711d64c5b0a14e68` |

Der Empfänger meldet beim Start seine erwartete MAC und Kanal 1. Dessen
PHY-Hinweis auf nicht initialisiertes NVS wird erhalten: diese Variante nutzt
WLAN-Konfiguration im RAM und initialisiert NVS absichtlich nicht. Ein
Bereitschaftslog ersetzt keinen tatsächlich beobachteten Funkempfang.

## Stand der Handyabnahme

Beide Anwendungen nach dem Schreiben auf dem jeweiligen Chip verifiziert.
Nach der korrigierten Boxinstallation Boot-/Partitionsbereich `0..0x10000`
und Schlüsselbereich `0x310000..0x316000` erneut gelesen: bytegleich mit der
frischen Sicherung. Erwarteter Boxstart beobachtet, während der achtsekündigen
Bootaufnahme kein `abort()` und keine Neustartschleife. Handy findet und
verbindet die Box per BLE.

Handy zeigt anschließend **„LED-Box verbunden · Zugang bestätigt“**. Die
ESP-NOW-Karte ist geöffnet, `A8:42:E3:CE:DE:D8` eingetragen und der
Sendeknopf ist freigegeben. Kein bestehender Matrix-Sammelstand gelöscht.
XML/Screenshot und Installationsmanifeste sind in den privaten Rohbelegen
erhalten. Das belegt Zugang und Bereitschaft, noch keinen bestätigten Empfang.

Noch kein Sendeknopf durch Codex betätigt. Erfolg, Wiederholung mit erhöhtem
Empfangszähler, Empfänger stromlos und Wiederverbindung sind nach dem
[gemeinsamen Ablauf](app-test.md) zu beobachten. Frühere isolierte Funkproben
und Hosttests gelten nicht als Handyabnahme des neuen Paars.
