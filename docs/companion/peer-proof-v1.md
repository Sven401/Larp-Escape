# Original gebundener Peer-Schlüsselnachweis v1

Aktuelle Fortsetzung: [Verifizierter Workerauftrag zum Originalaktor](peer-proof-grant-v1.md).
Die nachfolgenden Prüfzahlen und offenen Punkte beschreiben den jeweiligen
historischen Schnitt; dessen Nachweise werden hier nicht neu behauptet.

Stand: 7. Oktober 2026. Firmwarevertrag für einen schmalen Challenge-/Antwortpfad.
Der tatsächliche Verifizierer verwendet ECDSA P-256 und SHA-256. Im separaten
Hostpfad prüfen Mbed TLS2.28.4 und die unabhängige Gegenstelle cryptography45.0.7
dieselben öffentlichen Testbytes. Die isolierte SDKprobe bindet den tatsächlichen
ESP32-Verifizierer und die statische FreeRTOS-Queue ein, öffnet aber keinen Funk.

**Offen:** ursprüngliche SDK-Zuordnung der Sicherheitsevents, Einrichtung und
Persistenz vertrauenswürdiger Peer-Schlüssel/GM-Zuordnungen, tatsächliche
Authentisierungsendpunkte, Challenge-/Ergebnis-Ausgabe, gegenseitige
Geräteauthentisierung und physischer Funklauf. Der bestehende
[Dienst](gatts-service-sdk-v1.md) besitzt bislang nur Befehls- und Profilaufnahme.
Diese verlangen bereits GM-Rechte; sie sind kein Aufnahmeweg für diesen Beweis.
Appcode und produktiver Firmwareanschluss sind in diesem Schnitt unverändert.

## Vertrauen und Originalbindung

- `PeerCredential` stammt aus einer vertrauenswürdigen Registry: Peerlabel16Byte,
  öffentlicher Schlüssel65Byte und Generationu32 ungleich0. Ein empfangenes
  Label, eine MAC-Adresse, ein SDKhandle oder ein Bond vergibt keine GM-Rolle.
- Der kurze Quellcallback muss vor jeder Übergabe Gerät, Boot, Registrierung,
  SDKinterface und den vollständigen geschlossenen Verbindungstoken samt
  gepacktem SDKhandle kopieren. Nachträgliche Zuordnung zum aktuellen Handle
  ist unzulässig. `IPeerProofContext` prüft diese Originalbindung samt aktueller
  Dienstbereitschaft und den gesamten Registrydatensatz erneut.
- Schlüsselbesitz bleibt getrennt von original zugeordnetem, tatsächlich
  verschlüsseltem MITM-/Secure-Connections-/Bond-Nachweis.
  `bindPeerProofEvidence` darf nur frischen Nachweis aus `readVerified` mit
  vertrauenswürdigen Original-SDKflags verbinden. Es validiert allein die
  strukturelle Übereinstimmung; diese C++-Werte sind keine versiegelten Tokens.
  Der endgültige Zugangsbesitzer muss beide Quellen und Registry nochmals
  am atomaren Vergabepunkt prüfen. Die Flags des Hostbeispiels sind ausdrücklich
  **modelliert**. Eine Signatur setzt keine SDKflags und keine GM-Rolle.
- `IdentityAccess` erlaubt danach erst mit ausdrücklicher lokaler GM-Zuordnung
  die vorhandene kontrollierte Spielleiteraufnahme. Persistente Zuordnung und
  lokale Einrichtungsbedienung folgen als eigener kleiner Pfad.
- Die öffentliche Gerätekennung ist kein kryptografischer Servernachweis.
  Appsignierung, vertrauenswürdige Geräteeinrichtung und gegenseitige
  Authentisierung brauchen eine gesonderte Prüfung vor dem produktiven Zugang.

## Algorithmus und kanonische Bytes

Algorithmuscode1: ECDSA auf secp256r1/P-256 mit SHA-256 über die **exakten108Bytes**
des vom Firmwareeigentümer erzeugten Challenge-Dokuments. Es gibt keine
JSON-/DER-/Textnormalisierung. u16/u32 sind unsigned Little Endian.

Öffentlicher Schlüssel: exakt65Byte SEC1, `04 || X32 || Y32`, Koordinaten Big
Endian. Der tatsächliche Backend lädt P-256 und prüft den Punkt auf der Kurve.
Signatur: exakt64Byte `r32 || s32`, unsigned Big Endian; `0 < r < n`,
`0 < s <= floor(n/2)`. Ein mathematisch gültiger High-S-Zwilling wird abgelehnt.
Signierende Clients mit DER-Ergebnis müssen r/s exakt auf32Bytes auffüllen und
s gegebenenfalls durch `n-s` ersetzen. Es werden keine privaten Schlüssel in
Firmware, Fixtures oder Projektgedächtnis gespeichert.

