# Profilabhängige Spielsitzung

Stand: 5. Oktober 2026. `GameExecution::Session` verbindet den Graphkern mit Rollen, Kristallen und Geräteaufträgen aus dem aktiven Profil. Der bisherige `ReichstagGame` bleibt der Produktionscontroller. Reale Komponenteninitialisierung, FreeRTOS-Geräteadapter und Companion-Transport sind noch offen.

## Eigentümer und Speicher

`include/GameExecution/Session.h` / `src/GameExecution/Session.cpp` sind hardwarefrei und allokieren keinen Heap. Eine Sitzung besitzt eine Engine, begrenzten Fortschritt, Gerätebeobachtungen und Abfrageuhren. Das geliehene Profil bleibt bis zum erfolgreichen Stop unverändert und lebendig. Alle Aufrufe gehören einem Anwendungseigentümer; die zusätzliche Reentranzsperre schützt auch die Gerätebeobachtung vor Rückrufen. `snapshot()` ist eine Wertkopie, keine Tasksynchronisation.

- Rollen werden über Profil-IDs und die sieben UID-Bytes erkannt; keine Umwandlung in das feste Rollen-Enum. Bis 16 Rollen und acht Kristalle je Rolle.
- Kristalle werden in Eingangsreihenfolge mit einem Index in die aktuelle Rollendefinition und dem Audioanforderungsstatus gespeichert. Runden zählen sättigend bis uint32-Maximum; bereits abgeschlossene Rollen werden einmal gezählt.
- Status enthält Profil-/Zustandsrevision, Rollen-ID, benötigte/erkannte Kristallzahl, Rollenhistoriengröße, Runden, Fehler und begrenzte Geräteergebnisse. Keine Karten-UID und keine Zeiger. `rolePresent` bedeutet erkannte Rollendaten, keine aktuelle Kartenanwesenheit.
- Stop löst den Profilbezug. Die verbleibende Statuskopie benutzt kopierte Rollen-/Geräte-IDs und Zahlen. Ein neuer Start leert Identität und Fortschritt; ein Reset im gleichen Profil bewahrt Rollenmetadaten und vorige UID nach dem bisherigen Vertrag.

## Geräteport und Abfragen

`IProfileDevices` bietet vorab erfasste Gerätewerte und typisierte Aufträge für Karten/Matrix, Audio, Effekte und Aktivierung. Alle Methoden müssen begrenzt und nicht blockierend sein. Ein produktiver Adapter liest synchronisierte Kopien und schickt kopierte Anforderungen an den jeweiligen Gerätebesitzer. Das Interface allein ist kein Nachweis für diese Eigenschaften auf FreeRTOS.

| Abfrageergebnis | Bedeutung in der Sitzung |
| --- | --- |
| `Fresh` | Eine passende neue Antwort verbrauchen; auch eine frische leere Kartenantwort ist ein Ergebnis |
| `Queued` | Ein neuer Auftrag wurde angenommen; Abfrage bleibt anstehend |
| `Pending` | Bestehender Auftrag läuft; nicht erneut als angenommener Auftrag zählen |
| `Rejected` | Auftrag abgewiesen; Fortschritt unverändert |
| `Unavailable` | Quelle nicht lesbar; kein Kartenleer-/Kristallfehler daraus erfinden |

Eine Anforderung enthält Komponenten-/Aktions-ID, Profil- und Zustandsrevision sowie Zeitpunkt und Zeitgrenze. Der Geräteadapter muss Antworten diesem Auftrag zuordnen und verspätete Antworten aus alten Zuständen/Profilen verwerfen. Bei einem Revisionswechsel wartet die Sitzung nicht weiter auf den alten Auftrag. Bis dahin wird ein anstehender Auftrag beim nächsten aktiven Tick abgefragt. Jede Abfrageaktion läuft höchstens einmal je Tick.

