# Larp-Escape

ESP32-Steuerung des Reichstag-Spiels: Rollenkarten, Kristallmatrix, Audio, LEDs und ESP-NOW-Aktivierung.

**Entwicklungseinstieg · 20.10.2026:** Kanonische Basis ist
`plan/projektuebergabe-kuratierter-stand`, hier gegen `97a2584` geprüft.
Der [Maintainer-Stand](handoff/MAINTAINER.md) führt nächste Arbeit und Grenzen;
[Setup und Prüfwege](handoff/PROJEKT-SETUP.md) erklären die Arbeitsumgebung.
Die [technische Übergabe](handoff/README.md) erschließt datierte Belege und Archive.

| Arbeitsbereich | Vorhandene Grundlage | Einstieg und offene Fortsetzung |
| --- | --- | --- |
| **Larp Dev** · `companion/android/app/` | Baugruppenbedienung und Tests für LED, Wake, Matrix, Audio, NFC und ESP-NOW | [Android-Anleitung](companion/android/README.md); berührte Funktionen am passenden V2-Paar gezielt nachprüfen |
| **Larp GM** · `companion/android/gamemaster/` | Eigenständige App: Station vorbereiten, auswählen, automatisch anmelden und bestätigten Zustand lesen | [Stationspilot](docs/companion/spielleiter-stationspilot.md); Licht-/Spielaktionen und allgemeine Spielleitungsrechte bleiben offen |
| **Baugruppenfirmware** · `firmware/led/` | Gemeinsamer BLE-V2-Vertrag und getrennte Produkt-/Prüfvarianten; eigenständige Kamera-/Effektlabore | [Firmware-Anleitung](firmware/led/README.md), [Wire V2](docs/led/ble-wire-v2.md); gemeinsame Geräteabnahme und Spielanschluss bleiben offen |
| **Spiel- und Profilkern** · `src/`, `include/`, `lib/` | Bestehendes festes Spiel sowie konfigurierbarer Profilkern mit synthetischen Runtime-Proben | [Geräteausführung](docs/configuration/device-runtime.md), [Profilanwendung](docs/configuration/profile-application.md); reale profilabhängige Gerätebesitzer und Produktionsanschluss bleiben offen |

Die Produktquellen sind seit Stationspilot-Merge `7e19c95` unverändert;
die [Wartungsprüfung vom 20.10.2026](handoff/WARTUNG-PRUEFSTAND-2026-10-20.md)
belegt lokale Tests und Builds dieses Standes. Die historischen
[Baugruppenergebnisse](handoff/BAUGRUPPEN-NACHWEISE.md) und der begrenzte
GM-Gerätelauf bleiben erhalten; sie sind keine gemeinsame Gesamtspielabnahme.

**Verbindliches Zielbild:** [Dezentrale Veranstaltung (VA-01)](docs/veranstaltungsarchitektur.md)
mit autonomen Boxen, BLE je Station und unterbrechbarer Infrastruktur.
[D0 und Folgepakete](docs/veranstaltungsarchitektur-umsetzung.md) trennen die
integrierte Spezifikation von ihrer offenen Umsetzung. Für LED-Arbeit gelten
die [Projektgrenzen](docs/led/implementation-reference.md); Näheglut hat einen
[Laborstand und offenen Produktanschluss](docs/led/naeheglut-spielanschluss.md).

## Entwicklung ohne Hardware

PlatformIO Core **6.2.0**, Python **3.11** für B0; der vollständige Python-/Kameratestlauf benötigt **3.12**. Plattformen und Bibliotheken sind in `platformio.ini` festgelegt. Der Host verwendet Windows und C++11. Ein ESP32 wird für diese Schritte nicht benötigt:

```powershell
python scripts/check_architecture.py --self-test
python scripts/check_architecture.py
pio test -e host-regression
pio run -e az-delivery-devkit-v4 -e esp32-runtime-probe
python scripts/check_headers.py
python -m pip install clang-format==19.1.7
python scripts/check_format.py
```

