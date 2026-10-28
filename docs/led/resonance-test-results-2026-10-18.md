# Resonanzkreis: Softwareprüfstand · 18. Oktober 2026

Nachfolgender Gerätebeleg: [selbständiger Kameralauf mit synthetischen
Wake-Eingängen](resonance-camera-results-2026-10-18.md). Dieser Bericht bewahrt
den früheren Softwareprüfstand; Produktinstallation und gemeinsame Sensor-/BLE-
Abnahme bleiben offen.

**Belegt:** Produktquellen und Tests bei
`11da7fc9f05147d504ea51edc3219e0c5cdc2c7f` auf
`codex/led-resonanzkreis`. Dieser Stand enthält die ausdrücklich freigegebene
Übernahme von `0b0843d5fce475d14705c42c21f4f8bf4314df30` ausschließlich in den
Arbeitsbranch. Der spätere Zielstand `5f90d109e3589d74d73ac26f859dcd76c2902439`
ergänzt laut GitHub-Vergleich nur drei Übergabedokumente; keine Produktquellen.
PR: [#71](https://github.com/Sven401/Larp-Escape/pull/71), Ziel
`plan/projektuebergabe-kuratierter-stand`. Keine Zielintegration oder Main-Änderung.

## Lokale Ergebnisse am vereinten Produktstand

| Prüfung | Ergebnis |
| --- | --- |
| `python test/test_led_ble_wire/run.py` | Bestanden: 7.945 Assertions mit echtem Windows-HMAC und unabhängig erzeugten Python-Referenzbytes; Resonanzkreis-Fähigkeit, Start, Antwort nach Ausgabe, Duplikat und Trennen enthalten |
| `python firmware/led/tests/run_output_tests.py` | Bestanden: 21 LED-Enginefälle, vier RMT-Fälle sowie Wake-/Matrix-/I2C-Prüfungen |
| Neue Resonanzkreisfälle | Alle 16 Masken und alle 23 Pixel; Atmen; Zusammenführung; Loslassen; ungültiger Lesestand/Maske; Wiederkehr; bewusster Neustart; Stop; Effektwechsel; unveränderter Sendepuffer während TX; Uhrüberlauf; keine ungewollte RSSI-Dimmung |
| `python firmware/led/tests/run_camera_tests.py` | Alle sechs bestehenden Laborfälle bestanden; keine neue optische Messung |
| `python firmware/led/tests/run_audio_tests.py` | Elf Ausgabefälle und fünf UART-Fälle bestanden |
| `python firmware/nfc-probe/tests/run_tests.py` | Frame-/Target-/CRC-/NDEF-Prüfungen und 86 Transaktionsfälle bestanden |
| Android Protokollkern | 89 JVM-Fälle, null Fehler/Fehlschläge/ausgelassene Fälle |
| Android App | 77 JVM-Fälle, null Fehler/Fehlschläge/ausgelassene Fälle |
| Android Debug/Release | Beide APKs gebaut; `lintDebug` und `lintRelease` je null Fehler und drei bestehende Warnungen: `OldTargetApi`, `DataExtractionRules`, `MissingApplicationIcon` |
| Architekturprüfung | 145 Dateien, 302 direkte Includes; sieben Selbsttests bestanden |
| Fünf Firmwarevarianten | `larp-led-product`, `larp-nfc-test`, `larp-espnow-test`, `larp-matrix-test` und `larp-led-camera-lab` bestanden; gesamter Aufruf 7 min 44 s |

Android-Aufruf aus `companion/android/`:

```powershell
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:lintDebug :app:lintRelease :app:assembleDebug :app:assembleRelease --offline --no-daemon --console=plain
```

Lokale Umgebung: vorhandenes Gradle 8.9, JDK 21.0.8, SDK/Build Tools 35;
PlatformIO Core 6.2.0 aus der vorhandenen Python-Umgebung, Espressif32 6.6.0 und
Arduino ESP32 2.0.14 unverändert. Die ersten Compiler-/Temporärdateiversuche
in der Windows-Sandbox scheiterten an Umgebungseinschränkungen; die oben
bestandenen Läufe erfolgten mit freigegebenem Zugriff. Kein Ersatz grüner
Produktnachweise durch Sandboxfehler. Android meldete zusätzlich Hinweise
zum schreibgeschützten SDK und einen beim Start beendeten Kotlin-Daemon;
der vollständige Build wurde anschließend erfolgreich abgeschlossen.
Die Firmwarebuilds enthalten die bestehende Warnung in `esp32-hal-uart.c`
(`uartSetPins`, Rückgabe ohne Wert); kein neuer Compilerfehler.

Regenerierbare JUnit-XMLs liegen unter
`companion/android/{core/protocol,app}/build/test-results/`; Lintberichte unter
`companion/android/app/build/reports/`. Die Dateien bleiben außerhalb Git.

## Bereitgestellte lokale Artefakte

Die Kennungen bezeichnen die lokal erzeugten Dateien und kein installiertes
Paar. Release ist ein unsignierter Build, keine Veröffentlichung.

| Artefakt | SHA-256 |
| --- | --- |
| `companion/android/app/build/outputs/apk/debug/app-debug.apk` | `b9daf65475eef34f5b53d85e9ebb42280ce65cbe5426fef09fb4b773abb90de9` |
| `companion/android/app/build/outputs/apk/release/app-release-unsigned.apk` | `bc5346ee6f6989d51e974861045463be19564d1537382a74623cffdb4a8c6cba` |
| `.pio/led-product/larp-led-product/firmware.bin` | `ca5c68edb9384b3f46bf0f777341ae52912212153505e00e25f04fb742dbff52` |
| `.pio/led-product/larp-nfc-test/firmware.bin` | `a06bc783e4d7d2a674aee18eebe93152e2ab039c5699c15f083d4af755ad8562` |
| `.pio/led-product/larp-espnow-test/firmware.bin` | `1a486367548504880e82e905cc077e30018c9ef49ab568ddb2d53626d58c3a23` |
| `.pio/led-product/larp-matrix-test/firmware.bin` | `35326dbe7560287b78b39dd17d2a3f77e5a9baa2d4a1debacf2d7acd3e664505` |
| `.pio/led-product/larp-led-camera-lab/firmware.bin` | `48551ed41d3e6522271c87be79488b0c9a2175b3604281bfd6ffb4248615cd9e` |

## CI-Zuordnung

Die PR-Prüfung läuft gegen den tatsächlichen GitHub-Prüfmerge, getrennt vom
lokal geprüften Produktcommit. Für Produkthead `11da7fc` und Zielbasis
`5f90d10` wurde Prüfmerge `ba2921006def93fe466ff1a13a4dd12f56af3060`
im ausgelesenen Checkoutlog des gemeinsamen Protokoll-/Ausgabejobs bestätigt.
Die anfänglichen PR-Läufe sind
[Baugruppen](https://github.com/Sven401/Larp-Escape/actions/runs/39131581967),
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39131581987),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/39131581948) und
[ESP-NOW](https://github.com/Sven401/Larp-Escape/actions/runs/39131581876).
Beim Erstellen dieses Berichts waren die gemeinsamen Protokoll-/Ausgabetests,
Kamerawerkzeuge, beide Proben und hardwareunabhängigen Quality-Checks bereits
erfolgreich; übrige CI-Builds liefen noch. Das ist keine Behauptung einer
vollständig abgeschlossenen CI. Nach dem reinen Berichtscommit sind die neuen
Head-/Prüfmerge-Läufe unter [PR #71](https://github.com/Sven401/Larp-Escape/pull/71)
maßgeblich; ältere grüne Teilergebnisse werden nicht auf den neuen Head übertragen.

## Grenzen und nächster Schritt

**Offen:** Installation, tatsächliche Wake-/Lichtzuordnung am Gehäuse,
Reaktionszeit, Sichtbarkeit, gemeinsames Halten ohne App und Fehlerverhalten
am realen Aufbau. Kein Handy-, Bluetooth-, Kamera- oder Geräteversuch in
dieser Umsetzung. Die neue Funktion ist manuelle Baugruppenbedienung;
automatische Spielintegration bleibt getrennt.

Nächster konkreter Schritt ist der gesondert beauftragte Sichtlauf gemäß
[Prüfplan und Ergebnisbogen](resonance-test-preparation.md), nach Feststellen
des tatsächlich installierten Paars und Abgleich des chipgebundenen Rückwegs.