Die erste Beobachtung einer Abfrageaktion startet ihr Intervall und liest noch nicht. Spätere fällige Abfragen verwenden das konfigurierte Intervall; kein Nachholen verpasster Intervalle. Diese Uhren bleiben über Zustände und Reset erhalten. Sie verwenden Geräte-Wandzeit: Pause unterdrückt die Graphaktionen, nach Fortsetzen kann eine Abfrage sofort fällig sein. Die aktive Graph-/Waitzeit bleibt dagegen eingefroren. `Pending`, nicht fällig und keine Wake-Animation ergeben `deferred`; weder eine neue Queueannahme noch ein Fehler.

Ein Geräte-Sample besitzt Verfügbarkeit, Audio-BUSY und bis vier Wake-Eingänge. Audio-BUSY umfasst angenommene/queued Wiedergabe bis zu deren Abschluss; ein nackter BUSY-Pin kann vor Wiedergabebeginn noch LOW sein. Die Sitzung reserviert BUSY im aktuellen Tick direkt nach Annahme eines Play-Auftrags. Der Gerätebesitzer muss diesen Zustand in weiteren Samples fortführen. Pause beendet bereits angenommene Ausgaben nicht.

## Bewahrtes Spielverhalten und Zuordnungen

- Der Standard verwendet Kartenlesung nach 1.000 ms, Kristallabfrage nach 100 ms und Keystone-Beobachtung nach 2.000 ms. Nur frische Kartenantworten zählen als Leerlesung oder Überwachungsmiss.
- Das Kartenwechselverfahren bleibt unverändert: zwischen Runden zwei Gruppen von fünf Leerlesungen; nichtleere Antworten setzen den Retryzähler standardmäßig nicht zurück. `consecutiveEmptyReads=true` ist eine ausdrückliche Profilwahl. Neue Rollenmetadaten verwerfen alte Kristallindizes, ohne diese Karten-/Überwachungszähler heimlich zu leeren.
- Pro Matrixantwort wird der erste neue gültige Kristall akzeptiert. Schon bekannte Positionen werden übersprungen; die erste noch nicht übersprungene ungültige Position setzt den Kristallfehler. Eine spätere ungültige Position verdrängt keinen vorher angenommenen Kristall desselben Batches.
- Kristalle können während Audio-BUSY aufgezeichnet werden. `PlayCrystalTrack.value` bezeichnet die Ankunftsposition, nicht die Position im Rollenkatalog. Ein Audio-Versuch verbraucht die zugehörige Anforderung auch bei Ablehnung; daraus folgt keine tatsächliche Wiedergabe.
- `ReadCrystals.effect` kann einen Palette-Effekt referenzieren. Dessen Komponenten-ID bestimmt die LED-Zuordnung; Farben stammen aus dem akzeptierten Kristall bei `changesPalette=true`. Bei `effect=0` ist die bisherige implizite Zuordnung nur erlaubt, wenn genau ein aktiver LED-Streifen existiert. Mehrere Streifen brauchen die ausdrückliche Zuordnung. Das neue Standardprofil bindet Effekt 1.
- ResetRound leert Kristalle, ausstehende Kristallereignisse und Karten-/Überwachungszähler; ein bereits gesetzter Keystonefehler bleibt bis zum passenden Verbrauch/Reset bestehen. ResetGame leert zusätzlich Historie/Runden und aktuellen Kartenwert. ConsumeError verbraucht zuerst Kristallfehler, sonst Keystonefehler.
- CompleteRound zählt eine bekannte Rolle und setzt die vorige UID. Manuelle Wechsel können automatische Spielbedingungen umgehen; ohne erkannte Rolle wird die Abschlussaktion abgewiesen. Der Geräteport bleibt für Hardwaregrenzen zuständig.
- Wake-Animationen übernehmen die konfigurierten Matrixeingänge, LED-Zuordnung, Ursprünge und Framelänge. Aktivierungsfehler halten spätere Resetaktionen nicht auf. Fehlende Standby-Guards in späten Standardzuständen werden nicht automatisch ergänzt; Ausfall-/Recoveryvertrag bleibt offen.

