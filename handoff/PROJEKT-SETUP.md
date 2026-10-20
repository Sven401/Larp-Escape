# Projekt, Setup und Werkbank-Automatik

Stand: **20. Oktober 2026**, Zeitzone Europe/Berlin.
**Status: belegt gegen Quellstand und lokal ausgelesene Konfiguration.**
Geprüfte Produktbasis nach erfolgreichem Fetch:
`7e19c955aaa54bcc7ed318d594a6f69476de4aa9` auf
`origin/plan/projektuebergabe-kuratierter-stand` (Merge von PR #79). Abschließend mit Integrationsstand
`a4c83cb7351759808f85ede612abc7c2280212fc` abgeglichen: PR #87 ergänzt
ausschließlich Wartungsdokumentation, die Produktquellen sind unverändert.
**Integrationsnachprüfung vom 20.10.2026:** Zielbasis jetzt
`5b8596591cf6657c65d99fc20a70e5334264b368` einschließlich Maintainer-PR #86.
Dieser ergänzt ausschließlich Übergabedokumentation. Die beiden
Einstiegsverweise dieses Pakets erhalten seine Stations-/Wartungsfortschreibung;
Produktquellen, Hooks und Automatisierung bleiben unverändert.
Dieser Bereich ordnet Quellen und Arbeitsumgebung ein; technische Einzelverträge
bleiben an ihren verlinkten Stellen maßgeblich.

## 1. Projekt und heutiger Funktionsumfang

Larp-Escape verbindet ESP32-Boxen für ein LARP-/Escape-Spiel mit Android-Bedienung.
Zur Box gehören LEDs, vier Wake-Eingänge, Kristall-/Tastenmatrix, Audio, NFC und
ESP-NOW-Kommunikation. Der bestehende Spielkern und die separat entwickelten
Baugruppen sind noch kein gemeinsam abgenommenes Veranstaltungssystem.

| Bereich | Stand an der geprüften Basis | Maßgebliche Quelle |
| --- | --- | --- |
| Veranstaltungsarchitektur | **bestätigtes Soll:** autonome Boxen, mehrere Handys, BLE je Box, Funk bei Kontakt; Zentrale ohne dauernden Betriebszwang | [VA-01](../docs/veranstaltungsarchitektur.md), [Umsetzungsmatrix](../docs/veranstaltungsarchitektur-umsetzung.md) |
| Dev-App | **belegt:** eigene Android-App „Larp Dev“ für Baugruppenbedienung und Tests, bestehende Kennung und Speicher erhalten | [Apptrennung](../docs/companion/android-app-trennung.md), [Android](../companion/android/README.md) |
| Spielleiter-App | **belegt:** getrennte App „Larp GM“; Station vorbereiten, auswählen, gegenseitig anmelden und bestätigten Zustand lesen | [Nativer Stationspilot](../docs/companion/spielleiter-stationspilot.md) |
| Stationspilot am Gerät | **ausgelesener historischer Beleg vom 19.10.2026:** lokale Box/Moto G14, Wiederkehr und Lifecycle geprüft; Licht-/Spielaktionen bleiben Folgearbeit | [Umfang und Gerätebelege](../docs/companion/spielleiter-stationspilot.md) |
| Baugruppenprotokoll | **belegt:** gemeinsamer BLE-V2-Vertrag für LED, Wake, Matrix, Audio und NFC; keine allgemeine Spielleiterberechtigung daraus ableiten | [Wire-V2](../docs/led/ble-wire-v2.md), [Nachweisregister](BAUGRUPPEN-NACHWEISE.md) |
| Konfigurierbares Spiel | **belegt als Entwicklungsstand:** Profile, Graphausführung, Speicher-/Aktivierungsabläufe und synthetische Runtime-Proben; realer Produktanschluss und Gesamtbetrieb offen | [Profilmodell](../docs/configuration/profile-model.md), [Geräteausführung](../docs/configuration/device-runtime.md) |
| Näheglut | **ausgelesene Laborbelege:** Effektentwicklung, ESP-NOW und Stopdiagnose; Produktadapter und optische Freigabe offen | [Näheglut-Fortsetzung](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md) |
| Dezentrale Fortschrittsdaten | **belegt:** D0-Spezifikation integriert; Implementierungs-/Gerätetore bleiben eigenständig | [D0-Vertrag](../docs/veranstaltung/d0-vertrag-v1.md), [D0-Review](D0-REVIEW-2026-10-18.md) |

Historische Absätze in README und Übergaben können frühere Aussagen wie „native
Stationsbedienung offen“ enthalten. Die aktuellen Einstiege sind durch #86 und
die Android-README durch #87 berichtigt. Für den heute integrierten begrenzten
Lesepiloten gelten der [Stationsintegrationsbericht](STATIONSPILOT-INTEGRATION-2026-10-19.md)
und der datierte Stationsbericht zusammen mit den Quellen in `gamemaster`.
Die weiterhin offene Spielsteuerung ist dadurch nicht freigegeben.

## 2. Kanonische Ablage und Einstieg

| Ort | Zweck und Geltungsbereich |
| --- | --- |
| [AGENTS.md](../AGENTS.md) | Maintainer-Mandat, Branch-/Commitgrenzen, Gerätefreigabe und UI/UX-Verfahren |
| [MAINTAINER.md](MAINTAINER.md) | Einstieg in bestätigte Fortschritte, nächste Arbeit und Nachweisgrenzen |
| [handoff/README.md](README.md) | Technische Übergaben und datierte Fortsetzungen |
| `docs/` | Verbindliche Verträge, Prüfpläne, Geräte- und Laborberichte |
| `Hardware/` | Editierbare Hardwarequellen, Inventar, Abgleich und Hardwareplanung |
| [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25) | Operative Projektplanung; Live-Zustand vor Planung/Änderung neu lesen |
| Persönliche `~/.codex/werkbank` | Projektübergreifendes Wissen, persönliche Stränge und Hook-Werkzeuge |
| Frühere separate KI-Werkbank `Larp-Escape-AI` | Historische Belegquelle; keine aktuelle Steuerungsinstanz des Projekts |

Neue Projektfakten gehören in dieses Repository. Vorhandene Einzelverträge
verlinken statt in einer persönlichen Fachreferenz zu vervielfältigen.
Die persönliche `UEBERGABE.md` war am 20.10.2026 ein leeres Strangverzeichnis;
daraus folgt nicht, dass im Projekt keine Arbeit offen ist.

## 3. Git- und Arbeitsbereichssetup

**Gemessen am 20.10.2026:** Remote ist
[github.com/Sven401/Larp-Escape](https://github.com/Sven401/Larp-Escape).
Integrationsziel bleibt `plan/projektuebergabe-kuratierter-stand`.
`main` ist kein automatisch zu aktualisierendes Ziel.

Der bei Sitzungsbeginn geöffnete Checkout stand auf `codex/led-resonanzkreis`
bei `e7ec0f94de8abdc525e7cf5bdf7e049831dc7782`. Dort lagen eine Änderung an
`.vscode/extensions.json` und unversionierte `.tmp/`-Inhalte. Sie wurden nicht
übernommen. Ein frischer Fetch ergab die oben genannte neuere Integrationsbasis.
Diese Dokumentation entsteht auf `codex/werkbank-projektsetup`. Die
Integrationsnachprüfung erfolgt isoliert auf `codex/pr88-integration-review`;
der ursprüngliche Arbeitsbereich bleibt erhalten.

Für eine Fortsetzung zuerst im tatsächlich verwendeten Arbeitsbereich prüfen:

```powershell
git status --short
git branch --show-current
git rev-parse HEAD
git remote -v
git worktree list
git fetch origin
git rev-parse origin/plan/projektuebergabe-kuratierter-stand
```

Danach nur die zum Auftrag passenden Übergaben lesen und mit dem Code abgleichen.
Eigene Änderungen auf eigenem Branch vorbereiten, nur benannte Dateien/Hunks
stagen und den PR gegen den Integrationsbranch richten. Das Maintainer-Mandat
erlaubt dazu Commits, Push und PR; Merge, Release und grundlegende
Produktentscheidungen benötigen einen eigenen Auftrag. Andere Worktrees sind
mögliche laufende Arbeit, kein automatischer Integrationsauftrag.

## 4. Quellstruktur und technische Grenzen

| Pfad | Inhalt |
| --- | --- |
| `include/`, `src/` | Spielmodell, Konfiguration, Ausführung, Geräteplanung und Companion-Kern |
| `lib/` | Geräteadapter, FreeRTOS-Kanäle, Arduino-/Funk-/NFC-/Audioanbindung |
| `platformio.ini` | Legacy-Produkt, Hosttests, Profilwerkzeug und separate Runtime-/ABI-Proben |
| `firmware/led/` | Eigenständige Baugruppenfirmware und getrennte Kamera-/Funklabore |
| `firmware/nfc-probe/` | Isolierte PN532-/NFC-Probe |
| `firmware/espnow-probe/` | Isolierte Funkprobe und App-Testempfänger |
| `companion/android/app/` | Larp Dev: `de.larpescape.companion` |
| `companion/android/gamemaster/` | Larp GM: `de.larpescape.gamemaster` |
| `companion/android/core/protocol/` | Gemeinsamer plattformunabhängiger Protokollkern |
| `test/`, `scripts/`, `.github/workflows/` | Hosttests, Prüfwerkzeuge und CI |
| `design/box-pilot/`, `docs/ui-ux/` | Prototyp, Gestaltungs-/Zustandsverträge und Designbelege |

Die reine Spiellogik bleibt hardwareunabhängig; Adapter und konkrete Gerätebesitzer
haben eigene Nachweise. Produktfirmware, synthetische Runtime-Probe und Laborbild
sind unterschiedliche Artefakte. Ein erfolgreicher Build ersetzt keinen Anschluss
an das reale Spiel und keinen Gerätetest.

## 5. Entwicklungsumgebung: Vorgaben und lokaler Befund

**Ausgelesene Projektvorgaben** aus den Konfigurationen an der genannten Basis:

| Werkzeug/Abhängigkeit | Festgelegter Stand / Quelle |
| --- | --- |
| Firmware-Prüfweg | Python 3.11 und PlatformIO Core 6.2.0 in [Quality](../.github/workflows/quality.yml) |
| ESP32-Plattform | `espressif32@6.6.0`, Board `az-delivery-devkit-v4`; [Root-Konfiguration](../platformio.ini) |
| Baugruppen-Framework | `platformio/framework-arduinoespressif32@3.20014.231204`; [LED-Konfiguration](../firmware/led/platformio.ini) |
| Hostumgebung | `windows_x86@1.2.0`, C++11, Unity 2.6.1; getrennte Native-Tests zusätzlich in Linux-CI |
| Kryptografienachweis | echtes Mbed TLS 2.28.4 samt Archiv-/Quellaudit; [Prüfweg](../docs/quality-checks.md) |
| Formatierung | clang-format 19.1.7; bestehende explizite Baseline |
| Android | JDK 17 oder 21; SDK 35, Build Tools 35.0.0; beide Apps minSdk 33 / targetSdk 35 |
| Android-Build | Gradle 8.9 mit SHA-256 im [Wrapper](../companion/android/gradle/wrapper/gradle-wrapper.properties) |
| Android-Bibliotheken | AGP 8.7.3, Kotlin 2.0.21, Compose-BOM 2024.12.01; [Versionskatalog](../companion/android/gradle/libs.versions.toml) |
| Kamera-Werkzeuge | eigener Python-Abhängigkeitsstand in [requirements-led-camera.txt](../scripts/requirements-led-camera.txt); Kamera-CI verwendet Python 3.12 |

**Gemessen am lokalen Rechner, 20.10.2026:** Die aufgelöste Standard-Pythonversion
ist 3.12.6. Darin ist `platformio` nicht installiert. Das separat vorhandene
`~/.platformio/penv/Scripts/platformio.exe` meldet dagegen Core 6.2.0.
OpenJDK 21.0.8 und ADB 1.0.41 / Platform-Tools 36.0.0 sind über ihre vorhandenen
Installationspfade ausführbar. SDK-Plattform `android-35` und Build-Tools-Ordner
`35.0.0` existieren. Node meldet v24.19.0.

In der geprüften Shell waren `pio`, `java`, `adb` und `gh` nicht über
`Get-Command` auflösbar. Das ist eine PATH-Eigenschaft dieser Shell und kein
Beweis fehlender Installation. JDK/SDK nach Android-Anleitung über `JAVA_HOME`
und `ANDROID_HOME` setzen; vorhandenes PlatformIO gezielt aufrufen oder die
gepinnten Voraussetzungen in einer eigenen Umgebung bereitstellen.
Die Versionsabfragen belegen keine vollständige Buildfähigkeit oder Cachevollständigkeit.

Ein neuer Worktree übernimmt ignorierte `.local/`, Gradle-/PlatformIO-Buildausgaben,
`local.properties` und private Geräte-/Zugangsdateien nicht automatisch.
Offline-Builds sind nur mit vollständigen Downloads möglich. Die Android-App
benötigt keinen Node-/Vite-Build; Node wird hier unter anderem von den
persönlichen Werkbank-Hooks verwendet.

## 6. Firmwarevarianten und Auswahl

| Projekt / Umgebung | Zweck und Einschränkung |
| --- | --- |
| Root: `az-delivery-devkit-v4` | Legacy-Spielprodukt; nicht mit dem separaten Baugruppenprodukt gleichsetzen |
| Root: `esp32-runtime-probe`, `esp32-profile-runtime-probe`, `esp32-gatts-abi-probe` | Getrennte Laufzeit-/Profil-/ABI-Nachweise; kein gemeinsamer Gerätetest |
| Root: `host-regression`, `host-configured-control`, `host-peer-proof`, `profile-tools` | Hostprüfungen, echte Kryptografie und Profilwerkzeug |
| LED: `larp-led-product` | Standard-Baugruppenbild; startet nicht automatisch NFC, Matrix und WLAN |
| LED: `larp-nfc-test` | Bewusster NFC-/BLE-Testzweig |
| LED: `larp-matrix-test` | Historische Scanrichtung mit ausdrücklichem IO7/B7-Eingangs-Opt-in; Chipzulässigkeit offen |
| LED: `larp-espnow-test` | Bewusster BLE-/ESP-NOW-Testzweig |
| LED: `larp-led-camera-lab` | Isoliertes optisches Labor ohne Produkt-BLE-Besitzer |
| LED: `larp-led-camera-rssi-lab` | Kamera-/Live-Labor plus ESP-NOW-RSSI; keine automatische Produktzuordnung |
| NFC: `nfc-probe` | Eigenständige NFC-Prüffirmware |
| ESP-NOW: `espnow-probe`, `espnow-app-receiver` | Eigenständige Funkprobe beziehungsweise App-Testempfänger |

Quellen: [LED](../firmware/led/platformio.ini),
[NFC](../firmware/nfc-probe/platformio.ini),
[ESP-NOW](../firmware/espnow-probe/platformio.ini).
Arduino-ESP32 3.x ist [geplante Migration](ARDUINO-ESP32-3-MIGRATION.md),
keine bereits umgestellte Grundlage.

## 7. Hardware, Zugänge und Rückweg

Die [Hardwareplanung](../Hardware/HARDWAREPLANUNG.md) ist der Einstieg in
As-built-Pins, Quellenwidersprüche und Restnachweise. Historisch belegt sind
unter anderem 23 LEDs an GPIO15, Audioleitungen ESP-RX25/TX26/BUSY14 und
PN532-Software-SPI SCK16/MISO17/MOSI23/SS4. Alte Zeichnungen enthalten
abweichende Zuordnungen und dürfen nicht ungeprüft als heutige Verdrahtung gelten.

Wake- und Matrix-B7-Fragen, Chipmarkierungen, Versorgung, elektrische/CAD-Freigabe
und gemeinsame Last-/Betriebsabnahme bleiben gesondert offen. Historische
Sensorreaktion bestätigt keine Datenblattkonformität.

Vor einem ausdrücklich beauftragten Upload: konkretes Gerät, aktuellen
App-/Firmwarestand, Chipidentität und den chipgebundenen Flash-Rückweg aus der
in [Hardwareplanung](../Hardware/HARDWAREPLANUNG.md) beziehungsweise
[Issue #39](https://github.com/Sven401/Larp-Escape/issues/39) referenzierten
Hardwareanleitung abgleichen. Diese Übersicht enthält bewusst keinen
universellen Wiederherstellungsbefehl: Backup und Gerät müssen zusammenpassen.

Die GM-Vorbereitung verwendet eine vertrauenswürdige Box-Zugangsdatei im
Android-Dateidialog. Schlüsselmaterial und private Backups bleiben außerhalb
versionierter Projektdokumente. Der Baugruppen-HMAC-Zugang ersetzt weder das
[Veranstaltungszugangskonzept](../docs/companion/veranstaltungszugang.md) noch
allgemeine Rollen-/Bond-/Spielleitungsrechte.

## 8. Prüfwege und Nachweise

Der [vollständige B0-Prüfweg](../docs/quality-checks.md) bleibt für Host,
Kryptografie, Firmware, Header und Format maßgeblich. Die Workflows an der
geprüften Basis ergänzen inzwischen beide Android-Apps und separate Labortests:

**Ausgelesener zusätzlicher Prüfstand vom 20.10.2026:** Der integrierte
[Wartungsbericht](WARTUNG-PRUEFSTAND-2026-10-20.md) dokumentiert frische lokale
Host-/Kryptografie-/Android-/Firmwareprüfungen auf Produktbasis `7e19c95`.
Er nennt auch den vollständigen Python-Testlauf mit Python 3.12 und gepinnten
Kamera-Abhängigkeiten sowie die geringe Flashreserve des Legacy-Produktbuilds.
Dies sind Belege der separaten Wartungssitzung, keine hier wiederholten Tests.

| Workflow | Prüfbereich |
| --- | --- |
| [Quality](../.github/workflows/quality.yml) | Windows-Hostprüfungen, echte Kryptografie, Legacy-/ABI-Builds und Android beider Apps |
| [LED product](../.github/workflows/led-product.yml) | Synthetische Kamerawerkzeuge, Baugruppenprotokoll/-ausgabe, Firmwarevarianten und Android unter Linux |
| [NFC probe](../.github/workflows/nfc-probe.yml) | Native NFC-Tests und eigener Firmwarebuild |
| [ESP-NOW probe](../.github/workflows/espnow-probe.yml) | Funkprobe und App-Testempfänger; durch Pfadfilter nicht bei jeder Dokumentänderung ausgelöst |

Für beide Apps aus `companion/android/`, mit eingerichtetem JDK/SDK:

```powershell
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:assembleRelease :app:lintDebug :app:lintRelease :gamemaster:testDebugUnitTest :gamemaster:assembleDebug :gamemaster:assembleRelease :gamemaster:lintDebug :gamemaster:lintRelease --no-daemon --console=plain --stacktrace --continue
```

Dies ist ein aus dem aktuellen Quality-Workflow übernommener Prüfaufruf, kein
in dieser Dokumentationssitzung ausgeführter Build. Historische grüne Zahlen
werden nicht auf den neuen Commit übertragen. Ein späterer Bericht nennt
Commit, tatsächlichen Lauf, Ergebnisse je Bereich und verbleibende Grenzen.
Build, Hosttest, Geräteupload, beobachtetes Geräteverhalten und Gesamtspielabnahme
bleiben getrennte Nachweise.

## 9. Persönliche Werkbank-Hooks: tatsächlich erfasster Umfang

**Ausgelesen am 20.10.2026:** `~/.codex/hooks.json` verweist auf
`~/.codex/werkbank/werkzeuge/werkbank-automatik/werkbank-hook.js`.
Quell-Fingerabdruck SHA-256:
`b4b064d3dae2b4ba9cbdce839713535aae8e2ead86629c995ee9f862d9011417`.
Dieser lokale Fingerabdruck macht den untersuchten Stand zuordenbar; das Skript
wird nicht als zweite Kopie ins Projekt übernommen.

| Hook | Ausgelesenes Verhalten | Grenze |
| --- | --- | --- |
| `SessionStart` | Bei startup/resume/clear/compact: Sitzungskennung, Git-Zustand der persönlichen Wissens-Repositories, zuletzt journalisierte Bearbeiter und persönliche Strangtitel | Meldung zur persönlichen Werkbank ist kein Git-Status dieses Projekts |
| `PostToolUse` | Erkennt aus Werkzeugname/-eingabe Pfade und Aktionsarten; schreibt Zeit, Sitzungs-ID, Werkzeug, Repo, relativen Pfad und Art ins Journal | Heuristische Erkennung; kein vollständiger Dateisystemaudit und kein Testnachweis |
| `Stop` | Vergleicht offene Änderungen mit dem zuletzt journalisierten Schreiber; erinnert blockierend bei neu hinzugekommenen eigenen offenen Dateien | Erinnerung erzeugt weder Commit noch Projektübergabe; vorhandene Blockierung wird nicht endlos wiederholt |

Die Funktion `repoList()` umfasst die persönliche Werkbank und direkte,
nicht versteckte persönliche Skillverzeichnisse mit `.git`. Dieses
Larp-Escape-Repository ist dort **nicht registriert**. Daraus folgt:
Projektänderungen werden durch diese Automatik weder umfassend journalisiert
noch beim Abschluss als eigene offene Projektdateien erfasst.

Der Sitzungskontext hat die Werkbank-Startmeldung tatsächlich geliefert.
PostToolUse- und Stop-Verhalten wurden hier nur am Code gelesen, nicht durch
einen neuen Hook-Test nachgewiesen. Keine Hookdefinition wurde geändert oder
manuell ausgeführt. Vor einer ersten Ausführung einer neuen Definition gilt
die persönliche Prüfung und Vertrauensfreigabe unter `/hooks`.

## 10. Geplante GitHub-Automatisierung: Bestandsaufnahme

**Ausgelesen am 20.10.2026** aus
`~/.codex/automations/larp-escape-github-projektstand/automation.toml`;
zusätzlich über die Automation-Ansicht im Codex geöffnet:

| Eigenschaft | Konfigurierter Wert |
| --- | --- |
| Name / ID | Larp-Escape GitHub-Projektstand / `larp-escape-github-projektstand` |
| Typ / Zustand | Chatgebundener Heartbeat, `ACTIVE` |
| Termin | Täglich 09:00 Uhr; die TOML-Regel enthält keine eigene Zeitzonenangabe |
| Ziel | [GitHub-Projekt 3](https://github.com/users/Sven401/projects/3), Plan #25 und Meilenstein-Issues #26–#37 |
| Bisherige Quelle | Alte separate KI-Werkbank `Larp-Escape-AI`, dort `START.md`, `dashboard/dashboard.py --json` beziehungsweise `dashboard/status.py` |
| Schreibumfang | Markierte Plan-/Meilensteinabschnitte, Issue-Zustand und bei verfügbarem Zugriff Projektstatus |
| Zurückhaltung laut Auftrag | Bei Konflikt/fehlendem Beleg/Zugriff keine Änderung; bei unverändertem belegtem Stand still; keine lokalen Produkt-/Werkbankdateien, neuen Issues oder Pushes |

**Belegt: Quellenkonflikt.** Die Automatisierung bezeichnet das frühere
KI-Repository als kanonisch. Die aktuelle [Projektregel](../AGENTS.md) erklärt
es dagegen zur historischen Belegquelle und dieses Repository zum Ort neuer
Projektkenntnisse. Die alte feste Meilensteinzuordnung deckt zudem die heutigen
D0–D4-, UI/UX- und Baugruppenarbeiten nicht als eigene Pakete ab.

**Offen:** tatsächlicher letzter Lauf, dessen Änderungen, nächste Ausführung
und effektive Scheduler-Zeitzone wurden nicht geprüft. `ACTIVE` ist eine
Konfiguration, kein Beleg erfolgreicher Synchronisierung. Der Auftrag wurde
weder gestartet, pausiert noch geändert; die jetzige Nutzeranfrage galt der
Bestandsaufnahme und dem Projektbereich.

**Korrektur vorgeschlagen:** Bei einer beauftragten Anpassung diesen bestehenden
Heartbeat auf den aktuellen Integrationsbranch, `AGENTS.md`, Maintainer-Einstieg,
passende Fachbelege und live gelesene Issues umstellen. Alte Dashboardausgaben
nur als datierte historische Belege verwenden. Markergrenzen, Erhalt fremder
Texte, Zurückhaltung bei Unklarheit und stille unveränderte Läufe bewahren.
Neue Paketzuordnungen aus aktuellen Issues ermitteln und prüfen; keine alten
F0–A3-Zustände schematisch auf neue Architekturarbeit übertragen. Vor der ersten
externen Änderung den konkreten Vergleich von Quelle, Issue und Status vorlegen.
Diese Empfehlung ist keine neue installierte Automationsregel.

## 11. Fortsetzung und Prüfstand dieser Übersicht

Bei der nächsten Sitzung erst Branch/HEAD und passende aktuelle Belege prüfen.
Für Produktarbeit den spezifischen offenen Umfang wählen: GM-Licht-/Spielaktionen,
realer Profilbetrieb, dezentrale Persistenz/Kommunikation, Näheglut-Produktadapter
oder Hardware-/Gesamtabnahme. Diese Übersicht startet keine dieser Aufgaben.

UI/UX-Aufträge nutzen [Rollenvertrag](../docs/ui-ux/ROLLE.md),
[Projekt-Skill](../.agents/skills/ui-ux-design/SKILL.md) und bei entsprechender
Delegation den vorhandenen [Projektagenten](../.codex/agents/ui_ux_designer.toml).
Effektarbeit nutzt die [LED-Effekt-Architektenrolle](../docs/led/rollen/led-effekt-architekt.md)
und die [Live-Anleitung](../docs/led/interactive-development.md).

**Prüfweg der Bestandsaufnahme:** frischer Git-Fetch und Commitabgleich;
Konfigurationen und verlinkte Projektquellen gelesen; Hookdefinition und
Hookcode gelesen; vorhandene Automation angesehen; lokale Werkzeugversionen
abgefragt. Dokumentationsprüfung nach Integrationsnachprüfung: 43 relative
Linkziele dieser Übersicht und die drei Einstiegslinks vorhanden. Insgesamt 180
lokale Dateiverweise der vier betroffenen Dokumente sowie der vorhandene
Abschnittsanker geprüft; keine fehlenden Ziele, unausgeglichenen Codeblöcke oder
Konfliktmarker. Elf Abschnitte und der ausdrücklich benannte Änderungsumfang geprüft.
Keine neuen Firmware-/Android-Builds, Geräteprüfungen oder externen
Statussynchronisierungen sind Bestandteil dieses Nachweises.

**Integrationsreview, 20.10.2026:** Die beiden Textkonflikte mit #86 wurden unter
vollständigem Erhalt der Zielbranch-Fortschreibung aufgelöst; hinzu kommen nur
die Setup-Verweise. Quellkonfigurationen, Hardwareplanung, Hookcode einschließlich
SHA-256 und der ausgelesene Heartbeat-Auftrag wurden erneut abgeglichen.
Gegen Zielbasis `5b85965` bleiben ausschließlich diese Übersicht und drei
Einstiegsverweise geändert. Die zuvor grünen PR-Läufe am Head `03f9aeb` gelten
als historische Vorprüfung; der aktualisierte Head benötigt vor dem beauftragten
Merge eine eigene vollständig erfolgreiche PR-CI.