### Challenge: exakt108Byte

| Offset | Größe | Inhalt |
|---:|---:|---|
| 0 | 16 | ASCII `LARP-PEER-P256-1` |
| 16 | 1 | Version1 |
| 17 | 1 | Challengekind9 |
| 18 | 1 | Algorithmus1 |
| 19 | 1 | Reserviert0 |
| 20 | 16 | Öffentliche Gerätekennung, nicht null |
| 36 | 4 | Boot high |
| 40 | 4 | Boot low; vollständiger Bootwert nicht null |
| 44 | 4 | Ursprünglicher Registrierungstoken, nicht null |
| 48 | 4 | Geschlossener Verbindungstoken, nicht null |
| 52 | 2 | Vollständiger gepackter SDK-Verbindungshandle |
| 54 | 1 | SDKinterface1..254, gleich niedrigstem Handlebyte |
| 55 | 1 | Reserviert0 |
| 56 | 4 | Challenge-Sequenz, nicht null |
| 60 | 16 | Peerlabel aus Registry, nicht null |
| 76 | 32 | Kryptografische Firmware-Nonce, nicht vollständig null |

### Proof: exakt72Byte

| Offset | Größe | Inhalt |
|---:|---:|---|
| 0 | 1 | Version1 |
| 1 | 1 | Proofkind10 |
| 2 | 1 | Algorithmus1 |
| 3 | 1 | Reserviert0 |
| 4 | 4 | Passende Challenge-Sequenz |
| 8 | 64 | Kanonische Signatur r/s |

Der Proof transportiert keinen frei wählbaren öffentlichen Schlüssel und keine
GM-Rolle. Der Verifizierer signiert/prüft ausschließlich das eigene eingefrorene
Challenge-Dokument und den eingefrorenen Registry-Schlüssel.

## Frames und begrenzte FreeRTOS-Aufnahme

Beide Dokumente verwenden den vorhandenen8Byte-Header: Versionu8, Kindu8,
Nachrichten-IDu16 ungleich0, Gesamtgrößeu16 und Positionu16, u16 jeweils Little
Endian. Die Gesamtgröße muss exakt108 (Kind9) beziehungsweise72 (Kind10) sein.
Kind8 bleibt dem bestehenden Profilantwortpfad vorbehalten. Frames höchstens
128Byte, Nutzdaten mindestens1Byte. Bei20Byte-Werten entstehen9Challenge- und
6Proofwerte; bei128Byte-Werten genügt jeweils1Wert. Dies ist Größenrechnung
und geprüfte Kodierung, **kein gemessener Funkdurchsatz**.

`FreeRtosPeerProofQueue` wird einmal vor Callbacks/Tasks eingerichtet und lebt
für die Prozessdauer. Ein Quellproduzent kopiert höchstens128Bytes, Original
und erste Empfangszeit in vier statische Plätze. Vorbereitete/versetzte Writes,
Null-/Überlängen, falscher Kind oder falsche Gesamtgröße werden verworfen.
`xQueueSendToBack` und `xQueueReceive` verwenden Wartezeit0. Eine volle Queue
meldet `Full`; der Callback prüft keine Signatur und vergibt keine Rechte.

Ein separater serialisierter Worker verwirft je Aufruf höchstens4alte Einträge
und reicht höchstens1aktuellen weiter. Die Aufnahmefrist beträgt1000ms ab der
originalen Quellzeit; Kopieren/Dispatch ersetzt diese Zeit nicht.
`PeerProofReceiver` besitzt einen72Byte-Puffer. Er akzeptiert geordnete
Fragmente und identische enthaltene Wiederholungen, verwirft Lücken/Überlappung,
erkennt Nachrichtwechsel und widersprüchliche Duplikate. Ein neuer original
bestätigter Verbindungstoken verwirft die alte Assembly; eine alte Verbindung
kann die neue Assembly weder überschreiben noch als aktuelle übernehmen.
Erst ein vollständiger72Byte-Proof erreicht den Krypto-Eigentümer.

Beim abschließenden Vertragsabgleich wurde ein Fehler im neuen uncommitteten
Framecodec nachgewiesen: Kürzere Gesamtgrößen kamen aus dem allgemeinen
Decoder als Success zurück (gezielter Vorherfall Expected Invalid/Was Success).
Die neue feste Größenprüfung erhält das Decoderziel und der Encoder leert
Fehlerausgabe/written. Receiver/Queue verlangten bereits vorher exakt72Byte;
die Korrektur sichert dieselbe Pflicht unmittelbar in beiden Peerframecodecs.

## Lebensdauer, Entzug und Entropie