Die Sitzung bildet ein logisches Spiel ab. Mehrere konfigurierte Leser/Matrixquellen teilen dessen erkannte Rolle, Kristallfortschritt und Historie; getrennte Spiele benötigen getrennte Sitzungen. Quellbezogene Beobachtungen/Fehler sind über Komponenten-IDs adressiert.

## Am Rechner bedienen

```powershell
pio run -e profile-tools
.pio\build\profile-tools\program.exe --import docs/configuration/standard-profile.json --play docs/configuration/standard-play.txt
.pio\build\profile-tools\program.exe --import docs/configuration/extended-profile.json --play docs/configuration/extended-play.txt
.pio\build\profile-tools\program.exe --import docs/configuration/standard-profile.json --play docs/configuration/gamemaster-play.txt
```

`--play` verwendet echte Session-/Graph-/Fortschrittslogik mit synthetischen Geräten. Kartenbefehle wählen die UID aus dem Profil; die UID wird nicht protokolliert. Matrix-/Wakewerte und Audioende werden eingespeist; Geräteaufträge werden als JSON Lines ausgegeben. `applied` gilt hier für die synthetische Ausgabe. Das Werkzeug initialisiert keine Hardware und stellt keine Appverbindung her. Die ältere `--preview`-Ansicht setzt Predicate-Werte unmittelbar und führt weiterhin nur den Graphen aus.

| Befehl | Argumente |
| --- | --- |
| `card` | NFC-Komponenten-ID, Profil-Rollen-ID; 0 für keine Karte |
| `crystals` | Matrix-ID, null bis 81 Positionen `Zeile:Spalte` (1..9 / 0..8) |
| `wakes` | Matrix-ID, Bitmaske der konfigurierten Wake-Eingänge |
| `finish` | Audio-ID; synthetische Wiedergabe beenden |
| `device` | Komponenten-ID, Verfügbarkeit 0/1 |
| `start`, `tick` | Millisekunden |
| `state`, `edge` | Ziel-/Kanten-ID, erwartete Zustandsrevision, Millisekunden |
| `pause`, `resume`, `stop` | Erwartete Zustandsrevision, Millisekunden |
| `snapshot` | Keine |

Leere Zeilen und `#`-Kommentare erlaubt; maximal 1.023 Zeichen je Zeile und 100.000 Zeilen. Eingabebefehle ändern nur das nächste synthetische Sample; der Spielkern berechnet alle Predicate-/Fortschrittswerte. Eingangs- und Ausgabeobjekte gehören nur dem Rechnerwerkzeug; dessen Dateizugriff/Heap ist kein Firmwareport.

`gamemaster-play.txt` führt Start, manuelle Zustandswahl, Pause/Fortsetzen und Stop mit passenden Revisionen aus. Es ist ein eigenständiges, editierbares Bedienbeispiel.

Beispiele regenerieren: Standard/Hinweis aus dem gebauten Profilwerkzeug exportieren, anschließend `python scripts/generate_play_examples.py`. Der Generator liest das Standardartefakt, schreibt dessen Sechs-Runden-Skript und ein bearbeitetes Zusatzprofil samt Skript. Er führt keine Assertions oder Firmware aus. Zusatzprofil ergänzt Rolle **77** als siebten Katalogeintrag, einen vierten Kristall mit Track **178** und orangefarbener Palette `0xffaa00`, Zustand **88** und einen **750-ms-Wait** vor Abschluss. Bestehende Drei-Kristall-Rollen behalten ihre Abschlusskante; der komplette Katalog wurde im Zusatzprofil nicht durchgespielt.

## Beobachteter Gebrauch am 5. Oktober 2026

