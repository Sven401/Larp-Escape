# Verifizierter Nachweis vom Kryptografieworker zum ursprünglichen SDKaktor

Stand: 7. Oktober 2026, Basis a161539. Hardwarefreier vertikaler Schnitt;
physische Funk-/SMP-/Zeit-/Stack-/Bondabnahmen bleiben offen.

## Besitzer und Kopiervertrag

`FreeRtosPeerProofGrantRuntime` bindet genau einen Kryptografieworker. Er allein
liest `PeerProofOwner.readVerified`. Der SDKaktor erhält eine unveränderliche
Kopie; er liest den veränderlichen Nachweisbesitzer weder direkt noch über einen
Getter in einem Context. Der gemeinsam gelesene Context enthält ausschließlich
den im Konstruktor kopierten unveränderlichen Bootwert und geschützte
Source-/Registrykopien. Ein fremder Worker kann weder posten noch bestätigen
oder abbrechen. Konfiguration vor der ersten Originalverbindung, einmal pro
Sourcelebenszeit; Source/Context/Worker müssen für diese Lebenszeit bestehen.
Keine implizite Gerätekennung, Rolle oder Bootkennung aus Appdaten.

| Teil des internen Auftrags | Bindung |
|---|---|
| Actororigin | Gerät, Registrierung, Interface, Lebenszyklus, vollständige ursprüngliche geschlossene Authority |
| VerifiedPeerProof | ursprünglicher Boot/Handle/Verbindungstoken, Peercredential mit Generation und öffentlichem Schlüssel, Nachweisnummer, Issue-/Verifyzeit |
| Grantmetadaten | nicht umlaufende Jobnummer und einmaliger Eingangszeitpunkt |

Der reine `PeerProofGrantJob` belegt188Byte auf dem geprüften Nativeziel und ist
kein Appwireformat. Ein fester privater Sourceslot hält Auftrag und Bestätigung.
Ein nicht umlaufender opaker SDKpostingtoken verhindert, dass ein verspäteter
Callback einen neu belegten Slot konsumiert. Pending/Consuming bleiben belegt;
gleicher vollständiger Auftrag ist idempotent, anderer Auftrag Busy. Keine
Zeiger auf geliehene Proofdaten, keine zusätzliche dynamische Queue und keine
Wartezeit im Posting. Abgewiesenes Posting verbraucht seine Workerjobnummer;
der Worker hält die Originalkopie für einen exakten Abbruch. Keine stille
Wiederholung desselben fehlgeschlagenen Nachweises.

## Zeit und Freigabe

Die Warteschlangenfrist ist5000ms, die ursprüngliche Nachweisfrist30000ms ab
Issue. Beide gelten zugleich, modular über uint32-Wrap; Eingang darf den
Nachweis weder erneuern noch einen noch nicht verifizierten Zeitraum übernehmen.
2000ms der Spielbefehle werden nicht auf diesen Vertrag übertragen. Wiederholung,
Polling und SDKposting erneuern keine Frist. Diese Werte sind Policygrenzen,
keine gemessenen Laufzeiten oder garantierte Taskplanung.

Der tatsächliche SDKaktor prüft Originalbindung, Boot, Registrycredential,
Issue-/Eingangsfrist und die aktuelle SDKsecurity erneut. Der bereits gesicherte
ursprüngliche SMPmodus und die aktuelle SDKverschlüsselung/key16 bleiben
zwingend. Erst danach ruft er den sole IdentityAccess-Besitzer auf; die
vollständige ursprüngliche Authority wird atomar verglichen. Ausdrückliche
GMzuordnung kommt allein aus dem gespeicherten Register. Ein echter
Schlüsselnachweis ohne diese Zuordnung bleibt ohne Spielleitungsrecht.

Posting allein eröffnet keine Rechte. Erfolgreiche Actorvergabe veröffentlicht
ihre neue Authority und danach ihre gebundene ACKkopie. Workerconfirmation liest
nur diese ACK, prüft aktuellen eigenen Publisher, Original und Credential sowie
nochmals die Frist nach externen Beobachtungen. Eine bereits beobachtete gültige
ACK wird durch das Alter des damaligen Handshakes nicht nachträglich widerrufen;
Verbindungsende, Sicherheitsverlust und Rechteänderung bleiben wirksam.

