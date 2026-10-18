# RSSI-Erweiterung: Prüfstand am 18. Oktober 2026

Status: implementiert, softwareseitig geprüft, App/Box installiert und begrenzt
am angeschlossenen Paar beobachtet. Gemeinsame Abstands-/Lichtabnahme offen.
Funktionscode `e0123140f1105e201b322f072a64b57e2b4680ac`; reine Textkorrektur
der App `eca6c08c0c35629733419be4ee1777120257e1d8`.
[Vertrag, Bedienung und offener Abnahmeplan](rssi-test.md).

## Softwareprüfungen

- Native Wire-/RSSI-Prüfung: 7.873 Assertions mit echter HMAC und unveränderten
  bisherigen Goldenbytes bestanden. Neue Fälle prüfen Framegrenzen, OUI,
  Typ/Version, Quelle/Kennung, fehlende RSSI-Metadaten, Filter/Veraltung mit
  Uhrüberlauf, HMAC-Domänen, gemeinsamen Slot und Duplikatcache.
- LED-Ausgabe: 17 Engine- und vier SDK-Fälle sowie Wake/Matrix/I2C erfolgreich.
  Neuer Fall prüft Skalierung bei unveränderter Phase/Revision, Helligkeitsgrenze
  und fehlenden Einfluss auf Burst. Keine optische Abnahme dadurch behauptet.
- Android: 88 Protokoll- und 74 App-JVM-Tests ohne Fehler/ausgelassene Fälle.
  Debug/Release auf `e012314` gebaut. Nach Textkorrektur Debug erneut gebaut.
  Debug-Lint erfolgreich, null Fehler, drei bestehende Warnungen und eine Info.
- Vier Boxvarianten `larp-led-product`, `larp-nfc-test`, `larp-matrix-test`,
  `larp-espnow-test` erfolgreich gebaut. Framework-Pins unverändert.
- `git diff --check` ohne Befund. CI am jeweiligen veröffentlichten Head separat
  auswerten; lokale Prüfungen und Gerätelauf ersetzen keinen CI-Nachweis.

## Installation und Erhaltung

Box COM3, Chip `a8:42:e3:cd:4b:18`, vor Upload erneut identifiziert. Frische
vollständige 4-MiB-Sicherung auf dem Chip verifiziert, SHA-256
`bbce0236f97e3af380f51899579e504d39ffaf0e8be07c49788dbcdd92078ec0`.
Historische Partitionstabelle und Schlüssel stimmen. Nur App ab `0x10000`
ersetzt; danach Firmware verifiziert sowie Boot-/Partitionsbereich und
vollständiger Schlüsselbereich erneut bytegleich mit frischer Sicherung gelesen.
Erwarteten Start beobachtet. Erster Verbindungsversuch erreichte Downloadmodus
nicht und schrieb nichts; Folgeversuch erfolgreich, Fehlerbeleg erhalten.

Empfänger COM4 erneut als `a8:42:e3:ce:de:d8` bestätigt, Firmware unverändert;
Kennungsprüfung startete ihn neu. Daher begann dessen Empfangszähler erneut.
Handy-App mit passender Signatur und Datenerhalt aktualisiert, APK zurückgelesen.
Erstes Update erhielt alle acht Dateien. Nach App-Lauf und Textkorrektur sind
sieben gespeicherte Dateien einschließlich verschlüsseltem Box-Zugang unverändert;
einzige Abweichung zur Sicherung vor dem Lauf ist `files/profileInstalled`.
Keine Schlüssel-/Sammeldaten gelöscht. Box-Zugang am Handy bestätigt.

Aktuelle Dateien unter `outputs/espnow-rssi-20261018-ui/` im Hauptcheckout:

| Datei | SHA-256 |
|---|---|
| `LarpEscape-RSSI-debug.apk` | `27c27d5f816a28ee1bc69c77bdc91d9c937e737d887b6af9fba3e90c5c9b2a23` |
| `box-rssi-test.bin` | `1ee51656a95f730bfdc101bdcb29a83b7b61a08dceb80514e737263ec4d5374c` |

Boxbinary aus `e012314`, letzte App aus `eca6c08`; Empfänger bleibt SHA-256
`b23b87e8ee6d27f8c32a00fc1a086b810176b46f2bc54944711d64c5b0a14e68`.
Private Belege/Rückwege: `private/espnow-app/rssi-install-20261018/`.
Keine Rohschlüssel, Flashabbilder oder Zugangsdaten versioniert.

## Tatsächlich beobachteter Umfang

Reine Messung am Handy: −26 dBm roh, geglättet etwa −25 dBm, Ziel 100 %.
Neue Aufträge und Empfangszähler fortlaufend bis mindestens 30. Beim Beenden
verschwindet der Laufstatus; nach Veraltung zeigt die App Signalstärke unbekannt.
Erneuter bewusster Start im kombinierten Modus: weitere passende Rückantworten,
beispielsweise Auftrag 49 / Empfängerzähler 48, roh −28 dBm, geglättet −26 dBm.
Die Nummern unterscheiden sich korrekt durch den zusätzlichen LED-Auftrag.
Nach Beenden meldet die LED-Karte **„Box bestätigt: LEDs aus“** und
**„Box meldet: LEDs aus“**. XML und Screenshots erhalten.

Keine bekannten Abstände verändert oder gemessen. Daher keine Aussage über
Meterbezug, Messkurve, sichtbare Intensitätsänderung bei Bewegung, reale
Funkverlust-Rückfallkurve, Reichweite oder Dauerlast. Diese Punkte gemäß
verlinktem Ablauf gemeinsam prüfen. Frühere Einzelsendetests #1/#2 gelten
weiterhin für ihren zuvor installierten Stand, nicht als neue RSSI-Abnahme.