Im gebauten Werkzeug: Standard sechs Rollen vollständig, je drei Kristalle; Tracks in gespeicherter Eingangsreihenfolge und Rollenabschlussaudio, Bonus-Track 900, genau ein synthetischer Aktivierungsauftrag. Danach Idle mit Runden/Historie null und beibehaltenen Rollenmetadaten; keine abgewiesenen oder nicht verfügbaren Aktionen im beobachteten Ablauf. Zwischen Rollen zehn frische Leerlesungen.

Dasselbe Programm ohne Neukompilierung mit Zusatz-JSON: Rolle 77, vier Kristalle, Zustand 88, vierter Track 178, Palette `0xffaa00`, Rollen-Track 177 und Rückkehr nach Idle. Der 750-ms-Guard liegt nur im JSON. Das Spielleiterbeispiel führte Start → Idle (manuell) → Pause → Fortsetzen → Standby (manuell) → Stop aus, mit Zustandsrevisionen 1 bis 6. Dies belegt diese normalen Rechnerabläufe; keine vollständige Fehler-/Timer-/Reentranzabdeckung oder Gerätewirkung. Synthetische Ports liefern ausschließlich sofort frische Antworten; Queued/Pending/Korrelation brauchen noch Ausführungsabdeckung und einen realen Adapter.

Keine neue Unity-Suite ergänzt/ausgeführt, kein Upload und keine Remote-CI. Standardspielvergleich bisher anhand Quellvertrag und normalem Rechnerablauf; automatischer Vergleich mit dem alten Controller, alle Fehlerpfade und Echtzeit-/Hardwareabnahme offen. Nächster Schritt: Komponentenregistry und Adapter mit klaren Eigentümern/Queues, anschließend kontrollierter Produktionsanschluss. Speicherung/Übernahme, erweiterter Status-/Befehlsvertrag, direkter infrastrukturloser Appzugang und Android folgen.

## Frische Quellen- und Ressourcenprüfung

Architekturprüfung: **52 Dateien / 131 direkte Includes**, **36** isolierte C++11-Header mit Warnungen als Fehler, **91** Formatdateien mit clang-format 19.1.7. Cppcheck 2.1 für Engine, Session, Validator, Standardfactory und beide Werkzeug-Einstiege mit `warning,performance,portability` ohne Befund nach Korrektur einer Parameterbenennung. Die zusätzlich angefragte Stilanalyse meldete vorhandene Schleifen-/Initializerlist-Konventionen; sie wurden nicht als Laufzeitfehler umgeschrieben. `git diff --check` ohne Befund. Keine neue Testsuite ausgeführt.

ESP32-Compilergrößen (`sizeof`, `-Os`): Profil **28.636 Byte**, Engine **4.204 Byte**, Session einschließlich ihrer Engine **5.752 Byte**, Session-Statuskopie **272 Byte**, Effektwert **120 Byte**. Zwei Profile plus Session mindestens **63.024 Byte**, vor Gerätebesitzern, Queues, Parser, Transport und Appfunk. Größter einzelner gemeldeter Session-Frame **256 Byte**, Engine weiter **112 Byte**. Portaufrufe, verschachtelte Guards und Validierung kommen zur Gesamttiefe hinzu; keine gemessene Taskstackreserve oder Loopzeit daraus ableiten.

Alle drei Buildprofile erfolgreich: Spiel **RAM 55.148 / Flash 1.226.013 von 1.310.720 Byte (93,5 %)**, USB **RAM 21.480 / Flash 291.673 Byte**, Rechnerwerkzeug mit Warnungen als Fehler. Das Spielprofil instanziiert Session/Profile weiterhin nicht; Linkerentfernung macht diese Werte zu keiner Reservefreigabe für die spätere Integration. Letzter Spiel-/Werkzeugbuild nach den Quellenkorrekturen: `pio run -e az-delivery-devkit-v4 -e profile-tools -j 2`; USB zuvor im gleichen Schnitt erfolgreich gebaut. Kein Upload.
