# Semantischer Rules-Fortschrittsbesitzer

Status: **Ownerbasis und Local-/Timer-Folgekorrektur Host/nativ geprüft;
Produktanschlüsse offen**,
01.11.2026. Dieser Folgecheckpoint ergänzt Owner-/Orderingquellen in
[Draft #135](https://github.com/Sven401/Larp-Escape/pull/135).
Dieser Text beschreibt den zu prüfenden Anschluss der bereits qualifizierten
[Codecs](rules-progress-codec-v1.md), [signierten Belege](rules-signed-evidence-v1.md)
und [Effektbelege](rules-effect-codec-v1.md); er ersetzt keinen Laufzeitnachweis.
Führender Entwickler: developer_p1a3; unabhängige Gegenprüfung: quality_p1,
gezielte Sicherheits-/Bindungsprüfung: module_digest_review; Koordination: Root.

## Besitz und ursprüngliche Quellen

`rules_progress::Owner` koordiniert Journal, Bundles, Planer, manuelle
Transaktionen, Lifecycle und Intentübergabe auf einem exklusiven synchronen
Worker. Quellen und Arbeitsbereiche bleiben während eines Aufrufs exklusiv;
Callbacks dürfen keine ausgeliehenen Views behalten. Drei getrennte
Zustandsbänke schützen den letzten veröffentlichten Zustand während Replay.
`state()` bleibt eine Diagnose des zuletzt veröffentlichten Zustands und ist
selbst keine aktuelle Eingabe-, F5a-, D2- oder Ausführungsberechtigung.

Tatsächlich ursprüngliche Installation, Ursprungsschlüssel, Eingabe, akzeptierte
Autorisierung und Boot-/Zeitquelle sind Pflichtanschlüsse. Der belegte
PeerProof, heutige Registryeinträge, ein Geräteschlüssel oder ein generisches
GM-Flag ersetzen diese Quellen nicht. Fehlende konkrete Produzenten bleiben
geschlossen. Die Testanschlüsse für Installation, Rechte und Zeit sind als
synthetisch zu kennzeichnen; echte mathematische Signaturprüfung macht daraus
keine Produktberechtigung.

## Vollständige Wiederherstellung

Nach tatsächlicher Backendinitialisierung oder qualifiziertem Wiederöffnen und
Lowlevel-Recovery baut `recover()` alle ursprünglichen Kataloge, Anwendungen,
Controls und Effektbelege vorläufig wieder auf. Historische Belege werden aus
ihren gespeicherten Originalbytes geprüft. Eine aktuell fehlende Berechtigung
löscht keine gültige Historie; historische Gültigkeit aktiviert umgekehrt keine
heutigen Rechte. Auch ein Fehler spät im Journal verhindert die Veröffentlichung
eines früheren scheinbar gültigen Teilbestands über Status oder Duplikatpfade.

`SerializedOrdering` hält relevante Metadatenmutationen während der gesamten
Rekonstruktion geschlossen. Ein Recovery-Pin enthält Boot, Generation und
positiv gelesenes Inventory samt gegebenenfalls alter exakter Lease. `Empty`
ist ein eigener positiver Zustand; fehlende Daten bedeuten `Unavailable`.
Nach vollständiger Rekonstruktion prüft der Owner den unveränderten Pin und
bereitet dessen Auflösung fallibel vor. Erst danach erfolgt die vorbereitete
RAM-Veröffentlichung und die callbackfreie, nicht fehlschlagende Veröffentlichung
der Orderingmetadaten. Ein Abbruch hält die Recoverypflicht und eine alte
ungeklärte Lease aufrecht.

Eine gehaltene Operation wird nur anhand ihrer tatsächlichen Art, Kennung,
Parent-, Request-, Katalog-, Kontext-, Fence- und Raw-Commitmentbindung aufgelöst.
Ein vorhandener Claim belegt keinen Result-Commit. Ein inertes Katalogbundle
belegt keine Anwendungsübernahme. Nichtcommit darf nur aus der vollständigen
Rekonstruktion und einem passend unveränderten Ausgangszustand folgen.

## Dauerhafte Anwendung und Ausführung

Vor dem ersten Schreibversuch müssen vollständiger Plan, signierte Ursachen,
Envelope sowie alle später benötigten Claim-/Resultplätze in die konkreten
Budgets passen. Höchstens 32768 Bytes pro semantischer Bundletransaktion und
16 Chunks plus Final ändern weder die 3072 Payloadbytes pro physischem Record
noch das Profil mit 63 Records. Es gibt kein implizites Reclaim, Löschen oder
Vergrößern einer Partition.

Erst ein qualifiziert bestätigtes Application-Final erlaubt die vorbereitete
fachliche RAM-Übernahme. Original und manuelle Ersatzfreigabe verwenden den
gleichen fachlichen Once-Latch. Ein bestehender NoRelease-Beleg wird durch ein
späteres Original nicht umgeschrieben. Die signierte Outbox benötigt weiterhin
einen eigenen aktuellen D0-Anschluss für eine produktive Nutzung.

Eine unveränderte normale Anwendungswiederholung nennt den ursprünglichen
Finalstand. Auch ein inhaltlich identischer bereits verarbeiteter Eingang mit
neuer Korrelationsoperation nennt seine ursprüngliche Anwendung über
`Result.originalApplication`, deren physische Revision, Index und Factrange.
Die neue Korrelation und ihr Request werden dadurch weder gespeichert noch
als neue übernommene Operation bestätigt. Es entstehen keine neue Planung,
Signatur, Lease oder Wirkung. Facts verwenden die globale D0-FactId. Local
verwendet Bootgeneration/Folge, Timer die Timer-ID/Arm-Generation, jeweils
innerhalb event/run/station. Dieselbe Eingangsidentität mit geändertem vollständigem
Input oder eine mit fremden Phasen kollidierende Anfragekennung ist ein
Konflikt. Diese Antworten setzen vollständig veröffentlichte/recoverte Historie
und passenden aktuellen Kontext voraus; ein unbekannter Ausgang wird dadurch
nicht zu einem Duplikaterfolg umgedeutet.

Die aktuelle Zulässigkeit des ursprünglichen Auftrags wird vor dem Claim,
nochmals im tatsächlichen Claim-Commitcallback und nach dessen dauerhafter
Bestätigung unmittelbar vor der lokalen Intentübergabe geprüft.
Ein anderer heutiger Akteur oder eine neue Verbindung darf ihn nicht übernehmen.
Claim und Result verwenden ihre getrennten reservierten IDs. Die exakte Lease
wird vor dem Result-Schreibversuch ohne offene Zwischenphase von Claim auf
Result weitergegeben; ein unklarer Result-Ausgang bleibt dadurch an diesen
Result-Versuch gebunden. Wiederherstellung übergibt einen bereits beanspruchten
Intent niemals automatisch erneut. Eine ausdrücklich zulässige spätere
Unknown-Abrechnung erzeugt keine zweite Geräteübergabe.

`HandlerReturned` bezeichnet die Rückkehr des lokalen Handlers und keine
physische Beobachtung. Ein Gerätefehler rollt bestätigten fachlichen Fortschritt
nicht zurück. Signierter Erfolg, dauerhafter Commit und beobachtete Wirkung
bleiben unterschiedliche Nachweise.

## Prüfgrenzen

Die unabhängige Runtimekampagne hat auf dem aktuellen eingefrorenen Ownerstand
tatsächlich 252 C++-Assertions und zehn direkt ausgegebene COSE-Belege mit
unabhängiger Python-P-256-Prüfung bestanden. Der strenge C++11/O2/Werror-Link umfasst
16 Produktquellen einschließlich ManualOverrideOwner, Coordinator, SignedEvidence,
D0-Writer/-Verifier, Planer und tatsächlichem Journal-/Bundlepfad mit CNG.
Die Fälle umfassen beide Original-/Ersatzreihenfolgen, genau eine fachliche
Freigabe/Intentreservation, NoRelease mit späterem Original und unverändertem
altem Receipt, Claim vor genau einem Handleraufruf, Result-Unknown mit exakter
Result-Lease sowie neue Besitzer-/Backendinstanzen. Auch ein neuer Boot und
RestoreStopped zwischen Claim und Unknown-Abrechnung verändern ursprüngliche
Claim-Fence und Boot nicht. Kalte Historie verwendet keine heutigen Quellen;
der ursprüngliche Fact-COSEcache wird im Duplikatfall tatsächlich zerstört.

Gegenfälle prüfen späte semantische Replaykorruption ohne Status-/Outboxpräfix,
Entzug/Fristablauf und teure Callbacks vor Final, fehlende ursprüngliche Quellen,
separaten Control-Preflight, Kapazität vor dem ersten Schreiben, streng
nachgewiesenes prewrite Denied sowie tatsächliche Unsicherheit bei null oder
teilweise geschriebenen Bytes. Teilrecords bleiben geschlossen; eine neue
Objektinstanz auf dem alten Schreibcache ist kein Recoverybeleg. Neue
Factkorrelationen nennen ausschließlich den ursprünglichen Finalstand, während
abweichende Inputs und fremde Phasenkollisionen scheitern.

Zwei vollständige Artefaktsätze durchlaufen zusätzlich den tatsächlichen
Artefaktvalidator, Compiler und Programmkatalogcodec mit festem vollständigem
Byte-Sollabgleich. Sieben tatsächliche ursprüngliche Transaktions-/Katalogblobs
werden unabhängig kanonisch dekodiert/rekodiert; dabei werden Domain, Op/Request,
Kataloghash, Context, FactRef, Payloaddigest und neun eingebettete COSE-Vorkommen
geprüft. Diese neun Vorkommen sind keine neun zusätzlich verschiedenen Signaturen.
Frische und erhaltene Runtimebeobachtungen werden jeweils so geprüft; zwischen
beiden Läufen wird nur die geordnete Beobachtungsnamenliste verglichen. Das ist
kein vollständiger semantischer Sollbaumvergleich jedes frischen Plans. Harte
Erwartungen zu Zustand, Once, Release, Revisionen und Wirkungsfolge stehen
getrennt im unabhängigen C++-Korpus.

Die tatsächlich ausgegebenen Transaktionen messen 1776 bis 3872 Bytes, ihre
Anwendungen 1653 bis 3713 Bytes. Das sind Hostbeispiele mit synthetischen kurzen
Autorisierungsannotations, keine Größe vollständiger produktiver Nachweise oder
Zusage für alle Sprach-/Gerätegrenzen.

Installation, Eingabe, Control, GM-/Intentzulässigkeit und Zeitquelle sind
ausdrücklich synthetische Testports. Der folgende nachgelagerte native Lauf
ergänzt diesen Hostbeleg; produktive Quellen werden dadurch nicht qualifiziert.
[Unabhängiger Korpus und Reproduktion](../../test/fixtures/rules-progress-owner-review/README.md).
Geprüfte Owner-SHA256: Header
`745d5a7ca4aaa346f08ae71c744b960e7f3bb57ac74205c7843ea10c7d62d438`,
Body `43518a73805cf03c2b66815f8fa9b75ebc80913a11e99a41078f1ff0d10ed11b`.

Für den neuen Fact-Korrelationsfall wurde der byteidentische vorherige Body
`0e5f2400…` mit nachweislich zuvor übernommener Originalanwendung tatsächlich
rot geprüft. Davon getrennt ist eine frühere Retry-Rotbehauptung zurückgenommen:
Dort war die erste normale Aufnahme wegen eines falschen Test-Subjectindex
nicht ausreichend belegt. Sie zählt nicht als Vorfixnachweis.

Die unabhängige Orderingkampagne führt auf dem aktuellen Teilfreeze tatsächlich
830 Assertions aus: vollständige Bindungs-/Lease-/Pinvergleiche, Reentry,
Unknown-Hold, Prepare/Publish/Abort sowie die neue Claim→Result-Weitergabe mit
frischer Generation und abgewiesener alter Claim-Lease. Metadaten sind dabei
synthetisch; diese Tests führen keinen vollständigen Owner-Replay aus.
[Quellen und Reproduktion](../../test/fixtures/rules-commit-ordering-review/README.md).
Geprüfte SHA256: Header
`1ba658f6c797a28c952796d4e35ca831c5adfe0990628dde4deadfe47451c5a7`,
Body `9d6861b8b10e33a03126fcc98e22b73374d90f1a2db49826b963f7caf8a6ec78`.

### Tatsächliche native Qualifikation des veröffentlichten Heads

Head `ee5c87a541088fe734559ea010355a8aac343527` ist mit zwölf PR-Jobs in sechs
Workflows unabhängig qualifiziert. Alle zwölf Originalcheckoutlogs bestätigen
Prüfmerge `f02e033ef1183c3d0654ed348856554bb44ad106`, dessen Eltern exakt
Zielbasis `c4f1f824c6274e9a8d7676212a2a96be4d4a77b9` und Head sind.
pull_request, Attempt 1, Actions-App und abschließend unveränderte Refs bestätigt.

[Native 41263905608](https://github.com/Sven401/Larp-Escape/actions/runs/41263905608),
Originaljob 124722581457: **90/90 CTests ohne Skips/Disabled**. Der neue
`rules-progress-owner-review-psa` führt tatsächlich **300 Assertions** aus:
252 gemeinsame Fälle plus 48 PSA-Setup-/Cleanupprüfungen. Beide vollständigen
Artefaktsätze, zehn direkte und neun eingebettete COSE-Vorkommen sowie sieben
kanonische Blob-/Bindingprüfungen laufen im selben vollständigen Runner mit
verpflichtendem echtem `--artifact-binary`; kein verkürzter Modus oder neues
Goldenschreiben. Ordering führt separat seine 830 Assertions aus.

Tatsächliche Digest-, Verifier- und Signeradapter am Ownerziel sowie Owner/
Ordering und die Adapter im Coreimage sind anhand der Originalbuildlogs und
project_description bestätigt. Beide SDK-Images bauen am Pin
`9a97f6c54ec638111ce55cd36581b3c192f15207` mit rekursiven Submodulen.
Originalarchiv 13066732163, SHA256
`bebe28fb77b11797a9e5c4e6fa6592e00e83e0dfee1e275d9931f5bbda1085c1`,
lokal `private/pr135-ee5c87a5-quality`.

`quality_p1` prüfte diesen tiefen nativen Korpusbeleg; `module_digest_review`
prüfte getrennt alle zwölf Jobs, ursprünglichen Checkouts und Refs. Die
anderen vollständigen Läufe sind Quality 41263905631, LED 41263905584,
NFC 41263905630, ESP-NOW 41263905632 und Rollen 41263905610. Android-XML
dieses Heads: GM 35/Worker 9, Protokoll 111/fünf V1-Tests, Dev-App 77 ohne
Fehler oder Skips. Keine Geräte-/Approval-/Mergeaktion; PR bleibt Draft.

### Nachfolgende Local-/Timer-Korrelationskorrektur

Body `2db205d8af85336874085809578da58d9fb305e692b2e5ec822a28c6c8486e10`,
Header unverändert `745d5a7c…`: Der ursprüngliche Finalstand wird nun auch für
Local-/Timerwiederholungen unter neuer Korrelation aufgelöst. Der unveränderte
Vorbody `43518a73…` wurde tatsächlich mit beiden Fällen rot geprüft, jeweils
nach explizit nachgewiesenem Erstcommit. Keine Mutation als Vorfixbeleg.

Unabhängig **35 Local- und 52 Timerassertions** gegen zwei vollständige,
tatsächlich validierte/kompilierte Artefaktsätze. Frische Katalogbytes werden
gegen erhaltene Compilerresultate und unabhängige kanonische Rekonstruktion
verglichen; daraus folgt kein vollständiges unabhängiges semantisches Sollmodell.
Local erzeugt zuerst seinen echten Fact, Latch und Intent. Der Timer wird durch
einen tatsächlichen Localplan gearmt und erst nach seiner Deadline konsumiert.
Keine handgesetzten Readybits oder positiven Timerzustände.

Geprüft sind ursprüngliche ID/Revision/Range, keine neuen Belege, Leases,
Signaturen oder Schreibvorgänge, geänderte Eingabedaten, Phasenkollision, Namespacegrenzen,
Cancel/Rearm, alter konsumierter Timer neben neuem Arm und vollständiger kalter
Replay ohne heutige Quellen. Eine neue synthetische Localgeneration/Folge ist
ein anderer Eingang; der Once-Latch bleibt dennoch einmalig. Die lokale
Testgeneration ist ausdrücklich synthetisch und nie aus Clockboot128 gekürzt.

Gemeinsamer frischer Hostlauf: 44/44 CTests, PIO 263/263, Header 109,
Architektur 190 Dateien/416 Includes, Formatbaseline 380 und neue Fixture sauber.
Der zusätzliche CNG-/PSA-Korpus verlangt den tatsächlichen Artefaktcompiler und
führt im Standardgate beide Fälle aus.

**Nachgelagerte unabhängige Qualifikation von e821a548 am 01.11.2026:**
Alle zwölf PR-Jobs in sechs Workflows erfolgreich. Alle Originalcheckoutlogs
zeigen Prüfmerge `7011a31db0b07f3d9ffba4c468426c9d83312eb1` mit exakt
Basis `c4f1f824c6274e9a8d7676212a2a96be4d4a77b9` und
Head `e821a54898e74e0377fd3a6d743db5819c309374` als Eltern;
pull_request, Attempt 1, Actions-App und abschließend unveränderte Refs geprüft.

[Native 41268754756](https://github.com/Sven401/Larp-Escape/actions/runs/41268754756),
Originaljob `124736963645`: **93/93 CTests**, null Fehler, Disabled oder Skips.
Der neue Local-/Timer-PSA-Korpus führt beide tatsächlichen Artefaktsätze und
vollständigen Katalogvergleiche aus, ohne verkürzendes `--case`. Beobachtete
JSON-Zähler: **Local 37 + Timer 54 = 91 Assertions**. Die vier gegenüber CNG
zusätzlichen gezählten Prüfungen betreffen PSA-Initialisierung/Import pro World;
erfolgreiche Destroyprüfungen folgen erst nach JSON und sind nicht darin gezählt.
Der bestehende Owner-PSA-Korpus bleibt vollständig mit 300 Assertions,
zehn direkten und neun eingebetteten Signaturvorkommen sowie sieben kanonischen
Blob-/Bindingprüfungen; Ordering 830, Clock sechs Prozesse/192 und 15/426.
Beide echten SDK-Images am Pin `9a97f6c54ec638111ce55cd36581b3c192f15207`
samt rekursiven Submodulen erfolgreich; aktive SDK-Quellen/Includes ohne
Testfixture-/Stubpfade. Originalarchiv `13068389068`, SHA256
`fc7606657d3d0ed6e05edb77d555a20156e77356d054cc424fbd089c392804ad`.

Der tatsächliche LF-Gitblob des Ownerbodys hat SHA256
`fd11731c70381470cbd63469232a0ec06bdaef3d00031c95ab40b4ac1e555149`;
die oben genannte Windows-CRLF-Datei `2db205d8…` unterscheidet sich ausschließlich
in Quelltextzeilenenden. Keine Normalisierung signierter oder kanonischer Bytes.

[Quality 41268754781](https://github.com/Sven401/Larp-Escape/actions/runs/41268754781)
3/3, [LED 41268754759](https://github.com/Sven401/Larp-Escape/actions/runs/41268754759)
4/4, [NFC 41268754769](https://github.com/Sven401/Larp-Escape/actions/runs/41268754769)
2/2, [ESP-NOW 41268754782](https://github.com/Sven401/Larp-Escape/actions/runs/41268754782)
1/1 und [Rollen 41268754760](https://github.com/Sven401/Larp-Escape/actions/runs/41268754760)
1/1. Tatsächliche Android-XML: GM 35/Worker 9, Protokoll 111/fünf V1-Tests,
Dev-App 77, null Fehler/Skips. Androidarchiv `13068452207`, SHA256
`9f7dc2b5e3095cbaccde603260190920839ae62b1f87611b41957f6c6fe9a4d2`.
Andere Gegenprüfer: quality_p1 für den tiefen Nativekorpus,
module_digest_review für alle Originalcheckoutlogs, Android und Schlussrefs.
Keine Übertragung auf nachfolgende Änderungen, Geräte, Rechte oder SMP.

Die konkrete Orderingklasse ist ausschließlich für das beschriebene exklusive
Single-Worker-Profil ausgelegt. Alle tatsächlichen relevanten Metadatenwriter
müssen dessen Sperre beachten. Ein späterer SDK-/Taskbridge muss die gesamte
Eigentums- und Veröffentlichungszusage erneut belegen; eine fallible Queue ist
kein Ersatz für die nicht fehlschlagende Veröffentlichung.

Die native Qualifikation gilt ausschließlich für den genannten Head und seine
ausdrücklichen Testgrenzen. Für Local-/Timereingänge wurde die beim ee5-Stand
noch offene Korrelationsfrage anschließend am P0-Vertrag geklärt: Die
Eingangsidentität bleibt unabhängig von einer neuen Anwendungskorrelation,
innerhalb event/run/station. Die entsprechende Ownererweiterung ist ein eigener
nachfolgender Prüfstand; die obige ee5-Qualifikation bescheinigt sie nicht.
Dauerhaft vor Nutzung vergebene lokale Bootgeneration und tatsächliche
OriginalInputproduzenten bleiben Pflicht; die 128-Bit-Clockkennung ist
ausdrücklich kein Ersatz dafür.
Produktive Quellenadapter, nativer Ingress,
F5a, Gerätebudgets, echte Stromausfälle und Hardwarewirkung bleiben offen.
Kein Upload, Release, Merge oder Abschluss von #93 folgt aus diesem Stand.

Der nachfolgende [native Boot-/Clockanschluss](rules-boot-clock.md) implementiert
eine gemeinsame Startupkennung und Zeitquelle als eigenen begrenzten Schritt.
Seine Host-/SDK-/Gerätenachweise bleiben getrennt vom hier qualifizierten
ee5-Ownerstand. Er instanziiert keine Runtimebesitzer und erzeugt keine Rechte.
