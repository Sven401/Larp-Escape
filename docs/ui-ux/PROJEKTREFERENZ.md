# UI/UX-Projektreferenz: Dev-App und Spielleiter-App

**Verbindliches ergänzendes Architekturziel · 21.10.2026:**
[SP-01](../spielpakete-architektur.md) macht Larp GM zur Oberfläche für
nachladbare Spiel-/Regelpakete einschließlich generischer Bausteinbearbeitung
und Vorschau. Mehrboxbetrieb und Drittspiel-Nachweis gehören zur ersten
Produktversion. UX-01 mit getrennten Dev-/GM-Apps bleibt bestehen.
Dies ist ein neuer Auftrag an den späteren Editorentwurf, keine fertige UI.

Stand: **18. Oktober 2026**. Projektgebundene Referenz, keine Gerätefreigabe.

## Status und Herkunft

- **Bestätigt – UX-01:** zwei technisch separat ausgelieferte Apps. Quelle:
  Nutzerauftrag „technische Auslieferung als separate Apps“ und Auftrag, Trennung,
  Überblick und Hauptabläufe festzuhalten, Sitzung
  `01a150b6-981d-7791-a37d-34fac8dd18a3`.
- **Bestätigt:** die zuletzt für Tests verwendete App ist die Dev-App. Ihr Zweck
  ist Entwickeln, Testen, Ausprobieren und Experimentieren. Die Spielleiter-/Nutzer-App
  stellt den Endnutzerstand dar. Quelle: Nutzerklarstellung derselben Sitzung.
  Das bestimmt die fachliche Rolle, nicht die aktuell installierte Binärdatei.
- **Ausgelesen:** integrierte Code-/Vertragsbasis
  `7e72411bb1c416b5864036c79a1e17d9cf46b5ac`, Branch
  `plan/projektuebergabe-kuratierter-stand`, nach Fetch am 18.10.2026 geprüft.
  Der erste Überblick auf `e7ec0f9` wurde dagegen abgeglichen.
