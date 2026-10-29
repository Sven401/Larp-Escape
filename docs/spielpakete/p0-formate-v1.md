# P0 v1: Inhaltsformat, Modulvertrag und Deployment

Status/Quelle/Basis wie im [Hauptvertrag](p0-vertrag-v1.md). Dies sind neue
zu implementierende Formate; bestehende Profil-, D0- und BLE-Codecs bleiben
unverändert. Die unten stehende Grammatik ist der P0-Vertrag, kein bereits
implementierter Parser und kein neuer Transportkanal.

## 1. Kanonische Bytes und Identitäten

Alle drei neuen JSON-Formate verwenden dasselbe eingeschränkte JSON-Profil:
UTF-8 ohne BOM, genau ein Objekt, keine nachfolgenden Bytes, keine doppelten
oder unbekannten Felder. Objectkeys lexikografisch nach ihren ASCII-Bytes;
keine Leerzeichen außerhalb Strings. Arrays behalten ihre Reihenfolge.
Unicode-Strings **nicht normalisieren**: gleiche sichtbare Zeichen
mit anderen Codepunkten sind andere Bytes. Strings enthalten gültiges UTF-8
und keine C0-Zeichen; nur quote und backslash werden als `\"` bzw. `\\`
escaped; andere Zeichen stehen direkt in UTF-8. Zahlen sind ausschließlich
Integer in kürzester Dezimalschreibweise, ohne Plus, führende Nullen oder -0.
Keine Floats, Exponenten, null oder optionalen unbekannten Erweiterungsfelder.
Bezeichner/Schlüssel sind ASCII, Labeltexte dürfen UTF-8 verwenden.

Ein Import darf nichtkanonische gültige Entwurfsdaten darstellen, aber erst der
Validator darf nach strengem Feld-/Typ-/Duplikatcheck kanonisch exportieren.
Hash/Übernahme beziehen sich ausschließlich auf diese kanonischen Bytes.
P1 benötigt bytegenaue unabhängige Codecvektoren; der semantische P0-Prüfer
beweist keine Parser-/Signatur- oder Übertragungsfunktion.

`contentDigest = SHA-256(contentBytes)`, `moduleDigest = SHA-256(moduleBytes)`;
Digests stehen als 64 kleine Hexzeichen. Ihre jeweiligen Objekte enthalten
keinen eigenen Digest. Ressourcen werden über vollständige unveränderte Bytes
gehasht. Anzeigenamen und Semverstrings sind keine Identitäten.

| Artefakt | Wurzelfelder (alle erforderlich) | Grenze / Inhalt |
| --- | --- | --- |
| Inhalt | `format`, `packageId`, `revision`, `label`, `requires`, `modules`, `stations`, `subjects`, `resources` | format=`larp.package.v1`, packageId stabiler ASCII-Bezeichner, revision uint32>0; kein event/run/Boxschlüssel |
| Modul | `format`, `moduleId`, `version`, `label`, `parameters`, `inputs`, `outputs`, `values`, `timers`, `rules`, `instances`, `constraints`, `resources` | format=`larp.module.v1`, version exakt major.minor.patch aus uint16; keine Rekursion |
| Deployment | `format`, `event`, `run`, `contentDigest`, `stations`, `profiles`, `resources`, `rules`, `dependencies`, `requires` | format=`larp.deployment.v1`; vollständige unten benannte Bindungen; kein eigener packageDigest |

ID-indizierte Definitionslisten werden nach ihrer ID gespeichert; doppelte IDs sind
ungültig. Zusammengesetzte Deploymentlisten werden nach (station,id) bzw.
(station,ruleId,originStation,subject) sortiert; profiles nach station/path.
Aktions-, Argument- und Instanzverdrahtungsreihenfolgen werden nicht umsortiert.
Eine leere Liste ist explizit `[]`, kein fehlendes Feld. `requires`
ist eine nach `name` sortierte Liste aus `{name,version}` mit exakter positiver
Majorversion, keine Versionsbereiche in v1. Module sind über
`{moduleId,version,digest}` vollständig gepinnt und lokal im Distributionssatz
enthalten; kein Nachladen über Netzwerk beim Spielen. Gleiche ID/Version mit
anderem Inhalt ist Konflikt. Hashes und Versionswerte werden beide geprüft.