Der [vollständige Prüfweg](docs/quality-checks.md) umfasst zusätzlich konfigurierte
Steuerung, echte Kryptografie, die GATTS-ABI-Probe und Android. Die CI prüft bei
Pull Requests den Zusammenführungsstand mit dem Zielbranch und bewahrt
commitbezogene Prüfprotokolle auch bei Fehlern. Hosttests und Firmware-Build
sind getrennte Nachweise; sie bestätigen weder Verdrahtung noch Timing am Gerät.

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

Unter `companion/android/` liegen die getrennten Kotlin-/Compose-Apps Larp Dev
und Larp GM mit gemeinsamem `core:protocol`. [Apptrennung und Datenerhalt](docs/companion/android-app-trennung.md)
sowie der [lesende GM-Stationspilot](docs/companion/spielleiter-stationspilot.md)
sind implementiert. Allgemeiner Spielzugang, Spielsteuerung und veranstaltungsweite
Verteilung bleiben offen; der Baugruppen-HMAC-Zugang ersetzt keine GM-Rollen.

Die technische Übergabe in `handoff/` ist ohne Zugriff auf frühere interne
Projektwerkzeuge lesbar. Sie nennt die erhaltenen Commitstände, Prüfungen und
offenen Arbeiten.

## Qualitätsumfang

`check_architecture.py` prüft direkte Includes und Schichtrichtungen, nicht den vollständigen C++-Abhängigkeitsgraphen. Makro-Includes und unbekannte Projektincludes scheitern sichtbar. `--self-test` führt sieben isolierte Fixtures aus.

`check_headers.py` übersetzt jeden öffentlichen Header der reinen Schichten separat mit C++11, `-Wall -Wextra -Wpedantic -Werror`, ohne Hardware-Stubs. Der Hostcompiler wird nach den Hosttests von PlatformIO bereitgestellt.

`check_format.py` verwendet exakt clang-format **19.1.7** und `.clang-format`. Die explizite Baseline umfasst die reinen Schichten, die im Skript genannten überarbeiteten Adapter, beide Einstiegspunkte und die neuen Host-Suites. Alte Konfiguration, ArduinoClock, EspNowCommunicator und die frühere `test_regression` bleiben außerhalb der Formatbaseline. `--list` zeigt die konkreten Dateien, `--fix` formatiert ausschließlich diese geprüfte Liste. Den aktuellen Dateiumfang liefert `--list`; neue reine Schichten und der Profilkanal sind einbezogen.

Die Windows-CI ersetzt keine zusätzliche Prüfung von Include-Groß-/Kleinschreibung auf einem case-sensitiven System. Statische Analyse wurde lokal mit Cppcheck 2.1 für die reinen Implementierungen (`warning,performance,portability`) durchgeführt; clang-tidy und eine verpflichtende vollständige Analyse sind noch offen.

## Konfigurierbare Firmware und Companion

Der erste hardwarefreie Profilkern enthält Hardware, Rollen, Zustände, Übergangsbedingungen, Aktionen und Effekte. [Modell, Export und Grenzen](docs/configuration/profile-model.md), [Standardprofil](docs/configuration/standard-profile.json) und [strukturelles JSON-Schema](docs/configuration/profile-v1.schema.json). JSON-Import, Graphauswertung und eine profilabhängige Spielsitzung mit Rechnerbedienung sind implementiert. Komponentenplanung, Broker, statische FreeRTOS-Queues und kontrollierte Profilübernahme mit zwei privaten Profilspeichern sind implementiert und separat mit synthetischen Besitzern baubar. Zwei-Datensatz-Journal und synchronisierter Rechner-Dateispeicher sind implementiert; der ESP32-Speicheradapter ist an die separat gebaute FreeRTOS-Probe angeschlossen. Der gemeinsame Anwendungsablauf koordiniert Einrichtung, Speicherbestätigung und Spielleiterfreigabe in Rechnerwerkzeug und Probe. Für diesen Profilpfad bleiben reale profilabhängige Treiber, Produktionsanschluss, Geräte-/Stromausfallabnahme und die Anbindung an die Android-Apps offen.

Konfigurierbare Graphausführung und Rechner-Vorschau: [Laufzeitvertrag](docs/configuration/execution.md).
