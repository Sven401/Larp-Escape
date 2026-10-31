# Manuelle Ersatzfreigabe: Erweiterungsvertrag v2

Status: beauftragte technische Auswahl zu #93, 31.10.2026. Basis ist
`f716c353a036104a40f251ac941f74cb237f16e6`. Der Nutzer hat die bewusste,
aktuell berechtigte manuelle GM-Ersatzfreigabe mit eigenem T2-Beleg und derselben
fachlichen Once-Freigabe bestätigt. Der Maintainer hat die unten bezeichneten
Versionsgates und Gruppengrenzen bestätigt. Das ist der vollständige
Umsetzungsauftrag, keine Behauptung einer bereits verfügbaren Produktfunktion.

Implementiert ist zunächst nur das getrennte Python-Referenzmodell samt Tests.
Compiler, Artefaktprüfung, ProgramBuilder, Planner, konkreter Kontrollbesitzer,
D1-Codec und Wiederanlauf sind bis zum jeweiligen Nachweis offen. Insbesondere
ist atomarer RAM-Commit kein dauerhafter Anwendungscommit. Produktänderungen
folgen erst nach gezielter Vertragsgegenprüfung.

Maßgebliche Grundlagen: [P0 v1](p0-vertrag-v1.md),
[Sprachformat](p0-formate-v1.md), [D0](../veranstaltung/d0-vertrag-v1.md),
[D0-Sicherheit](../veranstaltung/d0-sicherheit-migration-v1.md),
[D0-Codec](../veranstaltung/d0-fact-codec.md) und
[D1](../veranstaltung/d1-persistenz-v1.md). Die Erweiterung verändert deren
bestehende Byteformate nicht stillschweigend. MUSS bezeichnet nachfolgend eine
Anforderung an die beauftragte Umsetzung.

## 1. Versionsschranke

- Neue Inhalte verwenden `format:"larp.package.v2"`; ihre erzeugten
  Stationsprofile verwenden `larp.rules.v2`.
- `larp.module.v2` ist für Module mit neuen Konstrukten erforderlich.
  Gewöhnliche v1-Module bleiben in v2-Inhalten verwendbar. Ein v1-Inhalt darf
  keine v2-Module oder neuen Konstrukte einschleusen.
- Inhalt und Deployment deklarieren ausdrücklich
  `{name:"rules_manual_override",version:1}`. Das ist die hier definierte neue
  Fähigkeit, keine automatisch erfundene Voraussetzung vorhandener v1-Pakete.
- `larp.deployment.v1` behält seine vollständige Feldgrammatik. Ein v2-Inhalt
  verlangt dort die neue Fähigkeit und genau ein passendes `larp.rules.v2`-Profil
  pro Station. Alte Leser müssen die unbekannte Profilfassung/Fähigkeit sperren.
- Fehlende/abweichende Gates sind Fehler. Kein Fallback auf v1, kein Upgrade
  durch bloßes Umbenennen. v1 bleibt byte- und bedeutungsgleich; seine bisher
  nicht implementierte `local_fallback`-Ausführung wird dadurch nicht aktiviert.

Die bisherigen sechs v1-Referenztraces bleiben unverändert. Es entsteht kein
drittes Referenzspiel, sondern eine gesonderte Folge von Verwaltungs- und
Freigabegegenbeispielen.

## 2. Grammatik und Gruppe

Zu den bestehenden Triggerobjekten kommt exakt `{kind:"override"}` hinzu.
Es besitzt keine Port-, Timer- oder Subjectliste und keinen lokalen Samplewert.
`input`/`port_is` sind ohne lokalen Samplekontext Typfehler. Andere reine,
typisierte Guardausdrücke behalten ihre bestehenden Eager-/Unknown-Regeln.

Zu den bestehenden Aktionen kommt exakt `{op:"override",subject:ID}` hinzu.
Sie erzeugt ausschließlich einen lokalen D0-Fakt Typ 2, true. `emit(subject)`
erzeugt weiterhin ausschließlich Typ 1. Die neue Aktion darf nur in der unten
validierten Ersatzregel stehen und verleiht selbst keine Berechtigung.

Eine Gruppe besteht aus genau einer fact-getriggerten Originalregel und genau
einer Ersatzregel derselben Station. Die Originalregel hat
`offline:{policy:"local_fallback",rule:ID}`. Die Ersatzregel hat Override-Trigger
und `offline:{policy:"local_continue"}`. Beide sind `once`.

