# Larp-Escape

ESP32-Steuerung des Reichstag-Spiels: Rollenkarten, Kristallmatrix, Audio, LEDs und ESP-NOW-Aktivierung.

**Entwicklungsbasis:** `plan/projektuebergabe-kuratierter-stand` im
[Repository Sven401/Larp-Escape](https://github.com/Sven401/Larp-Escape).
Die verbindliche Entwicklungsplanung steht in
[Issue #25](https://github.com/Sven401/Larp-Escape/issues/25); die reproduzierbare
Prüfgrundlage B0 wird in [Issue #38](https://github.com/Sven401/Larp-Escape/issues/38)
geführt. Änderungen werden auf eigenen Branches als Pull Requests gegen die
kuratierte Basis vorbereitet. Eine Integration nach `main` bleibt ein eigener Schritt.

Die [technische Übergabe](handoff/README.md) beschreibt die zusammengeführten
Grundlagen ab `a40365d` und offene Produkt-/Hardwareprüfungen. Ergänzung vom
14. Oktober 2026: Die sechs dort bezeichneten Archivbranches sind inzwischen
auf GitHub veröffentlicht. Sie sind Quellenlager; unfertige und verworfene
Entwürfe werden vor einer Wiederaufnahme einzeln bewertet.

## Entwicklung ohne Hardware

Die [Prüfanleitung](docs/quality.md) führt die vier Quality-Jobs, feste
Werkzeugversionen und lokale Aufrufe für C++-Hosttests, echte Hostkryptografie,
ESP32-Builds sowie Android-Tests/Build/Lint zusammen. PlatformIO Core **6.2.0**
und Python **3.11** bilden die Firmware-Prüfbasis; Plattformen und Bibliotheken
sind in `platformio.ini` festgelegt. Der Host verwendet Windows und C++11.

Die CI läuft bei Push, Pull Request oder manuellem Start. Ergebnisse sind dem
geprüften Commit im jeweiligen Lauf zuzuordnen. Hosttests, Firmware-Build und
Android-Build sind getrennte Nachweise; sie bestätigen weder Verdrahtung noch
Funk, Timing oder Bedienbarkeit am realen Gerät.

## Module

| Schicht | Ort | Aufgabe |
| --- | --- | --- |
| Domain | `include/GameDomain`, `src/GameDomain` | Rollen, Kristalle, Fortschritt und Resetregeln |
| Konfiguration | `include/GameConfiguration`, `src/GameConfiguration` | Begrenzte Profile, semantische Prüfung, JSON-Import/-Export |
| Ausführung | `include/GameExecution`, `src/GameExecution` | Konfigurierbarer Graph und profilabhängige Spielsitzung |
| Geräteplanung | `include/DeviceRuntime`, `src/DeviceRuntime` | Komponentenbesitz, korrelierte kopierte Aufträge und Antworten |
| Ports | `include/GamePorts` | Wertbasierte Schnittstellen für Geräte und Diagnose |
| Anwendung | `lib/GameState`, `lib/GameApplication` | Spielfluss und Reihenfolge der Dienste |
| Zustandsmaschine | `lib/Machine` | Zustandsbesitz, Guards, einmalige Eintrittsaktionen |
| Zeit-/Indexregeln | `include/GameRuntime` | Hostprüfbare Gerätepolitik ohne Treiber |
| Companion-Protokoll | `include/Companion`, `src/Companion` | Begrenzte Statusserialisierung ohne Netzwerkabhängigkeit |
| Adapter | übrige `lib`-Module | Arduino, FreeRTOS und konkrete Geräte |
| Einstieg | `src/main.cpp` | Aufbau, Start und Laufzeitdiagnose |

Die Spiellogik braucht weder Arduino noch Hardware-Stubs. Die alte Regression prüft zusätzlich die konkrete Farbpalettenzuordnung mit kleinen FastLED-Wertstubs; das ist ein Adaptervertrag.

## Laufzeit und Companion

- [ESP32 am USB ohne Zusatzhardware](docs/usb-probe.md)
- [NFC-Paketvertrag und gepinnte Bibliothekskorrektur](docs/nfc-packet-contract.md)
- [Komponentenplanung und FreeRTOS-Auftragskanal](docs/configuration/device-runtime.md)
- [Kontrollierte Profilübernahme und geänderte Einrichtung](docs/configuration/activation.md)
- [Aktives Profil und geschützter Entwurf](docs/configuration/profile-slots.md)
- [Dauerhafte Profile und Wiederanlauf](docs/configuration/profile-journal.md)
- [Gemeinsamer Einrichtungs-/Speicherablauf](docs/configuration/profile-application.md)
- [Profilabhängige Spielsitzung](docs/configuration/session.md)
- [Besitz, Tasks und Grenzen](docs/runtime.md)
- [Companion-Status v1](docs/companion/status-v1.md)
- [Statusschema](docs/companion/status-v1.schema.json)

Unter `companion/android/` liegt eine Kotlin-/Compose-Entwicklungsbasis mit
lesender BLE-Verbindungsprüfung und A3-Hostvertrag. Sie ist noch keine
freigegebene Spielleitungs-App. HTTP-Endpunkt, WLAN-Einrichtung und produktive
Fernsteuerbefehle sind offen.

Die technische Übergabe in `handoff/` ist ohne Zugriff auf frühere interne
Projektwerkzeuge lesbar. Sie nennt die erhaltenen Commitstände, Prüfungen und
offenen Arbeiten.

## Qualitätsumfang

`check_architecture.py` prüft direkte Includes und Schichtrichtungen, nicht den vollständigen C++-Abhängigkeitsgraphen. Makro-Includes und unbekannte Projektincludes scheitern sichtbar. `--self-test` führt sieben isolierte Fixtures aus.

`check_headers.py` übersetzt jeden öffentlichen Header der reinen Schichten separat mit C++11, `-Wall -Wextra -Wpedantic -Werror`, ohne Hardware-Stubs. Der Hostcompiler wird nach den Hosttests von PlatformIO bereitgestellt.

`check_format.py` verwendet exakt clang-format **19.1.7** und `.clang-format`. Die explizite Baseline umfasst die reinen Schichten, die im Skript genannten überarbeiteten Adapter, beide Einstiegspunkte und die neuen Host-Suites. Alte Konfiguration, ArduinoClock, EspNowCommunicator und die frühere `test_regression` bleiben außerhalb der Formatbaseline. `--list` zeigt die konkreten Dateien, `--fix` formatiert ausschließlich diese geprüfte Liste. Den aktuellen Dateiumfang liefert `--list`; neue reine Schichten und der Profilkanal sind einbezogen.

Die Windows-CI ersetzt keine zusätzliche Prüfung von Include-Groß-/Kleinschreibung auf einem case-sensitiven System. Statische Analyse wurde lokal mit Cppcheck 2.1 für die reinen Implementierungen (`warning,performance,portability`) durchgeführt; clang-tidy und eine verpflichtende vollständige Analyse sind noch offen.

## Konfigurierbare Firmware und Companion

Der erste hardwarefreie Profilkern enthält Hardware, Rollen, Zustände, Übergangsbedingungen, Aktionen und Effekte. [Modell, Export und Grenzen](docs/configuration/profile-model.md), [Standardprofil](docs/configuration/standard-profile.json) und [strukturelles JSON-Schema](docs/configuration/profile-v1.schema.json). JSON-Import, Graphauswertung und eine profilabhängige Spielsitzung mit Rechnerbedienung sind implementiert. Komponentenplanung, Broker, statische FreeRTOS-Queues und kontrollierte Profilübernahme mit zwei privaten Profilspeichern sind implementiert und separat mit synthetischen Besitzern baubar. Zwei-Datensatz-Journal und synchronisierter Rechner-Dateispeicher sind implementiert; der ESP32-Speicheradapter ist an die separat gebaute FreeRTOS-Probe angeschlossen. Der gemeinsame Anwendungsablauf koordiniert Einrichtung, Speicherbestätigung und Spielleiterfreigabe in Rechnerwerkzeug und Probe. Reale profilabhängige Treiber, Produktionsanschluss, Geräte-/Stromausfallabnahme, Companion-API und Android-App sind noch offen.

Konfigurierbare Graphausführung und Rechner-Vorschau: [Laufzeitvertrag](docs/configuration/execution.md).
