# Audiobedienung vollständig vorbereitet – 16. Oktober 2026

**Status: lokal softwaregeprüft; nicht installiert, kein Hörnachweis.**
Geprüfte Produktquellen: `958d6dcf530d780e328e6c3043aa063323571af7`.
Audio-Ergänzung `0e686ec` auf dem bestehenden [PR #48](https://github.com/Sven401/Larp-Escape/pull/48)
bei `d1c7077`; die bereits am Gerät geprüften NFC-Rücklesekorrekturen aus
`33bf31b` sind auf diesem eigenen Arbeitsbranch konfliktfrei übernommen.
Firmware-/Wire-Quellen zwischen `0e686ec` und `958d6dc` per Git bytegleich
abgeglichen; anschließend vollständigen Android-Lauf auf `958d6dc` ausgeführt.
Nachfolgende Änderungen betreffen ausschließlich diese Dokumentation und ihren Link.

Der eigene Branch heißt `codex/audio-controls-completion`. PR-Ziel ist der
vereinbarte Integrationsbranch `plan/projektuebergabe-kuratierter-stand`.
PR #48 bleibt unverändert; dieser Folge-PR enthält seine noch nicht integrierten
Voraussetzungen und zusätzlich die jüngsten NFC-Korrekturen. Kein Merge des
Integrationsbranches, kein Release und keine Geräteinstallation.
Eine zunächst unabhängig erstellte Alternative wurde wegen des entdeckten
vorhandenen Audio-Branches nicht veröffentlicht; sie ist keine Installationsbasis.

## Fertiger Bedienumfang

Play mit gewähltem Titel 1–255, Pause, Fortsetzen desselben pausierten Titels,
Stop und direkte Lautstärkeübernahme 1–10 ohne Titelneustart. Vorgabe bleibt 5.
Play startet bewusst neu; die Auswahl allein sendet nichts. Pause/Resume und
Lautstärke verlängern die bisherige 30-Sekunden-Grenze ab Play nicht.
Das zusätzliche Pausenbit benötigt das hier gemeinsam gebaute App-/Firmwarepaar.
UART-Übertragung, BUSY-Rohpegel und spätere Hörbeobachtung bleiben getrennt.

**Nutzerbestätigt am 16.10.2026:** DFPlayer-RX an ESP GPIO26, TX an GPIO25,
BUSY an GPIO14; Antwort „GPIO26/25/14 bestätigt“ in Sitzung
`01a1461f-ed41-72f0-a723-13979fa536c4`. Keine neue elektrische Messung;
GPIO13/WAKE wird weiter nicht getrieben. Titel 100 bleibt der aus PR #48
übernommene Startkandidat der vorhandenen Con-Karte ohne bestätigten Dateiinhalt.

## Durchgeführte Prüfungen

| Prüfung | Ergebnis |
|---|---|
| Native Wire-/HMAC-Suite | 4.226 Assertions, echte HMAC und vorhandene unabhängige Referenzbytes; neue Operationen, asynchrone Bestätigung und Wiederholungsschutz bestanden |
| Audio-Ausgabe | 11 Fälle bestanden, darunter Pause/Resume, direkte Lautstärke, unveränderte Stopfrist und Stop während Pauseübertragung |
| Tatsächlicher UART-Adapter gegen SDK-Stubs | Alle 5 vorhandenen Fälle bestanden |
| LED/Wake | 16 LED-/4 SDK-Fälle sowie Mapping, I²C, Fehler und Erholung bestanden |
| Android-Protokoll | 69 JVM-Tests, null Fehler/übersprungen |
| Android-App | 50 JVM-Tests, null Fehler/übersprungen |
| Android Debug-APK und Debug-Lint | Erfolgreich; null Lintfehler, drei bestehende Warnungen, eine Information |
| ESP32 `larp-led-product` | SUCCESS; RAM 42.496/532.480, Anwendungscode 1.134.745/3.145.728 Byte |
| `git diff --check` | Ohne Befund |

Aufrufe aus dem Repository:

```powershell
python test/test_led_ble_wire/run.py
python firmware/led/tests/run_audio_tests.py
python firmware/led/tests/run_output_tests.py
pio run -d firmware/led -e larp-led-product -j 2
# Aus companion/android, JDK21.0.8 / Android SDK35 / Gradle8.9:
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug --offline --no-daemon
```

Lint: `OldTargetApi`, `DataExtractionRules`, `MissingApplicationIcon`
(Warnungen), `AutoboxingStateCreation` (Information). Vorhandene SDK-Metadaten-
und Arduino-UART-Warnungen verhinderten den Build nicht. Native Compiler und
Entwicklungswerkzeuge mit freigegebenem Zugriff ausgeführt. Keine neue
Release-APK-/Release-Lint-Prüfung; ältere Ergebnisse gelten für deren alte Quellen.
Regenerierbare JUnit-/Lintberichte liegen in den jeweiligen `build/`-Verzeichnissen.

## Vorbereitete Artefakte

| Artefakt | Byte | SHA-256 |
|---|---:|---|
| Firmware | 1141040 | `1f787f9194a6d6e45493bf0e03c041a527ee53fd8830840c4cd562318e3c8507` |
| Partitionstabelle | 3072 | `22790624ada38b3379c977dd7fd8452c6a3c86c00c64ec118380c888655e5b91` |
| Debug-APK | 23303762 | `34dbeffd6a7c897c3c9d418cb1ad5b3251645fa155e080fb6c0703653ba8772a` |

Firmware: `.pio/led-product/larp-led-product/firmware.bin`;
APK: `companion/android/app/build/outputs/apk/debug/app-debug.apk`.
Die Partitionstabelle stimmt mit dem bisherigen Wake-/NFC-Nachweis überein;
dies ist kein frischer Abgleich am angeschlossenen Chip. Binärausgaben sind
lokal regenerierbar und bleiben außerhalb Git; die editierbaren Quellen
und ihre Tests stehen im PR.

## Offene gemeinsame Abnahme

Keine App-/Firmwareinstallation, kein Gerätezugriff, Flashabzug, Funk-/UART-
Hardwarelauf, keine Hörprobe, kein Zeit-/Pegel-/Versorgungsnachweis in dieser Sitzung.
Vor Installation aktuellen Chip und Zustand feststellen und den chipgebundenen
Rückweg frisch sichern/verifizieren. Danach den [Testplan](test-preparation.md)
mit [Ergebnisbogen](test-record-template.md) durchführen, einschließlich
Pauseposition, Fortsetzung, Stop/Neustart, Titelwechsel und Lautstärkevergleich.
GitHub-CI am veröffentlichten Commit ist ein gesonderter Nachweis.
