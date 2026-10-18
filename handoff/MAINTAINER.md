# Maintainer-Einstieg

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

Stand: **18. Oktober 2026**. Mandat: [AGENTS.md](../AGENTS.md).
Quelle dieser Aktualisierung: beauftragter Merge von #72 und Projektstandsabgleich.
Vorheriger Integrationsabschluss: beauftragter Merge von #69 und Planabgleich;
[Integrationsabschluss](INTEGRATION-2026-10-18.md). Frühere Hygiene:
[Prüfbericht und Bestandsaufnahme](REPOSITORY-HYGIENE-2026-10-18.md).

Aktuelle fachliche Ergänzung vom **18. Oktober 2026**:
[abgestimmtes veranstaltungsbezogenes Zugangskonzept](../docs/companion/veranstaltungszugang.md)
mit Rechteverwaltung, Schlüsselwechsel, Wiederherstellung, Unterbrechungspunkten
und offenen Abnahmekriterien. **Sollkonzept, keine Implementierung.**

## Aktuelle Arbeitsbasis

- **Belegt:** PR #72 übernimmt D0 einschließlich fachlicher Korrekturen
  (Inhaltsstand `a9481e0`) in den kuratierten Integrationsbranch. Ausgangsbasis
  nach frischem Fetch am 18.10.2026: `5f90d109e3589d74d73ac26f859dcd76c2902439`,
  einschließlich #56 (Baugruppenkonsolidierung), #57 (Hygiene), #67
  (Hardwareplanung), #69 und #70. Konkreter Merge-/CI-Beleg im Projektplan #25.
  Die frühere #56-Basis `784984d` bleibt Bezug der historischen CI unten.
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

- **D0 #59 abgeschlossen als Spezifikationspaket:** Nach Übernahme von #72
  D0-G1–G5 in #28/D1/D2/#30/#31/D3/D4 erfüllen. D1/D2 können nun gegen den
  integrierten Vertrag konkretisiert werden; die Zugangsspur #27 → #28 → #37
  bleibt eigenständig. Kein Beginn dieser Folgearbeiten durch den Mergeauftrag.
  D1–D4 bleiben offen; App-Messjournal und Funkprobe schließen
  weder dauerhaften Box-Spielfortschritt noch Veranstaltungsbetrieb ab.

- **Gemeinsame Geräteabnahme offen:** passendes V2-App-/Firmwarepaar gezielt
  prüfen. Zuletzt dokumentiert sind App-Journalstand `6e6f093` und
  RSSI-Firmware `e012314`; [Installation und Gehversuch](ESPNOW-UEBERGABE-2026-10-18.md).
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

Der [vollständige B0-Prüfweg](../docs/quality-checks.md) enthält echte
Kryptografie, konfigurierte Hosttests, vier Legacy-/ABI-Firmwareziele,
Android Debug/Release und Quellen-/Fehlerbelege. Dazu kommen die
[gemeinsame Baugruppen-CI](../.github/workflows/led-product.yml) für Wire,
LED/Wake/Matrix/Audio/NFC/ESP-NOW, vier getrennte Produktvarianten und die
isolierte Kamera-Laborfirmware sowie die [unabhängige NFC-Probe](../.github/workflows/nfc-probe.yml)
und [ESP-NOW-Probe](../.github/workflows/espnow-probe.yml).

**Ausgelesen am Zielcommit `784984d`: alle acht Push-Jobs erfolgreich.**
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
