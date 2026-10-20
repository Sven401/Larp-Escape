# Technische Projektübergabe

**Projekt und Setup · 20.10.2026:** [Detaillierter Überblick mit Werkbank-/Automationsprüfung](PROJEKT-SETUP.md), geprüft an Integrationsbasis `a4c83cb`.

**Nativer Stationspilot · 19. Oktober 2026:** [Lokale Vorbereitung, Stationsauswahl, automatische BLE-Anmeldung und bestätigter Zustand](../docs/companion/spielleiter-stationspilot.md) in Larp GM implementiert und an der lokalen Box/Moto G14 geprüft. Bluetooth-Wiederkehr, Hintergrund/Rückkehr, bewusste Pause, Prozessneustart, große Schrift und Rotation geprüft. Dev-Quellen und alle zehn erfassten Dev-Dateien unverändert; Licht-/Spielaktionen sind Folgearbeit.

**Android-Apptrennung · 18. Oktober 2026:** [Zwei Anwendungsprojekte und Erhalt der Dev-Daten](../docs/companion/android-app-trennung.md): Dev behält Paketkennung/Speicher; Spielleitung erhält eine eigene Kennung und einen leeren Veranstaltungseinstieg. [Beide Apps am Moto G14 installiert, Dev ohne Deinstallation aktualisiert und Daten erhalten](../docs/companion/android-app-geraetepruefung-2026-10-18.md). App-Auswahl: Larp Dev mit DEV-Badge und Larp GM mit übernommenem GM-Companion-Icon. Neue Spielsteuerung bleibt Folgearbeit.

**UI/UX · 18. Oktober 2026:** [Projektreferenz und bestätigte separate Appauslieferung](../docs/ui-ux/PROJEKTREFERENZ.md), [Dev-Hauptabläufe](../docs/ui-ux/DEV-APP-ABLAEUFE.md) und [Spielleitungs-Hauptabläufe](../docs/ui-ux/SPIELLEITER-APP-ABLAEUFE.md). Dokumentierter Designstand; Apptrennung samt begrenzter Geräteabnahme siehe Fortschreibung oben.

**PR #84 abgeschlossen · Stand 19.10.2026:** Merge-Dokumentation von #83 nach
9/9 grünen PR-Jobs bei `8af3ffaac7208ba3944b927bb472195e18b63510` integriert.
[Belege und fachliche Restpunkte](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).
Keine neue Produktimplementierung oder Geräteabnahme.

**PR #83 abgeschlossen · 18.10.2026:** CI-Statusfortschreibung nach 9/9 grünen
PR-Jobs bei `9d8bf44aff9da1d4c47f109c451cb251a9d626cd` integriert.
[Mergebeleg, getrennte CI-Nachweise und offene fachliche Punkte](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).
Keine neue Produktimplementierung oder Geräteabnahme.

