# Native ESP-IDF-Migration: Bestand, Entscheidungen und Prüfstand

Stand: 28.10.2026. **Teilumsetzung; keine abgeschlossene Produktmigration.**
Auftrag: Nutzer bittet in Sitzung `01a1840c-a839-7ae3-9954-bb6ffc96d5e4`,
die Ziele des [Migrationsplans](../../handoff/ARDUINO-ESP32-3-MIGRATION.md)
zu verfolgen. Der frühere reine Dokumentationsumfang ist damit für diese
Sitzung um Prüfung und Softwareumsetzung erweitert; Geräteupload bleibt separat.

**Aktualisierung 29.10.2026:** [Mergevorbereitung und neuer SDK-Wartungspin](../../handoff/PR101-MERGEVORBEREITUNG-2026-10-29.md). Die folgenden Versions-/Prüfangaben sind historische Belege des ursprünglichen Schnitts; aktuelle Qualifikation gesondert auswerten.

## Arbeitsbasis, Verantwortung und Prüfgrenzen

Eigener Branch `codex/esp-idf-migration`, aufgebaut auf BLE-Stand `8ac927f`
und anschließend mit Integrationshead `ab7de9b` abgeglichen (lokaler
Zusammenführungscommit `1f9a2cd`). Keine Übernahme nach main oder Zielbranch.
Der ursprüngliche Hauptcheckout und der BLE-Arbeitsbereich bleiben unangetastet.

Federführung Plattform/Systemintegration und Umsetzung: Maintainer `/root`.
Anderer Gegenprüfer für Plattform, Security und Nachweisgrenzen:
`/root/migration_review`, read-only. [Fachmandate](../quality/verantwortlichkeiten.md).
Varianten: neuer nativer Hostkern und isolierte native ESP32-Prüffirmware;
Legacy-/Baugruppenvarianten unten bleiben erhalten und noch zu portieren.
Benötigte Nachweise: echte Hosttests, negative SDK-Pinprüfungen, SDK-Build,
gezielter Quellenreview. Laufzeit/SMP, TLS, Geräte und gemeinsame Last bleiben
eigenständige Tore. Der Gegenprüfer führte selbst keine Tests aus.

## P1: erfasster Bestand und Portierungsmatrix

Quellenbasis: vier `platformio.ini`, reale Adapterquellen, Architekturprüfung
und BLE-Übergabe an obiger Arbeitsbasis. Versionen in der Tabelle sind deklarierte
Alt-Abhängigkeiten, kein neuer vollständiger PlatformIO-Resolverlauf.

| Varianten / Einstieg | Bestand und Grenze | Nativer nächster Schritt / Parität |
| --- | --- | --- |
| Root `az-delivery-devkit-v4`, `src/main.cpp` | Historischer Spielstart mit Arduino-Geräteadaptern | Domain/Ausführung erhalten; reale Eigentümer und Adapter einzeln anschließen; kein Ersatz durch synthetische Owner |
| Root `esp32-runtime-probe` | Synthetische Runtimeprobe | Native Runtime-Prüfung nach Kern-/RTOS-Build; keine Gerätefreigabe |
| Root `esp32-profile-runtime-probe` | Aktivierung, Journal, reale RTOS-Kanäle, synthetische Geräte | `millis`/Serial ersetzen; LittleFS-Komponente qualifizieren; bestehende No-Erase- und Generationstore erhalten |
| Root `esp32-gatts-abi-probe` | Compiler-/ABI-Probe mit privaten sdk446-/BTU-/SMP-Pins | Neue Originalaktor-/Lifecyclebeweise vor Produktanschluss; alte Pins gelten nur für alte Basis |
| `firmware/led`: `larp-led-product` | Eigene Effekte, BLE-V2, Wake/Audio; Arduino-BLE-Wrapper | Nativer Einstieg, BLE-V2/HMAC-Port und reale Adapter; allgemeine GM-Rechte nicht aus Baugruppen-HMAC ableiten |
| `larp-nfc-test`, `larp-matrix-test`, `larp-espnow-test` | Drei getrennte Baugruppenkombinationen | Betroffene Busse, Fehlerfälle und BLE-/Funkkoexistenz getrennt bauen und abnehmen |
| `larp-led-camera-lab`, `larp-led-camera-rssi-lab` | Zwei isolierte optische/Funklabore | Neue RMT-Schnittstelle; Laborbedienung/Referenzen erhalten; keine automatische Produktintegration |
| `firmware/nfc-probe`: `nfc-probe` | Eigener PN532-Kern und SDK-SPI-Anbindung | Native SPI-/Zeit-/Startanbindung; Paket-/Fehlervertrag und Fristen erhalten |
| `firmware/espnow-probe`: `espnow-probe`, `espnow-app-receiver` | Eigener Funkstart und zwei Einstiegspunkte | Neue RX-/TX-Metadaten, RSSI direkt statt Mitschnitt; Sender-/Paketbindung und Koexistenz prüfen |
| Host `host-regression`, `host-configured-control`, `host-peer-proof`, `host-mcp`, `profile-tools` | Bereits getrennte Test-/Werkzeugpfade | CMake-Kern/Profilwerkzeug und Steuerungsfälle vorhanden; echte PSA-Gegenfälle zusätzlich, übrige Legacy-Tests bleiben erhalten |

