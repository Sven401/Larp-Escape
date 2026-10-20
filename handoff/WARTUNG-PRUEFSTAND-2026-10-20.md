# Wartung: Softwareprüfstand und Dokumentationsabgleich

**Status: lokale Prüfung abgeschlossen, 20. Oktober 2026.**
Auftrag: Tests/Builds und Dokumentationsabgleich, daraus ein kleines Wartungspaket
bis zum PR. Sitzung `01a15add-4ca0-7c12-9db7-c42e4c87d633`.

## Basis und Umfang

Nach frischem Fetch geprüfte Integrationsbasis:
`7e19c955aaa54bcc7ed318d594a6f69476de4aa9` aus
`plan/projektuebergabe-kuratierter-stand`. Eigener Branch
`codex/wartung-pruefstand-2026-10-20`, eigene saubere Arbeitskopie vor den
Dokumentationsänderungen. Die ursprüngliche Arbeitskopie auf `e7ec0f9` und ihre
offenen Dateien bleiben erhalten. Produktquellen, Abhängigkeitspins und
Workflowdefinitionen sind unverändert. Die unten genannten lokalen Prüfungen
beziehen sich auf die Produktquellen dieser Integrationsbasis; PR-CI prüft
gesondert den veröffentlichten Dokumentationsstand samt Prüfmerge.