## 2. Stations- und Sprachgrammatik

`stations` enthält `{id,label,values,timers,ports,rules,instances}`. Subject:
`{id,originStation}`. `subjects` deklariert ausschließlich positive monotone
Erfolge; dauerhafte beliebige Remote-Integer sind nicht Teil dieser Sprache.
Ein Port enthält `{id,direction,type,capability}`; direction input/output,
type boolean_sample/int32_sample/light/audio. Die im Deployment überprüfte
Komponente implementiert die Capability; der Inhalt kennt weder GPIO noch MAC.

Werte: `{id,type,initial}` mit bool/int32. Timer: `{id,durationMs}` mit uint32
1..86400000. Ressourcen: `{id,kind,digest,bytes}`, kind light_preset/audio,
bytes uint32>0. Audio meint Mediendateiinhalt, light_preset die gehashten
Parameterbytes einer tatsächlich unterstützten Lichtoperation. Ein Preset
erfindet keine neue Effektprimitive. Pro Port sind die zulässigen Ressourcen
im Deployment explizit angegeben; fehlende Ressource sperrt Aktivierung.

Regel: `{id,generation,mode,priority,trigger,guard,offline,actions}`.
generation uint32>0; mode once/each; priority uint16. Trigger ist exakt
`{kind:"local",ports:[ID...]}`, `{kind:"timer",timer:ID}` oder
`{kind:"fact",subjects:[ID...]}`. Facttrigger passt auf einen Eingang aus der
aufgezählten Menge; Guard überprüft die gesamte benötigte Menge. Lokale
Verwaltungsersatzwege laufen über dieselbe Freigaberegel nach überprüftem
local_override, keine ungeschützte `admin=true`-Eingabe im Paket.

Offline ist `{policy:"wait"}`, `{policy:"local_continue"}` oder
`{policy:"local_fallback",rule:ID}`. Bei rein lokalen Regeln ist local_continue
erforderlich; Ersatzregel muss dieselbe fachliche ruleId-Freigabe adressieren
und einen separat autorisierten local_override-Eingang produzieren.

Ausdruck ist ein Objekt mit genau den zum op gehörenden Feldern:

| op | Weitere Felder | Bedeutung |
| --- | --- | --- |
| literal | `type`, `value` | bool/int32; keine Strings als Laufzeitwerte |
| value | `id` | Deklarierter Stationswert |
| input | `field` | `value` bei local; alle Ports eines local-Triggers haben denselben Werttyp |
| port_is | `id` | Bool: aktueller lokaler Eingangsport entspricht dieser ID |
| fact | `subject` | Deklarierter aktueller Erfolg, Herkunft und Kontext vorgeschaltet geprüft |
| not | `arg` | Bool/unknown |
| all, any | `args` | 2..8 Boolausdrücke |
| eq, lt, le, gt, ge, add, sub | `left`, `right` | Typen und Semantik gemäß Hauptvertrag |

Actions: `set`/`add` mit `{op,id,expr}`, `arm`/`cancel` mit `{op,timer}`,
`emit` mit `{op,subject}`, `effect` mit `{op,port,resource}`. Timerdauer stammt
aus der Deklaration, nicht aus einer dynamischen Aktion. Ein Effectport
verlangt die passende Ressourcenart. Unbekannter op oder falsche Feldmenge
wird abgewiesen, auch in ungenutzten Modulzweigen.

## 3. Zusammengesetzte Module und Bearbeitungsbeschreibung

Parameter: `{id,type,label,unit,min,max,default,description}` für int32;
für bool genau `{id,type,label,default,description}`. Einheiten sind
`none`, `count`, `ms`; Bereiche werden einschließlich Default typgeprüft.
Modulinputs/-outputs: `{id,type,label,description}` mit denselben Porttypen
oder `solved` für ein boolesches dauerhaftes Subject. Für solved gilt kein
beliebiges logisches Signalnetz; Ausgabe ist emit, Eingang ist D0-Fakt oder
lokal dauerhaft übernommenes Subject. Modulgrenzen erhalten D1-Atomarität.