Das geordnete Aktionsprogramm muss übereinstimmen, wenn in der Originalregel
jede `emit(S)`-Aktion durch `override(S)` ersetzt wird. Erfüllte Subjects und
geordnete Effekte mit konkreten Ressourcen sind damit gleich. Mindestens ein
`emit(S)`-/`override(S)`-Paar ist erforderlich; eine reine Effektgruppe ohne
eigenen T2-Erfolgsbeleg ist ausgeschlossen. Bestehende Subject-Duplikat- und
Produzentenkonfliktprüfungen gelten weiterhin. Werte-/Timerwrites,
Fallbackketten, Zyklen, Mehrfachzuordnung eines Mitglieds und fremde Stationen
sind in dieser Erweiterung ausgeschlossen. Die bestehenden Aktionsbudgets gelten
unverändert. Keine zusätzliche Gleichheit von Priorität oder Generation wird
erfunden.

Beide Deploymentzeilen müssen dieselbe nichtnull stabile Rule-ID enthalten.
Nur innerhalb dieser geprüften Gruppe ist die doppelte ID zulässig. Andere
Gruppen oder normale Regeln dürfen sie innerhalb derselben Station nicht teilen.
Original und Ersatz behalten verschiedene qualifizierte Namen und ihre eigene
Generation. Die Latchidentität bleibt `(event,run,station,stableRuleId)`.

Der Sourceverweis bezeichnet eine Regel derselben Definition. Die generische
Expansion qualifiziert den Verweis mit demselben Instanzpfad; kein Referenzieren
zufälliger Nachbarinstanzen oder Namenssonderfall für ein Modul. Die tatsächliche
IR und Deploymentzeile enthalten den vollständigen qualifizierten Verweis.
Auch ungenutzte Moduldefinitionen werden auf die neuen Sprachregeln geprüft.

## 3. Ganze Artefaktprüfung und Fakten

Die bisherige Prüfung tatsächlicher Source-/Modul-/Bindungs-/Profil-/Medienbytes,
Hashes, Längen, Pfade und aller Stationen bleibt erhalten. Die v2-IR wird erneut
erzeugt und bytegenau mit dem tatsächlichen Profil verglichen. Metadatenprofil,
caller-owned Speicher und explizite Kapazitätsfehler bleiben bestehen.

Die gemeinsame Ausgabe muss in der tatsächlichen lokalen Stationsbindung sowohl
für Typ 1 als auch für Typ 2 erlaubt sein. Diese Metadatenberechtigung ist keine
aktuelle GM-Autorisierung. Gruppengleiche Ressourcen sind innerhalb der beiden
gegenseitig exklusiven Varianten zulässig. Konkurrenz durch andere externe
Regeln/Produzenten bleibt nach den bisherigen Regeln ausgeschlossen.

Die Deployment-Abhängigkeiten sind je `(station,sharedRuleId)` die exakte,
sortierte statische Vereinigungsmenge beider Varianten. Die bestehende
Fixpunktanalyse über Fakten, Werte und Timer bleibt erhalten. Die Vereinigung
ist ein Katalog möglicher Abhängigkeiten, keine Laufzeitpflicht aller Original-
Fakten für den Ersatz. Der tatsächliche Guard-/Operanden-/Zustandsbeweis bestimmt
die konkreten ausgehenden FactRefs; höchstens acht je neuem D0-Fakt. Fehlende
Remote-Fakten werden durch die Verwaltungsentscheidung weder erfunden noch als
Abhängigkeit ausgegeben.

Interne zugelassene Fakten müssen ihren tatsächlichen Typ 1/2 zusätzlich zu ihrer
Zulässigkeitsklasse erhalten. Beide zugelassenen Erfolgstypen dürfen normale
Fact-Guards erfüllen. Historie, Typ 3, Receipts und Beobachtungen dürfen das nicht.
Der D0-Codec liefert nur `SignatureCheckedUnauthenticated`; erst ein gesonderter
Besitzer prüft installiertes Vertrauen, Kontext, Rechte, Epochen, Replay und
inhaltliche Erlaubnis. Signaturgültigkeit verleiht keine GM-Rechte.

## 4. Konkreter Prepare-/Confirm-Besitzer