Prüfwege: [B0](../docs/quality-checks.md), [Baugruppen-CI](../.github/workflows/led-product.yml),
[NFC-Probe](../.github/workflows/nfc-probe.yml),
[ESP-NOW-Probe](../.github/workflows/espnow-probe.yml) und
[Apptrennung](../docs/companion/android-app-trennung.md#bauen-und-artefakte).
Kein Upload, keine Installation, kein Merge und keine neue Geräteabnahme.

## Bestätigte Korrekturen

- Die Android-README bezeichnete die native Stationsbedienung trotz des
  integrierten lesenden Piloten weiterhin pauschal als offen. Sie unterscheidet
  jetzt den vorhandenen Pilotumfang von den offenen Licht-/Spielaktionen.
  Auch lokale Boxzuordnungen und Zielwechsel werden als implementierter
  Pilotumfang ausgewiesen; gemeinsame Mehrbox-/Mehrhandy-Abnahme bleibt offen.
  Quelle: [Pilotvertrag samt begrenzter Geräteprüfung](../docs/companion/spielleiter-stationspilot.md),
  `companion/android/gamemaster/` und der lokal geprüfte Mergecommit
  `7e19c955aaa54bcc7ed318d594a6f69476de4aa9` von PR #79.
- Der allgemeine Prüfweg bezeichnete die Geräteabnahme der Apptrennung weiterhin
  pauschal als offen. Er verweist jetzt auf den vorhandenen datierten Nachweis
  für Installation/Datenerhalt und auf die verbleibenden Grenzen.
- Die Android-README führte im Einstieg nur den Dev-Prüfbefehl. Sie enthält jetzt
  den bereits in beiden Android-CI-Jobs verwendeten vollständigen Prüfumfang für
  Dev und GM; die Belegtabelle im B0-Verfahren entspricht diesem Umfang.
- Der dokumentierte vollständige Python-Testlauf benötigt inzwischen die
  Kamera-Abhängigkeiten. Der vorgelagerte Installationsschritt aus der
  Baugruppen-CI und Python 3.12 sind ergänzt. Die lokal installierten
  Paketmetadaten bestätigen `Requires-Python: >=3.12` für NumPy 2.5.4;
  der Kamera-CI-Job verwendet bereits 3.12. Versionspins bleiben unverändert.

## Lokale Ergebnisse

Windows, Python 3.12, PlatformIO 6.2.0, vorhandener MinGW-Compiler;
JDK 21.0.8, SDK 35 und Gradle 8.9. Kamera-Abhängigkeiten und clang-format 19.1.7
liegen in einer isolierten, ignorierten Testumgebung. Android wurde mit
`--offline --rerun-tasks --no-daemon --console=plain --stacktrace --continue`
geprüft: 213 Aufgaben ausgeführt, keine bloße Übernahme früherer Testergebnisse.

| Prüfung | Ergebnis |
| --- | --- |
| Vollständige Python-Tests `unittest discover -s scripts/tests -v` | 64 bestanden |
| Architektur-Selbsttest / Architekturprüfung | 7 Fälle bestanden; 145 Dateien / 302 direkte Includes geprüft |
| D0-Vertragsmodell | 111 semantische Fälle bestanden; keine Kryptografie-/Speicher-/Geräteabnahme |
| Unabhängige C++11-Header | 86 bestanden |
| Hostregression / konfigurierte Steuerung | 105 / 509 Fälle bestanden |
| Echte Mbed-TLS-Hostprüfung | 519 Fälle bestanden |
| Kryptografiequellen und Negativprüfungen | 221 offizielle Dateien; 8 native und 2 SDK-Manipulationen abgewiesen; Originale unverändert |
| Android-JVM-Tests | Protokoll 88, Dev 76, GM 26; keine Fehler oder ausgelassenen Fälle |
| Android-Builds/Lint | Beide Apps Debug und Release erfolgreich; je App/Variante 0 Fehler und 3 Warnungen |
| APK-/Manifesttrennung | Alle vier APKs bestanden; Paketkennungen und Manifestgrenzen getrennt bestätigt |
| C++-Formatbaseline | 309 Dateien mit clang-format 19.1.7 bestanden |
| Gemeinsamer BLE-Wire-/Sicherheitsvertrag | 7873 Assertions mit echtem Host-HMAC und unabhängigen Python-Fixtures bestanden |
| Native LED-/Wake-/Matrix-/Audio-/NFC-Prüfungen | Alle fünf Skriptaufrufe für Wire, Ausgabe, Audio, Kamera und NFC mit Exitcode 0; NFC mit 86 Transaktionsfällen |
| Kamera-Referenzexporte | Funkeln, Lichtimpuls und Ripple erfolgreich aus dem tatsächlichen Effektkern exportiert; synthetischer Nachweis |
| Profilwerkzeug / Produktfirmware / Runtime-/ABI-Proben | `profile-tools`, `az-delivery-devkit-v4`, `esp32-runtime-probe`, `esp32-profile-runtime-probe`, `esp32-gatts-abi-probe` erfolgreich |
| Fünf Baugruppen-/Kameralabor-Builds | `larp-led-product`, `larp-nfc-test`, `larp-espnow-test`, `larp-matrix-test`, `larp-led-camera-lab` erfolgreich |
| Unabhängige NFC-/ESP-NOW-Builds | NFC-Probe und beide ESP-NOW-Ziele bestanden; Syntaxprüfung des ESP-NOW-Prüfskripts bestanden |

Lint meldet in beiden Apps `OldTargetApi`, `DataExtractionRules` und
`ObsoleteSdkInt` als Warnungen. Dev hat zusätzlich `AutoboxingStateCreation` als
Information. Diese Prüfung ändert keine Ziel-SDK-/Backup- oder UI-Entscheidung.
Der Android-Lauf meldete außerdem Schreibwarnungen für SDK-Paketmetadaten unter
Program Files; Tests, Builds, Lint und die APK-Prüfung waren dennoch erfolgreich.

**Ressourcenbefund:** `az-delivery-devkit-v4` belegt 55.148 / 532.480 Bytes RAM
(10,4 %) und 1.229.077 / 1.310.720 Bytes Flash (93,8 %, verbleibend 81.643 Bytes
in dieser Buildpartition). Der Build ist erfolgreich; weitere Funktionspakete
müssen diese geringe Flashreserve berücksichtigen. `larp-led-product` belegt
42.512 / 532.480 Bytes RAM und 1.136.157 / 3.145.728 Bytes Flash. Diese
statischen Buildwerte sind kein gemessener Laufzeitbedarf. Der Produktbuild
meldet außerdem Compilerwarnungen in FastLED/ESP32-Abhängigkeiten; keine
Abhängigkeits- oder Partitionsänderung in diesem Wartungspaket.

Die erste Python-Ausführung scheiterte an Sandbox-Tempzugriffen und fehlenden
Kamera-Modulen; ein vor Installationsende gestarteter Lauf ebenfalls. Sie gelten
nicht als bestandene Prüfungen. Nach abgeschlossener Einrichtung bestand der
vollständige Wiederholungslauf mit 64 Fällen. Kein Produktcode wurde dafür geändert.

## Dokumentprüfung und Nachweise

Inhaltlicher Abgleich von `handoff/MAINTAINER.md`, `handoff/README.md`,
`docs/quality-checks.md`, `companion/android/README.md`, Apptrennungs- und
Stationspilotvertrag mit Integrationshistorie, Android-Modulen und CI-Aufrufen.
162 lokale Dateiverweise aus diesen sechs Dokumenten und sechs aus diesem
Prüfbericht auf Existenz geprüft, keine fehlenden Ziele. Codeblöcke auf
ausgeglichene Zäune geprüft. Keine Prüfung externer URLs oder aller Markdown-Anker.
Historische datierte Gerätebelege und offene Abnahmegrenzen bleiben erhalten.

**Bereits separat vorbereitete Fortschreibung:** Der ältere UI/UX-Absatz im
Maintainer-Einstieg bezeichnet die fachliche native Stationsbedienung noch als
offen. Commit `499ec94` auf `codex/pr79-integrationsabschluss` korrigiert diese
Stelle bereits und ergänzt den gesonderten #79-Integrationsbericht sowie die
beiden Einstiege. Der zugehörige Worktree war beim Abgleich sauber; dieser Commit
ist noch nicht Teil der geprüften Zielbasis `7e19c95`. Das Wartungspaket
dupliziert diese vorhandene Fortschreibung nicht. Ihre Integration bleibt
gesonderte Folgearbeit, kein fehlgeschlagener Softwaretest.

Rohlogs, JUnit-Dateien, Lintberichte und APKs sind regenerierbar und bleiben in
der Arbeitskopie unter `artifacts/` bzw. den Android-Buildverzeichnissen. Für
den lokalen Lauf maßgeblich: `routine-python-final.log`, `routine-host.xml`,
`routine-peer.xml`, `routine-crypto-guards.log`, `routine-android.log`,
`routine-app-separation.log`, `check-results.json` und die dort genannten Logs.
Buildlogs: `routine-firmware.log`, `routine-components.log`,
`routine-nfc-probe.log` und `routine-espnow-probe.log`.
Der frühere, fehlgeschlagene Python-Eintrag in `check-results.json` wird durch
den erfolgreichen vollständigen Wiederholungslauf in `routine-python-final.log`
ergänzt; er wird nicht rückwirkend als erfolgreich umgeschrieben.

Alle 13 lokalen Buildziele und die oben ausgewiesenen abschließenden Prüfungen
sind erfolgreich. Das Paket enthält ausschließlich diese Dokumentationskorrekturen
und den Prüfbericht. Neue PR-CI am veröffentlichten Head und zugehörigen Prüfmerge
ist gesondert am Wartungs-PR auszuwerten; dieser Bericht behauptet dafür keinen
bereits erfolgreichen Lauf.