Ein `PeerProofOwner` läuft ausschließlich auf diesem einen Worker. Er reserviert
für gültigen Original-/Registry-Kontext eine Sequenz vor Schlüssel-/Nonceprüfung;
diese wird nie umgebrochen. Fehler nach dieser Reservierung versiegeln den
Versuch. Nur ein neuer ursprünglicher Verbindungstoken ermöglicht eine neue
Challenge. Sequenzu32-Maximum schließt weitere Ausgabe bis zu neuem Bootowner.

- Absolute Challengefrist30000ms ab **vor** Schlüssel-/Entropiearbeit; Fragmente,
  Lesen, Wiederholen und Signaturarbeit verlängern sie nicht. Die Befehlsfrist
  2000ms wird hier nicht übernommen. Diese Fristen sind lokale Richtlinien,
  keine gemessenen Funk-/Kryptografielatenzen.
- Vor externer Signaturprüfung wird der einzige Versuch verbraucht. Nach Erfolg
  werden Frist, ursprünglicher Dienst/Verbindung und gesamter Registry-Schlüssel
  erneut geprüft. Entzug, Schlüsselrotation oder Dienstverlust veröffentlicht
  keinen verwendbaren Nachweis. Bereits veröffentlichte Rechte müssen vom
  tatsächlichen Zugangsbesitzer beim Entzug eigenständig geschlossen werden.
- Eine byteidentische bereits verifizierte Wiederholung benötigt keine weitere
  Kryptografie und erneuert weder Frist noch Nachweis. Eine widersprüchliche
  Signatur schließt den aktuellen Nachweis. Eine falsche Sequenz oder fremde
  Originalbindung verbraucht keinen neuen gültigen Versuch.
- `Esp32PeerProofNonce` verlangt tatsächlich aktivierten Bluetoothcontroller
  **und** Bluedroid vor und nach `esp_fill_random`. Ohne sie bleibt die Ausgabe
  unverändert und die Challenge geschlossen. Es wird keine Boot-Entropiequelle
  über bestehende ADC-/GPIObesitzer hinweg aktiviert. Die ursprüngliche
  Quellzuordnung bleibt eine separate Pflicht des späteren Anschlusses.

Die eigene Queue/Assembly/Ownerverwaltung verwendet festen Speicher. Die reale
Mbed-TLS-Prüfung kann MPI-/Kurvenheap anfordern und blockierend rechnen; ein
zusätzlicher Worker benötigt gemessenen Stack, Heapreserve und Laufzeitbudget.
Ein Ablaufdatum verwirft das Ergebnis, präemptiert die Bibliothek aber nicht.
Produktive Kombination mit Spiel/Dateisystem/Funk bleibt offen.

## Tatsächlicher Compile-/Speicherstand

Fünf finale Builds erfolgreich. SDKprobe43.688Byte statisches RAM und
1.060.053Byte Flash (80,9% des1.310.720Byte-Appslots), netto+1.272RAM und
+39.184Flash gegenüber dem Diensteinrichtungsschnitt7adaa6f. Produktion
55.148/1.229.077, alteUSBprobe21.480/291.881 und Profilprobe107.560/406.865Byte
RAM/Flash unverändert; Profilprobe weiterhin zusätzlich57.272Byte Startup-
Profilheap plus Allocator/Dateisystem. Getrennte Builds sind keine kombinierte
Reserve mit Funk und neuen Gerätebesitzern.

Frisches ESP32-ELF: Owner280Byte, Receiver124, Queue804 einschließlich vier
176Byte-Plätzen und RTOSkontrolle, Verifizierer/Nonceadapter/geschlossener
Kontext jeweils4Byte. Originalquellenport392 und Serviceowner388Byte unverändert.
`begin()` übergibt tatsächlichem `xQueueCreateStatic` Tiefe4 und Itemgröße176.
Native FreeRTOS-Ports sind deterministische Single-Thread-Kopiermodelle; der
SDKbuild bindet den echten FreeRTOS-Port, führt ihn jedoch nicht am Gerät aus.

| Funktion | Eigener ESP32-Compilerframe, Byte |
|---|---:|
| Challengeencode / -decode | 144 / 128 |
| Proofencode / -decode | 112 / 112 |
| Owner issue / verify | 256 / 256 |
| Owner challenge / readVerified | 80 / 240 |
| Securityevidence kombinieren | 240 |
| Receiver receive | 400 |
| Queue copy / take / Dispatch | 256 / 224 / 240 |
| Backend validKey / verify | 272 / 496 |
| Nonceadapter make | 80 |

