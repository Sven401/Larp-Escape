# Maintainer-Einstieg

**Setup und GitHub-Abgleich · 20.10.2026:** [Projekt, Werkzeuge, Prüfwege und Werkbank-Automatik](PROJEKT-SETUP.md). Die anschließende [beauftragte Umstellung mit Verlaufsaudit und Lese-Probelauf](GITHUB-AUTOMATISIERUNG-2026-10-20.md) löst den alten KI-Werkbank-Bezug ab. Bestehender Heartbeat wieder aktiv; 23 Issues und 16 Schutzfälle geprüft, keine Issue-/Boardänderung im Probelauf. Fachlicher Konflikt #34 bleibt offen.

**Stationspilot #79 integriert · 19.10.2026:** Merge `7e19c95` in den kuratierten Integrationsbranch; Dateibaum identisch zum geprüften Head `98488ea`. [Mergebeleg, getrennte CI und offene Geräteabnahmen](STATIONSPILOT-INTEGRATION-2026-10-19.md). Der lesende native Pilot und die Apptrennung sind integriert; TalkBack, Mehrstationsbetrieb, echter Funkverlust und Langzeitbetrieb bleiben offen. Keine neue Geräteprüfung durch den Merge.

**Wartung #87 integriert · 20.10.2026:** Aktuelle Integrationsbasis `a4c83cb`; der Dokumentationsmerge erhält die Produktquellen von `7e19c95`. [Frische lokale Tests/Builds und korrigierte Prüfanleitungen](WARTUNG-PRUEFSTAND-2026-10-20.md) ergänzen die historischen Stationsbelege. Neun PR-Jobs am Wartungshead `44e7098` erfolgreich; eigene Merge-CI und die CI dieser Maintainer-Fortschreibung bleiben getrennte Nachweise.

**Nativer Stationspilot · 19. Oktober 2026:** [Lokale Vorbereitung, Stationsauswahl, automatische BLE-Anmeldung und bestätigter Zustand](../docs/companion/spielleiter-stationspilot.md) in Larp GM implementiert und an der lokalen Box/Moto G14 geprüft. Bluetooth-Wiederkehr, Hintergrund/Rückkehr, bewusste Pause, Prozessneustart, große Schrift und Rotation geprüft. Dev-Quellen und alle zehn erfassten Dev-Dateien unverändert; Licht-/Spielaktionen sind Folgearbeit.

**Android-Apptrennung · 18. Oktober 2026:** [Zwei Anwendungsprojekte und Erhalt der Dev-Daten](../docs/companion/android-app-trennung.md): Dev behält Paketkennung/Speicher; Spielleitung erhält eine eigene Kennung und einen leeren Veranstaltungseinstieg. [Beide Apps am Moto G14 installiert, Dev ohne Deinstallation aktualisiert und Daten erhalten](../docs/companion/android-app-geraetepruefung-2026-10-18.md). App-Auswahl: Larp Dev mit DEV-Badge und Larp GM mit übernommenem GM-Companion-Icon. Neue Spielsteuerung bleibt Folgearbeit.

**UI/UX-Entscheidung · 18. Oktober 2026:** [UX-01 und Projektreferenz](../docs/ui-ux/PROJEKTREFERENZ.md): Dev-App für Entwicklung/Tests/Experimente und Spielleiter-App für Endnutzer werden als zwei separate Apps ausgeliefert. Hauptabläufe für beide Apps sind ausgearbeitet; technische Trennung ist im oben verlinkten Folgeschritt umgesetzt; Installation/Datenerhalt sind oben geprüft; der lesende native Stationspilot ist über #79 umgesetzt; Licht-/Spielaktionen bleiben offen. Der über PR #78 integrierte Stationspilot bleibt die Grundlage der Spielleitungsbedienung.

