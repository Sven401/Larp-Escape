# Originalgebundene Challenge-Leseausgabe v1

Stand: 7. Oktober 2026; Basis a3d1667. Kleiner hardwarefreier vertikaler Schnitt:
kanonischer Vertrag, tatsächliche Source-/FreeRTOS-/SDKadapter, echter Proofowner
und unabhängige Toolgegenstelle. Funk, Android und Produktionsstart sind offen.

## Service und Appwerte

Die bestehende Service-UUID `407e6a1d-1cc2-43fd-856b-4aff85a20001` erhält
optional die **Challenge-Lesecharakteristik**
`407e6a1d-1cc2-43fd-856b-4aff85a20005`. Kommando und Profil behalten ihre
vorhandenen UUIDs. Der optionale Dienst braucht sieben Handles: Service plus
drei Charakteristikdeklarationen und Werte. Die vorhandene Minimalvariante
mit fünf Handles bleibt als historische Fixture/geschlossene Probe verwendbar.
Es gibt keine CCC/Notification in diesem Schnitt.

Challenge: READ, `ESP_GATT_PERM_READ_ENC_MITM`, Antwort BY_APP, Maximalwert128.
SDKrechte sind keine GMzuordnung. Der ausgelesene Wert ist **genau das bestehende
108-Byte-P256-Challengetranskript**, ohne zusätzliche Framehülle; Format und
Signatur bleiben im [Proofvertrag](peer-proof-v1.md) kanonisch.
Keine Credentials/öffentlichen Keys/Rollen im Leseantwortwert.

Die Gegenstelle liest mit ATT Read und bei Bedarf Read Blob. Auf dem gepinnten
IDF4.4.6 baut `attp_build_value_cmd` eine Antwort aus maximal MTU-1 Wertbytes.
Unser Source übergibt die verbleibenden108-Offset Bytes; der SDK begrenzt sie.
Bei MTU23 ergeben sich fünf **Größenmodell**-Fragmente:22,22,22,22,20 Bytes mit
Offsets0,22,44,66,88. Das sind keine gemessenen Funkpakete/Laufzeiten.

Ein langer Read am Offset108 liefert erfolgreichen Nullwert; Offset>108 oder
ein gewöhnlicher Read mit Offset!=0 ergibt ATT InvalidOffset7. Ohne verfügbare
aktuelle Challenge ergibt die ursprüngliche Anfrage InsufficientAuthorization8.
Nur diese beiden Fehlercodes und Ok0 sind zulässig. Andere Handles laufen durch
bestehende SDKpfade; ausgewählte Challenge-Reads werden auch bei Ablehnung
konsumiert, damit sie keine spätere Verbindung übernehmen.

## Besitzer und Originalkopien

Installation: einmaliger gefrorener `PeerChallengeReadSetup` mit Boot,
geschütztem `IPeerProofContext` und Prozesslebenszeit-Ingress. Erst tatsächlicher
Sourceaktor bestätigt Kommando-/Profil-/Challengehandles und Kontext, dann startet
der Serviceworker den Dienst. Ungültige/fehlende Bundlefelder, falsche UUID/
Handleordnung und späte Konfiguration scheitern geschlossen; kein SDKneustart.

Der Originalcallback liest den Read-Union mit Callbacktyp1 (kein raw ATT0x0a)
vor dem ersten externen Port. Er friert Boot/Gerät, vollständigen gepackten
Verbindungshandle und Geburtstoken, Registrierung/Interface, Lebenszyklus,
Adresse, Attributhandle, Transaktionsnummer, Offset/Longflag und Eingangszeit ein.
Geliehene Pointer verlassen den Callback nicht. Keine Kryptographie/Entropie/NVS
und kein SDKresponseposting beim Aufnehmen.