Die vorhandene Kette `PeerRegistryOwner -> IdentityAccess -> TrustedAuthority`
bleibt Quelle der lokal bestätigten GM-Zuordnung. Ein neuer Regelkontrollhandler
im serialisierten Anwendungsbesitzer verwendet diese Kette. Der heutige
`GameControl` bedient nur `ProfileApplication` und ist noch kein angeschlossener
Regeln-Besitzer. Sein altes `GameCommand` v2 und dessen uint16-Ziele werden nicht
als Override oder als vollständiger Regelkontext umgedeutet.

Ein neuer exakt versionierter Nachrichtentyp benutzt den bestehenden begrenzten
Kontrollweg; eine neue BLE-ID ist hierfür nicht erforderlich. Die vorhandenen
120 Byte Fragmentnutzlast/1024 Byte Dokumentumfang werden nicht still vergrößert.
Der vollständige Kontext wird serverseitig kopiert; Confirm sendet einen kleinen
opaken Token und begrenzte Replay-/Verbindungsdaten.

Der angezeigte Auswahlstand gehört zu einer frischen Besitzerinkarnation und
einem serverseitig kopierten Katalog-/Kontextstand. Ein alter Auswahlstand darf
auch nach vollständigem Verlust der RAM-Dedupetabelle nicht an einen neuen
Besitzer gebunden werden. Prepare und Confirm besitzen getrennte ursprüngliche
Phasenidentitäten. Wiederholtes Prepare mit derselben Identität und demselben
unveränderlichen Request-/Autoritätsinhalt liefert denselben ursprünglichen
Token und dieselbe absolute Frist. Geänderter Inhalt oder andere ursprüngliche
Autorität unter derselben Identität ist Konflikt. Eine neue Entscheidung
verlangt eine neue Phasenidentität, keinen automatischen Refresh.

Prepare speichert in einem begrenzten privaten Bereich unveränderlich:

- Event, Run, Station, Box, Ursprungsepoche, Deployment-/Bindungs-/Inhaltsdigest;
- konkrete Original- und Ersatzregel, shared Rule-ID und beide Generationen;
- Profil-/Fortschritts-/Rechte-/Vertrauens-/Ressourcen-/Kapazitätsrevisionen;
- ursprünglichen geprüften Peer, Registrierung/Lifecycle, Verbindungstoken und
  Handle sowie Prinzipal und Rollenepoche;
- unverwechselbaren frischen einmaligen Token, ursprüngliche Requestidentität
  und eine feste absolute lokale Frist.

Der aktive unveränderliche Regelprogrammstand bzw. sein Fingerprint muss vor
neuer Übernahme ebenfalls passen. Die gespeicherten Generationen und Mitglieder
sind keine bloßen Diagnosedaten. Vertrauen, Ressourcenmapping und Kapazität
besitzen eigene Fences; ihr Wechsel darf auch ohne Fortschritts- oder
GM-Verbindungsänderung einen vorbereiteten Plan sperren. Der finale Callback
liest deren aktuelle Werte, nicht nur Kopien vom Empfang des Confirm.

Prepare erzeugt weder Latch noch Fakt, Intent oder dauerhafte Erfolgsmeldung.
Ein voller Bereich erzeugt deferred, keine Verdrängung eines noch offenen
Vorgangs. Tokenentropie/-Nichtwiederverwendung und Fristüberlauf sind im echten
Besitzer nachzuweisen; das Python-Modell behauptet diese Qualifikation nicht.
Sicher abgelaufene oder durch Entzug/Mappingwechsel ungültige Vorbereitungen
werden terminalisiert und geben aktive Slots frei; ihre Replayidentität wird
nicht zu einer neuen Vorbereitung. Lebendes Pending oder Commit-Unknown wird
nicht verdrängt. Ist ein pending Eingang bereits dauerhaft aufgenommen, benötigt
seine Ablehnung einen dauerhaften Abschlussbeleg; RAM-Löschen reicht nicht.

