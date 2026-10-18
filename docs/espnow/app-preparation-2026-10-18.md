# ESP-NOW-Appbedienung: Softwareprüfstand

Stand: **18. Oktober 2026**. **Status: implementiert, lokal softwareseitig
geprüft und als Artefakt bereitgestellt; neue Geräteinstallation und
gemeinsame Handyabnahme noch offen.**

Geprüfter Quellcommit: `2dd4661b80d789563024dbd072b07d2a76bee848`.
Basis ist `49cb1d0` aus [PR #65](https://github.com/Sven401/Larp-Escape/pull/65),
auf Integrationsstand `d61f77f`. Der neue Branch enthält deshalb auch die
erhaltene isolierte Probe und deren früheren Gerätebericht. Keine Übertragung
dieser alten Geräteabnahme auf das neue App-/BLE-/Funkpaar.

## Änderung und konkret geprüfter Umfang

Die bestehende Baugruppenbedienung erhält Empfänger-MAC, einen expliziten
Sendeknopf und getrennte Ergebnisse für lokale Sendeannahme, MAC-Zustellung
und passende Empfängerrückantwort. Gemeinsamer BLE-Auftragszähler und eigene
HMAC-Domänen bleiben verbindlich. Neue Firmwarevarianten:
`larp-espnow-test` und `espnow-app-receiver`.
[Vertrag und gemeinsamer Handy-Testablauf](app-test.md).

| Prüfung | Tatsächlich gemessenes Ergebnis |
|---|---|
| Native Wire-/Funkzustandstests | 7.702 Assertions bestanden; echte Host-HMAC, bisherige Python-Goldenbytes, neue Auth-/Absender-/Token-/Duplikat-/Queue-/Frist-/Uhrüberlauf-/Auftragsfolgefälle |
| Android Protokollkern | 87 Tests, keine Fehler oder ausgelassenen Fälle |
| Android App | 74 JVM-Tests, keine Fehler oder ausgelassenen Fälle |
| Android Artefakte | Debug und Release gebaut |
| Android Lint | Debug und Release: null Fehler, drei Warnungen und eine Information |
| Bestehende Baugruppen-Hosttests | LED/Wake/Matrix/I2C, Audio/UART und NFC mit 86 Transaktionsfällen bestanden |
| Box-Firmware | `larp-led-product`, `larp-nfc-test`, `larp-matrix-test`, `larp-espnow-test` erfolgreich gebaut |
| Empfänger | `espnow-app-receiver` erfolgreich gebaut |
| Diff | `git diff --check` ohne Befund |

Lint meldet `OldTargetApi`, `DataExtractionRules`, `MissingApplicationIcon`
sowie `AutoboxingStateCreation` als Information. Sichtbare SDK-Metadaten-
Schreibhinweise für das schreibgeschützte installierte Android SDK und die
vorhandene Frameworkwarnung in `esp32-hal-uart.c` verhindern den erfolgreichen
Build nicht; sie werden nicht als neue Gerätefehler ausgegeben.

Die ESP-NOW-Boxvariante belegt im Build 59.408 Byte RAM und 1.489.433 Byte
Programm; der neue Empfänger 41.556 Byte RAM und 642.029 Byte Programm.
Das sind Buildwerte, keine gemessenen Laufzeitreserven unter Funk-/BLE-Last.

## Vorbereitete Dateien

Im Hauptcheckout unter dem ignorierten Ordner `outputs/espnow-app-20261018/`:

| Datei | Größe | SHA-256 |
|---|---:|---|
| `LarpEscape-ESP-NOW-debug.apk` | 23.383.526 Byte | `a75ddadbe776207d915c24f25c484390662d4d34d458f80f879ca45660f0a0b1` |
| `box-espnow-test.bin` | 1.495.728 Byte | `70897847836b0cf151d7de9432275c25cf4412b3360aabe4f3537b3ea5b66e2c` |
| `espnow-app-receiver.bin` | 648.320 Byte | `b23b87e8ee6d27f8c32a00fc1a086b810176b46f2bc54944711d64c5b0a14e68` |

`manifest.json` bindet Dateien, Quellcommit, JUnit- und Lint-Ergebnisse.
Die editierbaren Quellen stehen im eigenen Arbeitsbranch; keine privaten
Flashabbilder, Schlüssel oder Zugangsdaten in diesem Commit.
Lokale Android-Rohbelege: `companion/android/{core/protocol,app}/build/test-results/`
und `companion/android/app/build/reports/lint-results-{debug,release}.*` im
isolierten Arbeitsverzeichnis. Die Buildausgaben bleiben regenerierbar.

## Abnahmegrenze und nächster Schritt

**Kein neuer Upload auf Box, Test-ESP oder Handy in diesem Implementierungsschritt.**
Die Box bleibt auf ihrer zuvor vollständig wiederhergestellten Firmware;
der Test-ESP auf der früheren isolierten Probe. Der ADB-Abruf zeigte zum Ende
der Vorbereitung kein verbundenes Handy. Daher keine Behauptung einer Sicht-,
BLE-Koexistenz- oder Empfängerabnahme dieses neuen Paars.

Für die nächste Geräteprüfung Handy per USB anschließen und die Installation
des konkret vorbereiteten Paars beauftragen. Dann aktuelle Chips und frische
Rückwege abgleichen, vorhandene Handy-APK/Signatur sichern, nur die Anwendungen
auf den ESPs ersetzen und die App mit Datenerhalt aktualisieren. Anschließend
den verlinkten Handyablauf ausführen und Erfolg/Fehler getrennt protokollieren.
Reichweite, Dauerlast, verschlüsselte Peers und Gesamtspiel bleiben gesonderte
Prüfungen. Der neue Pull Request hat eigene CI-Ergebnisse; diese sind nicht
durch ältere grüne Läufe ersetzt.