`FreeRtosPeerChallengeReadRuntime` hat vier statische Requestkopien und genau
einen Kryptoworker. Die Bindung verlangt denselben bereits gestarteten
Grantworker; ein fremder Worker kann auch beim ersten Begin nicht binden.
Proofowner, Grantcontext und Source gehören zum selben vertrauenswürdigen
Aufbau. Dieser Worker liest allein `PeerProofOwner.challengeSnapshot`.
Der Getter liefert einen Snapshot mit derselben Challenge/Credential/Issuezeit;
kein erneuter Nonce und keine verlängerte Frist. SDKaktor und andere Worker
lesen niemals den veränderlichen Proofowner. Pro Serviceaufruf höchstens vier
alte Queuewerte verwerfen, eine aktuelle Anfrage bearbeiten und ein SDKposting.
Keine eigene Heapallokation, kein Wait, kein zusätzlich erzeugter Task/Timer.
SDK-/Bibliotheksallokationen bleiben getrennt ungemessen.

Ein fester Sourceslot bleibt bis Ende des Callbackaufrufs belegt. Nicht umlaufender
opaker globaler Postingtoken; alte Arbeit kann eine frische Belegung nicht
konsumieren. Busy, volle Queue, falscher Worker, SDKwechsel und Postfehler
verwerfen den betreffenden Wert ohne Retry/Fristerneuerung. Posting ist weder
Peerzustellung noch GMfreigabe. Service-/Quellen-/Queue-/Workerobjekte dürfen
während möglicher SDKcallbacks nicht gelöscht/reinitialisiert werden.

Vor Ausgabe prüft der ursprüngliche SDKaktor dieselbe ursprüngliche Verbindung/
Adresse/Lebenszyklus/Boot/Registrierung, geschütztes aktuelles Credential und
frisches originales SMP sowie aktuellen SDK-Sicherheitszustand. Nach den
Metadataports nochmals Original, Credential und Zeit. Direkter tatsächlicher
`GATTS_SendRsp` erhält Originalhandle, Transaktionsnummer und gefrorenen Slice;
610-Byte-SDKresponse bleibt lokal bis Rückkehr, kein Pointer wird aufgehoben.
Entzug, Wiederverbindung oder verlorene SDKsecurity verwerfen positive Daten.
Diese lokalen Checks behaupten keine atomare Peerzustellung während externer
SDK-/Funkarbeit.

## Zeit und offene Schritte

Queue und SDKantwort: **1000ms ab Originalaufnahme**, eigene lokale Policy.
Challenge: **30000ms ab ursprünglichem Issue**, bestehende absolute Frist.
Beide modular uint32; Aufnahme/Post/Blobreads erneuern keine der Fristen.
Die Spielbefehlsfrist2000ms wird hier nicht übernommen. Ohne Bedienung des
Workers oder SDKcallback kann kein selbsttätiger Zeitentzug zugesichert werden;
Produktion braucht Taskstart/regelmäßige Bedienung/Stillstandsüberwachung.

Die Challenge wird in diesem Schnitt vom vertrauenswürdigen Kryptoworker aus
dem geschützten Register ausgelöst. **Clientseitige Issue-/Proofframeaufnahme und
Ergebnischarakteristik sind der nächste Schritt**, nicht bereits implementiert.
Vorherige [Grantvertikale](peer-proof-grant-v1.md) bleibt gültig. Bondreencryption,
gegenseitiger Servernachweis und vertrauenswürdige Provisionierung folgen.

## Reproduzierbare Prüfung

`test/test_configured_control/PeerChallengeReadCases.h` kompiliert tatsächlichen
Source, Reader, Proofowner und Grant; nativer Fall nutzt MbedTLS2.28.4, originale
SDKsignaturen/SMP-/Taskmodelle und öffentliche Testwerte. Er exportiert nur mit
`LARP_EXPORT_PEER_CHALLENGE_READ_FIXTURE=1`; normaler Lauf vergleicht exakte Bytes.
[Fixtureherkunft](fixtures/peer-challenge-read/README.md),
`scripts/check_peer_challenge_read.py` (cryptography45.0.7) und erweiterter
`scripts/check_gatt_source_abi.py` prüfen unabhängig Wert/Signatur/Scope sowie
Read-ABI und unveränderte gepinnte offizielle SDKquellen.

Entropie, echte RF-/Android-/SMP-/NVSzustellung, vollständige Stack-/Heapspitzen,
Scheduler-/WCET-/Energieabnahme und Produktionsdienst bleiben getrennt offen.
