# Manuelle Ersatzfreigabe v2: früher Softwareprüfstand

Status: **laufender vollständiger Umsetzungsauftrag; vorbereitender Draft**,
31.10.2026. Ziel [#93](https://github.com/Sven401/Larp-Escape/issues/93).
Arbeitsbasis `27a8a5d4a5fb2ba77790997fc09c4a4f583ae6e1`, einschließlich der
qualifizierten D0-Integration `c4f1f824c6274e9a8d7676212a2a96be4d4a77b9`.
Der [bestätigte Erweiterungsvertrag](p0-manual-override-v2.md) bleibt der
vollständige Auftrag; dieser Prüfstand ersetzt ihn nicht.

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