Confirm muss die ursprüngliche Autorität, Frist, Kontextkopie, Zielzuordnung und
Ausgangsrevisionen erneut prüfen. Eine späte Suche nach dem aktuellen Ziel oder
dem neuesten Paket ist verboten. Disconnect, Rollenentzug, Neuzuweisung desselben
Peers oder Ersatz des Autoritätspublishers machen den alten Token ungültig.
Wiederholung verlängert weder Frist noch Berechtigung. Die Frist gilt auch am
definierten finalen Besitzerfreigabepunkt unmittelbar vor D1; Prüfung allein am
Empfang reicht nicht. Das Modell verwendet die inklusive Endgrenze
`preparedAt <= now <= expiresAt` und geprüfte uint64-Addition. Späterer Ablauf
nach Committed/Unknown löscht keinen Latch und erlaubt keine neue Vorgangs-ID.
Status-/Cachezugriff wird
ebenfalls autorisiert, bevor ein fremder Auftragserfolg offengelegt wird.

Ein bekannter abgeschlossener Vorgang liefert seinen vorhandenen Beleg. Hat
zwischen Prepare und Confirm bereits das Original denselben Latch bestätigt,
darf ein aktuell autorisierter Confirm nach Prüfung des vollständigen
unveränderten ursprünglichen Kontextes `already_released` ohne zweite Wirkung
erhalten. Das ist keine Übernahme eines Plans mit veralteter Revision.

Request32/Lease sind Transportreplaydaten, keine langlebige D1-Vorgangs-ID. Der
Besitzer ordnet dem bestätigten unveränderlichen Auftrag eine eigene dauerhaft
eindeutige Vorgangs-ID und einen vollständigen Inhaltsdigest zu. Eine erneute
Annahme desselben Auftrags darf keine neue ID oder neue Fact-Sequenz beanspruchen.

## 5. Besitzerzustände und Commit

| Zustand | Erlaubter nächster Schritt |
| --- | --- |
| prepared | Aktuell autorisiertes Confirm bis einschließlich expires; sonst terminale Ablehnung. |
| confirmed/pending | Derselbe eingefrorene Auftrag mit derselben Vorgangs-ID; keine Neuerzeugung bei Kapazitätsmangel. Rechte/Frist/Fences vor Übernahme erneut prüfen. |
| commit_unknown | Anwendungs-/Eingangspfad sperren; kein neuer Versuch und keine neue ID, bis qualifizierte Recovery denselben Vorgang auflöst. |
| committed | Vorhandenen Beleg zurückgeben; kein neuer Plan. |
| rejected/not_committed | Kein späteres Wiederbeleben. Ein weiterer Versuch braucht eine neue bewusste Entscheidung. |

Die Unterscheidung `not_committed` darf nicht aus bloßer Abwesenheit eines
Records oder einem alten RAM-Snapshot geraten werden. Sie benötigt das echte
qualifizierte Journal-Recoveryergebnis. Ein Neustart schafft keine neue
Verbindungsberechtigung; ein pending Auftrag darf nicht allein dadurch wirken.
Der neue Besitzer kann Recovery ohne alte RAM-Tokenzeile auflösen. Journalbild
und vollständig geprüfter Kontext müssen zusammenpassen; ein anderer Run oder
ein anderes Paket darf alte Fakten/Latches nicht übernehmen. Gespeicherte
Faktkontexte werden vor Guardauswertung erneut geprüft. Auch beim kombinierten
Fall aus Commit-Unknown und älteren attempted-Intents erfolgt nach qualifizierter
Recovery der Restart-Übergang attempted nach unknown; keine automatische Wirkung.
Ein neuer aktuell berechtigter Leser darf den dauerhaften Vorgangsbeleg nur über
einen getrennten Statusleseweg erhalten. Dieser Weg führt keinen alten Confirm
aus und bindet dessen Token nicht an die neue Verbindung um. Auch ein
`committed`-/`already_released`-Shortcut prüft vor der Offenlegung den vollständigen
passenden Kontext und die aktuelle Leseberechtigung.

Der Planner plant weiterhin alle passenden Regeln auf einem unveränderlichen
Snapshot. Ein Override-Eingang adressiert nur die vorbereitete Ersatzvariante.
Eine Gruppe darf in einem Plan genau einen Freigabeanspruch stellen. Zwei
Ansprüche auf denselben gemeinsamen Latch sind ein Konflikt ohne Teilwirkung;
keine Gewinnerregel durch Priorität. Ein bereits bestätigter Latch überspringt
beide Varianten. Neue Generationen und Paketwechsel setzen ihn nicht zurück.

