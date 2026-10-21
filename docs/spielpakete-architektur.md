# SP-01: Nachladbare Spiel- und Regelpakete

**Status: bestätigt, verbindliches Architektur-Soll; Umsetzung und Abnahme offen.**
Datum: **21. Oktober 2026**. Quelle: ausdrücklicher Nutzerauftrag in
Codex-Sitzung `01a16028-2a0b-7921-9b3a-16e5ba56d872`:

> Halte das als verbindliches Architekturziel fest und passe den Projektplan an: nachladbare Spiel- und Regelpakete, Mehrboxbetrieb ab der ersten Version und ein drittes Spiel ohne Änderungen an App oder Firmware als Nachweis.

Der Auftrag konkretisiert das Ziel, eigene Spiele vollständig in der App zu
konfigurieren, einschließlich heute noch unbekannter Regeln und Sonderfälle.
Code-/Dokumentbasis: `e61d7642d035ccdcf8eca689d6c50a82bcf1560b` des kuratierten
Integrationsbranches. Prüfweg: lesender Abgleich der unten verlinkten Quellen
und Einstiegspunkte am 21.10.2026; keine neue Laufzeit- oder Geräteprüfung.

SP-01 ergänzt [VA-01](veranstaltungsarchitektur.md),
[UX-01](ui-ux/PROJEKTREFERENZ.md) und den integrierten
[D0-Vertrag](veranstaltung/d0-vertrag-v1.md). Deren lokale Zuständigkeiten,
Offline-/Sicherheitsregeln und getrennte Dev-/Spielleiter-Apps bleiben gültig.
Die [Vertrags- und Arbeitspaketmatrix](veranstaltungsarchitektur-umsetzung.md)
führt die Umsetzung und Abhängigkeiten; dieses Dokument ist die kanonische
Quelle für das Architekturziel und seine Abnahme SP-T01–SP-T07.
[Produktplan #25](https://github.com/Sven401/Larp-Escape/issues/25) und
[Arbeitspaket #93](https://github.com/Sven401/Larp-Escape/issues/93) führen
die Priorität und die neuen Umsetzungsarbeiten.

## 1. Produktziel und erste Version

Die Spielleiter-App soll Spiele erstellen, bearbeiten, prüfen, vorab ausführen,
als wiederverwendbare Pakete speichern und auf geeignete Boxen verteilen können.
Spielabläufe und zusammengesetzte Regelmodule werden als Daten nachgeladen.
Für ein weiteres Spiel innerhalb der vorhandenen Plattformfähigkeiten dürfen
weder ein App-Neubau noch eine geänderte Firmware nötig sein.

**Die erste freigegebene Version muss zusammenarbeitende Boxen unterstützen.**
Mehrere unabhängige Einzelboxspiele oder eine reine Mehrstationsanzeige erfüllen
dies nicht: Ein nachvollziehbares Ergebnis einer Station muss nach einer im
Paket beschriebenen Regel das Spiel einer anderen Station beeinflussen.
Einzelbox-Zwischenstände bleiben zulässige Entwicklungsschritte, schließen
dieses Produktziel aber nicht ab. Die Zentrale bleibt gemäß VA-01 optional für
den vorbereiteten Spielbetrieb.

Die Referenz umfasst mehrere reale Boxen. Eine Anordnung mit drei Boxen ist
ein möglicher Prüfaufbau, keine hier festgelegte zusätzliche Mindestzahl.
Die bestehende D4-/VA-T01-Anforderung mit zwei BLE-Handys an verschiedenen
Boxen bleibt erhalten; pro Box zunächst eine aktive steuernde Sitzung.

## 2. Erweiterbarkeit in drei Schichten

| Schicht | Inhalt und Verantwortung | Änderung für ein neues Spiel |
| --- | --- | --- |
| Plattform | Geräteadapter, unterstützte Ein-/Ausgaben, begrenzte Regelinterpretation, Speicherung, authentifizierte Kommunikation | Stabiler App-/Firmwarestand; neue tatsächliche Hardwarefähigkeiten können ein Plattformupdate benötigen |
| Regelmodule | Wiederverwendbare, parametrierbare Zusammensetzungen aus vorhandenen Ereignissen, Bedingungen, Zuständen, Variablen, Timern und Aktionen; typisierte Ein-/Ausgänge und Bearbeitungsmetadaten | Als Daten ergänzen, kombinieren und versionieren; keine neue native Sonderfunktion je Rätsel |
| Spielpaket | Konkrete Regeln/Module, logische Stationen, Spielobjekte, Ressourcenbezüge und erwartetes Zusammenwirken | In der App erstellen, prüfen, exportieren, importieren und einem Aufbau zuordnen |

Der Regelkern muss allgemeine Mechaniken ausdrücken können, statt an feste
Reichstag-Rollen, Kristallzahlen oder einen Katalog vorgefertigter Spiele
gebunden zu bleiben. Die vorhandene Spiellogik bleibt als kompatible
Ausgangsbasis beziehungsweise wiederverwendbares Modul erhalten.

Neue Reihenfolgen, Kombinationen, Zähler-/Zeitbedingungen, alternative Wege und
stationenübergreifende Abhängigkeiten innerhalb der vorhandenen Ein-/Ausgaben
sind normale Spielregeln. Ihr Fehlen darf beim Nachweis nicht nachträglich als
„neue Hardwarefähigkeit“ umgedeutet werden. Ein neuer physischer Treiber oder
eine tatsächlich fehlende gerätenahe Operation kann dagegen ein Update
erfordern. Die Grenze muss anhand des vorab veröffentlichten Fähigkeitsvertrags
begründet sein; sie darf das Drittspiel-Tor nicht umgehen.

Diese Entscheidung wählt weder Lua, Wasm, eine andere Laufzeit noch ein neues
Paket-/Profilschema. Zuerst sind Ausdrucksbedarf, Ausführungssemantik und
gemeinsames Ressourcenbudget zu prüfen. Bestehende D0-Formate und die
Transportentscheidung aus VA-01 bleiben maßgeblich; SP-01 trifft keine neue
Funkentscheidung und ändert vorhandene Wire-/JSON-Versionen nicht still.

## 3. Allgemeiner Editor und Vorschau

Die App benötigt einen allgemeinen Regeleditor zusätzlich zu verständlichen
Vorlagen. Ein neues zusammengesetztes Modul beschreibt seine Parameter,
Typen, Einheiten, Grenzen, Ein-/Ausgänge und Bearbeitungshinweise als Daten.
Die vorhandenen Editorbausteine müssen es darstellen und verdrahten können,
ohne für jedes Modul einen neuen Bildschirm zu programmieren. Metadaten allein
erfinden keine bisher fehlende Laufzeitoperation.

Vorlagen sind bearbeitbare Ausgangspunkte. Die dritte Mechanik muss vollständig
in der App formulierbar sein; manuelles Bearbeiten von JSON oder Quellcode
außerhalb der App ersetzt diese Fähigkeit nicht. Wiederverwendbare Module
besitzen stabile Kennungen und festgehaltene Versionen. Eine spätere Änderung
einer Vorlage verändert kein bereits verteiltes Spiel unbemerkt.

Die Vorschau muss dieselbe Regelbedeutung wie die Boxausführung verwenden.
Gemeinsam genutzter Kern oder belegte semantische Gleichwertigkeit sind
zulässige Umsetzungen. Eingaben, Zeit, Nachrichtenverlust/-wiederkehr und
beobachtete Zustandswechsel müssen nachvollziehbar sein. Eine Vorschau beweist
keine physische Wirkung; ihr Ergebnis und der Gerätebefund bleiben getrennt.

## 4. Paket, Zuordnung und Spielstand

Ein wiederverwendbares Spiel referenziert logische Funktionen wie „Erfolgslicht“
oder „Tokenleser“. Eine ausdrückliche Zuordnung bindet Stationen und Funktionen
an konkrete Boxen und verfügbare Komponenten. Verdrahtung, GPIOs, MAC-Adressen
und gerätespezifische Audioablagen werden nicht zur fachlichen Spielidentität.
Ressourcen benötigen Inhaltsbindung, Verfügbarkeitsprüfung und eine belegte
Übernahme; ein Trackverweis allein belegt keine installierte Audiodatei.

Getrennt zu führen sind:

- bearbeitbarer Entwurf, unveränderliches veröffentlichtes Paket und tatsächlich
  bestätigter Stand je Box;
- logische Station, physische Box und versionierte Bindung nach D0;
- Spieldefinition, vorbereiteter Durchlauf, veränderlicher lokaler Fortschritt
  sowie dauerhafte Tatsachen und vorübergehende Beobachtungen.

Jedes Paket deklariert Sprach-/Formatversion, Modulabhängigkeiten, benötigte
Fähigkeiten und Ressourcen. Jede Zielbox prüft Versionen, tatsächliche
Verfügbarkeit, Referenzen und Budgets vor Übernahme. Unbekannte oder nicht
erfüllte Anforderungen führen zu einer verständlichen Ablehnung, nicht zu
still gekürztem oder anders interpretiertem Spielverhalten.

Teilbare Spiel-/Regelpakete enthalten keine Zugangsgeheimnisse. Rechte und
geschützte Geräteeinrichtung bleiben getrennt von exportierbarem Spielinhalt.

Installation und Aktivierung benötigen aktuelle lokale Berechtigung und
Inhaltsprüfung; ein Hash allein erteilt keine Rechte. Unvollständige Transfers
bleiben Entwürfe. Empfang, Validierung, eingerichteter Stand, dauerhafte
Bestätigung und laufendes Spiel sind unterscheidbare Ergebnisse. Jede Box
bestätigt ihren eigenen Stand; eine atomare globale Paketaktivierung wird
nicht versprochen. Regeln für Neustart, Reset, neuen Durchlauf, Paketwechsel
und Migration sind vor Ausführung festzulegen; neues Paket bedeutet nicht
automatisch neuen Durchlauf oder erlaubte Übernahme alten Fortschritts.

## 5. Begrenzte Ausführung und Offline-Zusammenarbeit

Die Interpretation muss pro Ziel begrenzte Speicher-, Variablen-, Regel-,
Timer-, Ereignis- und Rechenbudgets ausweisen und durchsetzen. Unbegrenzte
Schleifen, Rekursion, blockierende Regelaktionen und ungeprüfte native
Nachladefunktionen sind kein Erweiterungsweg für Spielpakete. Gerätearbeit
bleibt bei ihren Ressourcenbesitzern; Funkcallbacks kopieren Aufträge.

Gleiche Anfangszustände und dieselbe geordnete Folge geprüfter Eingaben samt
definierter Zeitwerte müssen dieselben fachlichen Entscheidungen erzeugen.
Prioritäten, gleichzeitige Auslöser, Timer, Mehrfachtreffer, Aktionsabschluss
und Budgetüberschreitung benötigen eindeutige Semantik. Daraus folgt keine
globale Reihenfolge auf allen Boxen und keine Exactly-once-Garantie für
physische Effekte.

Jede Box interpretiert ihre Regeln lokal. Handyverlust beendet nicht pauschal
das autonome Spiel; manuelle Baugruppenausgaben behalten ihre eigenen
Abbruchregeln. Für externe Abhängigkeiten gelten D0 und
[dessen Sicherheits-/Migrationsvertrag](veranstaltung/d0-sicherheit-migration-v1.md):
dauerhafte Tatsache, Beobachtung und zeitkritischer Auftrag bleiben getrennt.
Ursprung, Veranstaltung, Durchlauf, Paket und Stationsbindung werden geprüft;
Wiederholungen dürfen bestätigten Fortschritt nicht erneut auslösen.

Jede externe Regel benennt ihr Verhalten bei fehlendem Kontakt: lokal
fortfahren, ausdrücklich warten oder einen vorbereiteten Ersatzweg nutzen.
Unbekannt ist nicht falsch oder bereits erfüllt. Dauerhafte Ergebnisse können
nach geeignetem Kontakt abgeglichen werden; veraltete Sofortaufträge dürfen
nicht beliebig nachgeholt werden. Begrenzte Offlinezeit/-speicherung,
Wiederanlauf und unklare physische Ergebnisse bleiben sichtbar.

## 6. Verbindliche Abnahme: das dritte Spiel

Alle Kriterien sind **offen**. Die ersten zwei Spiele dienen der Entwicklung
der Plattform und müssen sich in ihrer Mechanik unterscheiden. Anschließend
wird der Ausführungsstand eingefroren. Erst danach wird eine tatsächlich
andere dritte Mechanik als neuer Prüfauftrag formuliert und datiert; ein
bereits eingebautes drittes Beispiel erfüllt den Nachweis nicht.

| ID | Gefordertes Ergebnis und Beleg |
| --- | --- |
| **SP-T01 – Eingefrorene Basis** | Nach zwei unterschiedlichen Spielen werden Quellcommits, Sprach-/Fähigkeitsvertrag und SHA-256 der tatsächlich eingesetzten APKs, Firmwareabbilder und sonstigen regelinterpretierenden Laufzeiten für alle beteiligten Zieltypen festgehalten. Eigenständige Simulatoren, Prüfkerne und Paketübersetzer gehören mit Version und Hash dazu. Unterschiedliche Zieltypen dürfen unterschiedliche, jeweils eingefrorene Abbilder haben. Zeitpunkt und Urheber des danach neu formulierten Drittspielauftrags sind belegt. |
| **SP-T02 – Neue Mechanik als Daten** | Das dritte Spiel hat eine fachlich andere Regel-/Fortschrittsstruktur, nicht nur andere Namen, Farben, Werte oder Ressourcen. Es wird mit den eingefrorenen Fähigkeiten vollständig als Spiel-/Regelpaket erstellt. Änderungen sind auf Pakete, zusammengesetzte Module samt Metadaten, Zuordnungen und Ressourcen beschränkt. Keine versteckte vorbereitete Spiellogik, native Nachladung, neue Operation, Hotfix oder verdeckte Änderung an App, Firmware, Interpreter, Simulator oder Paketübersetzer. Binärhashes vor und nach dem Nachweis stimmen überein. |
| **SP-T03 – Vollständiger Appweg** | Die Spielleitung erstellt und bearbeitet die neue Mechanik in der allgemeinen App, verwendet beziehungsweise ergänzt zusammengesetzte Module, prüft sie in der Vorschau, speichert/exportiert/importiert das Paket und bindet es an den Aufbau. Validierungsfehler und nicht erfüllte Fähigkeiten werden verständlich gemeldet. Kein externer Quellcode-/JSON-Edit und kein Entwicklerwerkzeug ersetzt einen notwendigen Autorenschritt. Vorschautraces stimmen für festgehaltene Eingabefolgen mit den fachlichen Geräteentscheidungen überein. |
| **SP-T04 – Übernahme und Rechte** | Das dritte Paket wird über den vorgesehenen Appweg auf die Zielboxen übertragen, geprüft, dauerhaft bestätigt und bewusst gestartet. Unterbrochener Transfer, falsche Berechtigung, inkompatibler Stand und fehlende Ressource werden geprüft; kein bloßes Sende-Acknowledgement wird als Installation gewertet. Je Box sind Paket-/Bindungsstand und tatsächlicher Aktivierungsstatus nachvollziehbar. |
| **SP-T05 – Echte Zusammenarbeit** | Mehrere reale Boxen führen das dritte Spiel gemeinsam aus: Eine lokale Tatsache wirkt nach der paketdefinierten Regel auf einer anderen Box. Zwei Handys bedienen gemäß VA-T01/D4 verschiedene Boxen mit getrennten Rechten und Antworten. Lokaler Spielbetrieb bleibt bei Handy-/Netz-/Zentralenausfall möglich; bei unterbrochenem Boxkontakt gilt der konfigurierte Warte-/Ersatzweg, nach Wiederkontakt der D0-Abgleich. Eine Simulation oder mehrere isolierte Spiele genügen nicht. |
| **SP-T06 – Neustart, Reset und Replay** | Derselbe Drittspielfall umfasst Boxneustart, bewussten Reset/neuen Durchlauf und erneutes Zustellen bereits angenommener Tatsachen, auch nach Unterbrechung. Bestätigte Freigaben werden nicht doppelt wirksam; alte Durchläufe, falsche Paket-/Stationsbindung und unberechtigter Ursprung lösen keine Freigabe aus. Persistenz-, Wiederanlauf- und gegebenenfalls Migrationsverhalten entsprechen dem vorab festgehaltenen Vertrag; unklare physische Wirkungen bleiben als solche ausgewiesen. |
| **SP-T07 – Nachweis und erneutes Tor** | Bericht verknüpft den datierten Auftrag, editierbare Pakete/Module, Ressourcen- und Bindungshashes, eingefrorene Softwareabbilder, Eingaben, erwartete/erhaltene Zustände und Fehlerfälle. Software-/Vorschauprüfungen, reale Gerätebeobachtungen und Nutzerabnahme sind getrennt. Muss die Plattform für den dritten Fall geändert werden, ist dieses Tor nicht bestanden: neue Basis einfrieren und einen erst danach neu formulierten, anderen Fall als dritten Nachweis durchführen. |

Der Nachweis belegt Erweiterbarkeit innerhalb der festgehaltenen Fähigkeiten;
er behauptet keine Unterstützung jeder denkbaren zukünftigen Hardware.
Ein unbequemes normales Spielverhalten darf nicht aus dem Prüfauftrag entfernt
werden, um SP-T02 nachträglich als erfüllt zu erklären.

## 7. Ausgangsstand und nächste Vertragsarbeit

**Belegt durch Quellenlesen am genannten Commit:** Das
[Profilmodell](configuration/profile-model.md), der
[Graphkern](configuration/execution.md), die
[Session](configuration/session.md),
[Profilanwendung](configuration/profile-application.md) und
[Fähigkeitskatalog](companion/profile-capabilities-v1.md) bieten wiederverwendbare
Grundlagen. Die Modellvokabeln in
[Profile.h](../include/GameConfiguration/Profile.h) sind teilweise noch
rollen-/kristallgebunden. [src/main.cpp](../src/main.cpp) startet weiter
`ReichstagGame`; die
[Profilprobe](../src/ProfileRuntimeProbe/main.cpp) nutzt synthetische
Gerätebesitzer. Der reale
[Baugruppenstart](../firmware/led/src/ProductMain.cpp) ist noch keine
angeschlossene allgemeine Spielpaketlaufzeit. Der
[GM-Stationspilot](companion/spielleiter-stationspilot.md) ist ein vorhandener
Einstieg, kein vollständiger Regeleditor.

D0 ist integriert als Spezifikation. D1-A in
[PR #73](https://github.com/Sven401/Larp-Escape/pull/73) bleibt nach dem
[Projektstand](../handoff/MAINTAINER.md) separat offen und ist keine an dieser
Basis integrierte Produktfunktion. D1-B/C, D2–D4 sowie allgemeiner Zugang und
reale Profil-/Spielintegration bleiben offen. Historische Teilbelege werden
durch SP-01 weder zurückgesetzt noch zu einer Gesamtspielabnahme aufgewertet.

Die [Umsetzungsplanung](veranstaltungsarchitektur-umsetzung.md) muss deshalb
Ausdrucks-/Paketvertrag, reale Laufzeit, allgemeine Autorenschaft und verteilten
Betrieb gemeinsam auf SP-T01–SP-T07 ausrichten. Nächste technische Festlegungen
sind der anhand zweier verschiedener Spiele begründete Umfang der Regelprimitiven,
Bearbeitungsmetadaten, Paket-/Versionsvertrag und das gemessene Gesamtbudget.
Technologiewahl und konkrete Schemata folgen diesen Nachweisen. Dieser
Beschluss enthält keine neue Implementierung, Installation oder Gerätefreigabe.
