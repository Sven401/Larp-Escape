# Semantischer Rules-Fortschrittsbesitzer

Status: **Hostimplementierung unabhängig geprüft; native Qualifikation offen**,
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
Finalstand. Auch ein inhaltlich identischer bereits verarbeiteter Fact mit
neuer Korrelationsoperation nennt seine ursprüngliche Anwendung über
`Result.originalApplication`, deren physische Revision, Index und Factrange.
Die neue Korrelation und ihr Request werden dadurch weder gespeichert noch
als neue übernommene Operation bestätigt. Es entstehen keine neue Planung,
Signatur, Lease oder Wirkung. Dieselbe FactId mit geändertem vollständigem
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
ausdrücklich synthetische Testports. Native PSA, vollständiges SDK-Wiring und
Produktquellen des neuen Owners sind mit diesem Hostlauf nicht qualifiziert.
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

Die konkrete Orderingklasse ist ausschließlich für das beschriebene exklusive
Single-Worker-Profil ausgelegt. Alle tatsächlichen relevanten Metadatenwriter
müssen dessen Sperre beachten. Ein späterer SDK-/Taskbridge muss die gesamte
Eigentums- und Veröffentlichungszusage erneut belegen; eine fallible Queue ist
kein Ersatz für die nicht fehlschlagende Veröffentlichung.

Die gemeinsame native PSA-/SDK-Qualifikation und die abschließende Zuordnung
des vollständigen Runtimekorpus stehen noch aus. Frühere Bausteinprüfungen oder
eigene Smokes ersetzen diese Kampagne nicht. Produktive Quellenadapter, nativer Ingress,
F5a, Gerätebudgets, echte Stromausfälle und Hardwarewirkung bleiben offen.
Kein Upload, Release, Merge oder Abschluss von #93 folgt aus diesem Stand.