**Hardware:** Konfiguriertes Board `az-delivery-devkit-v4`, Ziel klassischer
ESP32; ESP32-D0WD-V3 Rev.3 / 4 MiB ist historisch belegt, keine neue Chipabfrage
in dieser Sitzung. [Hardwareplanung](../../Hardware/HARDWAREPLANUNG.md) und
[BLE-Rückwegbeleg](../../handoff/BLE-ROLLEN-PROFILE-2026-10-28.md) bleiben maßgeblich.
Kein PSRAM-Bedarf oder weiteres Chipmodell als bestätigt angenommen.
GPIO15/23 LEDs, I²C21/22, UART-/NFC-Pins und beide B7-Fragen bleiben gemäß den
jeweiligen Adapter-/Hardwareverträgen erhalten; keine elektrische Freigabe.

| Abhängigkeit / Quelle | Befund und nativer Ersatz | Entscheidung / offener Nachweis |
| --- | --- | --- |
| Arduino 2.0.14, PlatformIO espressif32 6.6.0 | Alte IDF4.4.6-Basis; Start/Zeit/Serial/Wrapper | Neue Basis ohne Arduino; alte Buildpfade als Vergleich erhalten |
| PN5321.3.4 / BusIO1.17.4 und `scripts/patch_pn532.py` | Legacy-NFC-Bibliothek samt Sicherheitsreparatur; native Probe existiert bereits | Eigene PN532-Implementierung anhand Paket-/Fehlervertrag qualifizieren; Patch nicht unbemerkt verlieren |
| MCP23017 Arduino2.3.2 | Legacy-Geräteadapter; Baugruppen haben bereits eigenen Registervertrag | Native IDF-I²C-Anbindung; NACK/Kurzantwort/Ausfall und Wiederkehr bis Spielauswertung prüfen |
| Bounce2 2.72.0 | Legacy-Entprellung | Benötigte Flanken-/Zeitsemantik explizit erfassen und portieren |
| EspSoftwareSerial8.2.0, DFPlayerMini_Fast1.2.4, FireTimer1.0.5 | Legacy-Audio; neuer Baugruppenpfad nutzt bereits SDK-UART | SDK-UART-Besitzer und BUSY-Vertrag erhalten; FIFO-Erfolg bleibt keine Modulquittung |
| FastLED3.10.5, ghostl1.0.1 | Legacy-Effektabhängigkeiten | Bestehende eigene Effekte und neue RMT-Treiberanbindung qualifizieren; keine fremde Effektengine neu einführen |
| `driver/rmt.h` | In IDF6 entfernt; bestehende RMT-Symbole/Dauer/Reset-Verträge | Auf `rmt_tx.h`/Encoder portieren; feste Puffer bis TX_DONE behalten, Reset/Stop und Optik separat prüfen |
| ESP-NOW4.4-Callbacks und RSSI-Mitschnitt | Geänderte Metadaten-/Callbacktypen | Paketbezogene RX-Metadaten und geänderte TX-Signatur qualifizieren; keine Entfernungsgenauigkeit ableiten |
| MbedTLS2.28.4 / private GATT-/SMP-Hooks | Security- und Quellenpins bewusst eng | Neuer PSA-P256-Adapter; TLS-/GATT-/SMP-Produktweg bleibt geschlossen, bis neue Belege existieren |
| LittleFS, NVS, `larpkeys` | Profile/temporäre Quellen/Peerregister/Identitäten auf getrennten Speichern | IDF-Komponente mit Lockfile und Formatverträglichkeit prüfen; kein Autoformat/Erase bei Fehlern |

## P2/P3: technische Zielentscheidung