Eine zusätzliche rote Prüfung zeigte: Beim atomaren Grant leert der Identity-
Besitzer seinen beobachtbaren Authoritysnapshot kurz. Gleichzeitiges Worker-
Polling verwechselte diesen Übergang mit einem endgültigen Sicherheitsfehler
(Pending0 erwartet, Rejected2 beobachtet). Im Consumingzustand hält Polling jetzt
nur den exakt aktuellen Source-/Verbindungs-/Boot-/Credentialauftrag innerhalb
seiner Frist pending. Es liest keinen mutableOwner und bestätigt daraus keine
Rechte. Die nachfolgende ACKprüfung bleibt vollständig. Derselbe Test wurde
nach der Korrektur grün; der zusätzliche Polling-Lambdaframe beträgt32Byte.

Ein verloren gebliebenes ACK wird beim Workerpoll nach Fristablauf abgewiesen
und seine exakt eigene bereits vergebene Authority geschlossen. Dieser Adapter
legt keinen selbständig laufenden Task oder Timer an: der Produktionsstartpfad
muss den Worker regelmäßig bedienen und dessen Stillstand überwachen. Ohne
einen weiteren Worker-/SDKaufruf ist kein selbsttätiger Zeitentzug zugesichert.

## Abbruch, Entzug und Konflikt

Vor Actorvergabe maskiert ein Abbruch nur den exakt ursprünglichen Prooflink;
er stört keine vorgemerkte Registryrollenänderung. Nach Vergabe schließt er
höchstens seine vollständige Actor-ACKauthority, niemals einen inzwischen
anderen Publisher. Der geschützte Sourcesnapshot liefert diese eigene Erwartung;
der Abbruch liest keinen veränderlichen IdentityAccess-Besitzer im fremden Task.
Der synchronisierte Publisherport ist das einzige taskübergreifende Schließmittel.

Eine erfolgreich geschlossene eigene Authority erhält eine unveränderliche
Close-ACK. Der Originalaktor übernimmt sie nur für genau die noch eigene
Verbindung und Authority nach erneutem aktuellen Publishervergleich. Bei einem
parallel vorgemerkten Rollen-Close darf genau dieser bestätigte Übergang die
alte erwartete Authority ersetzen; Gerät/Registrierung/Lebenszyklus/Interface
und eigene Verbindung bleiben identisch. Der Rollenauftrag selbst bleibt
unverändert. Fremde oder neue Verbindungen werden daraus nicht übernommen.
Damit kann der berechtigte Rollenentzug anschließend bestätigt gespeichert
werden. Eine normale Wiederverbindung verlangt neuen Token und neuen Nachweis.

Bestätigte Korrektur gegenüber dem historischen Entzugsschnitt:
`advanceAuthorityIfCurrent` erhält bei Conflict den Ersatzpublisher und Zielwert.
Der alte Identitybesitzer wird lokal retired. Der FreeRTOSport friert seine
Erwartung vor Lockbeobachtung ein. Allgemeines `update`-/Faultfencing und
Zählermaximum bleiben unverändert. Ein alter Grant darf ebenso wenig wie ein
alter Disconnect die Rechte eines neueren Besitzers schließen.

## Vertikale Belege und nächste Lücke

Neue Hostfälle prüfen unverifizierten Besitzer, falschen Boot/Credential/Scope,
Kopieren vor externen Ports, falschen Worker/SDKaktor, vollen Slot, Tokenmaximum,
Postingfehler, Replay nach Slotwechsel, Frist-/Wrapgrenzen, Abbruch während
SDKmetadata, verlorene ACK, genaue Close-ACKübernahme, gespeicherte GMzuordnung,
parallelen Rollenentzug und Publisheraustausch. Laufendes Spiel bleibt bei
Rechteentzug bestehen; künftige Spielleitungsbefehle bleiben geschlossen.

Die zusätzliche Nativevertikale verwendet die tatsächlichen MbedTLS2.28.4-
P-256prüfer, sechs kopierte Proofframes, diesen tatsächlichen Workeradapter und
die tatsächliche Sourcecallbackfunktion. Sie führt anschließend13 echte
Befehls-/ATTframes durch den Anwendungskern und speichert einen Rollenentzug.
SDKmetadata, Taskwechsel und NVS sind deterministische Modelle.

