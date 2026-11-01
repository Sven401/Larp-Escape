# Manuelle Ersatzfreigabe v2: Komponentenprüfstand

Status: **laufender vollständiger Umsetzungsauftrag; geprüfter Komponenten-Draft**,
01.11.2026. Ziel [#93](https://github.com/Sven401/Larp-Escape/issues/93).
Ausgangsbasis `27a8a5d4a5fb2ba77790997fc09c4a4f583ae6e1`, einschließlich der
qualifizierten D0-Integration `c4f1f824c6274e9a8d7676212a2a96be4d4a77b9`.
Der [bestätigte Erweiterungsvertrag](p0-manual-override-v2.md) bleibt der
vollständige Auftrag; dieser Prüfstand ersetzt ihn nicht.

## Folgecheckpoint · 01.11.2026

Die folgenden zusätzlichen Komponenten sind auf dem Arbeitsbranch vorbereitet.
Der tatsächliche Gesamtkoordinator `RulesProgressOwner`, produktive ursprüngliche
Installations-/Eingangs-/Uhrbelege, Einrichtung und F5a bleiben offen. Sein noch
unveröffentlichter Header/Body und der separate Schlüsseldatei-Kandidat gehören
ausdrücklich nicht zu diesem Komponentencheckpoint. Der aktuelle veröffentlichte
Komponentenstand `0e9f7ad3…` ist einschließlich eigenem nativen PSA-/SDK-/PR-Lauf
qualifiziert; die genaue Zuordnung steht im nachgelagerten Abschnitt. Frühere
Prüfstände bleiben ausdrücklich historische Belege.

Kern-/Integrationsentwickler bleibt `developer_p1a3`; Root implementiert zusätzlich
den [Katalog-/Plancodec](rules-progress-codec-v1.md),
[signierte Originalbelege](rules-signed-evidence-v1.md) und
[Effektbelegcodec](rules-effect-codec-v1.md). `quality_p1` prüft andere tatsächliche
Byte-/Krypto-/Automatenfälle, `module_digest_review` die Sicherheits-/Vertragsgrenzen
und neue Control-/Auxspeicherfälle. Keine eigene Umsetzung gilt als unabhängig geprüft.

- Konkreter Prepare/Confirm-Automat mit vollständigem Katalogdigest, kopierten
  Originalfences, festen Phasenkennungen, Frist-/Entzugsprüfung und globaler
  Unknownsperre. Committed beschreibt Dauerhaftigkeit; das eigene Ergebnisfeld
  unterscheidet Released, NoRelease und Unresolved. Ein nicht ausgelöster Guard
  darf einen gespeicherten Receipt erzeugen, ohne T2 oder Ersatzfreigabe zu behaupten.
- Tatsächlicher kopierender PeerProof-/Registry-/Identity-Adapter und eigener
  Eventdatenspeicher. Die positive Einrichtung-/Eventprovenienz ist weiterhin
  nicht implementiert; Device-GM wird nicht als Eventrecht umgedeutet.
- Vollständige kanonische Katalog-/Plan-/Override-/COSEblobs, private Live- gegenüber
  inaktiver Historical-Admission und gemeinsamer semantischer Programmvalidator.
  Decode publiziert keinen ausführbaren Plan.
- Tatsächlich signierte Outbox und vollständige signierte Ursachenclosure mit
  eingefrorenen ursprünglichen Bytes/Schlüsseln. Aktuelle Lookupquellen bleiben
  bei historischer Prüfung unbenutzt. Mathematische Gültigkeit ist keine echte
  Installations-/GM-/Eingangsautorität; die nötigen Produktports bleiben geschlossen.
- Unwirksame Datenchunks und einzelner Finalmarker auf unveränderter Journal-ABI,
  vollständige Pool-/Slotreservierung und echte simulierte Backend-Neusitzung.
  Neue Control-/Auxbelege und Claim-/Resultdarstellung ersetzen noch keinen
  durchgängigen semantischen Coordinator oder eine Gerätebeobachtung.

Tatsächliche lokale Nachweise: vollständiger CMake-/CTestlauf **36/36**, danach
fünf betroffene/neue Ziele **5/5**. Die anschließend gezielt korrigierte RawSHA-
Recoverybindung der neuen Bundle-Auxsprache ist mit frisch gebautem unabhängigem
Bundle-Basiskorpus erneut geprüft: acht positive/239 fehlerhafte Bestände,
167269 Assertions und 16390 programmierte Präfixgrenzen. Der Lowlevel-
`ProgressJournal::requestDigest` bleibt opak; nur die neue Wrappersprache bindet
ihn beim Schreiben und Wiederherstellen an SHA256 der tatsächlichen Nutzbytes.
Zusätzliche [gezielte Control-/Auxfälle](../../test/fixtures/rules-bundle-aux-review/README.md)
prüfen mit 306 Assertions den tatsächlichen vorherigen Katalog-Final, neue
Backend-Neusitzung, alle vier Auxarten, falsche RawSHA, exakte Reserve sowie
Vorwrite-Abbruch gegenüber geschriebenem Chunk/Unknown. Consumer und Freigabe-
fence sind ausdrücklich synthetisch. Die drei isolierten RawSHA-Gegenfälle
scheitern ohne den neuen Guard in einer privaten Mutationsdatei; diese Datei
wird nicht als früherer Originalcommit ausgegeben.
`quality_p1` hat die drei Fixturequellen unabhängig gegengelesen und frisch
übersetzt: ebenfalls 306 Assertions sowie die drei isolierten RawSHA-Fälle mit
je 15 Assertions grün; ohne den neuen Guard schlagen alle drei gezielt fehl.

PlatformIO **263/263 in 25 Suiten** vor dieser letzten zweizeiligen Korrektur;
danach frischer Hostcompile und Control **5/5** am korrigierten Stand. Diese
Reihenfolge ist kein pauschaler neuer Gesamtlauf. Zusätzlich unabhängig:
[Owner 18 Gruppen/806 Assertions](../../test/fixtures/manual-owner-review/README.md),
[Codec 61 Artefakte/20 Programme, sechs Traces/24 Zustände und Fehlkorpus](../../test/fixtures/rules-progress-independent-review/README.md),
[SignedEvidence 2666+135 Assertions](../../test/fixtures/signed-rules-evidence-independent/README.md),
[Effect 3160 Assertions](../../test/fixtures/rules-effect-independent-review/README.md).
Die Authority-/Installations-/Clock-/Transactionports dieser Hosttests sind
ausdrücklich synthetisch; ihre Grenzen stehen in den jeweiligen Berichten.

Architekturguard **186 Dateien/403 Includes**, **107** unabhängige C++11-Header
und Formatbaseline **376 Dateien/19.1.7** sind an einer separaten Kopie des exakt
vorgemerkten Komponentenbaums geprüft. Darin sind Coordinatorheader/-body und
Schlüsseldatei-Kandidat nachweislich abwesend. Der reine spätere Aux-Testtarget-
Nachtrag ändert diese Guardflächen nicht.
Alle Quellen bleiben unter den unveränderten Pflichtprüfungen. Die nachgelagerte
commitbezogene CI steht im folgenden Abschnitt; Geräte-/Speicher-/Lastnachweise
bleiben weiterhin offen.

Die zwei bisherigen Referenzspiele und sechs Traces bleiben erhalten; keine
neue dritte Abnahmemechanik. Kein Upload, Release, main-Merge oder Abschluss #93.

### Nachgelagerte native Qualifikation und weiterer Recoveryfix

Der ursprüngliche Komponentenhead `ac81f84e2aa122743af7f32083a449054003aba0`
hatte elf erfolgreiche PR-Jobs und einen fehlgeschlagenen Native-Lauf mit 86/87
CTests. Ausschließlich der Quelldatei-Provenienzguard des Owner-Audits scheiterte
vor dessen Binärausführung an LF/CRLF. Der unabhängig geprüfte Drei-Dateien-Fix
`1d54cd7c11032b999d14a7be9e5b3b923e779985` bindet diesen Guard an die tatsächlichen
Gitblob-LF-Bytes; weitere Bytes werden nicht normalisiert. Windows und getrennte
LF-Gitblobkopie bestehen jeweils 18 Gruppen/806 Assertions; Produkt-, Katalog-
und Szenariobytes bleiben unverändert.

**1d54cd7c ist vollständig unabhängig qualifiziert: zwölf von zwölf PR-Jobs und
87/87 native CTests, null übersprungen.** Prüfmerge
`56bd3b7867024532c1ab88e86feb979975851076` hat exakt Ziel
`c4f1f824c6274e9a8d7676212a2a96be4d4a77b9` und Head als Eltern, Tree
`92a42f750717b135dcf516a5e13d000394a0bb8d`. Alle zwölf Originalcheckoutlogs,
sechs Workflowpfade, pull_request/Attempt1/Actions-App15368 und abschließend
unveränderte Remote-Refs wurden von `quality_p1` geprüft.
[Native41253041026](https://github.com/Sven401/Larp-Escape/actions/runs/41253041026)
prüft ManualOverrideOwner-Automaten806 mit synthetischen Authority-/Transactionports
und realer PSA-SHA, SignedEvidence2732+137, Codec-Traces620, Aux306,
Effect3160 und Bundle167269/16390Cuts. Tatsächliche PsaPackageDigest-/D0Verifier-/
D0Signeradapter, SDK-Pin `9a97f6c54ec638111ce55cd36581b3c192f15207` samt rekursiven
Submodulen sowie beide SDK-Images sind bestätigt. Originalarchiv13062710466,
SHA256 `4d4db8edc35e1203f20e3b0b8ee4549c59090b78f5bb018604219a5ccbb5c6aa`.
[Quality41253040992](https://github.com/Sven401/Larp-Escape/actions/runs/41253040992)
einschließlich aller vier Firmwarevarianten sowie LED41253040977, NFC41253040921,
ESP41253040938 und Rollen41253041045 sind erfolgreich. Der frühere Fehlerlauf
bleibt getrennt erhalten; erfolgreiche Builds sind keine Gerätebeobachtung.

Die weitere Gegenprüfung fand danach eine zusätzliche Wrapper-Sprachasymmetrie:
der Writer verbietet Auxoperationen mit der Kennung eines beliebigen Bundle-
Parents, Recovery prüfte diese globale Kollision bislang nur für Datenchunks.
Acht echte rote Fälle am unveränderten Body `032c01cb…` (alle vier Auxarten,
je vor/nach einem verwaisten Parent) belegen die unbeabsichtigte Annahme.
Der minimale Folgefix erweitert genau diese Prüfung auf Auxiliary; ein Final
behält weiterhin seine eigene Parentkennung. Der vollständige neue lokale
Auxkorpus umfasst 468 Assertions, die acht isolierten Fälle sind mit Fix grün.
Kein Authority-/Schema-/Lowlevelwechsel. **Diese Änderung ist nicht im grünen
1d54cd7c-Nachweis enthalten und benötigte einen eigenen neuen CI-Lauf.**

Dieser neue Lauf ist inzwischen unabhängig qualifiziert: Head
`0e9f7ad3446c0b7b9c8387c98f5034ba5488f6fb`, Prüfmerge
`4243abf5d539112c3ffcc253d1d367044e1c3b85`, exakt unverändertes Ziel `c4f1f824…`
und Head als Eltern; Tree `b7eb0c56559df9f8979d7d18272ee7287cde3502`.
**12/12 PR-Jobs, 87/87 native CTests, null übersprungen und beide SDK-Images grün.**
`quality_p1` prüfte alle zwölf Originalcheckoutlogs, sechs Workflowpfade,
pull_request/Attempt1/Actions-App15368 sowie frische REST-/Git-Refs.
[Native41253986154](https://github.com/Sven401/Larp-Escape/actions/runs/41253986154)
enthält tatsächlich den neuen Auxkorpus mit 468 Assertions und acht Parentfällen,
806 ManualOwner-Automatenchecks mit synthetischen Authority-/Transactionports
und realer PSA-SHA, SignedEvidence2732+137, Codec-Traces620, Effect3160 und
Bundle167269/16390Cuts. SDK9a97…/rekursive Submodule und die drei tatsächlichen
PSA-Adapter im Coreimage sind bestätigt. Originalarchiv13061723868, SHA256
`1fcaa4664b28293d50e954e6d76346450d69bb1521e50f825c943fa5edaf94e4`.
[Quality41253986284](https://github.com/Sven401/Larp-Escape/actions/runs/41253986284)
einschließlich vier Firmwarevarianten/Android, LED41253986142, NFC41253986136,
ESP41253986222 und Rollen41253986171 sind ebenfalls erfolgreich.
Die qualifizierten Komponenten erzeugen dadurch weder eine produktive
Setup-/GM-Autorität noch einen fertigen semantischen Gesamtowner oder Gerätebeleg.

Nutzerfreigabe am 01.11.2026: „Testfix und geprüfte Folgestände freigeben“.
Sie gilt für unabhängig geprüfte Folgestände im bereits beauftragten Umfang,
bestehenden Draft #135 und Branch `codex/p1a7-manuelle-ersatzfreigabe`.
Merges, Releases, Geräteuploads und neue Grundsatzentscheidungen bleiben davon
ausgeschlossen. Draftstatus und vollständiger offener Softwareauftrag bleiben.

Die nachfolgenden Abschnitte dokumentieren den früheren veröffentlichten Prüfstand.

## Verantwortung und tatsächliche Änderungen

Führender Kern-/Integrationsentwickler `developer_p1a3`; Root implementiert den
separaten D0-Writer/PSA-Signer. Anderer Qualitätsreviewer `quality_p1`, gezielter
Vertrags-/Securitygegenprüfer `module_digest_review`. Keine Gerätebeauftragung.

- Explizite package.v2/module.v2/rules.v2- und Fähigkeitsgates; gewöhnliche
  v1-Module bleiben wiederverwendbar. Der Standard-Deklarationsleser bleibt v1.
- Vollständige 1:1-Original-/Ersatzpaarung vor Ausnahmen für gemeinsame stabile
  IDs und Ressourcen. Gleiche geordnete Aktionen, mindestens ein emit/override-
  Paar, verschiedene Namen/Generationen/Prioritäten weiterhin eindeutig.
- Tatsächliche vollständige Quell-/CBOR-/IR-/Ressourcenprüfung für v2 mit exakter
  statischer Gruppenvereinigung. Die Vereinigung ist kein Laufzeitbeweis aller
  Originalfakten für einen Ersatz.
- Typisierte T1/T2-Fakten und Ausgaben, Gruppen-Claims im Planer und private
  Admission. Ein öffentliches Defaultobjekt erteilt keine Freigabe. Es gibt in
  diesem Stand noch keinen konkreten Besitzer, der eine gültige Admission mintet.
- Tatsächliche uint64-Bindungsgeneration bis Program/Context; explizite
  `epochsComplete`-Unterscheidung zwischen kompletter leerer Remote-Liste und
  alter Convenience-Epoche. Leere vollständige Liste erlaubt keinen Fakt; die
  ausgewählte lokale Epoche muss weiterhin tatsächlich enthalten sein.
- Neuer D0-Fact-Writer für alle bestehenden Faktentypen, kanonische Originalbytes
  und eigener öffentlicher PSA-ES256-Signierer. Der private Ursprungsschlüssel
  wird ausschließlich vom dafür zuständigen Besitzer bereitgestellt; der Adapter
  erzeugt/importiert/persistiert/exportiert/destroyt keine privaten Produktkeys.
- `SignedUnpublished` entsteht erst nach tatsächlicher Decoder-/Verifierprüfung
  der erzeugten Signatur gegen die ursprünglich exportierte PublicKeykopie.
  Diese XY-Kopie bleibt für historische Outboxprüfung erhalten. Ein späterer
  Registry-/PSA-Handlelookup ersetzt sie nicht. Fehler leeren Ausgabe, Schlüssel-
  kopie und Stagingbytes; ein später Cleanupfehler setzt auch NestedScope zurück.

## Nachweise dieses Zwischenstands

- Vollständiger CMake-/CTest-Lauf **23/23** und PlatformIO **21 Suiten / 232 Fälle**
  erfolgreich. Die anschließend erweiterten unabhängigen Fixtures sind zusätzlich
  gezielt erneut ausgeführt; ihre jeweils endgültigen Zahlen stehen in den
  versionierten [Qualitätsfixtures](../../test/fixtures/manual-override-v2-review/README.md).
- P0-Modell **63/63**, davon 45 neue v2-Fälle und 18 unveränderte v1-Tests mit
  sechs unveränderten Referenztraces. Die neuen Fälle sind Verwaltungs-/
  Zustandsgegenbeispiele, kein dritter Abnahmefall und kein Produktberechtigungsbeleg.
- Architektur **167 Dateien / 354 Includes**, **97** unabhängige C++11-Header,
  Formatbaseline **357 Dateien** und zusätzlich neue Auditquellen mit
  clang-format 19.1.7 geprüft.
- Unabhängige tatsächliche v2-Artefakte prüfen vollständige Source-/Modul-/
  Bindungs-/Profil-/Ressourcenbytes und das wirklich erzeugte Program. Echte
  Hashes, Namensqualifikation, Gruppen-IDs, unterschiedliche Generationen,
  T1/T2-Permissions, leere Remote-Epochenliste und statische Union sind enthalten.
  Default-Admission und beliebige Remote-Epochen sind ausschließlich negative
  Kernports; kein gefälschter positiver Besitzerzugang existiert.
- Bestehende echte Compiler/Planner-Regression: sechs unveränderte Traces,
  24 vollständige Zustandsvergleiche, 54 Szenarien und 4096/4097-Grenze frisch grün.
- Writer: 22 tatsächliche CNG-signierte Roundtrips aus unabhängig erhaltenen
  Payloads plus 14 Fehler-/Wiederverwendungspaare. Unabhängig zusätzlich
  22 Signaturrundläufe, 44 Eingabekopie-/Aliasfälle und 36 WrongKey-/Hash-/
  Verifiercleanup-Fehlerfolgen, jeweils erfolgreiche Wiederverwendung. Python
  prüft jede erfolgreiche Signatur unabhängig. Insbesondere scheitert eine
  tatsächlich gültige Key2-Signatur unter unverändertem Key1-kid.
- Gesonderte **554 API-/Lifecycleprüfungen** am wirklichen PSA-Signieradapter
  gegen eine absichtlich nichtkryptografische Fehlernaht. Fehlende Import-/
  Destroy-/PrivateExportfunktionen in dieser Naht verhindern versehentlichen
  Schlüsselbesitz. Mechanische Erfolgsbytes sind kein Signaturgültigkeitsbeleg.

[Writer-Reproduktion und vorhandene Pythonvoraussetzung](../../test/fixtures/d0-writer-review/README.md),
[getrennte Signieradapterfehler](../../test/fixtures/d0-signer-faults/README.md).
Native CI verwendet den bereits vorhandenen cryptography45.0.7-Pin; lokal wurde
die vorhandene gebündelte Runtime mit cryptography50.0.2 verwendet. Ein System-
Python ohne diesen unabhängigen Verifier ist keine grüne Ersatzprüfung.

### Nachgelagerte native und PR-Qualifikation · 31.10.2026

Der bei Erstellung offene produktive PSA-Nachweis ist für den veröffentlichten
Zwischenstand nachgeholt: [Draft #135](https://github.com/Sven401/Larp-Escape/pull/135),
Head `7a44ac502529cb66b45d6a802ff96b180012e8d3`, Ziel
`c4f1f824c6274e9a8d7676212a2a96be4d4a77b9`, tatsächlicher Prüfmerge
`94dc645d140d8a82ad861b6d596eacbaf98e2010` mit exakt diesen Eltern.
Anderer Qualitätsreviewer `quality_p1` hat alle zwölf tatsächlichen Checkoutlogs,
`pull_request`, Attempt 1 und GitHub Actions App 15368 sowie abschließend die
unveränderten Remote-Refs geprüft. Alle zwölf Jobs sind erfolgreich.

[Native 41240436263](https://github.com/Sven401/Larp-Escape/actions/runs/41240436263):
**72/72 CTests, kein Fehler oder Skip**, beide Writerkorpora mit produktivem
PSA-Signierer, tatsächlichem Testprivatkeyimport und unabhängiger Python-P256-
Ausgabeverifikation; 61 vollständige tatsächliche v2-Artefaktsätze. Die 554
synthetischen Lifecyclechecks bleiben ein getrennter Nachweis. SDK-Commit
`9a97f6c54ec638111ce55cd36581b3c192f15207` und rekursive Submodule sowie beide
SDK-Images sind bestätigt; der produktive Writer/Signer wird im Coreimage
tatsächlich kompiliert. Originalarchiv `13056829309`, SHA-256
`ddf1d14ba50ec4aa38b8dcd7b835be8efd49afd31a92c8bb52885efbfad2817e`.

[Quality 41240436644](https://github.com/Sven401/Larp-Escape/actions/runs/41240436644)
prüft auch alle vier Firmwarevarianten erfolgreich. Die weiteren vollständigen
Pflichtläufe sind [LED](https://github.com/Sven401/Larp-Escape/actions/runs/41240436527),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/41240436337),
[ESP-NOW](https://github.com/Sven401/Larp-Escape/actions/runs/41240436631) und
[Rollen](https://github.com/Sven401/Larp-Escape/actions/runs/41240436288).
Diese Qualifikation gilt für den genannten Zwischenstand, nicht für den unten
weiterhin offenen vollständigen Besitzer-/Authority-/D1-Pfad oder spätere Quellen.

## Speicher und Grenzen

Gemessen Host32/GCC5.1: Context184 B, OverrideAuthority48 B, OverrideRecord512 B,
OverrideAdmission512 B, Input968 B, Receipt1152 B, FactRef208 B, Program352 B,
SubjectBinding112 B. Die frühere Host-Ledgerkapazität256 benötigt damit allein
294912 B Receiptstorage; sie ist kein zulässiges implizites ESP-Profil.
Caller-owned Gerätepools, Gesamtbudget und tatsächliche Aktivierungsgrenzen
müssen vor dem Produktanschluss ausgewiesen und geprüft werden.

Writerworkspace2048 B und FrozenFact2888 B gehören dem Aufrufer. Gemessene einzelne
Host-O0-Frames: writeFact1248 B, valid112 B, payload48 B; verschachtelte Decoder/
PSA-/Aufruferframes kommen hinzu. Keine ESP-Gesamtstack-/Heap-/Zeitqualifikation.

## Verbindliche nächste Umsetzung

Der vollständige Auftrag umfasst weiterhin den konkreten Prepare/Confirm-
Besitzer, echten aktuellen Authorityadapter und Event-Aktivierungsbesitzer,
vollständigen Plan-/Autorisierungs-/Outboxcodec, Journalkoordinator mit echter
Reopen-/Recoverysemantik, tatsächliche signierte Ausgabe-/Loopbackintegration
und End-to-End-Fehlerprüfungen. Der ausdrücklich präzisierte Bundlevertrag hat
strikt unwirksame Control-Datenchunks und genau einen wirksamen Finalmarker;
32 KiB ist ein eigenes Transaktionsprofil, nicht das P0-Metadatenbudget.

Der vorhandene generische Device-GM-Registerkern enthält kein Eventrecht.
Ein heutiges Program.event an alte Rechte anzuhängen ist unzulässig. Das
[Einrichtungsschlüsselformat und die Erstbereitstellung](../companion/veranstaltungszugang.md)
sind als eigener noch offener Vorentscheid benannt. Ein neuer Setup-Anker wird
nicht still aus dem D0-OriginKey abgeleitet; dessen privater Schlüssel bleibt
auf der Box. Der konkrete zusätzliche Vorschlag wird gezielt geprüft.

Kein Abschluss des Ersatzwegs mit Modell, Parser oder RAM-Latch allein; kein
aktivierter GM-/D1-/Mehrboxbetrieb durch diesen Draft. Keine Hardwareprüfung,
kein Upload, Release oder main-Merge. #93/P1–P4 und das unveränderte Drittspiel-
Tor bleiben offen.
