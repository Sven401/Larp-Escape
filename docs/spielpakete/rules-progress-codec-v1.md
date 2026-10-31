# Vollständige Rules-Speicherblobs v1

Status: implementiert und unabhängig auf dem Host geprüft, 01.11.2026;
nativer PSA-Nachweis für diesen Stand noch offen. Bestandteil des
vollständigen manuellen Ersatzwegs in [Draft #135](https://github.com/Sven401/Larp-Escape/pull/135),
kein abgeschlossener Owner-/Authority-/F5a-Pfad. Root implementiert
`RulesProgressCodec`, `module_digest_review` prüft unabhängig die Byte-/Sicherheits-
grenzen; `quality_p1` führt davon getrennte unabhängige Integrationsgegenfälle.

Diese inneren Blobs werden vom [Bundlevertrag §7](p0-manual-override-v2.md)
vollständig gebunden. Unveränderte physische Journal-ABI, keine zusätzlichen
wirksamen Planfragmente oder Partitionen. Höchstens 32768 kanonische Blobbytes;
Kapazitätserschöpfung lehnt den vollständigen Blob ab. Das ist nicht das
P0-Quellmetadatenbudget und keine Zusage, dass jeder P0-Maximalplan hineinpasst.

## Darstellung

Deterministisches CBOR: kürzeste Integer-/Längenkodierung, definite Container,
feste Feldzahlen und Reihenfolge, keine Tags, unbekannten Felder, Duplikate,
Fließkommazahlen oder angehängten Bytes. Bool verwendet ausschließlich false/true.
Identitäten sind bstr16, Digests bstr32, öffentliche P-256-Koordinaten bstr64
mit X gefolgt von Y. Vorzeichenbehaftete Werte sind int32; Zähler behalten die
vollen uint64-Werte. Es werden keine C++-Speicherbilder oder Pointer gespeichert.

Ein Pool wird als Array seiner vollständigen Elemente gespeichert. Views in den
eigenen Epoch-/Evidencepool sind `[nullbasierterStart,Anzahl]`; eine leere View
ist kanonisch `[0,0]`. Grenzen werden ohne Pointerdifferenz fremder Objekte
geprüft. Eingabebytes, komplette Zielkapazitäten und sämtliche Zielpools sind
disjunkt. Rekonstruierte Zeichenketten, Epochen, Evidenzen und COSEbytes gehören
dem Aufrufer; kein Span verweist in die eingelesenen Blobbytes.

## Programmkatalog

Exakte Map mit Integerlabels 0 bis 15:

| Label | Inhalt |
| --- | --- |
| 0 | `larp.rules.catalog.v1` |
| 1 | Context |
| 2 | `[language,manualOverride]` |
| 3 | vollständiger Zeichenkettenpool als bstr, einschließlich Nullterminatoren |
| 4 | Stringoffsets |
| 5 | Nodes |
| 6 | Actions |
| 7 | Rules |
| 8 | Value-Symbole |
| 9 | Timer-Symbole |
| 10 | Ports |
| 11 | Epochpool |
| 12 | Subjects einschließlich eigener Epochviews |
| 13 | Ressourcen |
| 14 | Ressourcen-Allowlist-IDs |
| 15 | Trigger-IDs |

Feste Elementarrays, in dieser Reihenfolge:

- Context: `[event,run,station,box,sourceEpoch,deployment,binding,content,bindingGeneration]`.
- Location: `[offset,source,station,index,subindex,role]`.
- Node: `[location,op,type,value,ref,[alle acht children],count]`.
- Action: `[location,op,target,resource,expr]`.
- IR-Rule: `[location,owner,id,guard,firstAction,firstTrigger,timer,fallback,group,generation,priority,actionCount,triggerCount,trigger,policy,once]`.
- Rule: `[ir,name,stable]`.
- Symbol: `[name,type,initial]`.
- Port: `[name,type,input,exclusive,firstResource,resourceCount]`.
- Subject: `[name,subject,station,box,epoch,binding,epochView,factTypes,epochsComplete]`.
- Ressource: `[name,type]`.

Die Integerwerte der Enums entsprechen den ausdrücklich begrenzten C++-Enums
in `PackageModules.h`/`RulesPlanner.h`; unbekannte Werte werden verworfen.
Die zusätzliche SubjectBinding.name-Projektion muss null sein und wird nicht
als fremder Pointer erhalten. Vor erfolgreicher Katalogrückgabe läuft derselbe
vollständige Repräsentations-/Semantikvalidator wie im ProgramBuilder.
`Program.ready` bedeutet dabei nur vollständige interne Programmkonsistenz;
Aktivierung, authentifizierte Installation und heutige Rechte folgen daraus nicht.

## Anwendung

Exakte Map mit Integerlabels 0 bis 7:

| Label | Inhalt |
| --- | --- |
| 0 | `larp.rules.application.v1` |
| 1 | ursprüngliche Parent-/Anwendungsoperation |
| 2 | ursprünglicher Requestdigest |
| 3 | vollständiger gebundener Katalogdigest |
| 4 | vollständiger Plan |
| 5 | ursprüngliche signierte Faktenbelege |
| 6 | vollständige endgültige signierte Outbox in Emissionsreihenfolge |
| 7 | versionierte ursprüngliche Owner-Autorisierungsbelege als bstr |

Feste Elementarrays:

- FactId: `[event,origin,epoch,sequence]`.
- FactRef: `[id,payloadDigest,deployment,binding,run,station,subject,type]`.
- Fact: `[subjectIndex,ref,classification]`.
- Fence: `[progress,profile,trust,facts,ledger,resources,rights,capacity,lastIssued,activeTime]`.
- OverrideAuthority: `[peer,principal,roleEpoch,connectionToken,registration,lifecycle,connectionHandle,registryRevision,eventRevision]`.
- OverrideRecord: `[context,program,expected,authority,ownerBoot,token,prepareId,confirmId,operation,rule,prepareDigest,confirmDigest,original,replacement,originalGeneration,replacementGeneration,preparedAt,expiresAt,confirmedAt]`.
- Input: `[context,kind,bootGeneration,sequence,port,timer,armGeneration,type,known,value,fact,rawOverrideRecord]`.
- Receipt: `[context,input]`.
- Plan: `[context,expected,receipt,consumedTimer,evidencePool,values,timers,latches,emissions,intents,admitsFact,fact]`.
- ValueWrite: `[target,type,value,evidenceView]`.
- TimerWrite: `[target,active,deadline,generation,evidenceView]`.
- Latch: `[rule,generation,evidenceView]`.
- Emission: `[subjectIndex,rule,generation,candidateId,dependencyView,type]`.
- Intent: `[port,resource,rule,exclusive,evidenceView]`.
- SignedFact: `[FactRef,originalPublicXY,originalCOSEbytes]`.

COSE ist je Beleg höchstens 2048 Byte und niemals leer. Die Faktenbelege enthalten
je Fact-ID höchstens einen konsistenten Beleg und decken alle Planevidence sowie
den tatsächlichen Fact-Eingang ab. Die vollständige Abhängigkeitsabschlussmenge
aus den echten COSEpayloads bleibt zusätzlich vom tatsächlichen Consumer zu prüfen.
Outboxzahl ist exakt Emissionszahl, höchstens acht; Reihenfolge, IDs, Typ und
lokaler Kontext müssen übereinstimmen. Payload- und Envelopehash sind nicht
austauschbar. Der Codec erfindet keinen erneuten Signieraufruf beim Wiederholen.

## Keine Admission aus gespeicherten Projektionen

Beim Schreiben muss der rawOverrideRecord mit der tatsächlich vom Owner erzeugten
kopierten Admission übereinstimmen. Bei anderem Eingang ist er vollständig leer.
`program` bindet den tatsächlichen vollständigen kanonischen Katalogdigest und
muss mit dem Katalogdigest der Anwendung übereinstimmen; Contextgleichheit allein
genügt nicht. Der Owner muss den tatsächlichen aktuellen Katalog erneut hashen,
statt einen vom Aufrufer behaupteten Fingerprint zu übernehmen.
`eventRevision` bindet zusätzlich die tatsächlich verifizierte Eventzuordnungs-
generation; ein späteres Schließen und erneutes Zuordnen darf auch bei gleicher
Veranstaltung und gleichem Register keinen alten Grant wieder öffnen.
Zwischen ursprünglicher Prepare-Fence und privater Planungsfence darf allein
`activeTime` monoton fortschreiten. Alle Revisionen und `lastIssued` bleiben
gleich; der ursprüngliche Record wird nicht umgeschrieben. Diese reine
Darstellungsrelation ist kein Uhrennachweis: der tatsächliche Owner prüft den
separaten ursprünglichen ClockWitness gegen vorherigen dauerhaften Zustand,
Boot, monotones Zeitdelta und Betriebsmodus, bevor die Übernahme möglich ist.
Beim Lesen entsteht ausschließlich der rohe historische Record; die Admission
im Input bleibt leer und `Plan.ready` false. Ein öffentlich konstruierbarer
Codecwert darf niemals Prepare/Confirm oder eine neue GM-Autorisierung ersetzen.

`DecodedUnauthenticated` bestätigt nur vollständige kanonische Darstellung und
die beschriebenen Konsistenzprüfungen. Ursprüngliches COSE und ursprüngliches XY
beweisen selbst nach tatsächlicher Signaturprüfung keine damalige Origin-/Typ-/
Eventberechtigung. Der tatsächliche Owner muss für **jeden** genutzten Fakt die
ursprünglichen Rechte-/Binding-/Installationsbelege aus einer dauerhaft gebundenen
Quelle nachweisen. Die opaque Autorisierungsbytes sind ein Transportfeld, keine
Freigabe. Für Override müssen unter anderem ursprünglicher Principal, Verbindung,
Rollen-/Eventstand, Prepare-/Confirmidentitäten, Fences und feste Fristen vollständig
belegt sein. Lokale/Timerherkunft folgt aus dem vorherigen dauerhaften Ownerzustand.

Erst vollständige mathematische/historische Prüfung, erneute Ableitung aller
Planänderungen und Abgleich des einzigen endgültigen Fact-ID-Bereichs gestatten
semantische Recovery. Auch dann bleibt historische Receiptwiederherstellung von
einer neuen live ausführbaren Admission getrennt. Alle Blobprüfungen sind bis zur
abschließenden gemeinsamen Recoverypublikation vorläufig.

## Tatsächlicher Prüfstand und offene Nachweise

Eigene strenge C++11/Wpedantic/Werror-Kompilierung und 5683 Darstellungsprüfungen
an leerem Katalog/lokalem Plan: sämtliche kürzeren Präfixe, Kapazitäten, Reuse,
uint64-Maximalwerte, INT32_MIN, Alias-Frühfehler mit unveränderten Altflags/-counts,
abgelehnte Outbox-Doppel-/Überlaufbereiche und leere Decode-Admission. Das sind keine echten
Compiler-/Planer-/Signatur-/Ownerpositiven und keine unabhängige Freigabe.

Unabhängige Katalogvorstufe durch `quality_p1` am 31.10.2026: alle 61 vollständigen
Artefaktsätze erneut validiert, daraus 20 echte Programme durch vollständigen
Encode/Decode und ein eigenes dokumentbasiertes Python-CBOR-Orakel geprüft.
80 gespeicherte Gruppenmanipulationen wurden abgelehnt. Nach Zerstören originaler
CBOR-, String-, Node- und Epochpools bleiben rekonstruierte Views/Bytes und
erneute exakte Kodierung korrekt; 120 C++-Prüfungen. Dies qualifiziert die
Katalogvorstufe am Codecbody `fbe555bb5e47c200c8a867f369f7671f088c4c0f2ccfdc4fb8097fbd225a37bc`,
noch keine vollständige Application-/Ownerintegration. Die 20 Artefaktfixtures
ergeben 816 bis 2570 Katalogbytes; das sind noch nicht die beiden unveränderten
P0-Referenzspiele oder ein belegtes Geräteprofil.

Nachgelagerte Applicationvorstufe durch `quality_p1`: sechs unveränderte P0-Traces
mit 24 vollständigen Zustandsvergleichen, daraus 20 tatsächliche Ready-Pläne durch
Encode/Decode und eigenes dokumentbasiertes CBOR-Orakel. 12 COSE-Beleg-/Outbox-
Vorkommen entstehen tatsächlich über D0-Writer, CNG-Signierer und Decoder/Verifier;
Python-P256 prüft Signaturen/OriginalXY/kid/AAD/Payloaddigest unabhängig.
Authorityarray9 und Overridearray19 sind enthalten; keine positive OverrideAdmission.
Kataloge der beiden Referenzspiele: 872 bis 1704 Byte; Planblobs: 1582 bis 4813 Byte.
Decode bleibt nicht ausführbar und kopiert Originalbytes/Evidence in eigene Pools.
Vollständige beschädigte Eingaben, Aliase/Kapazitäten/Reuse, native PSA und echte
Owner-/Trust-/Durabilitätsintegration sind damit noch nicht abgenommen.
Gesamtbudgets und Gerätedurabilität bleiben gesondert zu prüfen. Kein Upload,
Release, main-Merge oder Abschluss von #93.