Ein erfolgreicher Plan enthält den T1- oder T2-Fakt, gemeinsamen Latch, lokale
Erfolgsmarkierung, Eingangs-/Autorisierungsbeleg und begrenzte Intents. Das D1-
Anwendungsverfahren muss diese samt tatsächlicher ID-/Outboxzuordnung atomar
oder gleichwertig wiederaufnehmbar übernehmen. Der ursprüngliche Autorisierungs-
beleg wird vollständig kopiert und an Vorgangs-ID sowie T2-ID/Payload-Digest
gebunden. Unmittelbar vor Übernahme hält der Besitzer alle aktuellen Fences
bis zum eindeutigen Ergebnis; Unsicherheit verlängert die Sperre.

D0-v1-T2 enthält keinen GM-Prinzipal oder ausführlichen Verwaltungsgrund. Diese
gehören in den lokalen D1-Beleg. Es wird kein neues D0-Wirefeld eingeschleust.
Empfänger prüfen die ausdrücklich installierte T2-Ursprungsberechtigung; sie
erfinden keinen Nachweis über eine einzelne entfernte GM-Verbindung.

Eigene T1-/T2-Fakten werden erst nach Commit mit derselben ID und demselben
Digest später lokal angenommen. Ein Latch mit pending eigener Outbox, aber noch
ohne Loopback-Fakt, ist gültig. Kein fremdes Subject wird lokal gefälscht.
Pause/Stop/Recovery behalten ihre Sperren; ein Override ist kein verstecktes
Resume. Effekte folgen D1 prepared/attempted/unknown/observed. Absturz nach
attempted führt zu unknown, nicht zu automatischem Replay oder erneutem Latch.

## 6. Nachweise und offene Produktumsetzung

Das Modell `scripts/p0_manual_override_v2_model.py` ist eine getrennte dekodierte
Referenz für Gruppen/Claims, Autoritätswechsel und Besitzer-/Commitzustände.
Es verwendet die vorhandene Ausdruckssemantik. Sein Speicherjournal ist ein
abstrakter atomarer Referenzspeicher, kein Flash- oder Produkt-D1-Nachweis.
Modell-Token/Vorgangskennungen sind symbolische Testwerte, keine Entropiequelle.
Der eindeutige Symbolgeber überlebt im Modell den Austausch des Besitzerobjekts
als idealisierte Testannahme, nicht als Nachweis eines produktiven Allocators.
Geschlossene Replayeinträge und mathematische Beweisalternativen sind dort keine
RAM-Budgetqualifikation; die echte Umsetzung benötigt begrenzte eigene Speicher
mit expliziten Deferred-/Profilgrenzen und darf alte Aufträge nicht wiederbeleben.

Neue Tests müssen mindestens Original zuerst/Ersatz zuerst, doppelte Claims,
gleiche Ressourcen, verspätete Originale, Entzug und Regrant, falschen Kontext,
Fristgrenze, CAS-Wechsel, Kapazitätsmangel, Commit-Unsicherheit mit und ohne
bestätigte Übernahme, Generationswechsel und attempted-Effekte prüfen. Die
bestehenden sechs v1-Traces werden unverändert erneut ausgeführt.

Prüfstand dieses Arbeitsbaums, 31.10.2026:
`python -B -m unittest discover -s scripts/tests -p 'test_p0*model.py' -q`:
63 Tests bestanden (45 neue v2-Gegenfälle, 18 bestehende P0-Tests einschließlich
der sechs unveränderten Referenztraces). Führender Bearbeiter `developer_p1a3`,
anderer Vertrags-/Securitygegenprüfer `module_digest_review`: gefundene Replay-,
Kontext-, Fences-, Prozessverlust- und Slotfreigabefehler korrigiert und unabhängig
nachgeprüft; keine offenen P1/P2 im abgegrenzten Referenzumfang. Root hat den
gesamten 63-Fälle-Lauf erneut ausgeführt. Eine grüne Modellsuite gibt keinen
Produkt- oder D1-Pfad frei.