[Öffentliche Hostbeobachtung](fixtures/peer-grant/observation.json) und
[unabhängige Gegenstelle](../../scripts/check_peer_grant.py) korrelieren Kopie,
ursprüngliche Challengebytes, echte Signatur, drei Registerrecords und ACK.
Der isolierte SDKbuild verlinkt denselben geschlossenen Workerpfad; er startet
weder SDK/Funk noch NVS. Reproduzierbarkeit steht im
[Fixtureverzeichnis](fixtures/peer-grant/README.md).

Offen: eigene Authentisierungsattribute vor GMfreigabe, tatsächliche Challenge-/
Ergebnisausgabe, persistente Bondwiederverbindung, vertrauenswürdige Einrichtung,
gegenseitiger Servernachweis, ursprüngliche GATTausgänge/CCC sowie der geordnete
Produktionsstart mit Taskbedienung und gemessenen Ressourcen. Keine Appcode-
oder Appwireänderung in diesem Schnitt.

## Abschließender Prüf- und Speicherstand

Nach der Kopieroptimierung:463/463 Basishostfälle und470/470 mit echter
Kryptografie, ohne Fixtureexport.22 neue Basisfälle und eine neue
Nativevertikale. Die neue Gegenstelle weist90 falsche Beobachtungen/Typen/
Record-/Challenge-/Proofkorrelationen ab. Bestehende Gegenstellen/Schemas
weiter gültig;12 App-v2-Paare erneut byte-/SHAgleich, keine Appprüfung.
Architektur140 Dateien/293 Includes,83 unabhängige C++11-Header und294
Formatdateien erfolgreich.221 Original-MbedTLS-Dateien/acht native und zwei
SDKnegative geprüft;11 offizielle SDKquellen/fünf APIs/fünf Completionoffsets
und zehn Assoziationsmodelle unabhängig bestätigt. Diese Quellenprüfung
ist kein SMPmesslauf.

Fünf finale Builds SUCCESS/Exit0: Produktion55.148ByteRAM/1.229.077Flash
(93,8%), USBprobe21.480/291.881, Profilprobe107.560/407.077 und
isolierte SDKprobe46.920/1.079.297(82,3%) sowie Profilwerkzeug. Profilprobe
zusätzlich57.272Byte Startup-Profilheap plus Allocator/Dateisystem.
SDKdelta zu a161539:488ByteRAM/6.940Flash; Profilflash+8Byte. Getrennte
Ziele sind keine kombinierte Produktionsreserve.

Tatsächlicher ESP32-Compiler und ELF: Source916Byte(vorher664), fester
Grantjob188, Workeradapter224 und kopierter VerifiedProof140Byte.
Identity148, Registryowner1644 und geschützte Veröffentlichung772 bleiben
unverändert. Der neue Sourceslot ist statisch; keine eigene zusätzliche
Queue-/Heapallokation. SDKinternes Posting und Bibliothekspeicher sind davon
nicht als frei abgeleitet.

Eigene Compilerframes: Actorgrant336, Sourceposting240, Confirmation/Cancel
je272, Workersubmit416Byte. Private Scopeprüfung272→80 und rein strukturelle
Jobvalidierung224→32 sparen jeweils192Byte durch Weglassen einer unnötigen
zweiten Kopie. Alle öffentlichen Eingänge bleiben vor externen Ports
unverändert eingefroren. Rollenactor224, eigene Close-ACKübernahme112Byte.
Die Worker-Posting-/Credential-Lesekette enthält bereits1792Byte und der
Actor-Credential-Lesepfad1472Byte benannter eigener Frames, jeweils vor
weiteren Bibliotheks-/Sperr-/Aufruferkosten. Reader-/Kernframes256/176 und
frühere Registryframes/konservative4080Byte-Kette bleiben erhalten.
Gesamtstack, Worst-Case-Zeit und Heap-/SDKspitzen erfordern Geräteabnahme.

Finales SDK-ELF enthält Worker-submit/Source-grant, Original-SMPregister/
Wrapper und die vier ursprünglichen GATT-/BTMmetadatenAPIs. Ungeöffneter
Compile-/Linkpfad; Produktion verwendet weiterhin den bisherigen Controller.
Es gab keinen COMzugriff oder Upload in diesem Schnitt.