Modul-`resources` enthält `{id,kind}` als logische Ressourcenslots; eine
Effectaktion referenziert dort die Slot-ID. Instanz-`resources` ordnet jeden
Slot vollständig einer konkreten Inhaltsressource passender Art zu. Bei der
Expansion wird die ID ersetzt. Modul-eigene ports sind die deklarierten inputs
und outputs; Regeln referenzieren deren IDs, Effectaktionen einen Outputport.
Keine heimlichen Geräte-/Ressourcenbezüge außerhalb dieser Deklarationen.

Instanz: `{id,moduleId,version,digest,parameters,inputs,outputs,resources}`.
parameters ist die vollständige Map paramId→typisierter Literalwert,
inputs/outputs vollständige Maps Port-ID→Stationsport-/Subject-ID. Fehlende,
zusätzliche oder falsch gerichtete Zuordnung ist Fehler. Ein Modul darf
andere Instanzen enthalten; DAG wird vor Aktivierung expandiert. Qualifizierte
interne IDs sind Instanzpfad + lokale ID, deren dauerhafte Zuordnung zur
ruleId liegt im Deployment. Instanzumbenennung ist deshalb eine Migration.
Keine dynamische Instanzerzeugung, Rekursion oder ungebundene Abhängigkeit.

`constraints` ist eine Liste reiner boolescher Ausdrücke aus parameter,
literal, not/all/any, Vergleichen und add/sub. Keine value/input/fact/port_is-
Blätter in Constraints. Jede Bedingung muss nach Parameterersetzung true sein;
unknown/Überlauf/false lehnt die Instanz ab. Damit wird first!=second als
not(eq(parameter(first),parameter(second))) geprüft, ohne moduleId-Sonderfall.

Innerhalb einer Moduldefinition darf `{op:"parameter",id:ID}` einen Literal-
Ausdruck und `{parameter:ID}` einen deklarierten Dauer-/Schwellwert ersetzen.
Expandierung ersetzt diese Stellen typgeprüft durch Literalwerte. Andere
Textsubstitution, native Funktionen und semantische Defaults sind verboten.
Label, Beschreibung und Einheit beeinflussen ausschließlich Bearbeitung;
Guard/Actions bleiben die alleinige Ausführungsbedeutung.

GM soll diese Typen mit vorhandenen Zahlen-, Auswahl-, Wahrheitswert- und
Portzuordnungsfeldern vollständig bearbeiten können. Fehler erhalten Objektpfad,
Fehlercode und lesbare Ursache. Ein fehlendes Pflichtfeld wird nicht durch eine
Vorlage erraten. Visuelles Layout, TalkBack und tatsächlicher Appweg benötigen
P1c samt bestehendem UI/UX-Rollenvertrag; P0 entwirft keine neuen Screens.

Die zwei Referenzen benötigen wiederverwendbare Module `threshold`,
`ordered_window` und `all_solved`: Parameter/Ports/Innenregeln und Beispiele
stehen im [Referenzvertrag](p0-referenzspiele-v1.md). Neue Kombinationen dieser
Operationen dürfen keine Fallunterscheidung nach moduleId erzeugen. Auch die
mitgelieferten Module durchlaufen denselben Expandierer wie Nutzerbausteine.

## 4. D0-kompatibles Deployment und Ressourcen

