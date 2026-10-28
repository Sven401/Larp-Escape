# Automatisierungen und Fachrollen: Bestandsaufnahme

**Status: ausgelesen und fachlich eingeordnet, 23.10.2026, Europe/Berlin.**
Auftrag: alle projektbezogenen Automatisierungen untersuchen und eine Ergänzung
für die Aktualisierung der Fachkompetenz entwerfen.
[Entwurf des Fachaudit-Verfahrens](../docs/quality/fachaudit-entwurf.md).

## Grundlage und Umfang

- Live geprüfter Remote-Head von `plan/projektuebergabe-kuratierter-stand`:
  **`e61d7642d035ccdcf8eca689d6c50a82bcf1560b`**. Prüfung per Remote-Ref-Abfrage;
  Dateien aus dem zugehörigen sauberen kanonischen Checkout gelesen.
- Der beim Einstieg geöffnete Branch `codex/led-resonanzkreis` lag bei
  `e7ec0f94de8abdc525e7cf5bdf7e049831dc7782` und enthielt fremde lokale Änderungen.
  Er ist keine aktuelle Integrationsbasis; seine Änderungen wurden erhalten.
- [AGENTS](../AGENTS.md), [Maintainer](MAINTAINER.md), [Übergabe](README.md),
  versionierte Workflows, Buildmanifeste, Skriptfamilien, Rollen und passende
  Verträge geprüft. Eigener Dokumentationsbranch auf dem oben genannten Head.
- Zusätzlich lokale Codex-Automationskonfiguration, persönliche Hookdefinition
  und zugehörigen Quellumfang, Git-Hooks sowie Windows-Aufgabenplanung lesend
  untersucht. Keine Hooks, Gerätehelfer oder Produktprüfungen ausgeführt.