Danach folgen tatsächliche v2-Quellen durch Compiler/Artefaktprüfung/ProgramBuilder,
Planner und Besitzer, echte Autoritätsadapter, der gemeinsame Plan-/Belegcodec,
Journal-Recovery und End-to-End-Faulttests. Das bestehende ProgressJournal bietet
3072 Byte Nutzlast je physischem Record und höchstens acht neue Fact-IDs pro
Application-Commit. Abschnitt 7 beschreibt die ausdrücklich beauftragte
Bundle-Schicht für größere vollständige Pläne. Es gibt keine stille Kürzung,
Aufteilung in mehrere wirksame Anwendungscommits oder Partitionsvergrößerung.
Passt der vollständige Plan nicht in das ausgewiesene Transaktions-/Slotprofil,
bleibt er deferred, statt eine RAM-Freigabe als dauerhaft zu melden. D2-Transport
wird wiederverwendet, nicht neu erfunden. Geräteuploads und Trustpolicyänderungen
sind nicht Teil dieses Schnitts.

## 7. Atomare Bundle-Schicht über dem vorhandenen Journal

Status: am 31.10.2026 nach konkretem Entwurf und unabhängiger Gegenprüfung vom
Maintainer als technische D1-Umsetzung bestätigt; Produktcode und Powercut-
Nachweise noch offen. Die vorhandene Journal-ABI und Partition bleiben gleich.

Das eigenständige semantische Transaktionsprofil erlaubt höchstens 32768 Byte
vollständige kanonische Blobbytes. Es ist ausdrücklich NICHT das P0-
Quellmetadatenbudget. Der Blob enthält entweder einen vollständigen unveränderlichen
Programmkatalog oder einen vollständigen Plan einschließlich ursprünglicher
Autoritäts-/Eingangsbelege, Zustandsschreibungen, Beweisen, endgültigen signierten
COSE-Outboxbytes und Intents. Beide Größen werden getrennt begründet ausgewiesen.
Es gibt keine Zusage, dass jeder erlaubte P0-Plan in dieses Betriebsprofil passt.

Der Blob wird in höchstens 16 unveränderlichen Datenchunks mit je 2048 Byte
Nutzdaten vorbereitet; nur der letzte darf kürzer sein. Jeder Chunk ist ein
vollständiger `ProgressKind::Control`-Record mit `issueCount=0`. Er hat keine
fachliche Wirkung, erzeugt keinen Freigabe-Latch und vergibt keine Fact-ID.
Ein einziger abschließender Application-Marker bindet sämtliche vollständigen
Bytes und vergibt gemeinsam den gesamten ausgehenden Bereich von null bis acht
Fact-IDs. Katalogfinalisierung ist ebenfalls nur Control, niemals Anwendung.

Die kanonischen CBOR-Maps haben exakt folgende Integerlabels; kürzeste definite
Kodierung, keine unbekannten Felder, Duplikate oder angehängten Bytes:

| Label | Gemeinsames Feld |
| --- | --- |
| 0 | Textdomain `larp.rules.bundle.v1` |
| 1 | Rolle: 1 Datenchunk, 2 Finalmarker |
| 2 | Art: 1 Katalog, 2 Anwendung |
| 3 | unveränderliche Parent-/Bundle-Operation-ID, bstr16, nichtnull |
| 4 | unveränderlicher Parent-Requestdigest, bstr32 |
| 5 | tatsächlicher Deploymentdigest, bstr32 |
| 6 | vollständiger Katalogdigest, bstr32 |
| 7 | vollständiger Blobdigest, bstr32 |
| 8 | gesamte Bloblänge, positiver uint32 innerhalb des Transaktionsprofils |

Ein Chunk ergänzt Labels 9 Anzahl, 10 nullbasierter Index, 11 Daten-bstr. Anzahl
ist exakt `ceil(totalBytes/2048)`; alle Datenlängen müssen die eindeutige vollständige
Abdeckung ergeben. Der Journalheader hat eine eigene sichere einmalige physische
Operation-ID und als Requestdigest SHA-256 des tatsächlichen kanonischen
Chunkpayloads. Die physischen IDs werden einmal gewählt und anschließend
eingefroren; keine Hashtrunkierung oder unbegrenzte Regeneration bei Kollision.

Ein Finalmarker ergänzt Labels 9 Anzahl und 10 eine vollständige Liste
`[[journalIndex,physicalChunkOperationId,rawChunkRequestDigest]...]` in
Chunkindexreihenfolge. Seine Journal-Operation-ID ist die Parent-ID; sein
Requestdigest bindet den tatsächlichen Finalpayload. Der Marker muss die echten
gespeicherten Chunkbytes, deren Header, vollständige geordnete Abdeckung und den
SHA-256 der gesamten Blobbytes prüfen; behauptete Digestfelder genügen nicht.
Alle gültigen Chunks derselben Parent-ID müssen dieselbe Art, Requestidentität,
Deployment-/Katalogbindung, Größe, Anzahl und denselben Blobhash besitzen. Auch
vor Final ist widersprüchliche Wiederverwendung Conflict, keine auswählbare
passende Teilmenge. Ein Anwendungskatalog muss zuvor vollständig finalisiert und
verifiziert sein. Kein neuer Stand ersetzt still einen alten gebundenen Katalog.