Zusätzliche Peerframecodec-/Helperframes sind im lokalen Objekt-Dump separat
ausgelesen. Diese Werte sind einzelne ausgelesene Xtensa-`entry a1`-Frames. Aufrufketten,
Mbed-TLS-/SDKframes, Heap, tatsächliche Workerstackreserve und Signaturlatenz
sind zusätzlich zu prüfen. Verifizierer-, SHA256-, Kurvenprüf- und Entropiesymbole
sind tatsächlich im SDK-ELF vorhanden; SDK-/RFausführung bleibt offen. Der
vollständige Build vor der letzten Codecgrößenkorrektur meldete einen bestehenden Herstellerhinweis in Arduino2.0.14
`esp32-hal-uart.c:153` (`return` ohne Wert); eigene Sources bestehen mit Werror.

## Reproduzierbare Belege

- `PeerProofCases.h`:391Hostfälle einschließlich26neuer Policy-/Codec-/Queuefälle;
  zusätzliche2Fälle mit tatsächlichem MbedTLS ergeben393im separaten Ziel.
  Dort gelangen6Proofwerte über die tatsächliche Queue, den Empfänger und reale
  ECDSA-Prüfung; ausschließlich modellierte Transportflags plus ausdrücklich
  gesetzte GM-Rolle ermöglichen anschließend13Befehlswerte und Spielstart.
- `fixtures/peer-proof/`:108/72Byte-Dokumente, öffentlicher Testschlüssel,
  20Byte-Framefolgen und tatsächliche Hostbeobachtung. Die bekannte öffentliche
  Kurven-Testbelegung ist ausdrücklich niemals eine Deployment-Zugangsvorgabe.
- `scripts/check_peer_proof.py`: unabhängige Punkt-/Signaturprüfung,246verfälschte
  Kryptodaten und12fehlerhafte Framefolgen; Metadaten/Native-Testarrays müssen
  genau denselben Bytes entsprechen. Benötigt cryptography45.0.7; die bestehende
  Schema-/Fixtureprüfung benötigt diese zusätzliche Bibliothek nicht.
- `scripts/make_peer_proof_fixture.py`: deterministische öffentliche Testvektoren
  mit cryptography45.0.7. Regeneration schreibt genau diese Fixtures und
  `PeerProofVectors.h`; anschließend den normalen C++-Formatter anwenden.
  `verification-observation.json` entsteht nur aus dem tatsächlichen Nativefall
  bei ausdrücklich gesetztem `LARP_EXPORT_PEER_PROOF_FIXTURE=1`.

Nativeziel `host-peer-proof` verlangt externe offizielle Mbed-TLS2.28.4-Quellen
und ihr Originalarchiv in `LARP_PEER_MBEDTLS_DIR` und
`LARP_PEER_MBEDTLS_ARCHIVE`. Herkunft:
`https://codeload.github.com/Mbed-TLS/mbedtls/tar.gz/refs/tags/mbedtls-2.28.4`.
SHA256 `504bd29af6e7f9f3de1f0f7b7e16c73987a4194338681acd72b82383a49d55d5`.
Der tatsächliche Pre-Buildwächter prüft alle221Include-/Librarydateien exakt
gegen das Archiv und weist zusätzliche/fehlende/veränderte Dateien und
Symlinks zurück. Ein fehlender echter Backend führt zum Fehler.

Die SDKprobe verlangt den bestehenden libbt-Pin und zusätzlich
`libmbedcrypto.a` SHA256
`ef9cface38ac8a137309bad2fc6bc24969fcf13ad3206f9ee9a7e4c3f6cbf887`;
Header verlangen MbedTLS2.28.4. Der tatsächliche SDKbackend enthält ESP-ALT-
Implementationen; Native prüft die offiziellen Softwarequellen. SDKcompilation
belegt den Link, keine Ausführung der ESP-ALTpfade.

Prüfaufrufe (PIO, Compiler und Pythonpakete müssen verfügbar sein):

```text
pio test -e host-configured-control -f test_configured_control
pio test -e host-peer-proof -f test_configured_control
python scripts/check_peer_proof.py
python scripts/native_peer_mbedtls.py --audit --root <offizielle-quellen> --archive <originalarchiv>
python scripts/check_peer_crypto_sources.py --root <offizielle-quellen> --archive <originalarchiv> --sdk <arduino-framework>
```

Der letzte Prüfer verwendet isolierte temporäre Kopien;8veränderte Native- und
2SDK-Kryptografieeingaben werden durch dieselben tatsächlichen Wächter
zurückgewiesen. Originalquellen und installierter SDK bleiben erhalten.

## Aktueller Registryanschluss

Der neue [öffentliche Registerpfad](peer-registry-v1.md) liefert aktuelle persistierte Credentials und getrennte ausdrückliche GMrollen. Sein Hostpfad bindet Proofgültigkeit zusätzlich an `IdentityAccess::proofConnectionCurrent`; eine Rollenänderung entwertet die alte Verbindung auch bei unverändertem Schlüssel. Tatsächliche SDKsecurity und der Produktions-Actoranschluss bleiben offen.