**Näheglut-Diagnose und Spielanschluss integriert · 18.10.2026:**
[PR #81](https://github.com/Sven401/Larp-Escape/pull/81), Merge
`c4870215a155b9abff2e36fccfe93d42e1a03043`, zuvor 9/9 PR-CI-Jobs erfolgreich.
**Zusätzliche Prüfungen abgeschlossen:** #82-PR-CI 9/9, Quality am #82-Merge
`8648f57` 3/3 und LED am #81-Merge 4/4 erfolgreich. Alter Quality-Abbruch bleibt
historisch erhalten; siehe getrennte Laufbelege in der folgenden Übergabe.
[Stopdiagnose und konkreter Spielanschluss](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).
Digitaler Stop bestätigt; optischer Restpegel auch am Papierrand nachgewiesen,
Ursache weiterhin ohne kontrollierten Gerätevergleich offen. Adapterentwurf
mit Instanz-/Stop-/Frische-/Profilvertrag, noch keine Produktimplementierung.

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

[D0-Vertrag zu #59](../docs/veranstaltung/d0-vertrag-v1.md) über
[PR #72](https://github.com/Sven401/Larp-Escape/pull/72) integriert:
Identitäten, Ereignisse, Ausfallregeln, Vertrauen und Migration, einschließlich
[fachlicher Korrekturen und 111 Modellfälle](D0-REVIEW-2026-10-18.md).
Ausgangsbasis `5f90d10`, fachlicher Inhaltsstand `a9481e0`; tatsächlicher
Merge-/CI-Beleg im [Projektplan #25](https://github.com/Sven401/Larp-Escape/issues/25).
D0 ist als Spezifikationspaket abgeschlossen. D0-G1–G5, D1–D4 und die allgemeine
Zugangsspur bleiben offen; keine neue Produktfunktion oder verteilte Abnahme.

Abgeschlossene Zusammenführung von #58/#64/#65/#66/#68 über PR #69,
Mergecommit `0b0843d5fce475d14705c42c21f4f8bf4314df30`:
[Integrationsprüfung vom 18. Oktober](INTEGRATION-2026-10-18.md).
Der [abgeschlossene RSSI-Gehversuch](../docs/espnow/distance-camera-result-2026-10-18.md)
ergänzt die älteren Gerätebelege; die fünf Original-PRs sind als gemergt geschlossen.

**Verbindlicher Architekturstand · 18. Oktober 2026:** [VA-01: dezentrale Veranstaltung](../docs/veranstaltungsarchitektur.md) und [Vertrags-/Arbeitspaketmatrix](../docs/veranstaltungsarchitektur-umsetzung.md). Kein vorausgesetztes gemeinsames WLAN, keine kontinuierliche Erreichbarkeit. Mehrere Handys bedienen verschiedene autonome Boxen; Zentrale sammelt und vermittelt bei Kontakt. Umsetzung und Abnahme bleiben offen.

## Aktueller Stand · 18. Oktober 2026

- [ESP-NOW-Sitzungsabschluss: RSSI, Funkelsteuerung und vollständiger Kameravergleich](ESPNOW-UEBERGABE-2026-10-18.md); PR #66 ist über #69 integriert. Der Sitzungsabschluss bleibt der historische Beleg vor Integration.

**Belegt:** [PR #78](https://github.com/Sven401/Larp-Escape/pull/78) ist bei
`42853bc1aa93de23b5fcfc72e66d2c1a397361af` integriert; Eltern `7e72411` +
`3b2f6b4`, vollständiger Dateibaum identisch zum geprüften Head. Drei PR-Workflows
erfolgreich; eigene Merge-/Dokumentations-CI getrennt in
[Integrationsbeleg und Fortsetzung](UI-UX-INTEGRATION-2026-10-18.md) und #25.
Frühere Integrationen #56/#57/#67/#69/#70/#72/#74/#76/#77 bleiben erhalten.
LED/Wake/Matrix/Audio/NFC/ESP-NOW verwenden weiterhin den
[gemeinsamen V2-Vertrag](../docs/led/ble-wire-v2.md); der isolierte Webpilot ersetzt
keine Android-App oder Gerätefunktion. Weitere Nachweise im [Maintainer-Einstieg](MAINTAINER.md).

- [Konsolidierung: Herkunft, Konfliktentscheidungen, acht Ergebnis-IDs](KONSOLIDIERUNG-2026-10-17.md).
- [Nachweisregister: historische Gerätebelege und Sicherungsreferenzen](BAUGRUPPEN-NACHWEISE.md).
- [Repository-Hygiene: ersetzte PRs, Worktree-Bestand und Prüfstand](REPOSITORY-HYGIENE-2026-10-18.md).
- [Operative Planung #25](https://github.com/Sven401/Larp-Escape/issues/25)
  und [abgeschlossene Softwarekonsolidierung #53](https://github.com/Sven401/Larp-Escape/issues/53).

**Offen:** native Umsetzung des Stationspiloten, reale Einrichtung, Mehrstations-,
Android-/BLE-/TalkBack-/Lifecycle- und Nutzerabnahme; außerdem gemeinsames V2-Paar
am Gerät, Matrixproblemstellen #54,
LED-/NFC-/Sitzungsfehlerfälle sowie allgemeiner Spielzugang und Profilbetrieb.
Für Näheglut fehlen echter Spieleinsatz/Nähequelle, Produktadapter und optisches
Nutzerurteil. D1-A/PR #73 bleibt gesondert in Review; #60/D1-B/C bleibt offen.
Historische bestandene Teiltests bleiben erhalten. Ein Geräteupload ist ein
separater Auftrag; vorab tatsächliches Paar und chipgebundenen Rückweg prüfen.

Die folgenden Abschnitte bewahren frühere Ablesestände. Aussagen wie
„nicht installiert“, „Integration offen“ oder „aktuell“ gelten dort nur für
das jeweilige Datum; sie ersetzen nicht den heutigen Einstieg oben.

---

# Historische Übergabe und Ergänzungen · 13.–17. Oktober 2026

Historische Fortsetzung: [Baugruppen-Konsolidierung vom 17. Oktober 2026](KONSOLIDIERUNG-2026-10-17.md).

**Abgeschlossener Folgeprüfstand:** [Sammelergebnis](../docs/matrix/collection-result-2026-10-16.md)
mit 68 erkannten Koordinaten, vier offenen Problemstellen und neun
nutzerbestätigt unbestückten, in der App deaktivierten Feldern.
Die folgenden früheren Vorbereitungs-/Startangaben sind historische Belege.


## Historische Fortsetzung: Sammeldurchgang, 16. Oktober 2026

Der [Sammeldurchgang](../docs/matrix/collection-run-2026-10-16.md) ersetzt für die nächste
Bestückungserfassung die Einzelbestätigungen. Umsetzung, Arbeitskopie,
Speicherung und Abschlussauswertung stehen dort. Der folgende frühere
Arbeitsstand ist historisch; den neuen Geräte-/Prüfstand im verlinkten
Sammellaufbericht lesen.


## Tastenmatrix: Softwarekorrektur nach Originalrichtung, 16. Oktober 2026

Der [Gerätebericht](../docs/matrix/device-test-2026-10-16.md) hält die
verifizierte Installation von `c613ace` und die erfolglosen Kontaktversuche
fest. Auf neuen Nutzerauftrag wird zuerst die Prüffirmware aus der
ursprünglichen Scanrichtung und Pinbelegung korrigiert. Der
[Korrekturbericht](../docs/matrix/scan-direction-correction-2026-10-16.md)
führt Quellen, Änderung und Softwareprüfstand; eine neue Installation oder
Geräteabnahme ist nicht erfolgt. Die Box ist laut Nutzer vollständig stromlos,
die Matrixleitungen bleiben angeschlossen. Einige Kontakte sind bewusst
nicht bestückt; ihre genaue Liste ist offen. Arbeitsstand, offener
B7-Herstellerkonflikt und nächster gezielter Gerätetest stehen in der
[Matrix-Sitzungsübergabe](matrix-test-2026-10-16.md).

## Ergänzung: eigenständige LED-Produktfunktion, 15. Oktober 2026

Der neue Produktzweig ergänzt die echte Android-App und einen geschützten
BLE-Zugang mit eigener Effektberechnung und SDK-RMT-Ausgabe. Umfang, Quellen,
USB-Einrichtung und Geräteabnahme stehen in
[firmware/led/README.md](../firmware/led/README.md).
Die [gemeinsame Geräteprüfung](../docs/led/device-test-2026-10-15.md)
bestätigt Funkeln, Stop sowie Lichtimpuls/Ripple mit selbstständigem Aus am
installierten eigenen Paar; weiterführende Abnahmefälle bleiben offen.
Die frühere Einbindung einer externen Beispielimplementierung war eine
Fehlinterpretation und ist zurückgenommen; die Owner-Klarstellung steht in
[PR #44](https://github.com/Sven401/Larp-Escape/pull/44).
Die Spielintegration bleibt getrennt. Die folgenden Abschnitte beschreiben
den historischen Übergabestand vom 13. Oktober, nicht die neue LED-Abnahme.

Historische technische Fortsetzung: [Integrationsvorbereitung vom 16. Oktober 2026](INTEGRATION-2026-10-16.md).
Bestätigter Baugruppenfortschritt bleibt im [Nachweisregister](BAUGRUPPEN-NACHWEISE.md)
mit festen Quellen und Sicherungsreferenzen unabhängig von der Zielintegration erhalten.
Sie erfasst die offenen Produktzweige, Konflikte und Prüftore; die folgende
Übergabe bleibt der historische Quellenstand. Produktplanung: [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25),
Konsolidierung: [#53](https://github.com/Sven401/Larp-Escape/issues/53).

## Einstieg und Stand

Seit dem 14. Oktober 2026 gilt der [Maintainer-Einstieg](MAINTAINER.md) für die
weitere Betreuung: Arbeitsbasis, Mandat, Prüfwege und offene Abhängigkeiten.
Die folgende technische Übergabe bewahrt den Stand vom 13. Oktober 2026.

Dieser Branch `plan/projektuebergabe-kuratierter-stand` beginnt bei
`a40365d715b2055ee23f767fa53b55ac25243437` (Merge von PR 24 auf
`SvensBranch`). Er integriert die Firmware-Grundlagen E1–E4/F0 aus
`e973818edde56be4f4907a024f9c4f347802a3cc`, die Android-Grundlagen
A0–A3 aus `6383c5906b8fb63748cf4b8de6e9182802b83fa4` und das
Hardware-Quellen-/Schnittstellenpaket aus
`2204d5447236601ed339b6068edb2a830f34743b` durch getrennte
Merge-Commits. Die ursprünglichen Branches und Worktrees wurden dabei nicht
umgeschrieben. Die Zusammenführung ist ein Entwicklungsstand, keine
Geräte-, Funk- oder Fertigungsabnahme.

Die frühere Diagnose-App samt Design und paketweiser Fortsetzung ist am
13. Oktober 2026 als gescheitert verworfen worden. Ihr Code bleibt als
historische Quelle im Diagnose-Archiv; sie ist kein Einstieg für eine
Hardwareabnahme. Das fortbestehende Nutzerziel ist eine am Handy auffindbare,
verständliche und bewusst wiederholbare echte Baugruppenbedienung. Erster
Referenzfall: LEDs starten, stoppen und bewusst erneut starten, mit
korrelierter Geräteantwort und getrennter Nutzerbeobachtung. Am ursprünglichen Übergabestand vom 13. Oktober war dieser Ablauf
weder implementiert noch physisch abgenommen. Der aktuelle begrenzte LED-Nachweis
steht in der oben verlinkten Geräteprüfung vom 15. Oktober.

## Wake-Inputs: gemeinsamer Gerätetest

Die [Wake-Testvorbereitung](../docs/wake/test-preparation.md) führt den neuen
Umfang auf Basis des gemeinsam geprüften LED-Paars: vier Live-Kacheln,
2×2-Tauschvorschau, gespeicherte Anordnung sowie getrennte Software- und
Geräteabnahme. Der [Gerätetest vom 15. Oktober](../docs/wake/device-test-2026-10-15.md)
belegt das installierte Paar `ca86b93`, die räumliche Zuordnung **2 / 1 / 4 / 3**,
Vorschau und Tausch, Loslassen außerhalb/in Rasterlücken sowie Speicherung
nach Neustart/Wiederverbindung.
Die funktionale Sensorreaktion ist beobachtet; Chipmarking und reale Leitung
von Wake 1/B7 bleiben physisch offen.

## NFC: Vorerkundung und echter Tag

Aktueller NFC-Gerätenachweis vom 16. Oktober 2026:
[Handy-Vorerkundung](../docs/nfc/device-preparation-2026-10-16.md) mit zwei
HCE-Zyklen und [ein echter NTAG213-Rundlauf](../docs/nfc/physical-tag-test-2026-10-16.md)
App → Tag → Box → Tag → App sind belegt. Der echte Lauf erforderte nach
zunächst unklarem App-Schreibergebnis ein erneutes Lesen; kein zweites
Schreiben. Die begrenzte NFC-Prüffirmware ist temporär, Wake wurde jeweils
verifiziert wiederhergestellt. Andere Tagtypen und Spielintegration bleiben offen.

Die [Korrektur der Erstschreibprüfung](../docs/nfc/first-write-readback-2026-10-16.md)
ist auf App-Commit `c0f76ee` mit einem weiteren leeren NDEF-Tag am moto g14
belegt: Nach der veralteten Android-Leerantwort führt die App eine neue
Erkennung aus und bestätigt UID sowie alle 59 Bytes automatisch, ohne
erneut zu schreiben. 105 JVM-Tests, Build, Lint und die drei CI-Workflows
sind erfolgreich. Die Box blieb unverändert auf Wake; die App ist wieder
authentifiziert verbunden. Der Bericht trennt den erfolgreichen Lauf von
vorherigen Funkfehlern und durch Bildschirmdrehung verworfenen Aufträgen.

## Owner-Klarstellung — 15. Oktober 2026

Die [externe Funktionsreferenz](../docs/led/implementation-reference.md) dient
zum Vergleich mit einer Beispielimplementierung. WLED ist keine vorgesehene
Projektkomponente, darf keine Dependency und keine direkte Referenz in App oder
Firmware sein und darf in PRs/Veröffentlichungen nicht als Bestandteil geführt
werden. Die zuvor von Codex abgeleitete Einbindungsvorgabe ist zurückgenommen.
Die folgenden Architekturangaben werden dadurch nicht neu entschieden.

## Architektur und Start

Die Firmware liegt in `include/`, `src/` und `lib/`. Die Ausführung trennt
Spiel-/Profilmodell, Geräteplanung, Companion-Vertrag und konkrete Adapter;
`src/main.cpp` ist der Produktstart. Verträge und Fixtures stehen unter
`docs/companion/`, Konfigurations- und Laufzeitverträge unter
`docs/configuration/` und `docs/runtime.md`. `platformio.ini` enthält
getrennte Windows-Hosttests, Produktionsbuild und synthetische ESP32-Proben.

Die Android-Entwicklungsbasis liegt unter `companion/android/` (Kotlin,
Compose, `app` und `core:protocol`). Am Übergabestand vom 13. Oktober war ihr Produktpfad eine
lesende BLE-Verbindungsprüfung; der A3-Peer-Auth-Kern ist hostseitig. Für
Voraussetzungen, Berechtigungen und Buildbefehle siehe
`companion/android/README.md`. Ein Debug-Build ist kein freigegebener
Spielleitungsclient. Der Webprototyp und die .NET-Diagnose sind separate
historische Quellen.

Die editierbaren und exportierten Hardwareeingänge stehen unter `Hardware/`.
`QUELLENINVENTAR.md`, `SCHNITTSTELLENABGLEICH-2026-10-10.md` und
`HARDWAREVERTRAG-V0.1-2026-10-11.md` kennzeichnen Zuordnung und Pinvertrag
ausdrücklich als vorläufig. Originalbox, CAD-Revision und Softwareannahmen
sind nicht gleichzusetzen.

Der vollständige [B0-Prüfweg](../docs/quality-checks.md) ergänzt die unten stehenden Einstiegsbefehle um echte Kryptografie, GATTS-ABI, Android-Lint und die zugehörigen CI-Belege.

Zum Start ohne Hardware:

```powershell
pio test -e host-regression
pio test -e host-configured-control
pio run -e az-delivery-devkit-v4 -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug
```

Der Android-Build braucht JDK 17/21, Android SDK 35 und auf einem neuen
Rechner herunterladbare Gradle-Abhängigkeiten. Für weitere Umgebungen und
isolierte ABI-/Kryptografieprüfungen siehe `platformio.ini` und die
jeweiligen README-Dateien. Die tatsächlichen frischen Ergebnisse und
Grenzen stehen in [PRUEFSTAND.md](PRUEFSTAND.md).

## Quellarchiv und Wiederaufnahme

[quelleninventar.json](quelleninventar.json) erfasst die 40 vorgefundenen
Produkt-Worktrees, alle 41 vorherigen lokalen Branchspitzen, offene
Dateien und begründete Ausschlüsse. Von diesem Branch zweigen folgende
Archivbranches ab; ihre Veröffentlichung auf GitHub wurde am 14. Oktober 2026
über die Branchliste geprüft:

| Branch | Inhalt |
| --- | --- |
| `plan/projektuebergabe-firmware-f1-archiv` | F1-Bond-/Wiederverbindungszweige und vier offene Firmware-Worktrees |
| `plan/projektuebergabe-android-entwuerfe-archiv` | frühere Android-Basis und unveröffentlichte Companion-Dateien des Hauptcheckouts |
| `plan/projektuebergabe-hardwarequellen-archiv` | frühere Hardware-/Prüfzweige, zusätzliche CAD-/POC-Quellen und EasyEDA-Brückenpatch |
| `plan/projektuebergabe-diagnose-historie-archiv` | sämtliche gestoppten Diagnosezweige, offene Guided-UI und vorhandener Bericht |
| `plan/projektuebergabe-led-entwurf-archiv` | aktueller, noch unversionierter LED-Steuerungsentwurf |
| `plan/projektuebergabe-experimente-archiv` | übrige Versuche und lokale Konfigurationsänderungen |

Jeder Archiv-Branch behält seine Original-Commitspitzen als Git-Eltern
erreichbar. Die Dateien im Checkout bleiben der kuratierte Stand; offene
Änderungen liegen unter `handoff/quellarchiv/` mit Manifest,
SHA-256-Prüfsummen, ursprünglichem relativem Pfad und Patch. Einen Patch
nur in einer separaten Kopie seines bezeichneten Quell-Commits anwenden.
Archiv-Branches sind Quellenlager, keine lauffähigen Produktvarianten.
Verschachtelte Worktrees werden über ihre eigenen Einträge erfasst;
erzeugbare Caches und Buildprodukte wurden nicht kopiert.

Die Übergabe ist eigenständig. Ein separates früheres KI-Repository wird
für Verständnis, Build und Bewertung nicht benötigt. Der kuratierte Branch
und die sechs oben genannten Archivbranches sind auf GitHub veröffentlicht.
Ihre Existenz im Remote bedeutet keine fachliche Freigabe der archivierten
Entwürfe. In einem nur auf den kuratierten Branch begrenzten Klon müssen
benötigte Archivbranches gezielt zusätzlich geholt werden.

## Offene Entscheidungen und Grenzen des ursprünglichen Übergabestands

- F1-SDK-/SMP-Herkunft, Bond-Wiederverbindung und Rechtebindung sind nicht
  integriert oder am Funk abgenommen; dafür die F1-Quellen einzeln prüfen.
- Eine echte, wiederholbare LED-Bedienung in Android samt Firmwareantwort,
  Stop und erneuter Auslösung fehlt. Der verworfene Diagnoseclient ist
  hierfür kein akzeptierter Ersatz.
- Die Hardware-Unterlagen enthalten keine bestätigte CAD-Netzliste, kein
  fertiges PCB und keine ERC-/DRC- oder Fertigungsfreigabe. GPIO15 für die
  reale LED-Datenleitung ist eine Nutzerbeobachtung ohne vollständiges
  Messprotokoll. Die Zuordnung von Wake/Matrix, der MCP23017-B7-Einsatz,
  Audio, NFC und sichtbare LEDs bleiben physisch offen.
- Die Originalbox trägt einen getrennten Test-Firmware-Stand. Vor jedem
  weiteren Upload gilt der chipgebundene Flash-Rückweg in der vorhandenen
  Hardwaredokumentation; diese Übergabe lädt oder restauriert nichts.