D0 `packageDigest` ist **SHA-256(deploymentBytes)**, nicht contentDigest.
Das Manifest enthält event/run (je nichtnull 128 Bit, 32 kleine Hexzeichen),
für jede logische Station vollständige Box-ID, Stations-ID, Bindungsgeneration,
Bindungsdigest und erlaubte Ursprungsepoche gemäß D0. `stations`-Eintrag:
`{id,stationId,boxId,generation,bindingDigest,bindingPath,ports,subjects}`.
bindingPath referenziert die vollständig mitgelieferten deterministischen
D0-CBOR-Bindungsbytes `{0:1,1:event,2:station,3:generation,4:box,5:rulesetId,
6:[epoch...],7:[[factType,subject]...]}`. bindingDigest wird darüber geprüft;
alle JSON-Duplikatfelder müssen dem CBOR-Inhalt entsprechen. Listen erlaubter
Epochen und Erlaubnisse sind vollständig zu prüfen, nicht aus dem Digest zu
erraten. Diese Bytequelle hat denselben sicheren relativen Pfadvertrag wie
profiles; sie ist selbst über bindingDigest in deploymentBytes gebunden.
ports ist eine Liste aus `{id,componentId,channel,resources}` mit componentId
1..65535, channel uint16 und resources sortierten Ressourcen-IDs. Keine
Signaltransformation; channel hat die vom konkreten Capabilityvertrag
festgelegte Bedeutung, etwa Matrixposition oder Wakeeingang. subjects ist
`[{id,subjectId}]`, subjectId nichtnull 128 Bit als kleine Hexzeichen.
ports bindet logischen Port an lokale Komponenten-ID und zulässige Ressourcen;
subjects bindet Inhalts-ID an nichtnull D0-Subject-ID (128 Bit). Eine Quelle
kann ausschließlich ihre eigenen Subjects erzeugen. Das Installationsmanifest
ist für alle beteiligten Boxen identisch; jede prüft ihren Ausschnitt und die
zur Prüfung externer Abhängigkeiten nötigen Gegenstellenbindungen.

profiles: `{station,path,digest,bytes,format}`; Ressourcenliste enthält
`{station,id,path,digest,bytes,locator,verification}`. station/id Bezeichner,
bytes uint32>0, digest SHA-256-Hex; format larp.rules.v1 oder profile.v1.
locator ist `{bank,track}` mit bank/track uint16>0 für Audio, oder
`{preset}` mit preset uint16>0 für Licht; keine Code-/Pfadausführung.
verification ist unten benanntes Enum. Pfade sind relative
ASCII-Pfade ohne leere Segmente, Laufwerk, Backslash, `.` oder `..`; Locator
ist nur eine Gerätezuordnung, kein fremd auszuführender Pfad.
`rules` enthält `{station,expandedId,ruleId,generation,offline}`;
`dependencies` `{station,ruleId,originStation,subject}` mit vollständiger
statischer Herkunfts-/Abhängigkeitsliste. Das Manifest bindet außerdem die
vollständigen neuen IR-Stationsbytes als profiles mit format larp.rules.v1;
Profil-v1-Dateien können zusätzlich als gekennzeichnete Legacydaten enthalten
sein, werden aber nicht automatisch mit dem neuen Kern parallel ausgeführt.

Format einer expandierten Stationsdatei ist genau
`{format,station,values,timers,ports,rules}` mit format larp.rules.v1 und ohne
instances/parameter-Ausdrücke. Keine Modulmetadaten im Laufzeitspeicher nötig.
Der Validator rekonstruiert die Expansion aus dem Inhalt und vergleicht die
kanonischen Bytes/Hashes; ein fremder Compiler darf keine versteckte Logik
hinzufügen. Deploymentmanifest bindet Expansion und Quellinhalt gemeinsam.

DFPlayer: Locator z.B. bank/track ist keine Inhaltsprüfung. verification ist
`device_readback` oder `operator_inventory`. Ohne Geräte-Rücklesen sind
Dateihash/Zuordnung aus belegter Medienvorbereitung und bewusste Bestätigung
pro Box nötig; der Status zeigt deren schwächere Aussage. Fehlendes Inventar
oder unbestätigte Zuordnung sperrt Audioaktivierung. Der App-Upload auf die
SD-Karte wird nicht vorausgesetzt. `bytes` meint Mediendateigröße, nicht
Übertragungsbudget; große Audiodateien werden nicht im ESP-Heap gehalten.

Veröffentlichen, Installieren, dauerhaft eingerichtet, aktiv und running sind
getrennte Zustände je Ziel. Keine atomare Mehrboxaktivierung. Das neue Format
verlangt #30/#28, echte D0-Verifikation sowie ausdrückliche Capability; kein
Einfügen in reservierte V2-Bytes. Unvollständige Daten bleiben Entwurf.
Bei verlorenem Ergebnis zuerst denselben Vorgang nachlesen, nicht blind neu
starten. Alle Signaturen/Trustprüfungen bleiben im bestehenden Sicherheits-
und Verwaltungsvertrag; ein Inhaltsdigest verleiht keine Aktivierungsrechte.