- **Ausgelesen, inzwischen integriert:** [UI/UX-Pilot PR #78](https://github.com/Sven401/Larp-Escape/pull/78)
  am veröffentlichten Head `3b2f6b4ffd561681dc5b14d13d2f0225eb640cd4`.
  Sein [Rollenvertrag](https://github.com/Sven401/Larp-Escape/blob/3b2f6b4ffd561681dc5b14d13d2f0225eb640cd4/docs/ui-ux/ROLLE.md)
  und [Stationsablauf](https://github.com/Sven401/Larp-Escape/blob/3b2f6b4ffd561681dc5b14d13d2f0225eb640cd4/docs/ui-ux/BOX-PILOT.md)
  dokumentieren bestätigte Nutzerkorrekturen. Diese Referenz erhält den dortigen
  Stationsbediengrundsatz. Während der Erstellung wurde #78 bei
  `42853bc1aa93de23b5fcfc72e66d2c1a397361af` in den Zielbranch integriert; der
  eigene Dokumentationsbranch wurde damit und mit dem anschließenden
  Projektstandsabschluss #80 bei `7e5f4f38e88ed76358e31dc66d267da9cb0fbc1f`
  abgeglichen. Android-/Firmwarequellen
  sind gegenüber `7e72411` unverändert. Der Pilot wird nicht erneut implementiert.
- **Abgeleitet:** die Hauptabläufe sind ausgearbeitete Designvorgaben zur weiteren
  Umsetzung. Neue Detailentscheidungen sind als Vorschlag/offen gekennzeichnet.
- **Historischer Umfang dieser Referenz:** Apptrennung, Sicht-/Nutzerprüfung und Geräteabnahme waren offen. Hier zunächst nur
  Quellen-/Dokumentprüfung; im ersten Überblick war kein Android-Gerät erreichbar.
  Keine neue Prüfung des Webprototyps oder der Figma-Datei; fremde Testerfolge
  werden nicht auf diese Referenz oder neue Apps übertragen.

**Technische Fortschreibung:** [Apptrennung im Android-Projekt](../companion/android-app-trennung.md) implementiert zwei Anwendungskennungen/Artefakte. Die Dev-App behält ihren Speicher; die Spielleiter-App beginnt mit einem eigenständigen Leerzustand. Die nachfolgende Bestandsanalyse bleibt auf ihre genannte Basis datiert.

**Native Fortschreibung · 19.10.2026:** [Stationspilot in Larp GM](../companion/spielleiter-stationspilot.md): lokale Vorbereitung, Stationsklick, automatische gegenseitige Boxprüfung und frisch bestätigter Zustand; begrenzte reale Geräteprüfung erfolgreich. Dev-App und alle zehn vorhandenen Datenfiles unverändert. Licht-/Spielaktionen bleiben Folgearbeit.

## 1. Verbindliche Trennung und Auslieferung

| | Dev-App | Spielleiter-/Nutzer-App |
| --- | --- | --- |
| Zweck | Entwickeln, untersuchen, testen, experimentieren | Vorbereitetes Produkt im Veranstaltungseinsatz bedienen |
| Einstieg | Gerät, Fähigkeiten, Baugruppe, Versuch | Veranstaltung und Station |
| Detailtiefe | Parameter, Messwerte, Auftragsstufen, Beobachtung und Diagnose | Nächste Handlung, aktueller Zustand und konkrete Ausnahmehilfe |
| Funktionsreife | Experimentelle Funktionen ausdrücklich zulässig und gekennzeichnet | Für Endnutzer vorgesehener Funktionsstand mit passendem Prüfstand |
| Hauptabläufe | [Dev-App](DEV-APP-ABLAEUFE.md) | [Spielleiter-App](SPIELLEITER-APP-ABLAEUFE.md) |

**UX-01 bedeutet zwei Apps, keine Modusumschaltung in derselben ausgelieferten App.**
Abgeleitete technische Akzeptanzkriterien: unterschiedliche Android-Anwendungskennungen,
separate Installationsartefakte, nebeneinander installierbar, erkennbare Namen/Icons.
Ein Update der Dev-App darf die Spielleiter-App nicht ersetzen oder deren Daten ändern.
Lokale Speicher und Zugänge bleiben getrennt; der Name „Dev“ verleiht keine Boxrechte.

Gemeinsamer Code für Protokoll, Domäne und geeignete UI-Komponenten bleibt möglich;
getrennte Repositories sind nicht beschlossen. Debug/Release ist eine Bauart,
Dev/Spielleiter ein Produktzweck: ein Release-Build der Dev-App wird dadurch nicht
zur Spielleiter-App. Die vorhandene Debug-Laborfreigabe ersetzt die Produkttrennung nicht.

**Umgesetzt im technischen Folgeschritt:** Dev bleibt `de.larpescape.companion` im Modul `:app`; Spielleitung erhält `de.larpescape.gamemaster` in `:gamemaster`. In der App-Auswahl heißen sie „Larp Dev“ und „Larp GM“, innerhalb der Apps „Larp Escape Dev“ und „Larp Escape Spielleitung“. Bestehende Dev-Daten werden nicht migriert. [Details und Updatebedingungen](../companion/android-app-trennung.md). [Installation, Dev-Datenerhalt und paralleler Appbetrieb](../companion/android-app-geraetepruefung-2026-10-18.md) sind am Moto G14 geprüft. Produktionssignierung/Verteilungsweg, spätere Inhaltsimporte und fachliche Gerätefunktionen bleiben offen.

## 2. Produktverständnis und Zuständigkeiten

Rollenkarten/NFC, Kristallmatrix, Wake-Eingänge, Licht und Audio bilden das physische
Spiel. Spielende interagieren mit der Station; die Apps unterstützen Spielleitung
und Entwicklung. Eine eigene Spieler-Handy-App ist daraus nicht beschlossen.
App-Rückmeldung und physische Wirkung müssen zusammenpassen; das Spielerlebnis
bleibt am realen Aufbau zu prüfen.

[VA-01](../veranstaltungsarchitektur.md) ist verbindlich: autonome Boxen, mehrere
Handys an verschiedenen Boxen, zunächst eine aktive steuernde BLE-Sitzung je Box,
unterbrechbare Infrastruktur. Die optionale Zentrale ist eine weitere Oberfläche
mit offener Ausgestaltung, keine Laufzeitvoraussetzung. Veranstaltung, Station,
physische Box, Durchlauf und Zugang sind getrennte Begriffe. Stationsname oder
Funkadresse beweisen keine Identität.

Der [Veranstaltungszugang](../companion/veranstaltungszugang.md) bestimmt Rechte:
Spielleitungen verwalten Zugänge an ihren berechtigten Zielboxen. Boxzuordnung,
Schlüsselwechsel und Notfallwiederherstellung benötigen den boxgebundenen
Einrichtungsschlüssel. Keine zusätzliche Hauptadministratorrolle.

## 3. Ausgelesener Bestand und Gestaltungshypothesen

| Bereich | Stand an der Codebasis | Designfolge |
| --- | --- | --- |
| Android | Kotlin/Compose/Material 3, ab API 33; eine Anwendung `de.larpescape.companion` | Dev-Ausgangsbasis; separate Apps noch nicht umgesetzt |
| Navigation | ESP, NFC-Tags, Szenario; ESP-Seite bündelt Verbindung, Matrix, Wake, Funk, Audio, LEDs | Aufgabenorientierte Dev-Struktur ausarbeiten |
| Sitzung | Rotation erhält ViewModel; Hintergrund beendet Sitzung; Szenario-Wechsel ruft `foregroundEnded()` auf | Folgen von Navigation/Appwechsel sichtbar behandeln |
| Baugruppen | V2-Antwort-/Frischeregeln; Auswahl löst nicht automatisch aus | Fachlich wertvolle Trennung erhalten |
| LEDs | Integrationsbasis: Funkeln, Lichtimpuls, Ripple | Resonanzkreis im separaten PR #71 nicht als integriert behaupten |
| NFC | Echter Tag, Boxprüfung und aktivierter Handy-Testtag; vorläufiges Testformat | Entwicklerprüfung von späteren Spielobjekten trennen |
| Szenario | Lokaler Textentwurf, Leseprobe, Speicherung, JSON-Import/-Export | Kein ausführbares Profil oder veröffentlichtes Veranstaltungspaket |
| Gestaltung | Helles Material-Schema; Matrix/Wake mit Semantik und Zusatzsymbolen | Kein vollständiges Designsystem oder native Accessibility-Abnahme belegt |
| LED-Labor | Serielles Live-Werkzeug und Kamera außerhalb der Android-App | Kein bereits eingebauter Parameter-/Kameraeditor |

Belege: [Android-Anleitung](../../companion/android/README.md),
[Build](../../companion/android/app/build.gradle.kts),
[App-Einstieg](../../companion/android/app/src/main/java/de/larpescape/companion/MainActivity.kt),
[Baugruppen-UI](../../companion/android/app/src/main/java/de/larpescape/companion/ui/device),
[NFC-UI](../../companion/android/app/src/main/java/de/larpescape/companion/ui/nfc),
[Szenario](../../companion/android/app/src/main/java/de/larpescape/companion/ui/scenario/ScenarioScreen.kt),
[LED-Live-Anleitung](../led/interactive-development.md).

**Abgeleitete Hypothesen:** die lange Baugruppenseite erschwert gezielten Zugriff;
Zugangsimport innerhalb der LED-Karte versteckt eine Einrichtungsvoraussetzung;
Szenario-Wechsel kann eine unerwartete Trennung verursachen. Strukturbeobachtungen,
keine gemessenen Nutzungsprobleme; im nativen Review überprüfen.

**Korrektur des ersten Überblicks:** ein visueller Stationsentwurf mit Figma und
Websimulation ist über PR #78 inzwischen integriert. Die Aussage zum Material-Standardschema
beschreibt ausschließlich die integrierte Android-App, nicht den gesamten
Designbestand. Verworfene Diagnose-App und historischer Webeditor werden damit
nicht als aktuelle Produkte wiederbelebt.

## 4. Gemeinsame Sprache, unterschiedliche Darstellung

| Fachlicher Unterschied | Dev-App | Spielleiter-App |
| --- | --- | --- |
| Verbindung / Recht / Bereitschaft | Einzelne Prüfstufen und Ursachen | Automatisch bis „Bereit“; Hilfe bei Hindernissen |
| Auswahl / Auftrag / Zustand | Parameter, Vorgangsbezug, Antwortstufe | Auswahl, „wird gestartet/gestoppt“, bestätigter Zustand |
| Antwort fehlt | „Ausgang unklar“ mit Prüfweg | „Ergebnis offen“ mit automatischer Zustandsprüfung |
| Veraltete Beobachtung | Quelle, Empfangsalter, Frischegrund | „Aktueller Zustand unbekannt“; letzte Beobachtung als Vergangenheit |
| Physische Wirkung | Geräteantwort und Sicht-/Hörbeobachtung separat erfassen | Keine ständigen Abnahmetexte, dennoch kein unbestätigter Erfolg |
| Simulation | Isolierter Testkontext mit Herkunft | Keine Simulationsregler in der Produktbedienung |

[D0](../veranstaltung/d0-vertrag-v1.md) und [BLE V2](../led/ble-wire-v2.md) bleiben
maßgeblich. Automatisches Verbinden wiederholt keine Aktion. Frischer Zustand
beweist nicht automatisch den Ausgang eines alten Auftrags. Unbekannt bedeutet
weder aus noch Spiel gestoppt. Manuelle Testausgaben und autonomes Spiel haben
verschiedene Abbruchregeln; kein globaler Funk-Not-Aus wird versprochen.

## 5. Verantwortung, Reihenfolge und Prüftore

UI/UX verantwortet Aufgabenstruktur, Navigation, Interaktion, Sprache, visuelle
Hierarchie, Komponenten und Zugänglichkeit beider Apps. Technische Verträge bleiben
maßgeblich; grundlegende Produktentscheidungen beim Nutzer. Diese Referenz ergänzt
den [integrierten Rollenvertrag](ROLLE.md), ohne einen zweiten Agenten, globalen
Skill oder neue Arbeitsregeln einzuführen. Der [Stationspilot](BOX-PILOT.md) bleibt
die konkrete Gestaltungsgrundlage für die Spielleiter-App.

Vorgeschlagene Reihenfolge, keine Terminzusage:

1. Appauslieferung und Datenmigration festlegen; bestehende Dev-Installation erhalten.
2. Dev: Gerät → Baugruppe → vollständiger Versuch mit Ergebnis und Abbruch.
3. Spielleitung: vorhandenen Stationspilot nativ anbinden, einschließlich Hilfe/Wiederaufnahme.
4. Mehrstationenwechsel, konkurrierende Sitzung und Rechteabläufe ausarbeiten.
5. Vorbereitung/Profilaktivierung und späteren Zentralenüberblick anschließen.

Gestaltungsziele: erkennbare Appidentität, Zielkontext, verständliche Hauptaktion,
Status zusätzlich zu Farbe, große Schrift und Screenreader. 48-dp-Bedienziele,
320-dp-Breite und 200 % Schrift sind Prüfziele aus dem Pilot, keine nachgewiesene
Zugänglichkeit. Nacht-/Außeneinsatz, Kontrast, Touch, Fokus, Systemdialoge,
Rotation und Hintergrund nativ prüfen.

Abnahme trennen: Dokument/Zustandsmodell → bedienter visueller Entwurf → Android/
Semantik/Lifecycle → passendes App-/Firmwarepaar → Nutzerurteil. Bestehende
Baugruppenbelege werden nicht auf neue Appstände übertragen. Die
[Vertrags-/Backlogmatrix](../veranstaltungsarchitektur-umsetzung.md) bleibt
kanonisch für technische Abhängigkeiten; keine ihrer offenen Aufgaben wird
mit dieser Dokumentation geschlossen.