**Beschlossen im Maintainer-Mandat für die Qualifikation:** ESP-IDF **v6.1**,
Commit `fff9895c82d744c7237be8847347bdd1b07c6643`, klassisches Ziel `esp32`,
CMake/Ninja/idf.py, ohne Arduino. Das ist eine Build-/Portierungsbasis,
keine Security-/Produktfreigabe. Hersteller-Latest und Tag wurden am 28.10.
über Releasequelle und `git ls-remote` bestätigt; keine ältere Linie als
leichterer Ersatz gewählt. CMake prüft SDK-Commit, Untermodule und tracked
Änderungen über `scripts/check_idf_sources.py`, außerdem Version und Ziel.
Untracked SDK-Dateien erfasst dieser Wächter nicht.

Die [offizielle Versionspolitik](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/versions.html)
nennt 30 Monate Support ab initialem Stable-Release (12 Service, 18 Maintenance).
v6.1 wurde am 27.08.2026 veröffentlicht und liegt im Servicezeitraum;
Supportgrenzen und Advisories vor einer späteren Freigabe erneut prüfen.
Die [6.1-Releasehinweise](https://github.com/espressif/esp-idf/releases/tag/v6.1)
und Migrationen [4.4→5.0](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/migration-guides/release-5.x/5.0/index.html),
[6.0 Peripherie](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/migration-guides/release-6.x/6.0/peripherals.html)
und [Security/PSA](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/migration-guides/release-6.x/6.0/security.html)
begründen die betroffenen Prüftore. Zwischenreleases und komponentenbezogene
Advisories sind bei den noch offenen Adapterportierungen weiter zu prüfen;
ein vollständiger Advisory-/Kompatibilitätsaudit wird hier nicht behauptet.

**Arduino-Nutzenprüfung:** Für die aktuelle native Basis kein erheblicher
Nutzen belegt. Der reine Kern ist bereits unabhängig; LED-Effekte,
PN532-Pakete, Audio-UART und MCP-Registerlogik besitzen eigene Grenzen.
Arduino würde die Prüfung der privaten BLE-Hooks und der Kryptografie nicht
ersetzen. Die [Arduino-Komponentendokumentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
nennt Arduino3.3.12 mit IDF5.5, keinen Kompatibilitätsnachweis für6.1.
Deshalb keine Arduino-Abhängigkeit und keine Absenkung des IDF-Ziels dafür.
Ein zukünftiger konkreter Nutzen müsste mit passendem Versionspaar,
Ressourcen-/Wartungskosten und nativem Rückweg neu belegt werden.

**Pins:** SDK-Untermodul MbedTLS `a2b32072ea898afc1ed5b6caf6931e36028c91d6`
(4.1.0 / TF-PSA1.1.0), SDK-Werkzeuge aus dessen `tools/tools.json`;
Xtensa-GCC15.2.0 `esp-15.2.0_20251204`. Unity2.6.1 am Vollcommit
`cbcd08fa7de711053a3deec6339ee89cad5d2697`. CI nutzt den offiziellen
v6.1-Container am Manifestdigest
`sha256:81893c71bb5e570088901f21def8684c25cd2a9020281bd01b843a7655edb18c`.
Keine externen Firmwarekomponenten aufgenommen; deshalb noch kein künstliches
leeres Component-Manager-Lockfile. Bei LittleFS/weiteren Komponenten Manifest
und echtes aufgelöstes Lockfile versionieren. Der neue CI-Host-Wirelauf benötigt
zusätzlich libssl-dev aus Ubuntu; dessen Paketauflösung ist noch kein vollständig
festgelegter Systempaket-Snapshot.

## P4: implementierter Schnitt und bisherige Nachweise

- Explizite gemeinsame Quellliste für die sechs hardwarefreien Schichten;
  CMake baut Kern und das vorhandene Profilwerkzeug ohne PlatformIO/Arduino.
- CTest führt die vorhandene Steuerungssuite und Wirefälle aus; ein zusätzlicher
  Lauf nutzt echte neue PSA-Kryptografie statt des alten2.28.4-Adapters.
  Zwei Host-Headerweiterleitungen bilden verbleibende Espressif-Includepfade
  auf die originalen privaten TF-PSA-Header ab, ohne Algorithmusänderung.
  Hostkonfiguration und tatsächliche ESP32-Konfiguration bleiben getrennt.
- Neuer `PsaPeerProofCrypto` erhält feste P256-/SHA256-/r,s-Wirewerte,
  strikte low-S-Kanonisierung, Punktvalidierung und eingefrorene Callerwerte;
  volatile PSA-Schlüssel werden je Aufruf freigegeben. Keine private SDK-ABI.
  Der Legacyadapter samt Versionsguard bleibt unverändert als Vergleich.
- Isolierte Prüffirmware kompiliert Kern, echte RTOS-Kanäle und PSA-Adapter.
  Sie startet keine Radio-/Geräteadapter und setzt keine Autorität.
  Build-/sdkconfig-Ausgaben sind ignoriert. [Native Anleitung](../../firmware/idf/README.md).

**Frisch ausgeführt vor Commit:** MinGW5.1-CMake-Build (Kern/Tool/Modelle),
524 Steuerungstests und 7.873 Wireassertionen erfolgreich; Architektur145Dateien/
302Includes und86unabhängige C++11-Header erfolgreich. Mit w64devkit2.9.1 /
GCC16.2 zusätzlich echter neuer TF-PSA-Hostbuild und536Tests erfolgreich.
**Abschließend am gesicherten Codecommit
`e5ab595c0ab59254332b80156184f99bb1786be3` ausgeführt:** nativer CMake-Rebuild,
CTest3/3 (524 Modellfälle,536echtePSA-Fälle,7.873Wireassertionen),
`idf.py -C firmware/idf -DIDF_TARGET=esp32 build` und `idf.py ... size`
erfolgreich. Prüffirmware-Image187.903Bytes, statisches DRAM34.634Bytes;
kein gemessener Heap-/Stackrest, kein Budget des vollständigen Produkts.
Sieben echte SDK-Guardfälle bestanden vor diesem Commit; deren Quellfassung
ist im Commit unverändert gesichert. Formatbaseline325Dateien mit19.1.7
bestanden; Architektur/Headerprüfung wie oben.

[Entwurfs-PR#101](https://github.com/Sven401/Larp-Escape/pull/101) richtet sich
gegen den kuratierten Integrationsbranch. Er enthält die unintegrierten
Vorarbeiten von [#100](https://github.com/Sven401/Larp-Escape/pull/100) als
Voraussetzung, ohne diesen PR oder Zielbranch zu verändern.
**Nachfolgender geprüfter Codecommit:**
`d81dc5ac122af64f3832ba17f0672ecb6c103ee3` ergänzt den POSIX-Journalpfad.
Der Windows-Aufruf bleibt erhalten; Linux nutzt fsync(fileno()). Flush, Sync
und Close müssen weiterhin alle erfolgreich sein. Neuer echter CLI-Test prüft
Journalanlage, Wiederlesen/Export im zweiten Prozess und Ablehnung eines
fehlenden Verzeichnisses ohne dessen Erstellung. Keine Powerloss-/Syncfehler-
Injektionsbehauptung. Lokal CTest4/4 erfolgreich; Legacy-PlatformIO-Pfade
host-configured-control524 und host-peer-proof536 ebenfalls erfolgreich.
Unabhängiger POSIX-Quellenreview ohne konkreten offenen Befund.

**Native CI erfolgreich:** [PR-Lauf40764974516](https://github.com/Sven401/Larp-Escape/actions/runs/40764974516)
und eigener [Push-Lauf40764968617](https://github.com/Sven401/Larp-Escape/actions/runs/40764968617)
am Head d81dc5a: SDK-Wächter/Negativtests, vier native Hosttests und tatsächlicher
ESP32-Build im gepinnten Linux-Container erfolgreich. Der PR-Lauf prüft den
virtuellen Zusammenführungsstand, der Push-Lauf den Arbeitsbranch; keine
Integration in den Zielbranch. Belegartefakt `native-idf-40764974516-1`,
ID12849643191,57.195Bytes, nicht abgelaufen (ausgelesen28.10.2026), mit JUnit,
CTest-Log, IDF-Buildlogs, Projektbeschreibung und tatsächlicher sdkconfig.
Weitere Repository-CI wird getrennt ausgewertet; diese Ergebnisse sind kein
Geräte- oder Gesamtproduktnachweis. Lokale Logs/JUnit unter `private/host-psa` und
`private/*build.log`, nicht versioniert; CI-Artefakte werden getrennt benannt.

Unabhängiger Quellenreview: ursprünglicher CTest-Fixturepfad und fehlende
exakte SDK-Prüfung korrigiert; private BLE-Epochengrenze bleibt offen.
PSA-Punktimport anhand tatsächlichem Treiber geprüft, low-S/caller-freeze/
Keylebensdauer ohne konkreten Securitybefund. Kein eigener Testlauf des Reviewers.

## Noch offene Abschlusskriterien und nächste Reihenfolge

1. Native CI-Qualifikation ist belegt. Alle bisherigen
   Firmware-/Hostvarianten bleiben separate Paritätstore, keine pauschale Ablösung.
2. Originalverbindungs-/BTU-/SMP-Ereignisordnung am gepinnten6.1-Quellpfad
   nachweisen oder neuen geprüften Anschluss entwerfen. Reconnect mit gleichem
   Handle/Adresse, späte Callback-/Read-/Write-/Antwortwerte, Entzug zwischen
   Prüfung und Ausgabe, schwache Sicherheit und Posting-/Registrierungsfehler
   gezielt prüfen. Öffentliche auth_success-/Bondlistenwerte allein reichen nicht.
3. Gegenseitigen TLS-Kanal am neuen Backend portieren und den archivierten
   echten Gegenstellenversuch mit negativen Record-/Pin-/Zertifikatsfällen
   wiederholen. PSA-P256-Erfolg ist kein TLS- oder GM-Produktnachweis.
4. Native RMT-/I²C-/UART-/NFC-/ESP-NOW-Anbindung und reale Profileigentümer
   vervollständigen; Busfehler, Fristen und Generationen bis Verbraucher prüfen.
5. Vor Daten-/Geräteumstellung NVS-/LittleFS-/Bond-/Schlüssel-/Profilformate
   vergleichen. Unveränderte CSV erhält Adressen, beweist aber keine Formatparität.
   Sie hat nur einen Appslot: OTA-Rollback ist damit nicht verfügbar.
6. Geräteupload separat beauftragen und tatsächlichen Chip samt Rückweg neu
   zuordnen. Danach eindeutig zugeordnetes App-/Firmwarepaar, Wiederanlauf,
   BLE-/ESP-NOW-Koexistenz und gemeinsame Stack-/Heap-/Timing-/Dauerlast prüfen.

Die erhaltene BLE-/Rollen-/Profilproduktintegration und ihre bereits alten
Restaufgaben stehen weiter in der [BLE-Übergabe](../../handoff/BLE-ROLLEN-PROFILE-2026-10-28.md).
Kein grünes Hostmodell oder Minimalbuild schließt diese Aufgaben automatisch.


### Erprobte lokale Aufbaugrenzen

Das erste SDK-Checkout scheiterte an langen Windows-Pfaden; acht Herstellerdateien
waren zunächst nicht ausgecheckt. Nur der private frische SDK-Klon wurde mit
`git -c core.longpaths=true` vervollständigt. Die Python-Werkzeuginstallation
brauchte kurze vorhandene8.3-Pfadnamen. Nach einer zuvor abgebrochenen Extraktion
konnte GCC bereits seine Version ausgeben, obwohl der interne Assembler fehlte;
das vollständig aus dem geprüften Archiv entpackte Werkzeug baute erfolgreich.
Kein Registrywechsel oder globaler Git-Konfigurationswechsel vorgenommen.
Der alte Host-MinGW5.1 scheiterte an fehlendem intsafe.h; die auf Hersteller-
SHA256 geprüfte portable w64devkit2.9.1-Toolchain baute dieselben Quellen.
Archiv-SHA256: `9208c19755cd4964b7915b9afcf02c66d493a4c870c4b3e83f6c538d9c1237a5`.
Dieser Wechsel deckte zudem einen spät innerhalb eines Testnamensraums geladenen
Standardheader auf; `<functional>` ist nun vor den Testnamensräumen eingebunden.
Keine SDK-Sicherheitsguards oder Compilerwarnungen wurden dafür abgeschaltet.


### Gefundene und korrigierte Linux-Grenze

Der erste [native PR-Lauf an96eb63c](https://github.com/Sven401/Larp-Escape/actions/runs/40764291187)
lehnte den Build des Profilwerkzeugs wegen des bisher unbedingten Windows-
Headers io.h ab; der ESP32-Schritt wurde deshalb übersprungen. Das war eine
Portierungslücke im Hostwerkzeug, kein fehlgeschlagener Gerätetest. Die oben
belegte POSIX-Korrektur samt echter Journalprobe behebt diesen konkreten Fall;
der erfolgreiche Folgelauf schließt den Linux-Build-/Testnachweis dieses Schnitts.
