# Ursprüngliche Rules-Belege v1

Status: implementiert und unabhängig auf dem Host gegen signierte Originalbytes
und Fehlerfälle geprüft; **kein vollständiger produktiver Trust-/Owner-/D1-Anschluss**,
01.11.2026. Der native PSA-Nachweis steht für diesen Stand noch aus.
Root implementiert `CompanionIntegration/RulesSignedEvidence`,
`module_digest_review` prüft die Quelle, `quality_p1` die getrennten Kryptogegenfälle.
Das Fachmandat bleibt innerhalb des
beauftragten vollständigen manuellen Ersatzwegs in #93; der Gesamtauftrag ist
mit dieser Schicht nicht abgeschlossen.

## Verantwortung und fehlende positive Quellen

Der Adapter verbindet tatsächliches `writeFact`/`checkFact` mit einer vollständigen
eigenen Kopie der ursprünglichen COSE-, Schlüssel-, Installations- und
Eingangsbelege. Rohbytes aus `IOriginalSources` sind keine Berechtigung.
`IOriginalInstallations` und `IOriginalInputs` stehen standardmäßig auf
Unavailable. Sie müssen von konkreten authentifizierten Produktbesitzern
implementiert werden; weder eine SubjectBinding noch heutige GM-Rechte oder ein
gültiges XY/COSE-Paar ersetzen diese Herkunft.

Ein produktiver D0-PrivateKey-/Installationsbesitzer ist noch nicht angeschlossen.
Display-ID, Setupschlüssel, öffentlicher Testscalar oder späterer PSA-Handlelookup
sind kein Ersatz. Der konkrete Signierer wird nur ausgeliehen; tatsächlicher
exportierter PublicKey und endgültige Signaturbytes müssen zur ursprünglichen
Installation passen. Diese Schicht erzeugt oder persistiert keine privaten Schlüssel.

Der ursprüngliche GM-Besitzbeleg ist der exakt akzeptierte S3-PeerProof-Transcript
mit Signatur und Credential. Er beweist allein keine GM-Rolle, Eventzuordnung,
Annahmezeit oder sichere Kanalbeobachtung. Dafür bleiben originale authentifizierte
Registry-/Event-/Installationsbelege und die vertrauenswürdige ursprüngliche
Ownerannahme im bestehenden Journal-Threatmodell erforderlich. Kein S3/D0-
Domainreuse, kein zusätzlicher D0-Wireparameter für Prinzipal/Verwaltungsgrund.

## Eigene kanonische Autorisierungsdarstellung

Die opaque `Application.authorization` aus dem [Rules-Codec](rules-progress-codec-v1.md)
enthält eine exakte deterministische CBOR-Map mit Labels 0 bis 4:

| Label | Inhalt |
| --- | --- |
| 0 | Text `larp.rules.evidence.v1` |
| 1 | ClockWitness `[boot,monotonicBefore,monotonicAfter,activeBefore,activeAfter]` |
| 2 | vollständiger ursprünglicher Programminstallationsbeleg als nichtleerer bstr |
| 3 | vollständiger ursprünglicher Eingangsannahmebeleg als nichtleerer bstr |
| 4 | geordnete Ursprungsliste, exakt ein Eintrag je `Application.proofs` |

Ein Ursprungseintrag ist `[FactId,payloadDigest,originalerInstallationsbeleg]`.
FactId ist wie im Rules-Codec `[event,origin,epoch,sequence]`; Identitäten bstr16,
Digest bstr32, alle Zähler uint64. Ursprungsliste und tatsächlich gespeicherte
Faktenbelege müssen exakt dieselbe ID-/Digestreihenfolge besitzen. Keine
Duplikate oder fremden nachträglich nachgeschlagenen Installationen. Sämtliche
ursprünglichen Belegformate bleiben selbst versioniert und tatsächlich zu prüfen;
die Verpackung erteilt ihnen keine Autorität.