Physische Journalrevision und logische Planner-/Grantrevision sind getrennt.
Control-Chunks erhöhen die erste, niemals die zweite. Kandidaten für die
einmalige Signierung sind `capturedLastIssued+1..N`; Chunks ändern LastIssued
nicht. Der Application-Finalheader vergibt genau dieses vollständige N.
Vor Final sind auch bereits korrekt signierte Blobbytes ausschließlich private
Vorbereitung: keine Outbox, kein Fact-Export, kein Relay und kein Loopback.
Nach Erfolg verwenden Outbox und späterer eigener Eingang genau dieselben
ursprünglichen Bytes, IDs und Payloaddigests.

Die Phasenidentitäten, ConfirmedAt, ursprüngliche Autorität, Signaturen und
sämtliche schon versuchten physischen Requests einschließlich ihrer
expectedJournalRevision bleiben beim Retry unverändert. Eigene Chunkwrites
dürfen weder den Preparestand rebasieren noch die Frist verlängern. Unmittelbar
vor Final prüft der Besitzer erneut den ursprünglichen vollständigen Kontext,
aktuelle Originalautorität/Verbindung, feste Frist, alle logischen Rechte-/Trust-/
Ressourcen-/Kapazitätsfences und tatsächliches LastIssued. Entzug/Ablauf vor Final
führt ohne fachliche Teilwirkung zum terminalen Abbruch. Bei Unknown ist die
Sperre bis zur qualifizierten Auflösung bindend, auch nach Fristablauf.

Vor dem ersten Write werden alle benötigten Katalog-/Plan-/Finalslots sowie die
nachfolgend notwendigen Intent-Claim-/Resultbelege reserviert bzw. im serialisierten
Besitzerbudget gesperrt. Neue Arbeit darf dieses Budget nicht stehlen. Vollständige
valide vorbereitete Chunks ohne Marker bleiben nach qualifiziertem Recovery
unwirksam und verbrauchen Slots. Kein Erase/Reclaim wird dazu erfunden.

Unknown schon bei einem Chunkwrite verlangt echten Backend-Reopen und
`ProgressJournal::recover`, nicht bloß das erneute Lesen eines Caches oder einen
neuen Wrapper. Belegte/torn Records und fehlende reservierte Tails bleiben harte
Journalfehler und werden niemals als harmlose Orphans ignoriert. Ein fehlender
Finalmarker beweist erst nach vollständig qualifiziertem Recovery Nichtanwendung.
Ein bestätigter Finalmarker mit fehlender/falscher Seite oder unpassendem Kontext,
Katalog, Umfang oder Hash sperrt die semantische Recovery; kein Rückfall auf einen
älteren Spielzustand und keine Präfixpublikation.

Erst nachdem der vollständige wiederhergestellte Bestand geprüft ist, wird ein
vollständiger Snapshot publiziert. Alte bestätigte Fortschritte und ursprüngliche
Identitäten/Belege bleiben erhalten. Ihre mathematische Signaturprüfung ist von
heutigen GM-Rechten zu trennen; aktueller Entzug löscht keine historische
Freigabe. Heutige Autorität gilt für neue Finalisierung und Statusleserechte,
nicht als Ersatz für den damaligen Beleg. Frühere attempted-Intents werden zu
unknown und niemals automatisch wiederholt.

Pflichtnachweise sind unter anderem jeder programmierte Teilpräfix/Sync-Cut,
Chunk- und Final-Unknown, echter simuliert qualifizierter Reopen, tatsächlicher
Ownerverlust, falsche/fehlende/vertauschte Seiten und Parentkonflikte, Slot-/
Intentreservierung, Entzug und Ablauf zwischen Chunks/Final, vollständige
Original-/Ersatz-End-to-Endfälle sowie korrekt signierte unveränderte Outboxbytes.
Simulation ersetzt keine spätere Geräte-Powerlossqualifikation.