- Aktuelle Planung in [#25](https://github.com/Sven401/Larp-Escape/issues/25)
  gelesen: SP-01/Mehrbox-Spielpakete sind beschlossen, Dokumentations-PR
  [#94](https://github.com/Sven401/Larp-Escape/pull/94) ist dort noch als offen
  ausgewiesen. Bestätigte Produktentscheidung und integrierter Code bleiben getrennt.

Die Bestandsaufnahme umfasst den zugänglichen Projekt- und lokalen
Automationsbestand. Sie ist kein organisationsweiter GitHub-Audit. Serverseitig
aktivierte Apps, Dependabot-Alerts, Regeln und sonstige Dienste wurden nicht
vollständig administrativ inventarisiert. Eine fehlende Repositorydatei beweist
nicht die Abwesenheit eines solchen Dienstes.

## 1. Zeitgesteuerte Automatisierung

Im lokalen Codex-Automationsverzeichnis wurde **ein Auftrag** gefunden:

| Eigenschaft | Ausgelesener Stand |
| --- | --- |
| Name / ID | Larp-Escape GitHub-Projektstand / `larp-escape-github-projektstand` |
| Typ / Zustand | Chatgebundener Heartbeat / `ACTIVE` |
| Termin | Täglich 09:00; Regel ohne eigene Zeitzonenangabe. Prompt verlangt Datumsangaben Europe/Berlin; effektive Scheduler-Zeitzone und pünktliche Ausführung dadurch nicht belegt. |
| Grundlage | Tatsächlicher Remote-Head des kuratierten Integrationsbranches, aktuelle Issues ab #25 und bestehendes Projektboard |
| Aufgabe | Belegten technischen Fortschritt in genau einem eigenen Markerabschnitt je Issue und vorhandenen Boardstatus abgleichen |
| Grenzen | Keine neuen Issues/Kommentare, Kriterien, Zuständigkeiten, Prioritäten oder Produktentscheidungen; keine Produktdateien, Builds, Uploads, Commits, PRs oder Merges |
| Verhalten | Vorher frisch lesen, nachher zurücklesen; fremden Text erhalten; unveränderter Stand still; Konflikte gezielt ausnehmen |

Die installierte Fassung wurde mit dem
[versionierten Prompt-Snapshot](github-sync-audit-2026-10-20/prompt.md) abgeglichen.
Der ausgelesene Prompt-Hash stimmt mit dem dort belegten SHA-256 überein:
`ed34b0a306432fef0b2500f31e6e43a5904bb6c42094183a5a7072f9d35cf9a0`.
Die ausführliche [Umstellung vom 20.10.2026](GITHUB-AUTOMATISIERUNG-2026-10-20.md)
bleibt Herkunft. Der Prompt-Snapshot ist Dokumentation, kein eigener Scheduler.

**Jüngster ausgelesener Laufverlauf:** Die drei letzten abgeschlossenen Läufe
des gebundenen Automationschats wurden am 23.10.2026 gelesen. Keine privaten
Chatprotokolle ins Repository übernommen.

| Laufdatum | Ausgelesener Verlauf und Grenze |
| --- | --- |
| 21.10.2026 | Drei Issue-Updates (#25/#33/#37); Boardabgleich für #33/#37/#60. Die letzten beiden Statusänderungen erfolgten nach zusätzlicher Nutzerfreigabe im Lauf. Rücklesen/Neuladen im Verlauf dokumentiert; #34 bleibt wegen Kriterienkonflikt unverändert. |
| 22.10.2026 | Kein neuer integrierter technischer Fortschritt; SP-01 bereits in Issues erfasst, #94 offen. Keine Änderungen gemeldet. |
| 23.10.2026 | Integrationsstand, zugeordnete Issues und Board unverändert; keine Änderung und keine Benachrichtigung. |

Das sind gelesene Laufbelege, kein hier erneut ausgeführter Schreibtest und
keine vollständige Revision aller früheren Issue-Bodies. Der aktuelle #25-Body
enthält den entsprechenden Fortschrittsabschnitt. Die ältere Aussage
„geplanter Lauf noch nicht beobachtet“ gilt nur für den Bericht vom 20.10.

## 2. GitHub Actions

**Vier versionierte Workflows, maximal zehn Jobs je passendem Ereignis.**
Keiner enthält einen Zeitplan. Jobanzahlen sind Konfiguration, keine Aussage
über aktuell erfolgreiche Läufe.

| Workflow | Trigger | Automatisierter Umfang / Nachweisgrenze |
| --- | --- | --- |
| [Quality](../.github/workflows/quality.yml), 3 Jobs | Push, PR, manuell | Windows-Hosttests, echte Host-Kryptografie, Architektur/Header/Format, D0-Modell, vier Firmwareziele, beide Android-Apps mit Tests/Debug/Release/Lint/Pakettrennung. Quellen-/Laufbelege über quality_evidence. Keine neue Herstellerrecherche. |
| [LED product](../.github/workflows/led-product.yml), 4 Jobs | Push mit Pfadfilter, alle PRs, manuell | Synthetische Kamera-/Laborwerkzeuge, Wire-/LED-/Wake-/Matrix-/Audio-/NFC-Kerne, vier Produkt- und zwei Laborfirmwareziele, Android. Kein optischer Gerätelauf. Beleghelfer nicht über alle Jobteile gleichmäßig eingesetzt. |
| [NFC probe](../.github/workflows/nfc-probe.yml), 2 Jobs | Push mit Pfadfilter, alle PRs, manuell | Native NFC-Transport-/Tagtests und eigener Firmwarebuild; kein reales Tag-/RF-Verhalten. |
| [ESP-NOW probe](../.github/workflows/espnow-probe.yml), 1 Job | Push und PR mit Pfadfilter, manuell | Sender/Empfänger bauen und Gerätetestskript auf Syntax prüfen; Gerätetest selbst wird nicht ausgeführt. |

Quality trennt PR-Prüfmerge, Head/Basis und Push-Commit in den Belegen.
Vorhandene historische grüne Läufe wurden nicht auf den heutigen Head übertragen.
Prüfverfahren: [quality-checks.md](../docs/quality-checks.md).

## 3. Buildhooks, Prüfskripte und weitere Helfer

Die folgenden Familien wurden über das versionierte Dateiinventar und ihre
Aufrufer geprüft. Sie laufen beim Build, über CI oder auf expliziten Aufruf;
sie besitzen keinen eigenen periodischen Rechercheauftrag.

| Familie | Einstieg / Auslöser | Ergebnis und Grenze |
| --- | --- | --- |
| PlatformIO-Vor-/Nachbereitung | [Rootmanifest](../platformio.ini), [LED](../firmware/led/platformio.ini), [NFC](../firmware/nfc-probe/platformio.ini), [ESP-NOW](../firmware/espnow-probe/platformio.ini); production_sources.py / skip_arduino_start.py | Varianten und Quellfilter; originale Quellen für getrennte Proben. Build ist keine Geräteabnahme. |
| Quellen-/Patchintegrität | [PN532-Patch](../scripts/patch_pn532.py), [SDK-Pins](../scripts/pin_gatt_source_sdk.py), [Mbed-TLS-Vorbereitung](../scripts/prepare_peer_mbedtls.py), [Quellaudit](../scripts/native_peer_mbedtls.py), check_peer_crypto_sources.py | Bekannte Quellen/Archive/Binärhashes, Negativfälle; unbekanntes SDK verlangt neuen Audit. Prüft Integrität, nicht heutige Empfehlungswürdigkeit. |
| Architektur und lokale Qualitätsregeln | [check_architecture.py](../scripts/check_architecture.py), check_headers.py, check_format.py, [check_d0_contract.py](../scripts/check_d0_contract.py), [check_android_apps.py](../scripts/check_android_apps.py) | Includes, Header, Formatbaseline, D0-Semantik, Apptrennung. Keine neue Standardauslegung. |
| Protokoll-/Security-Gegenprüfungen | [check_companion_fixtures.py](../scripts/check_companion_fixtures.py), check_profile_control.py, check_gatts_*, check_peer_*, check_device_identity_store.py, check_identity_retirement.py, check_application_file_owner.py und ABI-Prüfer | Fachmodelle, Fixtures und Besitz-/Rechtegrenzen. Die vollständige Python-Gegenprüferfamilie ist nicht als Gesamtlauf in den vier Workflows eingebunden. Modelle ersetzen keine RF-/SDK-Laufzeitprüfung. |
| C++-/Wire-Prüftreiber | [Wire-Treiber](../test/test_led_ble_wire/run.py), firmware/led/tests/run_*_tests.py und [NFC-Tests](../firmware/nfc-probe/tests/run_tests.py) | Goldenbytes, echte HMAC-Gegenprüfung, reale Kerne mit Fakes/Stubs; kein Gerät. |
| Generatoren | generate_profile_schema.py, generate_play_examples.py, make_peer_proof_fixture.py, make_peer_registry_fixture.py, test/test_led_ble_wire/generate_vectors.py | Schreiben Schemata/Beispiele/öffentliche Testvektoren. Regeneration ist keine unabhängige Prüfung; nicht automatisch durch einen Auditor auslösen. |
| Gerätezugang und Funk/NFC | [USB-Zugang](../scripts/provision_led_ble.py), [NFC-Probe](../scripts/nfc_probe.py), [ESP-NOW-Gerätetest](../firmware/espnow-probe/run_device_test.py) | Bewusster Gerätezugriff, teilweise reale Aktionen/Resets. Kein Bestandteil einer allgemeinen Recherchefreigabe. |
| Kamera und LED-Labor | scripts/led_camera*.py, [Live-Anleitung](../docs/led/interactive-development.md) | Aufnahme, ROI/Kalibrierung, Referenzen, Bewertung, serielle Parameter-/Lichtsteuerung und RSSI; synthetische Tests getrennt. Gerät und Rückweg benötigen passenden Auftrag. |
| Datierte Modelle/Auswertung | docs/led/probes/naeheglut, docs/espnow/probes/naeheglut, [Stop-Audit](../docs/led/probes/stop-audit/HERKUNFT.md) | Wiederholbare Auswertung bestimmter Belege; keine neue physische Beobachtung. |
| Hardwarequellen | [extract_source_nets.py](../Hardware/pruefung/extract_source_nets.py) | Geometrische EasyEDA-Verbindungsprüfung und strukturelle SCHDOC-Inspektion ohne Connectivity-Decodierung; kein CAD-ERC, keine physische Netzliste oder elektrische Freigabe. |
| Webpilot | [package.json](../design/box-pilot/package.json), [Sites-Hostingkonfiguration](../design/box-pilot/.openai/hosting.json), design/box-pilot/scripts | Hashprüfung geschützter Runtime-Dateien vor dev/build, TypeScript/Vite/Sites-Verpackung mit versionierter Hostingkonfiguration, kein daraus abgeleiteter Deploymentauftrag; manuelle Zustands-/Playwright-/Workerprüfungen. Kein GitHub-CI-Aufrufer gefunden; keine dritte Produktplattform. |
| Historischer Companion | companion/app/test/*.test.mjs, [Firmware-Snapshotprüfer](../companion/contract/scripts/verify-firmware-v2-snapshot.mjs) | Szenario-/Leseprobentests und zwölf Fixture-/Schemahashes; Kopienintegrität belegt keine heutige Gerätekompatibilität. |
| GitHub-Sync-Probelauf | [check_dry_run.py](github-sync-audit-2026-10-20/check_dry_run.py) | Zwei datierte Snapshots und 16 Schutzfälle. Keine produktive GitHub-Schreibschnittstelle; erzwingt nicht das Modellverhalten späterer Läufe. |

## 4. Persönliche Hooks, lokale Scheduler und Rollen

**Ausgelesen:** Die persönliche Hookdefinition enthält SessionStart,
PostToolUse und Stop; alle verweisen auf dasselbe Werkbank-Skript.
Sein SHA-256 ist unverändert gegenüber dem [Setupbericht](PROJEKT-SETUP.md):
`b4b064d3dae2b4ba9cbdce839713535aae8e2ead86629c995ee9f862d9011417`.
`repoList()` umfasst persönliche Werkbank und direkte persönliche Git-Skills,
nicht dieses Projekt. Startmeldung, heuristisches Zugriffjournal und Erinnerung
an offene Wissensdateien sind daher keine umfassende Projektjournalisierung.
Definition und Quellumfang wurden gelesen; kein Hook manuell ausgeführt oder geändert.

- Keine aktiven Repo-Git-Hooks gefunden: kein core.hooksPath, nur Sampledateien.
- Keine VS-Code-Taskkonfiguration im untersuchten Projektstand gefunden.
- Die lesende Windows-Aufgabenabfrage fand keine Aufgaben mit Larp, Codex oder
  Werkbank in Name/Aktion. Dies ist eine gezielte lokale Suche, kein Beweis
  gegen beliebig anders benannte Dienste oder entfernte Scheduler.
- Formalisierte Rollen: [Maintainer](../AGENTS.md),
  [LED-Effekt-Architekt 2.0](../docs/led/rollen/led-effekt-architekt.md),
  [UI/UX Designer](../docs/ui-ux/ROLLE.md) mit Projekt-Skill und Agent-TOML.
  Diese Rollen sind auftragsgebunden, keine eigenständigen Zeitpläne.
  Das Vorliegen der UI/UX-Konfiguration belegt nicht deren Laden im laufenden Desktop.
- Weitere projektgebundene Agentenprofile wurden im untersuchten kanonischen
  Bestand nicht gefunden. Firmware/RTOS/Security/Hardware sind Fachgebiete und
  Verträge, bislang ohne eigene formalisierte Agentendefinition.

## 5. Lücken und Konsequenzen für die Ergänzung

**Belegt im untersuchten Repository:** Keine versionierte Dependabot-/Renovate-,
CodeQL-, SBOM-, Advisory- oder Standards-Auditkonfiguration. Keine automatisierten
Release-/Deployment-Workflows in GitHub Actions; die Sites-Hostingkonfiguration
und Buildverpackung des Webpiloten sind dagegen vorhanden (siehe oben).
Keine regelmäßige Quellenrevision oder dokumentierte Kette vom neuen externen
Fachbefund zur aktualisierten Rollenreferenz gefunden. Serverseitige Dienste
bleiben von dieser Negativaussage ausgenommen.

**Abgeleitet:** Benötigt wird ein fachlicher Aktualisierungskreislauf mit
Versionsbezug, Empfänger, Entscheidung und Nachprüfung. Zusätzliche Buildjobs
allein schließen diese Lücke nicht. Pins, ABI-Hashänderung, PN532-Patch und
Frameworkmigration bieten konkrete Auslöser; WLED bleibt Lernreferenz.

Der bestehende Fortschrittsabgleich ist dafür kein zulässiger direkter
Änderungskanal: neue Aufgaben und Kriterien liegen außerhalb seines Mandats.
Der [Entwurf](../docs/quality/fachaudit-entwurf.md) ergänzt eine eigene
Recherche-/Reviewstrecke und erhält den bisherigen Abgleich für belegte Ergebnisse.

## Prüfstand dieses Dokuments

Durchgeführt: Quellen-/Konfigurationsinventar, live Remote-Abgleich, Lesen der
letzten drei Automationsläufe und aktueller Planung, gezielte lokale Scheduler-
und Hookprüfung sowie offizielle Quellenrecherche für den Entwurf.
Produktbuilds, Host-/Geräteprüfungen, neue Automationsläufe und Konformitätsaudit
wurden nicht durchgeführt. UTF-8, lokale Verweisziele und Diff wurden geprüft. Getrennte Gegenprüfungen
von Rollen/Mandat, offiziellen Fachquellen und Automationsinventar erfolgten;
zwei Präzisierungen zu Sites-Hosting und SCHDOC wurden eingearbeitet.
Der veröffentlichte Commit und sein eigener CI-Status stehen im PR.
Historische Ergebnisse gelten nur für ihre jeweils genannten Stände.