**Merge-Dokumentation #84 integriert · Stand 19.10.2026:**
[PR #84](https://github.com/Sven401/Larp-Escape/pull/84) nach 9/9 grünen PR-Jobs
bei `8af3ffaac7208ba3944b927bb472195e18b63510` übernommen; Dateibaum identisch
zum geprüften Head `0a8bb41`.
[Merge-/CI-Belege und unverändert offene Folgearbeit](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).

**Näheglut-CI-Status integriert · 18.10.2026:**
[PR #83](https://github.com/Sven401/Larp-Escape/pull/83) nach 9/9 grünen PR-CI-Jobs
bei `9d8bf44aff9da1d4c47f109c451cb251a9d626cd` übernommen. Vollständiger
Dateibaum identisch zum geprüften Head `0fb6ab0`.
[Merge-/CI-Belege und unverändert offene Folgearbeit](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).

**Näheglut-Stopdiagnose und Spielanschluss integriert · 18.10.2026:**
[PR #81](https://github.com/Sven401/Larp-Escape/pull/81) nach 9/9 erfolgreichen
PR-CI-Jobs bei `c4870215a155b9abff2e36fccfe93d42e1a03043` übernommen.
**Zusätzlicher CI-Abschluss geprüft:** #82-PR-CI 9/9 und Quality am tatsächlichen
#82-Merge `8648f57` 3/3 erfolgreich; LED am #81-Merge 4/4 erfolgreich.
Der durch den Folgemerge abgebrochene alte Quality-Lauf bleibt als solcher dokumentiert.
[Befund, Spezifikation und nächste Schritte](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).
650 Originalbilder erneut geprüft; digitaler Stop bestätigt, flächiger Anstieg
auch außerhalb der CAL-Masken. Physische Ursache/optische Freigabe weiter offen.
Konkreter Produktadaptervorschlag liegt vor; keine Produktimplementierung oder
neue Geräteprüfung. Nutzerbestätigter Start: korrespondierende Station erkannt;
Näheglut läuft durch wie Funkeln. Quellenzuordnung/Pausenregel bleiben offen.

**UI/UX-Pilot integriert · 18. Oktober 2026:** [PR #78](https://github.com/Sven401/Larp-Escape/pull/78)
ist bei `42853bc1aa93de23b5fcfc72e66d2c1a397361af` in den kuratierten
Integrationsbranch übernommen. [Rolle](../docs/ui-ux/ROLLE.md),
[aktueller Zustandsvertrag](../docs/ui-ux/BOX-PILOT.md),
[Prototyp/Figma](../design/box-pilot/README.md) und
[PO-/Figma-Nachprüfung](../design/box-pilot/figma-sync-review.md) sind integriert.
Drei P2-Punkte behoben; 56 editierbare Figma-Screens, 148 geprüfte Verknüpfungen.
**Design-/Prototyppaket, keine native Produktumsetzung oder Geräteabnahme.**
[Mergebeleg, Prüfnachweise und konkrete Fortsetzung](UI-UX-INTEGRATION-2026-10-18.md).

**Näheglut mit realem ESP-NOW geprüft · 18. Oktober 2026:** Drei Laborläufe
belegen Funkanbindung, Veraltung/Wiederkehr und gezielten Stop ohne Wiederanlauf.
Optische Dunkelprüfung und zwei Übergangsabweichungen bleiben offen;
[Ergebnis und Diagramm](../docs/espnow/naeheglut-result-2026-10-18.md).
Originalinstallation vollständig wiederhergestellt und verifiziert.
[Arbeitsstand, Belege und Grenzen](NAEHEGLUT-ESPNOW-2026-10-18.md);
keine Produkt- oder Gesamtspielabnahme.
[PR #76](https://github.com/Sven401/Larp-Escape/pull/76) ist nach grüner CI
bei `1646bc37d43f29f0fb48b3cf13adf597dfdc2cf4` im kuratierten Integrationsbranch gemergt.
[Integrationsbeleg und Fortsetzung](NAEHEGLUT-ESPNOW-INTEGRATION-2026-10-18.md).

**LED-Effektentwicklung integriert · 18. Oktober 2026:**
[PR #74](https://github.com/Sven401/Larp-Escape/pull/74) ist bei
`d4dd7c67cd6ba0e1903d8bf6c5df256c49fe5c3e` in
`plan/projektuebergabe-kuratierter-stand` gemergt.
[Mergebeleg, Prüfstand und offene Folgearbeit](LED-EFFEKT-ARCHITEKT-INTEGRATION-2026-10-18.md).
Die [Architektenrolle 2.0](../docs/led/rollen/led-effekt-architekt.md) wurde mit
gemeinsamem Effektkern, seriellen Liveparametern und parallelem Kamerafeedback
praktisch erprobt. [Näheglut-Versuchsbericht](../docs/led/naeheglut-live-results-2026-10-18.md):
Farb-/Kurveniteration, zwei vollständige Folgeläufe und verifizierter Flash-Rückweg.
[Ausführbare Anleitung](../docs/led/interactive-development.md).
Produktadapter, produktseitiger Nähe-/Spielwertvertrag und optisches Nutzerurteil bleiben offen;
die [Rechnerprobe 1.0](LED-EFFEKT-ARCHITEKT-ERPROBUNG-2026-10-18.md) bleibt
historischer Beleg.

**D0 integriert · 18. Oktober 2026:** [Vertrag zu #59](../docs/veranstaltung/d0-vertrag-v1.md)
mit Ereignisidentitäten, Offline-/Replayregeln, Ursprungsvertrauen und Migration.
[PR #72](https://github.com/Sven401/Larp-Escape/pull/72) übernimmt den fachlich
nachgeprüften Inhalt `a9481e0` auf Basis `5f90d10` samt dieser Statusfortschreibung.
Quelle: ausdrücklicher Merge-/Projektstandsauftrag vom 18.10.2026;
[Review, 111 Modellfälle und offene Implementierungstore](D0-REVIEW-2026-10-18.md).
Das D0-Spezifikationspaket ist abgeschlossen; D0-G1–G5 sowie Implementierungs-
und Gerätenachweise bleiben offen. Tatsächlicher Mergecommit und neue CI: PR #72/#25.

**Integration abgeschlossen · 18. Oktober 2026:** [geprüfte PR-Stände,
Konfliktentscheidungen und Prüftore](INTEGRATION-2026-10-18.md).
[PR #69](https://github.com/Sven401/Larp-Escape/pull/69) hat #58/#64/#65/#66/#68
bei `0b0843d5fce475d14705c42c21f4f8bf4314df30` in den kuratierten Branch
integriert. Alle fünf Original-PRs sind als gemergt geschlossen; ihre Heads erhalten.
Der abgeschlossene [RSSI-Gehversuch](../docs/espnow/distance-camera-result-2026-10-18.md)
und die [isolierte Kameraabnahme](../docs/led/camera-results-2026-10-18.md)
ergänzen die bisherigen Teilbelege. Gesamtspiel, Reichweite und Dauerlast bleiben offen.

**Neue verbindliche Produktentscheidung · 18. Oktober 2026:** [VA-01](../docs/veranstaltungsarchitektur.md) legt autonome Boxen, BLE-Bedienung je Box, unterbrechbare ESP-NOW-/WLAN-Kommunikation und die Zentrale ohne Betriebszwang fest. [Vertrags-/Backlogmatrix](../docs/veranstaltungsarchitektur-umsetzung.md), neue Pakete #59–#63. Zugangskonzept PR #58 bleibt ergänzend maßgeblich. **Architektur-Soll; keine Implementierung oder neue Geräteabnahme.**

Stand: **20. Oktober 2026**. Mandat: [AGENTS.md](../AGENTS.md).
Quelle: vorbereiteter #79-Abschluss vom 19.10.2026 und beauftragte Nachprüfung
gegen den um #87 fortgeschriebenen Zielstand am 20.10.2026. Frühere Integrationen bleiben erhalten.
Vorheriger Integrationsabschluss: beauftragter Merge von #69 und Planabgleich;
[Integrationsabschluss](INTEGRATION-2026-10-18.md). Frühere Hygiene:
[Prüfbericht und Bestandsaufnahme](REPOSITORY-HYGIENE-2026-10-18.md).

Aktuelle fachliche Ergänzung vom **18. Oktober 2026**:
[abgestimmtes veranstaltungsbezogenes Zugangskonzept](../docs/companion/veranstaltungszugang.md)
mit Rechteverwaltung, Schlüsselwechsel, Wiederherstellung, Unterbrechungspunkten
und offenen Abnahmekriterien. **Sollkonzept, keine Implementierung.**

## Aktuelle Arbeitsbasis

- **Dokumentationsbasis:** PR #87 ist bei `a4c83cb7351759808f85ede612abc7c2280212fc`
  integriert; Dateibaum identisch zum geprüften Wartungshead `44e7098`.
  [Wartungsprüfstand](WARTUNG-PRUEFSTAND-2026-10-20.md). Diese Maintainer-Fortschreibung
  führt den vorbereiteten Inhalt `499ec94` auf dieser Basis weiter; ihr PR ist noch keine Integration.
- **Belegt:** PR #79 ist bei `7e19c955aaa54bcc7ed318d594a6f69476de4aa9`
  integriert. Frischer Fetch bestätigt Zielbasis `494f1c4` und Head `98488ea`
  als Eltern sowie identischen vollständigen Dateibaum zum geprüften Head.
  [Integrationsbeleg und eigene Merge-CI](STATIONSPILOT-INTEGRATION-2026-10-19.md).
- LED, Wake, Matrix, Audio und NFC sind auf dem
  [gemeinsamen V2-Vertrag](../docs/led/ble-wire-v2.md) zusammengeführt.
  [Herkunft und Erhaltung aller acht Ergebnis-IDs](KONSOLIDIERUNG-2026-10-17.md)
  sowie [historische Gerätebelege](BAUGRUPPEN-NACHWEISE.md) bleiben maßgeblich.
- Die früheren Einzel-PRs sind Herkunft, keine erneut einzuspielende Mergekette.
  Die [Hygieneübersicht](REPOSITORY-HYGIENE-2026-10-18.md) dokumentiert ihre
  Schließung und erhaltenen Branches/Sicherungen.
- `main` bleibt der historische Default-Branch bei `29da937c`; eine Integration
  dorthin benötigt einen gesonderten Auftrag. Neue Arbeit auf eigenem Branch
  gegen den kuratierten Integrationsbranch vorbereiten.
- Fremde Änderungen und private lokale Artefakte erhalten. Die frühere separate
  KI-Werkbank bleibt historische Quelle; Projektwissen wird hier gepflegt.

## Nächste Arbeit und Abnahmegrenzen

[Issue #25](https://github.com/Sven401/Larp-Escape/issues/25) führt die operative
Planung. [#53](https://github.com/Sven401/Larp-Escape/issues/53) dokumentiert
abgeschlossene Softwarekonsolidierung und Integration durch #56.

- **Nativer Stationspilot integriert:** Lokale Vorbereitung, automatische
  Anmeldung und bestätigter Zustand sind implementiert und begrenzt am Gerät
  belegt. [Gerätefolgeprüfungen und Fortsetzung](STATIONSPILOT-INTEGRATION-2026-10-19.md):
  TalkBack, mehrere reale Stationen, Funkverlust bei Bluetooth an und Dauerlauf.
  Licht-/Spielaktionen und allgemeine Spielleitungsrechte bleiben Folgearbeit.

- **D0 #59 abgeschlossen als Spezifikationspaket:** Nach Übernahme von #72
  D0-G1–G5 in #28/D1/D2/#30/#31/D3/D4 erfüllen. D1/D2 können nun gegen den
  integrierten Vertrag konkretisiert werden; die Zugangsspur #27 → #28 → #37
  bleibt eigenständig. Kein Beginn dieser Folgearbeiten durch den Mergeauftrag.
  D1–D4 bleiben offen; App-Messjournal und Funkprobe schließen
  weder dauerhaften Box-Spielfortschritt noch Veranstaltungsbetrieb ab.

- **LED-Architektenrolle 2.0 erprobt und integriert:** Für weitere Effektarbeit
  [Rolle](../docs/led/rollen/led-effekt-architekt.md), [Live-Anleitung](../docs/led/interactive-development.md)
  und [Näheglut-Bericht](../docs/led/naeheglut-live-results-2026-10-18.md) wiederverwenden.
  Konkreter Spieleinsatz/Nähequelle, Produktadapter mit Epochen-/Profilgrenzen,
  produktseitige Mischung sowie Nutzerurteil am realen Licht bleiben offen.
  Der Laborprototyp schließt #29/#31/#37 oder die optische Abnahme nicht ab.
- **D1-A separat in Review:** [PR #73](https://github.com/Sven401/Larp-Escape/pull/73)
  ist weiterhin offen; vor einer eigenen Integration gegen die dann aktuelle
  Basis prüfen. D1-B/C und #60 bleiben offene Folgearbeit.

- **Gemeinsame Geräteabnahme offen:** passendes V2-App-/Firmwarepaar gezielt
  prüfen. Historischer RSSI-Beleg: App-Journalstand `6e6f093` und
  Firmware `e012314`; der neuere GM-Pilot hat einen eigenen
  [begrenzten Gerätenachweis](../docs/companion/spielleiter-stationspilot.md).
  Historische [Installation und Gehversuch](ESPNOW-UEBERGABE-2026-10-18.md).
  Vor erneutem Gerätezugriff tatsächliches Paar feststellen. Ein Upload
  braucht eigenen Auftrag und chipgebundenen Flash-Rückweg.
- **Hardwareplanung fortgeschrieben, 18.10.2026:** [Übernahme aller acht
  Ergebnis-IDs](../Hardware/HARDWAREPLANUNG.md) mit bestätigten As-built-Pins,
  zusätzlicher Audioversorgung und gezielten Restnachweisen. Teilbefunde sind
  erhalten; elektrische/CAD-Freigabe und gemeinsamer Gerätelauf bleiben offen.
- **Matrix #54 / Hardware #39:** C3, G3, H3 und A8 im erhaltenen Sammellauf
  nachprüfen; 68 Koordinaten/110 Beobachtungen, neun unbestückte Felder und
  vier Problemstellen erhalten. Chip/B7 und räumliche Zuordnung bleiben offen.
- **LED #29/#37, NFC #35, Status/Sitzung #33/#34:** berührte V2-Bedienung und
  Fehlerfälle prüfen. Historische Teilabnahmen bleiben gültig für ihren Umfang.
  Audio-R3 (kurzes natürliches Titelende) bleibt ausdrücklich ausgenommen.
- **Allgemeiner Spielzugang #27/#28:** Bond, Rollen und Einrichtung bleiben
  eigenständige Aufgaben; der Baugruppen-HMAC-Zugang ersetzt sie nicht.
- **Spielintegration #31/#30:** zuerst F5a-Geräteausführung, dann reale
  Profilübernahme, anschließend F5b-Ressourcen-/Betriebsabnahme.
- **Notwendige zukünftige Framework-Aktualisierung:**
  [Arduino-ESP32 3.x](ARDUINO-ESP32-3-MIGRATION.md) am 18. Oktober 2026 vom
  Nutzer zur Einplanung beauftragt. Umsetzung, Zielversion und Termin offen;
  jetzt ausschließlich dokumentieren, keine Migrationsarbeiten beginnen.

Softwareintegration, begrenzte historische Gerätebeobachtung und gemeinsame
Produktabnahme sind getrennte Nachweise. Der Baugruppenstand ist kein
abgenommenes Gesamtspiel. Keine neue Geräteprüfung in der Hygienesitzung.

## Aktuelle Prüfwege und CI

**PR #79:** 9/9 PR-Jobs erfolgreich; eigene Push-CI am Merge `7e19c95`
mit 7/7 Jobs ebenfalls erfolgreich. Getrennte Belege im [Integrationsbericht](STATIONSPILOT-INTEGRATION-2026-10-19.md) und #25.

**UI/UX #78, Head `3b2f6b4`: alle drei PR-Workflows erfolgreich.**
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39164422680),
[LED/Android/Kamera](https://github.com/Sven401/Larp-Escape/actions/runs/39164422724),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/39164422681).
Die [eigene Quality-Push-CI am Merge `42853bc`](https://github.com/Sven401/Larp-Escape/actions/runs/39165151009)
war beim Dokumentationsentwurf noch aktiv. Nachfolgende Dokumentations-/Push-Läufe
werden gesondert in #25 ausgewiesen; kein übertragener grüner Status.

Der [vollständige B0-Prüfweg](../docs/quality-checks.md) enthält echte
Kryptografie, konfigurierte Hosttests, vier Legacy-/ABI-Firmwareziele,
Android Debug/Release und Quellen-/Fehlerbelege. Dazu kommen die
[gemeinsame Baugruppen-CI](../.github/workflows/led-product.yml) für Wire,
LED/Wake/Matrix/Audio/NFC/ESP-NOW, vier getrennte Produktvarianten und die
isolierte Kamera-Laborfirmware sowie die [unabhängige NFC-Probe](../.github/workflows/nfc-probe.yml)
und [ESP-NOW-Probe](../.github/workflows/espnow-probe.yml).

**Merge #74, `d4dd7c6`:** Eigene Push-Prüfungen sind
[Quality, drei Jobs](https://github.com/Sven401/Larp-Escape/actions/runs/39145590308)
und [LED/Baugruppen, vier Jobs](https://github.com/Sven401/Larp-Escape/actions/runs/39145590228).
Den abschließend ausgelesenen Status und den anschließenden Dokumentationsstand
führt [Projektplan #25](https://github.com/Sven401/Larp-Escape/issues/25).
[Integrationsbeleg mit getrennten PR-Nachweisen](LED-EFFEKT-ARCHITEKT-INTEGRATION-2026-10-18.md).
Es gab bei diesem Merge-/Dokumentationsauftrag keinen Gerätezugriff oder Upload.

**Historisch am Zielcommit `784984d`: alle acht Push-Jobs erfolgreich.**
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/38980094469),
[Baugruppen](https://github.com/Sven401/Larp-Escape/actions/runs/38980094470),
[NFC-Probe](https://github.com/Sven401/Larp-Escape/actions/runs/38980094423).
Das sind Läufe des tatsächlichen Mergecommits. Alte PR-Läufe bleiben historische
Belege. Nach Änderungen den neuen Head und PR-Prüfmerge gesondert bewerten.

Lokal ohne Hardware: die Befehle in [quality-checks.md](../docs/quality-checks.md)
und [Android-Anleitung](../companion/android/README.md) verwenden. Fehlende
Werkzeuge oder Sandboxfehler sind weder bestandene Tests noch Produktfehler.

## Historischer Prüfstand der Übernahme vom 14. Oktober 2026

- **Historisch belegt:** [Quality-Run 38470091132](https://github.com/Sven401/Larp-Escape/actions/runs/38470091132)
  auf Diagnosearchiv-Commit `8e1fc9ab` war erfolgreich. Der Vergleich vollständiger
  Git-Bäume am 13. Oktober 2026 zeigte gegenüber `42610abf` ausschließlich sechs
  zusätzliche Archivdateien unter `handoff/`; Produktdateien und Workflow waren
  identisch. Das ist kein CI-Lauf auf dem neuen Maintainer-Commit.
- **Damals verwendeter CI-Prüfort:** Die Checks des
  [Maintainer-PRs](https://github.com/Sven401/Larp-Escape/pulls?q=is%3Apr+head%3Acodex%2Fmaintainer-uebernahme)
  liefern das Ergebnis am jeweiligen veröffentlichten Commit. Nach einem neuen
  Commit dessen eigenen Quality-Lauf auswerten; einen früheren grünen Lauf
  nicht auf den neuen Stand übertragen.
- **Gemessen am 14. Oktober 2026:** Android-Aufruf oben mit `--offline --no-daemon`
  auf Basiscommit `42610abfef07c971d9902ea6ca5a350ed68981bf`, JDK 21.0.8 und
  Gradle 8.9 erfolgreich; 58 Aufgaben ausgeführt. 45 JVM-Fälle bestanden
  (29 Protokollkern, 16 App), keine Fehler oder ausgelassenen Tests; Debug-APK
  gebaut. Lint meldet null Fehler und drei bestehende Warnungen:
  `OldTargetApi`, `DataExtractionRules`, `MissingApplicationIcon`.
  Belege sind die erzeugten JUnit-XML-Dateien unter
  `companion/android/{core/protocol,app}/build/test-results/` und
  `companion/android/app/build/reports/lint-results-debug.html`.
  Diese Buildausgaben sind lokal regenerierbar und werden nicht versioniert.
  Der vorhandene Gradlecache genügte für den Offline-Lauf; der anfängliche
  Sandbox-Schreibfehler wurde durch genehmigten Cachezugriff behoben.
  Kein Handy-, Bluetooth-, NFC- oder sonstiger Hardwarelauf wurde durchgeführt.
- **Am 14. Oktober noch offen, durch B0 inzwischen belegt:** Ein erneuter echter Kryptografie-Hostnachweis benötigt
  das gepinnte Mbed-TLS-2.28.4-Archiv und den Quellpfad gemäß
  [native_peer_mbedtls.py](../scripts/native_peer_mbedtls.py).

## Branchschutz und spätere Integration nach main

**Ausgelesen am 18. Oktober 2026:** Das aktive
[Ruleset „Main“](https://github.com/Sven401/Larp-Escape/rules/24680694)
gilt für den Default-Branch. Es sperrt Updates ohne Bypass und verlangt
signierte Commits, lineare Historie sowie einen Review, Code-Owner-Review,
Freigabe des letzten Pushs und aufgelöste Reviewthreads. Erlaubte Methode
im Ruleset: Squash. Die Liste konkret erforderlicher Statuschecks ist leer.
Repositoryweit sind Merge, Squash und Rebase aktiviert; Auto-Merge ist aus.

Der kuratierte Integrationsbranch wird von diesem Ruleset nicht erfasst und
wird von GitHub als ungeschützt gemeldet. CI läuft, wird dort aber durch den
sichtbaren Regelsatz nicht erzwungen. Eine passende Review-/Check-Policy für
diesen Branch ist als Entscheidung vorgemerkt; Schutzregeln wurden nicht geändert.
Die zusätzliche klassische Branch-Protection-Abfrage ist mit der App-Verbindung
nicht zugänglich (403); das ist keine Aussage über deren Abwesenheit.

Vor einem beauftragten späteren Main-Update Regelsätze neu prüfen und die
Update-Sperre fachlich klären. Diese Hygiene ändert weder Main noch Regeln.