Definite Container, exakte Feldzahl und Reihenfolge, kürzeste Integer-/Längen-
kodierung, keine unbekannten Felder oder angehängten Bytes. Autorisierung und
einzelne ursprüngliche Belege höchstens 32768 Byte; COSE je Fakt höchstens
2048 Byte. Gesamter App-/Transaktionsblob muss zusätzlich in das echte
Bundleprofil passen. Bounded Betrieb: höchstens 256 Proofs mit je acht Dependencies;
dies ist ein eigenes Suchprofil, kein erhöhtes P0- oder Gerätebudget.

## Freeze vor tatsächlichem Commit

Der unveränderte dauerhafte Vorzustand und der vollständige tatsächliche
Plannerplan bleiben erhalten. Der Adapter prüft den tatsächlichen Katalog-SHA
aus neu kanonisch kodierten Programbytes. Alle Quellen und Zielkapazitäten sind
disjunkt; Belege, COSEbytes, PublicXY und Autorisierung werden vollständig kopiert.
Plan-Beweisansichten müssen echte Bereiche im eigenen Evidencepool sein.
Zustandsspans umfassen nur ihre lebenden Einträge; der Coordinator muss zusätzlich
die vollständigen aktiven/staging Speicherbänke getrennt halten.

Die Closure beginnt bei Planevidence, tatsächlichem Fact-Eingang und erhaltener
Value-/Timer-/Latch-/Factkausalität. Tatsächlich geprüfte COSE-Dependencies werden
iterativ vollständig nachgeladen. Gleiche Fact-ID mit widersprüchlichem Digest/
Typ/Kontext, fehlende Belege, Zyklen und erschöpfter Such-/Speicherbereich werden
geschlossen behandelt. Keine Rekursion oder still gekürzte Beweiskette.
Jeder vorhandene Beleg der eigenen aktuellen Event-/Box-/Epochquelle muss
innerhalb des damals bereits dauerhaft vergebenen `before.lastIssued` liegen.
Ein neuer Closurefund darf insbesondere keine erst durch diesen Final zu
vergebende Outbox-ID als schon vorhandene Ursache belegen. Historisch gilt
dieselbe damalige Grenze; heutige Zähler werden nicht eingesetzt.

Eigene Ausgaben verwenden die endgültige Kandidatenfolge und die tatsächliche
Stationsbindungs-generation `Program.context.bindingGeneration`; die fachliche
Rulegeneration der Emission ist ein anderer Wert. Tatsächlicher Writer/Verifier
prüft jede Ausgabe gegen die ursprünglich exportierte Schlüsselkopie. Die
Originalinstallation muss auch diese tatsächlich verwendete Keycopy und den
vollständigen FrozenFact einschließlich Generation erlauben.

`FrozenOriginal` bedeutet ausschließlich private, vollständig eingefrorene Belege.
Kein Journalcommit, Replayeintrag, Outboxexport, Loopback, Intent oder Admission
entsteht daraus. Nach validem Speichervorcheck werden Fehlerausgaben geleert.
Reentry oder ungültig überlappender Speicher wird vor Änderungen zurückgewiesen;
vorherige Flags oder Counts sind dabei kein neuer Erfolg. Immer Result.scope prüfen.

## Historische Prüfung

Historische Verifikation verwendet ausschließlich gespeicherte ursprüngliche
Bytes, Schlüssel und Belege. Sie ruft **keine** der aktuellen lookup/local/input-
Quellen auf. Mathematische Signaturen, gesamte Closure, ursprüngliche Install-/
Eingangsbelege, tatsächlicher Katalog, ClockWitness und der vollständig vergebene
Finalbereich werden erneut geprüft. Heutiger GM-Entzug löscht keinen bestätigten
Fortschritt; heutige Berechtigung legitimiert keinen damaligen Eingang.

Der Adapter sieht den tatsächlichen ProgressRecord, nicht den Bundle-Key.
Der Coordinator muss deshalb vorher Parentoperation, Request-/Katalog-/
Deployment-/Blobbindung der ganzen TransactionEnvelope gegen inneres Application
und überprüfte Bundlebytes vergleichen. Ein behaupteter ProgressRecord allein
beweist keinen Finalcommit.

