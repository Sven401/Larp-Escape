# P1a-6: tatsächlichen Artefaktsatz prüfen und Laufzeitbindungen ableiten

Status: **implementiert und hostseitig unabhängig gegengeprüft; native PR-Prüfung offen**,
31.10.2026. Auftrag: [#93](https://github.com/Sven401/Larp-Escape/issues/93).
Arbeitsbasis `deaf9dd11daaa1de010a7fd64c402f12847c55f4`, ausdrücklich auf PR #130
gestapelt. Führender Entwickler `developer_p1a3`, anderer Reviewer `quality_p1`,
technische Grundsätze/Koordination übergeordneter Maintainer; gezielter
Byte-/D0-Grenzreview `module_digest_review`. Kein Geräteauftrag.

## Ziel und Ergebnisgrenze

Deployment-JSON, Bindungs-CBOR, Inhalt, Module, Stationsdateien und Ressourcen
werden aus tatsächlich gelesenen Bytes geprüft. Die eigene Katalogdarstellung
leitet daraus die Zuordnungen für den ProgramBuilder ab; vom Aufrufer gelieferte
Projektionsbehauptungen ersetzen diesen Nachweis nicht. Regenerierte Stations-
dateien werden gegen die mitgelieferten tatsächlichen Bytes und Digests geprüft.
Eine reine Metadatenliste oder ein Vergleich fremder behaupteter Digests erfüllt
den Auftrag nicht.

Der Ergebnisstatus bestätigt Integrität und die hier benannten Bindungen,
keine Autorisierung, reale Capability-/Medienverfügbarkeit oder Aktivierung.
COSE-/Rollen-/Grantprüfung und der autorisierte Fallbackpfad bleiben gesonderte
Tore. Die Vorgängerprofile und vorhandene Legacy-Profilbedeutung bleiben erhalten.

Quellen: [P0-Formate §4](p0-formate-v1.md), [P0-Semantik/Budgets](p0-vertrag-v1.md),
[D0](../veranstaltung/d0-vertrag-v1.md), [Modulcompiler](p1a4-module-compiler.md),
[Laufzeitbindungen](p1a5-rules-planner.md). Folgende technische Präzisierungen
sind ausdrücklich Maintainerentscheidungen dieses Schritts; sie werden nicht
als bereits wörtlich im älteren P0 beschriebene Byteform ausgegeben.

## MetadataSourceProfile-v1

Der 32-KiB-Begriff war im P0 noch nicht bytegenau über mehrere Quelldateien
definiert. Dieses versionierte konservative **Quellmetadaten-Betriebsprofil**
zählt die rohen kanonischen Bytes des Inhalts plus jeder verschiedenen gepinnten
Moduldefinition genau einmal, auch wenn sie nicht instanziiert wird. Davon werden
ausschließlich die vollständigen Arraywerte (einschließlich ihrer Klammern) von
`content.stations[].rules`, `module.rules` und `module.constraints` abgezogen.
Instanzen, Verdrahtungs-/Parametermaps, Pins, Labels und Deklarationen zählen.
Exakt 32768 Bytes sind zulässig; mehr wird als Profilgrenze gemeldet.

Die Zählung ist schemaspezifisch, nicht ein Textfilter nach dem Wort `rules`.
Deployment, Bindungs-CBOR, abgeleitete IR und Mediendaten werden getrennt gezählt
und erhalten ihre eigenen ausdrücklich ausgewiesenen Quell-/Speicherprofile;
sie sind keine Quelldarstellung dieses Budgets. Das behauptet keine allgemeine
P0-Definition für alle künftig möglichen Metadatenarten. Der ältere P1a-2-Zähler
ohne Instanzinhalte bleibt unverändert als begrenztes DeclarationsOnly-Profil.
Ein gemeinsamer neuer Zähler und unabhängige Grenzvektoren prüfen die neue Regel.

## Deployment und Bindungen

- Vollständige kanonische Feldmengen und Listen: Stationen nach ID, Regeln nach
  `(station,expandedId)`, Ressourcen nach `(station,id)`, Profile nach
  `(station,path)`, Abhängigkeiten nach `(station,ruleId,originStation,subject)`.
- `ruleId` wird ausdrücklich als nichtnull 128 Bit in 32 kleinen Hexzeichen
  festgelegt, passend zur vorhandenen Laufzeitbindung. `expandedId` bleibt die
  qualifizierte Pointer-Schreibweise mit bis zu 485 Bytes. Keine Hashtrunkierung
  oder Ableitung einer Regelidentität aus Namen oder Generation.
- `rules[].offline` verwendet dasselbe exakte Objekt wie die expandierte Regel,
  keine zusätzliche Kurzschreibweise als String. Manifest und Regeneration
  müssen übereinstimmen. `requires` enthält alle Inhaltsanforderungen mit exakt
  derselben positiven Majorversion; zusätzliche sortierte eindeutige
  Deploymentanforderungen sind zulässig. Die positive Integerdarstellung bleibt
  lexikalisch ohne erfundene uint64-Obergrenze. Die tatsächliche Erfüllung aller
  Manifestanforderungen bleibt Geräte-/Installationsprüfung, kein Hashbeleg.
- Bindungs-CBOR ist die tatsächliche deterministische Map mit acht Schlüsseln
  0 bis 7: Version 1, event/station/box/rulesetId als nichtnull bstr16,
  generation als positives vollständiges uint64, geordnete eindeutige Epochen
  und Berechtigungen. Nur minimale Integer-/Längenkodierungen und definite
  Container; keine unbekannten Schlüssel, Tags, Floats oder Trailingbytes.
- `rulesetId` bleibt ein expliziter Wert **je Bindung**. Weder Ableitung aus
  packageId/contentDigest noch Gleichheit aller Stations-rulesetIds wird erfunden.
  Er ist über die tatsächlichen Bindungsbytes und deren Digest gebunden.
- Alle JSON-Duplikatfelder müssen den CBOR-Feldern entsprechen. Stations-IDs
  sind eindeutig; Box-IDs dürfen bei verschiedenen logischen Stationen gleich
  sein. Jede Stations-Subjectliste enthält genau deren eigene Inhalts-Subjects.
- Erforderliche Emits benötigen task_solved-Berechtigung (Typ 1). Typ 2 für
  eigene Subjects ist Metadatenzulässigkeit, keine tatsächliche Overridefreigabe.
  unused_range (Typ 3) verwendet die Box-ID als Subject, niemals ein Guardobjekt.
  Zusätzliche fremde Berechtigungen dürfen als ausdrücklich beschränktes
  Artefaktprofil abgewiesen werden, sind dadurch nicht allgemein D0-ungültig.
- Die aktuelle lokale Ursprungsepoche kommt vom eingerichteten Besitzer,
  ebenso tatsächliche lokale Box-/Ereignisidentität. Sie wird mit den geprüften
  Bindungen und der vollständigen Epochenliste verglichen, niemals aus dieser
  Liste ausgewählt. Nur die ausgewählte lokale Station erhält ein gebundenes
  Programm; die anderen Stationsartefakte werden vollständig geprüft, ohne
  unbekannte aktuelle Geräteidentitäten zu behaupten.

## Vollständiger Artefakt- und Abhängigkeitsabschluss

Je Station ist genau ein regeneriertes `larp.rules.v1`-Profil erforderlich.
Zusätzliche gekennzeichnete `profile.v1`-Dateien werden mit dem tatsächlichen
vorhandenen Legacy-Reader in eigenem inaktivem semantischem Scratch geprüft.
Dessen eigene Grammatik/128-KiB-Grenze wird nicht durch den neuen kanonischen
JSON-Vertrag ersetzt; kein automatischer Parallelbetrieb oder Konverter.

Jede Inhaltsressource wird anhand tatsächlicher Bytes geprüft, auch eine
unbenutzte. Nicht im Deployment platzierte Pins benötigen deshalb einen
ausdrücklichen Ressourcen-Pinzugang des Providers, keinen erfundenen Dateipfad.
Verschiedene Pfade werden nicht allein wegen gleichen erwarteten Hashs als
geprüft markiert. Ein gemeinsamer Pfad mit kompatiblen Erwartungen darf einmal
gelesen werden, muss aber alle erforderlichen semantischen Prüfungen erfüllen.
Rawressourcen können gültig dieselben Bytes wie ein anderes Artefakt verwenden;
eine pauschale Rollentrennung wäre keine belegte P0-Regel.

Physische Locatorwidersprüche werden nach tatsächlicher Box, betroffenem
Komponentenbesitzer und Locator geprüft. Unterschiedliche Komponenten dürfen
denselben bank/track-Wert verwenden. Gleiche physische Adresse mit anderen
Inhaltsbytes ist Konflikt. Wiederholte Box- oder Ressourcenidentität allein ist
kein Konflikt. Ohne qualifizierten Gerätegruppenkatalog wird Komponenten-
Exklusivität konservativ als Profil benutzt, keine unbelegte Kanalunabhängigkeit.

Statische Abhängigkeiten enthalten die vollständige mögliche Herkunft, nicht
nur den später gewählten hinreichenden Laufzeitbeleg. Ein begrenzter Fixpunkt
berücksichtigt Trigger-/Ausdrucksfakten, Wertschreiber einschließlich altem Wert
bei add, Timerarm-Ursachen und Timer-Eingänge. Die Manifesttupel müssen dieser
Menge genau entsprechen; weder fehlende noch erfundene zusätzliche Abhängigkeiten.
local_fallback wird syntaktisch geprüft, bleibt aber bis zur echten autorisierten
Shared-Latch-Zuordnung als UnsupportedScope sichtbar.

## Quellen, Hashes und Veröffentlichung

Compiler und Artefaktprüfung verwenden zwei verschiedene Hashobjekte. Ein
aktiver Hashkontext darf nicht zwischen gleichzeitig laufenden Parser-/Vergleichs-
phasen geteilt werden. Profilvergleich liest die reale Quelle zusammen mit der
regenerierten Ausgabe und prüft Länge, rohen SHA-256 und exaktes EOF. Ein
Quellenfehler unmittelbar nach der letzten deklarierten Byteposition bleibt
Fehler. Spezifische Artefaktfehler dürfen nicht als bloßer SinkFailure verschwinden.

Ressourcenlängen sind uint32, kein 256-KiB-Audiolimit. Medien werden mit begrenzten
Chunks und geprüftem uint64-Zähler/Fehleroffset gestreamt, nicht vollständig
kopiert. Alle Kataloge, abgeleiteten Bindungen und Programme bleiben in privatem
Staging, bis sämtliche Quellen, Hashes, Referenzen und Consumerprepare bestanden
sind. Erst dann veröffentlicht ein nicht fehlschlagender Abschluss das Ergebnis.

Pfade sind exakte virtuelle ASCII-Bundleschlüssel: relativ, ohne leere Segmente,
Backslash, Doppelpunkt, `.` oder `..`, ohne URL-/Großklein-/Unicode-Normalisierung.
Eine zusätzliche Längengrenze ist Betriebsprofil. Der echte Provider muss
aufgelöste Dateisystembegrenzung, Symlink/Reparse- und Archivgrenzen sicherstellen;
die Zeichenprüfung allein beweist das nicht. Locatorwerte werden nicht als
Pfade oder Code interpretiert. verification beschreibt die verlangte Methode,
keine bereits ausgeführte Geräteprüfung oder Betreiberbestätigung.

## Finaler lokaler Prüfstand

Nach finaler Formatierung, MinGW GCC 5.1.0 und C++11/Werror:

- CMake/Ninja und **15/15 CTest-Ziele** erfolgreich, einschließlich tatsächlicher
  Artefaktketten und der separaten gemeinsamen Metadatenprüfung. Bestehende
  Modul-/Planer-/Journal-/Partition-/Transport- und Legacyprüfungen bleiben erhalten.
- PlatformIO 6.2.0 `host-regression`: **20 Suiten / 225 Fälle** erfolgreich,
  darunter acht neue Unity-Gruppen; beide neuen Produktquellen sind ausdrücklich
  im Hostfilter und der gemeinsamen CMake-Quellliste angeschlossen.
- P0-Modell **18/18**, Architektur **163 Dateien / 346 Includes**, **95 unabhängige
  C++11-Header**, Formatbaseline **353 Dateien** mit clang-format 19.1.7; neue
  Unity-/Auditquellen zusätzlich direkt geprüft.
- Unabhängig `quality_p1`: **87 vollständige tatsächliche Artefaktsätze**, davon
  31 positive, erfolgreich gegen den echten Validator geprüft. Enthalten sind
  zwei/acht Stationen, wiederholte Box- und stationslokale Regel-IDs,
  nicht-erste aktuelle Epochen, uint64-MAX-Generationen, große lexikalische
  Majorversionen, genaue IR-Bytes, Ressourcen-/Locator-/Bindungskonflikte,
  Closure über Werte und Timer sowie die 32768/+1-Quellmetadatengrenze.
- **33 Quellenfehler-/Wiederverwendungspaare**, **55 Hashfehler-/Wiederverwendungs-
  paare** über alle beteiligten Hashkontexte, **55 gepaarte EOF-Prioritätsfälle**
  und **drei Alias-/fehlgeschlagene-Open-/Wiederverwendungsfolgen** bestanden.
  Begin/Update/Finish/falsche Digestlänge/Abortfehler erzeugen keine Veröffentlichung;
  Fehler löschen Ready und Ergebnisdigests.
- Neuer CBOR-Parser: **314 kürzere Präfix-/Wiederverwendungspaare** und **632
  Jede-Byte-Quellenfehler-/Hashbegin-Prioritäts-/Wiederverwendungspaare**, an
  minimalen und uint64-MAX-Bindungsbytes mit präziser Quellenidentität/Offset,
  erfolgreich. Es werden keine Mockhash-Annahmen als Erfolgsnachweis verwendet.
- Gemeinsamer Zähler zusätzlich isoliert durch tatsächliche Modulkompilierung:
  **neun feste Fälle** und **sechs Quellenfehler-/Wiederverwendungspaare**.
  Unbenutzte Module, doppelte Pins, Instanzmaps namens `rules`, direkte Regeln,
  Modulconstraints, UTF-8 und Escape-Rohbytes sind ausdrücklich enthalten.
- Tatsächliche Legacy-Readerprüfung akzeptiert zulässige Whitespace-/Unicode-
  Escapeschreibweisen und kompatible Rawressourcenaliase, lehnt aber semantisch
  falsches repinntes Legacy-JSON ab. Tatsächliche Audioressourcen mit **262145
  Bytes** werden gestreamt und geprüft. uint32-MAX-Deklarationen mit frühem
  Quellenfehler/Kurzantwort sind korrekt klassifiziert; **kein** 4-GiB-Latenz-
  oder Gerätespeichernachweis wird daraus abgeleitet.
- Keine verbleibenden P1/P2 im dokumentierten unabhängigen Umfang. Gezielte
  Byte-/D0-/Hash-/Veröffentlichungsprüfung zusätzlich `module_digest_review`.

[Reproduzierbare unabhängige Quellen und Aufrufe](../../test/fixtures/package-artifacts-review/README.md).
Bei bereits feststehender Überlänge wird höchstens bis zur deklarierten Länge
plus einem Byte bzw. zur jeweiligen Parserprofilgrenze gelesen: Quellenfehler-
priorität wird für den tatsächlich gelesenen Bereich nachgewiesen, keine
unbegrenzte Drainage beliebig langer bereits ungültiger Mediendaten behauptet.

Host32/GCC5.1/O0: Validator **31608 B**, Resultat **688 B**, Katalog-Metadaten
**252 B**, Quellmetadatenzähler **1656 B**, Program **336 B**, Legacy-Profile-
Scratch **28636 B**. Aufrufereigene Pools kommen hinzu und gehören nicht auf
einen kleinen Taskstack. Größter neuer einzelner statischer Frame **1232 B**
in `Validator::validate`, weitere 1200 B in `dependencies`/1104 B in `expanded`;
verschachtelte Compiler-/Backendaufrufe kommen hinzu. Keine ESP-Heap-, kumulierte
Stack- oder Latenzqualifikation.

Geprüfte SHA-256 nach Formatierung:
PackageArtifacts.cpp `d0dfcd88cef84fc951f8dcc8bbf3c2589e2432c8ebdbaef7d49ade2d90252788`,
PackageArtifacts.h `212d8ffff5ffa30b8232cb39da24a16fa20ea43ae08eef58efda21830fe7f3dc`,
PackageMetadata.cpp `146fceb7cd338e65555b94a7fe8559ad1eb73b9e0958dffb5faf1970ca8a1854`,
PackageMetadata.h `38c1039442b98e0db57c8682a421ea44760ba0d4932f4667996448dd7f9196c4`.
Veröffentlichter Commit und nachgelagerte PR-Nachweise werden im PR zugeordnet.

## Noch ausstehend

Native reale PSA-Targets für dieselben vollständigen Artefakt- und Metadaten-
prüfungen sind angeschlossen, ihr tatsächlicher SDK-/PR-Lauf ist bei Erstellung
dieses Commits noch offen. Der lokale SDK-Ordner mit abweichendem Sicherheitspin
wird nicht als qualifizierter Nachweis verwendet.

Vollständige Autorisierung/COSE, tatsächliche Gerätecapabilities, Medieninventar,
autorisierter Fallback mit Shared-Latch, dauerhafte D1-Planübernahme, F5a,
GM-Autorenschaft und reale Mehrbox-/Drittspielabnahme bleiben Anforderungen des
gesamten Ziels #93. Das Ergebnis des neuen Validators ist ausdrücklich
`IntegrityCheckedUnauthenticated`, kein aktivierter Spielbetrieb.
