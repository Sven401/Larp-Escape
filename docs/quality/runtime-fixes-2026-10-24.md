# Fachmandate und Laufzeitkorrekturen A1-01 / A2-01

**Status: umgesetzt und intern fachlich gegengeprüft; Softwareprüfungen bestanden.**
24.10.2026, Europe/Berlin. Nutzerauftrag in Sitzung
`01a16ab1-34d7-73b0-924a-fc2d245b9027`; PR
[#95](https://github.com/Sven401/Larp-Escape/pull/95) gegen
`plan/projektuebergabe-kuratierter-stand`, Basis
`e61d7642d035ccdcf8eca689d6c50a82bcf1560b` erneut am 24.10.2026 abgeglichen.
Mandate zuerst übernommen in `0a3a3942e44de7c532eed6d701417635f4138b69`;
anschließend Laufzeitcode, Regressionen und CI in **`c4b84fca18b0732201fe39ec54e8a1482046600b`**.
Die lokalen Abschlussprüfungen begannen auf diesem sauberen Commit.
Der nachfolgende Berichtscommit ändert ausschließlich Dokumentation und Belege.

## Verantwortlichkeit in dieser Umsetzung

| Arbeitspaket | Fachlich verantwortlich | Anderer Gegenprüfer | Varianten und Prüfauftrag |
| --- | --- | --- | --- |
| A1: Kanal, Taskkontext, Autorität | Plattform/Systemintegration: Subagent `official_sources` | Qualität/Nachweise: `responsibility_map`; Maintainer mit eigenem Abschlusslauf | Profilruntime und GATTS-ABI; Kontextguard, FIFO/Mailbox, Widerruf/Epochwechsel, echte Hostthreads und SDK-Builds |
| A2: MCP, Matrix und Spielwirkung | Geräteintegration/Hardwarevertrag: `repo_automation` | Qualität/Nachweise: `responsibility_map`; Maintainer prüft Bibliothekspfad und Verbraucher | Root-Legacyfirmware; echte Adafruit-Quellen, NACK/Kurzantwort/Schreibfehler bis Spielwirkung, Recovery, SDK-Build |
| Integration und Prüfpflichten | Maintainer | Fachautoren prüfen ihren Anschluss; Qualitätsreview der Gesamtgrenzen | Verbindliche Rollen in AGENTS und Rollenvertrag, CI, Quellenstand und getrennte Nachweisarten |

Die Aufgaben und Gegenprüfer wurden vor den Codeänderungen benannt.
Der unabhängige Review meldete keine blockierenden Befunde; seine Empfehlung,
beim Start auch das Ausbleiben jedes GPIO-/OLAT-Schreibversuchs zu prüfen,
ist im MCP-Test enthalten. Interne Gegenprüfung ist keine externe Zertifizierung.
Die [Fachmandate](verantwortlichkeiten.md), [Prüfpflichten](../quality-checks.md)
und der [Rückweg der Rollenübernahme](../../PROMOTION.md) sind verbindlich.

## A1: atomare Übergabe ohne RTOS-Aufruf in der kritischen Sektion

Der [Kanal](../../lib/FreeRtosGameControlChannel/FreeRtosGameControlChannel.cpp)
verwendet zwei feste Wert-FIFOs mit unveränderter Kapazität vier und eine
überschreibbare Statuskopie. Dieselbe kurze Sperre schützt Autorität und
sämtliche Speicherzugriffe einschließlich Rohdiagnostik. Zulassung und
Einreihung bleiben atomar; ebenso Empfängerauswahl und Entnahme. Kopien sind
statisch auf triviale Werttypen begrenzt. Keine weitere Sperre, Allokation oder
FreeRTOS-API wird darin benötigt. `begin` bleibt vor Task-/Callbackfreigabe.

Der neue Kontextguard fand außerdem drei bestehende Task-ID-Abfragen unter
der GATTS-Sperre: `peerRoleState`, `peerProofGrantState`, `cancelPeerProofGrant`.
Die Identität wird jetzt davor gelesen; Taskzuordnung und veränderlicher
Grantzustand bleiben unter der Sperre. Bestehende Scope-/Autoritätsprüfungen
bleiben erhalten. Foreign-Task-Gegenfälle sichern die Zuordnung ab.
Autorisierungsregeln, Wireformat und erneute Epochprüfung vor physischem Senden
bleiben erhalten. [Versionierte IDF-Regel](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-guides/freertos-smp.html#restrictions-and-considerations).

Der neue [Runner](../../test/test_configured_control/run_channel_checks.py)
kompiliert die tatsächliche Kanalquelle. Gegen Ausgangscommit `dc0e3f12…`
kompiliert er erfolgreich und scheitert gezielt an **drei RTOS-Aufrufen unter
Lock trotz Nullwartezeit**. Mit der Korrektur: null solche Aufrufe, drei
erzwungene Konkurrenzsituationen an derselben Sperre, 20.000 FIFO-Kommandos
und Antworten sowie kohärente Statuskopien mit 20 Fortschrittspunkten.
[Wiederholung und Grenzen](../../test/test_configured_control/README.md).

## A2: Busfehler bleiben bis zum Spielverbraucher ungültig

[MCPHandler](../../lib/MCPHandler/MCPHandler.cpp) verwendet die Bool-Overloads
der unverändert gepinnten Adafruit-BusIO-Version. Konfigurations-, Lese- und
Schreibzugriffe prüfen ACK und vollständige Empfangsmenge. Ausgangsänderungen
erhalten andere Bits über OLAT; der Adapter ruft den allokierenden
`begin_I2C`-Pfad weiterhin höchstens einmal auf.

Wake- und Kristalleingaben tragen ein ausdrückliches `valid`. Ein fehlerhafter
Read erzeugt keine Wake-Lichtwirkung, keinen Standby-Austritt und keinen
Kristallfortschritt. Ein später Matrixfehler verwirft auch schon gesammelte
Kontakte. Nach fehlgeschlagenem Select, Read oder Release versucht die Matrix
zunächst die ausgewählte Spalte freizugeben und merkt den Recoverybedarf.
Vor dem nächsten Scan versucht sie alle Spalten freizugeben; eine neue
LOW-Auswahl bleibt bis zur bestätigten vollständigen Freigabe gesperrt.
Auch ein negativ quittierter Schreibzugriff
kann bereits physisch gewirkt haben; dieser Gegenfall ist getestet.

Der Rotvergleich kompiliert die ursprünglichen Adapter, Ports und den realen
Spielcontroller aus `dc0e3f12…` gegen dieselben Bibliotheken: drei gezielte
Fehlerassertionen (NACK und Nullantwort verlassen Standby; NACK wird als
Schreiberfolg gemeldet). Die korrigierte Suite besteht elf Fälle; zwei weitere
Verbrauchertests enthalten absichtlich plausible Nutzdaten mit `valid=false`.
[Reproduktion, Bibliothekspins und Modellgrenzen](../../test/test_mcp_adapter/README.md).

## Tatsächlich ausgeführte Abschlussprüfungen

Werkzeuge: Python 3.12.6, PlatformIO 6.2.0, MinGW G++ 5.1.0 und
clang-format 19.1.7. ESP32: espressif32 6.6.0 / Arduino 2.0.14 / IDF 4.4.6;
MCP23017 2.3.2 / BusIO 1.17.4; echte, hashgeprüfte Mbed TLS 2.28.4.
Keine Abhängigkeitspins wurden dafür geändert.

| Prüfung | Ergebnis am Codecommit |
| --- | --- |
| `host-regression` | 122/122, Exit 0 |
| `host-mcp` | 11/11, Exit 0 |
| `host-configured-control` | 513/513, Exit 0 |
| `host-peer-proof` | 523/523, Exit 0 |
| Kanal mit Hostthreads | 0 RTOS-Aufrufe unter Lock; 3 erzwungene Konkurrenzfälle; 20.000 Kommandos/Antworten und kohärente Statuskopien, Exit 0 |
| `az-delivery-devkit-v4` | Build erfolgreich; RAM 55.188 Byte. Flash 1.229.369 Byte |
| `esp32-runtime-probe` | Build erfolgreich; RAM 21.480 Byte. Flash 291.989 Byte |
| `esp32-profile-runtime-probe` | Build erfolgreich; RAM 107.320 Byte. Flash 406.073 Byte |
| `esp32-gatts-abi-probe` | Build erfolgreich; RAM 50.672 Byte. Flash 1.091.713 Byte |
| Architektur | 7 Guardfälle; 145 Dateien / 302 Includes, Exit 0 |
| Header / Format | 86 eigenständige C++11-Header; 320 Formatdateien, beide Exit 0 |
| PN532-Patchschutz | 10/10, Exit 0 |

[Maschinenlesbarer Prüfstand](audits/2026-10-24-runtime/pruefungen.json)
enthält Commit, Aufrufe mit normalisierten lokalen Pfaden, tatsächliche
Exitcodes, Testzahlen und Ressourcenwerte.
Vollständige lokale Logs/JUnit liegen unter
`outputs/fachaudit-fixes-2026-10-24/verified/` im Hauptarbeitsverzeichnis.
Die bestehende Quality-CI führt den MCP-Lauf und den Kanal-Threadlauf nun
zusätzlich aus; vier Host-JUnit-Berichte und getrennte Befehlslogs werden
bewahrt. CI am veröffentlichten PR-Stand ist ein eigener Nachweis.

## Verbleibende Grenzen und Rückweg

Die zwei Softwarebefunde A1-01/A2-01 sind korrigiert; das ist keine Gerätefreigabe.
ESP32-SMP-/ISR-/Latenzmessung, reale Busunterbrechung und elektrische Recovery
bleiben am Gerät zu prüfen. Bei defektem Bus ist ein physischer HIGH-Pegel
nicht zugesagt. Startpegel, Pinbelegung und Chiprevision wurden nicht geändert;
der bekannte B7-Konflikt bleibt offen. BusIO 1.17.4 prüft die Empfangsmenge,
aber nicht jedes negative `Wire.read`: vorausgesetzt wird der exklusive,
synchrone Wire-Besitzer mit vollständigem angekündigtem RX-Puffer.

Die [IDF-Supportentscheidung](../../handoff/ARDUINO-ESP32-3-MIGRATION.md),
die End-to-End-Frist-/SPI-Grenzen des [NFC-Vertrags](../nfc-packet-contract.md)
und die Abnahme anderer Baugruppen bleiben eigenständige Arbeit. Kein neuer
Scheduler/Heartbeat, keine Firmwareinstallation, Migration, Merge oder Release.
Ein Rückbau muss Adapter und geänderte Port-/Verbraucherverträge gemeinsam
betrachten; er würde die dokumentierten Fehler wieder öffnen. Die historischen
[Pilotbelege](audits/2026-10-24-pilot/README.md) bleiben erhalten.
