# P1a-5: vollständiger Mehrregelplaner

Status: **implementiert und hostseitig unabhängig gegengeprüft; native PR-Prüfung offen**,
31.10.2026. Auftrag: [#93](https://github.com/Sven401/Larp-Escape/issues/93).
Basis `5baee7498eac4d303937b08cacdf242efdc90ede`, ausdrücklich auf dem offenen
PR #124 gestapelt. Entwickler `developer_p1a3`, anderer Qualitätsreviewer
`quality_p1`; technische Grundsätze und Koordination übergeordneter Maintainer;
gezielte FactRef-/D0-Gegenprüfung `module_digest_review`. Kein Geräteauftrag.

## Ziel

Der neue Pfad muss aus tatsächlich kompilierten Modulen ein unveränderliches
Stationsprogramm übernehmen, alle passenden Regeln gegen denselben Snapshot
auswerten und einen vollständigen begrenzten Plan erzeugen. Eine separate
revisionsgeprüfte RAM-Übernahme dient der Rechnerabnahme. Sie ist kein dauerhafter
D1-Commit. Geräteaktionen dürfen während Planung und RAM-Übernahme nicht erfolgen.
Ein Metadatenstatus oder das Ausführen einzelner Aktionen erfüllt den Auftrag
nicht. Quellen: [P0-Semantik](p0-vertrag-v1.md), [Format](p0-formate-v1.md),
[D0](../veranstaltung/d0-vertrag-v1.md), vorhandene sechs unveränderte
Referenztraces und unabhängiges P0-Pythonmodell.

Der bestehende Profil-Engine-Pfad wählt einzelne Transitionen und kann nach
abgelehnten Aktionen fortfahren. Er bleibt erhalten. Seine unveränderte
Aktionsschnittstelle erfüllt die geforderte atomare Mehrregelplanung nicht.
Der neue RulesProgram-/RulesPlanner-Pfad übernimmt die bestehenden
Besitzer-/Snapshotkonventionen, ohne Profil-v1 still umzudeuten.

## Vor der Umsetzung festgelegte Grenzen

- Reale vollständige Planbildung: Sortierung nach Priorität und gebundener
  stabiler D0-Regel-ID (lexikografische Bytefolge), getrennt vom qualifizierten
  Diagnosenamen; keine Gewinnerregel bei konkurrierenden Schreib-/Ausgabezielen.
  Operanden stammen aus dem alten Wertesnapshot; keine sofortige Neuauswertung
  nach `set`. Alle relevanten Revisionen werden am Übernahmepunkt erneut geprüft.
- Der vom Deployment erwartete Inhaltsdigest muss dem tatsächlichen
  Compilerergebnis entsprechen; ein anderer Digest darf den Erwartungswert nicht
  still überschreiben. Regel-, Subject- und Portzuordnungen werden vollständig
  in das eigene Programm kopiert, ohne geliehene Namen oder Compilerpuffer.
- Portbindungen enthalten ausdrücklich die exklusiven Ressourcenidentitäten
  und erlaubten Ressourcen. Verschiedene logische Ports mit derselben exklusiven
  Geräteidentität dürfen nicht zwei konkurrierende Intents im selben Plan
  erzeugen. Reale Medien-/Capability-/Installationsprüfung bleibt zusätzlich
  Verantwortung des Ressourcenbesitzers und von P2.
- Stabile D0-Regelidentitäten werden ausdrücklich an expandierte IDs gebunden.
  Andere Generationen erzeugen keinen neuen Once-Latch. Fallback/Override bleibt
  ohne autorisierte Deployment-/Shared-Latch-Zuordnung ausdrücklich unsupported.
- 32 explizite Aktionen pro Plan; impliziter Timerconsume ist ein zusätzliches
  begrenztes Metadatenfeld. Consume und Rearm desselben konsumierenden Plans
  bilden gemäß P0 einen Timerwrite. Sämtlicher Speicherbedarf wird vorab geprüft.
- Erfasste fällige Timer sind Kandidaten. Vor jedem Eingang denselben Arm samt
  Generation und Deadline erneut prüfen; bestätigter Cancel/Rearm entwertet alte
  Kandidaten. Aktive Zeit, Pause, Recovery und Generationen verwenden geprüfte
  Überlaufgrenzen; kein Timerfortschritt durch bloße Funkstille.
- Unknown-Guard feuert nicht; auch ein Eingang ohne passende Regel erhält einen
  Vorgangsbeleg. Nicht verfügbare Fact-/Ledger-/Speicherdienste bedeuten Deferred.
  Überlauf und ungültige Aktionsoperanden verwerfen den vollständigen Plan.
  Ausdruckszweige werden auch bei bereits feststehendem Booleanergebnis geprüft.
- Begrenztes Vorgangsledger ohne stilles Verdrängen. Volles Ledger bedeutet
  Deferred. Gleiche Vorgangskennung mit anderem gebundenen Kontext/Payload ist
  Konflikt, keine harmlose Wiederholung.
- Ein zugelassener Fact ist bereits im Eingangs-Snapshot für sämtliche Guards
  sichtbar und wird mit dem Ergebnis atomar erhalten. Die Zulassung verlangt
  vollständigen D0-Kontext, echte FactRef und Digest vom verantwortlichen
  Eingangsbesitzer; ein beliebiger Subjectstring ist kein Beleg. Eigene erzeugte
  Facts liefern erst später wieder einen separaten Eingang, keine Rekursion.
  Nach Emitcommit darf daher der Latch bereits gesetzt sein, während der eigene
  Fact noch nicht in der angenommenen Faktmenge steht. Die ausstehende lokale
  Zustellung ist kein inkonsistenter Snapshot. Dauerhafte Outbox-/Restorekohärenz
  muss der spätere D1-Besitzer separat nachweisen.

## Fachlich gegengeprüfte Provenienzentscheidung

Für `any` ist ein deterministischer hinreichender Wahrheitsbeleg vorgesehen:
kleinste Referenzmenge, bei gleicher Größe lexikografische Ordnung. Lokale Auswahl
je OR-Knoten genügt nicht: `all(any(a,z),z)` hat den ausreichenden Beleg `{z}`.
Die exakte begrenzte Suche erhält deshalb alle nicht durch Teilmengen dominierten
Beweisalternativen und verbindet sie vor der abschließenden Auswahl mit den
weiteren notwendigen Ursachen. Fehlende OR-Zweige werden nicht als
AND-Abhängigkeit erfunden. Suchkapazität oder Suchbudget erschöpft bedeutet
Deferred/Betriebsprofilgrenze, niemals abgeschnittene Alternativen oder eine
behauptete allgemeine P0-Ungültigkeit.

Lokale Werte behalten die begrenzte FactRef-Provenienz ihrer bestätigten
Schreibursache. Neue Provenienz entsteht aus Guard und Operand, bei `add`
zusätzlich aus dem alten Wert; spätere Reads reichen diese Ursachen weiter.
`set` übernimmt nur die neuen Ursachen. Ein Timer behält entsprechend die
Arming-Ursachen; sein späterer Eingang reicht diese weiter. D1-B muss diese
Provenienz atomar mit Werten und Timerarmen persistieren und wiederherstellen.
Der aktuelle Hostplaner ist noch kein Nachweis dieser dauerhaften Übernahme.

Der auslösende Fact bleibt vollständig im Eingangsbeleg und in den Commitfences.
Er belegt jedoch keinen zusätzlichen Ausgangs-FactRef-Slot, wenn der hinreichende
logische Beleg ihn nicht benötigt. Widersprüchliche Digests derselben Fact-ID
sind Konflikt, keine alternative Beweisauswahl.

Die Acht-Referenzen-Grenze gilt **je neuem Fakt**. Sie ist keine Grenze für die
gesamte Planauditmenge, Intents oder beliebige Wert-/Timerprovenienz. Eine Regel
mit neun Bedingungen und ausschließlich Effectaktion darf dadurch nicht als
allgemein ungültig gelten. Die Speicher- und Beweismengen für andere Verbraucher
sind konfigurierbar oder ausdrücklich als Betriebsprofil begrenzt. Unbenutzte
Beweisalternativen verändern keinen Booleanwert; reale Zahlen-/Typfehler werden
trotzdem in allen Zweigen geprüft. Emissionsdrafts enthalten vorläufige
Modell-IDs und prüfen die im Modell sichtbaren Konflikte. `lastIssued` bezeichnet
die höchste im aktuellen lokalen Epochenmodell vergebene Sequenz, kein dauerhaftes
Reservierungsende. Erst der reale Besitzer kann die dauerhafte D0-Vergabe und
vollständige Wirefakt-/Reservierungs-/Recoveryprüfung beweisen. Ein Draft mit
vorläufigen Modell-IDs behauptet diesen Nachweis nicht.

## Erforderliche Nachweise

Compiler → transaktionale Programmkopie → Planer → RAM-Übernahme mit allen sechs
unveränderten Referenztraces (24 Schritte), generischen und umbenannten Modulen.
Bestehende 18 Modelltests einschließlich Referenztest bleiben unabhängige
Referenz (17 zusätzliche Modellgegenfälle). Hinzu kommen
Revisionrennen, echte FactRef-Zuordnung, Unknown, eager Overflow, alter Timerarm,
Pause/Recovery, Ledgerkapazität und Grenzen exakt/+1 für Aktionen, Facts, Intents
und Rechenschritte. Externe Konfluenz verlangt monotone Guards und disjunkte
Ausgaben sowie Permutations-/Replaybelege; lokale Reihenfolgespiele werden nicht
als global kommutativ ausgegeben.

## Finaler lokaler Prüfstand

Nach finaler Formatierung mit MinGW GCC 5.1.0 und C++11/Werror:

- CMake/Ninja und **9/9 CTest-Ziele** einschließlich des unabhängigen
  Compiler-/Planerkettenlaufs sowie der typisierten Budgetprüfnaht erfolgreich.
- PlatformIO 6.2.0 `host-regression`: **17 Suiten / 168 Fälle erfolgreich**,
  darunter elf neue Unity-Gruppen. Der explizite Quellfilter enthält den Planer;
  es wurde keine bestehende Suite entfernt.
- P0-Modell **18/18**, Architektur **155 Dateien / 322 Includes**, **91
  unabhängige C++11-Header**, Formatbaseline **345 Dateien** mit clang-format
  19.1.7; neue Test-/Auditquellen zusätzlich direkt geprüft.
- Anderer Bearbeiter `quality_p1`: alle **sechs unveränderten Referenztraces mit
  24 vollständigen Zustandsvergleichen** über tatsächliche kanonische
  Modulkompilierung, eigene Programmspeicher, Planer und abwechselnde separate
  RAM-Stagingbereiche erfolgreich. Unveränderte versionierte Referenzquelle
  (Git-Blob mit LF) SHA-256
  `58f696b6439b6cb162b378c44e56439755243a866720370c7a2f169820476605`.
  Der Windows-Checkout derselben Datei hat ausschließlich durch CRLF-Umsetzung
  den Rohhash `473346cfc53aea72de41a9f2be355964c8f7a4d037174a5716f7b22bccd9ac02`.
  Die Referenzprüfung normalisiert ausdrücklich nur CRLF zu LF; die Originaldatei
  bleibt unverändert und alle weiteren Byteänderungen bleiben erkennbar.
- **54 unabhängige vollständige Kettenfälle** für Provenienz, Konflikte,
  Überlauf, Grenzen, Ledger, Revisionsfences, physische Ressourcenaliase,
  widersprüchliche Fact-Metadaten, zulässige und gefälschte eigene Fact-Eingänge
  sowie zusätzliche unbekannte Bindungen erfolgreich. Erfolgreiche Fälle
  verwenden tatsächlich Windows-BCrypt-SHA-256, keine Mockhash-Annahme.
- Typisierte DAG-Prüfnaht: **4096 Besuche** werden vorgeprüft, geplant und
  übernommen; **4097** vor Programmveröffentlichung als `OperatingProfile`
  abgewiesen. Der Graph hat 512 Knoten und Tiefe drei. Diese Prüfung behauptet
  keine in JSON kodierte Knotenreferenzsyntax; der normale Bytepfad kopiert
  Ausdrucksbäume. Der statische Besuchscheck betrachtet den gesamten möglichen
  Eingang mit gemeinsam genutztem Memo, nicht jede Regel getrennt. Eine
  konservativ nicht beweisbare Grenze ist Betriebsprofil, keine allgemeine
  P0-Ungültigkeit. Der Laufzeitzähler bleibt zusätzlich begrenzt.
- Keine verbleibenden P1/P2 im dokumentierten unabhängigen Umfang. Die gefundenen
  Lücken bei ganzen Eingangsbudgets, eigenen Fact-Sequenzen/Provenienzen,
  lokalen Bindungskontexten und zusätzlichen Mapschlüsseln sind korrigiert und
  gegengeprüft. Die gezielte D0-/Provenienzprüfung erfolgte zusätzlich durch
  `module_digest_review`.

[Reproduzierbare unabhängige Quellen, unveränderte Goldens und Aufrufe](../../test/fixtures/rules-planner-review/README.md).
Die zwölf erschöpfend berechneten Zertifikatsgoldens sind eine andere
Berechnungsmethode als der produktive Antichain-Algorithmus.

Host32/GCC5.1/O0: Planner **40 B**, Program **336 B**, Builder **224 B**, Plan
**1176 B**, Workspace-Metadaten **8232 B**, State **312 B**, StateStorage
**84 B**, FactRef **200 B**. Das schließt die aufrufereigenen Pools nicht ein.
Größter eigener statischer Compilerframe **1136 B** in `combine`; rekursive
Aufrufketten und Backendbedarf kommen hinzu. Keine Geräte-Heap-, kumulierte
Taskstack- oder Latenzqualifikation daraus ableiten. Die Proofarena und deren
separates Sucharbeitsbudget (voreingestellt 262144 Schritte) sind ausdrückliche
Betriebsprofile, getrennt von den 4096 P0-Knoten-/Aktionsbesuchen.

Geprüfte SHA-256: RulesPlanner.cpp
`06aace615142e6923669ae71b7f930178855bd03c76b5e6577f3b87224a1c49b`,
RulesPlanner.h `c62b674f073cfdde18b3e0cae4b75c75aa25faaa06673e6ec65de4f574306db7`.
Commit und nachgelagerte PR-Nachweise werden im veröffentlichten PR zugeordnet.

## Offene Nachweise und Eigentümergrenzen

Der native Target `rules-planner-review-psa` baut den produktiven PSA-Adapter
und führt dieselben Referenz-/Gegenfälle und den Budgettreiber aus. Dessen
tatsächlicher Lauf am qualifizierten SDK und die zwölf aktuellen PR-Checks
stehen bei Erstellung dieses Commits noch aus. Der vorhandene lokale SDK-Ordner
mit abweichendem Sicherheitspin wird weiterhin nicht als Nachweis verwendet.

`CurrentSolved` ist die Projektion bereits verifizierter D0-Daten. Tatsächliche
Signatur-/Rollen-/Grantprüfung, Remote-Ursprungszuordnung im Deployment, genaue
eigene Outbox-ID-zu-Digest-Zuordnung, dauerhafte Reservierung/Unused-Ranges und
atomare durable Commitfence bleiben Eigentümeraufgaben. Der Binder prüft seinen
erwarteten Inhaltsdigest und die gelieferten Zuordnungen, authentisiert aber
kein Deploymentmanifest. Alte zulässige Epochen sind Listen, keine erfundene
Ein-Epochen-Grenze. Physische Medien-/Capabilityverfügbarkeit ist Voraussetzung
des Ressourcenbesitzers. Recovery wird nicht still in Running umgewandelt.

Dauerhafte D1-Übernahme einschließlich Wert-/Timerprovenienz, Authentisierung,
Geräteausführung, GM-Autorenschaft, vollständige Paket-/Metadaten-/Deployment-
prüfung und reale Mehrbox-/Drittspielabnahme bleiben Anforderungen des
vollständigen Ziels #93. Der erfolgreiche RAM-Commit schließt diese Tore nicht.