Clockrelation allein ist kein Uhrbeleg: activeBefore gehört zum aktuellen
dauerhaften Vorzustand, activeAfter zur privaten Planungsfence. Originaler Boot,
monotone Differenz und Running-Lifecycle müssen wirklich belegt sein. Nach
Crash bleiben laufende Verarbeitung und fällige Timer bis zur bestehenden
berechtigten Wiederaufnahmeentscheidung geschlossen. Kein verstecktes Resume.

`VerifiedHistorical` erzeugt keine LiveAdmission und publiziert nichts. Danach
muss der tatsächliche Coordinator alle Planänderungen gegen historisches
Programm/Vorzustand vollständig nachrechnen und vergleichen; erst gemeinsame
erfolgreiche Recoverypublikation öffnet den bestätigten Zustand. Historischer
Replayplan bleibt inaktiv; attempted-Intents werden unknown, nicht erneut ausgeführt.

## Tatsächliche Nachweise und Rest

Erste eigene 28 Grenztests mit tatsächlichem Planner und echtem CNG-SHA prüfen
Metadaten, vollständige Kopie, historische Quellentrennung, fehlende Policy,
Fehlerclearing und konkrete Alias-/Viewfälle. Die Install-/Inputports sind darin
ausdrücklich synthetisch. **Diese Tests führen noch keinen Signierpfad aus.**
Erste unabhängige Sourceprüfung fand fehlenden Ledger-/Solved-Aliasschutz und
fehlende Own-Evidence-Viewprüfung; beide sind korrigiert und in den eigenen
Grenztests enthalten. Die folgende unabhängige Kampagne prüft den korrigierten Stand.

Nachgelagerte unabhängige Kryptogegenprüfung durch `quality_p1` am Body
`ed8079ad0fa3d868cfdf4411ae3581abee24c46fe04873a77058fca73fbe4b7a`:
sechs unveränderte Traces/24 vollständige Zustände, 20 tatsächliche Plannerpläne
durch Freeze und historische Prüfung, 12 echte Proof-/Outboxvorkommen über
Writer/CNG-Signierer/Verifier und unabhängiges Python-P256. Eigene unabhängige
CBOR-Orakel prüfen Original-IDs/Digests/Installationsreihenfolge; nach Zerstören
der Quelldaten und Schließen aller aktuellen Quellen/Currentpolicies gelingt
die historische Prüfung ausschließlich mit gespeicherten Bytes/XY.
**2666 C++-Prüfungen** im Tracepfad und **135** im tatsächlichen T2-/uint64-max-
Pfad. Zwei zusätzliche replante Fälle sichern auch signierte Ursachen außerhalb
des hinreichenden direkten Regelwitness. Signierte Zyklen, zukünftige eigene
Sequenzen, falsche angebotene Schlüssel und widersprüchliche ID-/Payloaddigests
bleiben in aktueller und historischer Prüfung geschlossen. Arenen exakt/-1,
volle Aliasbereiche, Quellen-/Kryptofehler, Wiederverwendung, Reentry sowie
Final-/Clockfehler sind geprüft. Descriptorplatzmangel wurde dabei von Range
auf Capacity korrigiert; echte Generation-/Zählerfehler bleiben Range.
Tatsächliche Applicationblobs der Basistraces: 1647 bis 5062 Byte, vollständige
Closurevarianten 5787 Byte und T2/uint64-max 2411 Byte.
Der [unabhängige Fixturebericht](../../test/fixtures/signed-rules-evidence-independent/README.md)
hält die tatsächlichen signierten Kategorien, Originalbytes und Reproduktion fest.
Installations-/Input-/Clock-/Finalmetadaten sind dabei ausdrücklich synthetische
begrenzte Testannahmen, keine echte GM-/Event-/D1-Annahme.

Offen: produktive Herkunfts-/Key-/Inputowner, derselbe Korpus mit nativem PSA,
vollständiger D1-/Control-/Effectanschluss, Gesamtbudget, Geräte- und Powerloss-
Nachweis. Kein Upload, Release, main-Merge oder Abschluss von #93.
