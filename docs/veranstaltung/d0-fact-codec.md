# D0-Fact-Codec und Signaturprüfung

Status: **implementiert und hostseitig unabhängig geprüft; native PR-Prüfung offen**,
31.10.2026.
Auftrag [#93](https://github.com/Sven401/Larp-Escape/issues/93),
Arbeitsbasis `d74026b0f800f478d383641b0e231c151af6cb8e`, auf PR #133
gestapelt. Führender Entwickler `developer_p1a3`, anderer Gegenprüfer
`quality_p1`, gezielter Kryptografie-/Bytegrenzenreview `module_digest_review`;
technische Grundsätze und Koordination übergeordneter Maintainer.

## Auftrag und Aussagegrenze

Der [bestehende D0-Sicherheitsvertrag](d0-sicherheit-migration-v1.md) beschreibt
COSE_Sign1 mit ES256 und deterministischem CBOR. Dieser Schritt implementiert
den tatsächlichen Fact-Bytepfad einschließlich Signaturprüfung. Er definiert
keine neue Vertrauensregistrierung und keinen Ersatzfreigabeablauf.

Erfolg heißt `SignatureCheckedUnauthenticated`: Die tatsächliche Nachricht
entspricht dem geprüften Factformat und ihre Signatur passt zum übergebenen
Prüfschlüssel. Der Schlüssel ist eine Prüfkandidatenkopie, keine bestätigte
Zuordnung zu einem berechtigten Ursprung. Eine passende Schlüsselkennung
beweist ebenfalls keine Berechtigung.

Das Ergebnis darf weder als bereits zugelassener `CurrentSolved`-Eingang noch
als `event_transport::Verification::Verified` ausgegeben werden. Die vorhandene
`OriginVerifier`-Grenze bleibt ohne produktive Autorisierungs-/Replayintegration
verweigernd. Der bestehende BLE-PeerProof verwendet ein anderes Format und wird
nicht als D0-Verifizierer umgedeutet.

## Unveränderte Bytes und begrenzte Prüfung

Die vollständige Nachricht wird einmal aus einer synchronen Quelle in
aufrufereigenen Speicher übernommen. Die harte D0-Grenze beträgt 2048 Bytes;
nach genau 2048 Bytes wird noch EOF beziehungsweise Quellenfehler geprüft.
Ein weiteres Byte ergibt Überlänge. Bereits als überlang erkannte beliebige
Quellen werden nicht unbegrenzt leer gelesen. Keine Heapstruktur, kein DOM,
kein erneutes Lesen der Quelle und keine geliehenen Quellzeiger im Ergebnis.

Geprüft werden obligatorisches Tag 18, COSE-Array mit vier Elementen,
geschützte Header exakt `{1:-7,4:kid32}`, leere ungeschützte Map, eingebettetes
Payload als Bytefolge und rohe 64-Byte-Signatur. Das eingebettete Payload hat
exakt die D0-Maplabels 0 bis 9 mit `kind=1` und das vorgeschriebene Factarray.
Bestimmte Längen, minimale Integer-/Längenkodierungen und feste Mapordnung
verhindern alternative Kodierungen; unbekannte Felder, Zusatz-Tags, Floats,
doppelte Schlüssel und angehängte Bytes werden nicht akzeptiert.

Alle drei vorhandenen Faktentypen werden strukturell geprüft. Typ 1/2 haben
`value=true` und keine `unusedRanges`. Typ 3 hat `subject=origin`, keine
Abhängigkeiten und 1 bis 8 disjunkte aufsteigende Bereiche mit
`0 < first <= last < sequence`. Höchstens acht unterschiedliche Abhängigkeiten,
keine Selbstreferenz; der Vertrag verlangt für dieses Array keine zusätzliche
Sortierung. Die Dekodierung von Typ 2 ist keine lokale Overridefreigabe.
Generationen und Sequenzen erhalten den vollen positiven uint64-Bereich.
Andere Nachrichtenarten bleiben ein ausdrücklich nicht unterstützter Umfang.

Originalbytes, Offsets/Längen und kopierte Factfelder müssen nach Erfolg für
die anschließende Eigentümerprüfung verfügbar bleiben. Vor vollständig
gelesenem EOF, Format- und Kryptografieerfolg wird kein Erfolg veröffentlicht.
Quellen-, Format-, Bereichs-, Schlüssel- und Kryptografiefehler bleiben
unterscheidbar. Jeder Fehler setzt den veröffentlichbaren Ergebniszustand und
seine Digests zurück; Wiederverwendung nach Fehlern ist ein eigenes Prüftor.

## Tatsächlicher Kryptografiepfad

Die feste öffentliche COSE_Key-Darstellung hat 77 Bytes:

`canonicalCBOR({1:2,3:-7,-1:1,-2:x32,-3:y32})`

Ihr SHA-256 muss mit `kid` übereinstimmen. Der Signaturhash entsteht aus
`["Signature1", rawProtected, bstr("larp.event.v1"), rawPayload]`.
Das externe AAD sind genau 13 ASCII-Bytes. Die ursprünglichen geschützten
Header- und Payloadbytes werden nicht semantisch neu formatiert.
Separat entsteht der Fact-Digest aus `SHA-256(rawPayload)`, nicht aus der
COSE-Hülle oder Signatur. Zwei gültige Signaturen desselben Payloads begründen
keine unterschiedliche Factidentität.

Ein eigener D0-Adapter verwendet öffentliche PSA-APIs für P-256-Schlüsselimport,
SHA-256/ES256-Prüfung und kontrollierte Bereinigung. Keine eigene ECC-Arithmetik.
Die Low-S-Einschränkung des vorhandenen BLE-PeerProof ist kein Bestandteil des
D0-Vertrags und wird hier nicht übernommen. Fehlgeschlagene Hash-/Schlüssel-
bereinigung darf keinen Erfolg ergeben.

Am 31.10.2026 gegen die Primärquellen abgeglichen:
[COSE-Signaturstruktur, RFC 9052 §4.4](https://www.rfc-editor.org/rfc/rfc9052.html#section-4.4),
[ECDSA-Signaturdarstellung, RFC 9053 §2.1](https://www.rfc-editor.org/rfc/rfc9053.html#section-2.1),
[deterministisches CBOR, RFC 8949 §4.2.1](https://www.rfc-editor.org/rfc/rfc8949.html#section-4.2.1).
Der bestehende Projektvertrag legt das engere D0-Profil fest.

## Betroffene Varianten und erforderliche Nachweise

Hardwarefreier C++11-Core, PlatformIO-Hosttests und native CMake-/PSA-Targets.
Das ESP32-Core-Prüfimage bindet über `larp_core` den Decoder und über
`larp_crypto` den D0-PSA-Adapter sowie `PsaPackageDigest` ein. Das separate
BLE-Prüfimage hat diese Abhängigkeiten nicht: Ein erfolgreicher BLE-Build ist
Regression, kein D0-Adapter-Buildnachweis. Keine Änderung an Radio,
Sitzungsrechten, Schlüsselpersistenz oder Aktivierung; kein Uploadauftrag.

- Unabhängig erzeugte tatsächliche COSE-Signaturen für alle Facttypen sowie
  gleiche Payloads mit gültigen alternativen High-S-/Low-S-Signaturen.
- Falsches AAD, `kid`, Payload, Schlüssel, Algorithmus und ungültiger Kurvenpunkt;
  exakte Originalbytes und separate Payloadidentität.
- Exakte Feldmengen, CBOR-Kanonizität, volle Zahlenbreiten, Identitäts-/Referenz-
  und Bereichsgrenzen, acht Abhängigkeiten/Bereiche und jeweils +1.
- Kürzungen, EOF-/Quellenfehler, 2048 und +1, keine Teilveröffentlichung,
  unveränderte Originalbytes und Wiederverwendung nach Ablehnung.
- Hash-/PSA-Fehler und Bereinigung; synthetische Fehlerports getrennt von
  positiven Signaturprüfungen mit tatsächlichen Kryptografiebackends.
- Bestehende P0-/Planer-/Artefakt- und Profilregressionen, Header, Architektur,
  Format, Host-/SDK-Builds und aktuelle PR-Pflichtprüfungen.

## Ausgeführte Hostprüfungen

Der formatierte Produktstand ist eingefroren. Führender Bearbeiter und
unabhängige Gegenprüfer bleiben wie oben getrennt; keine offenen P1/P2 im
dokumentierten Umfang. Die Quellen sind keine Referenzprojektion:

- Vollständiger CMake-Build mit **18/18 CTest-Zielen** erfolgreich, einschließlich
  des tatsächlichen D0-Codecs, vorhandener Paket-/Planer-/Persistenzregressionen
  und des separaten Adapter-Lebenszyklusaudits.
- PlatformIO **21 Suiten / 230 Fälle**, alle erfolgreich; fünf neue synthetische
  D0-Fälle sind Fehler-/Ablaufprüfungen, keine Behauptung echter Signaturannahme.
- Unabhängig **225 feste tatsächliche COSE-Bytefälle**, davon **22 positive mit
  echten CNG-SHA-256/P-256-Signaturprüfungen**. Alle Payloadlabels einzeln fehlend,
  doppelt, unbekannt ersetzt und nichtminimal kodiert; außerdem Header-/Container-,
  Null-/Bool-/Zahlen-/Schlüssel-/Kurvenpunktgrenzen. Abweichungen: **0**.
- **1107 Präfix-/Wiederverwendungspaare**, **1109 Jede-Byte-Quellenfehler-/
  Wiederverwendungspaare**, **15 Hashfehler-/Wiederverwendungspaare**, **sieben
  Krypto-/Resetfehlerpaare** und **drei genaue 2048/2049-EOF-/Limitpaare** grün.
  Originalbytes, Slices, kopierte Factfelder, Payload-Digest und tatsächlich an
  die Kryptografie übergebener Signaturhash wurden unabhängig exakt geprüft.
  Nach Rückkehr überschriebene Aufruferquellen/-schlüssel verändern das Ergebnis
  nicht. Der synchrone Vertrag verbietet Mutationen während des Aufrufs.
- Unabhängig **105 Lebenszyklusassertions gegen den tatsächlichen PSA-Adapter**
  mit absichtlich nichtkryptografischer PSA-Fehlernaht: Import-/Verifikations-/
  Bereinigungsfehler, unveränderte Puffer, unsichere Schlüsselfreigabe, gesperrter
  erneuter Import und Wiederverwendung nach erfolgreicher Bereinigung. Dies ist
  kein Signaturgültigkeitsnachweis.
- P0-Modell **18/18**, Architektur **165 Dateien / 350 Includes**, **96**
  unabhängig kompilierte C++11-Header; Formatbaseline **355 Dateien** plus
  ausdrücklich geprüfte neue Adapter-/Unity-/Auditquellen, clang-format 19.1.7.

[Unabhängige tatsächliche Bytefixtures und Reproduktion](../../test/fixtures/d0-fact-review/README.md),
[getrennter Adapter-Fehlernachweis](../../test/fixtures/d0-crypto-faults/README.md).

Die festen positiven Maximalformen des unabhängigen Korpus haben **806 Bytes**
mit acht breiten Abhängigkeiten beziehungsweise **470 Bytes** mit acht breiten
Bereichen. Das feste erlaubte Factschema ist selbst flach und begrenzt: Es wird
kein synthetischer positiver 2048-Byte-/Tiefe-8-Fall durch unerlaubte Füllfelder
erfunden. 2048/2049 belegen den begrenzten Quell-/EOF-Vertrag an abgelehnten Bytes.

Host32/GCC5.1: `FrozenFact` **2888 B**, `Fact` **760 B**, `Result` **12 B**,
Prüfschlüssel **64 B**. Die Ausgabe gehört dem Aufrufer, nicht auf einen kleinen
Taskstack. Direktes Zurücksetzen im vorhandenen Speicher vermeidet ein temporäres
vollständiges Ausgabeobjekt. Einzelne statische Compilerframes: `checkFact`
**240 B bei O0 / 320 B bei O2**, `parse` **208 B bei O0**, `clear` **16 B bei O0**.
Verschachtelte Aufrufe und Kryptografie kommen hinzu. Kein kumulierter ESP-Stack-,
Heap-, Zeit- oder Gerätelastnachweis.

Geprüfte SHA-256 nach Formatierung:

- `D0Fact.h`: `6166707435763174fa9cdf53ae40a8823616e03dd5cf65a0ca95c39f644d38ef`
- `D0Fact.cpp`: `c259fceffb730d5e392c88f38564805d7d1906e3052937a04e423d2373f94403`
- `PsaD0FactCrypto.h`: `ca7230c0e6c009c1b9ce68370462c671ce3b55030617b0b967bb867c3b0c6d97`
- `PsaD0FactCrypto.cpp`: `3026da90503e5db2ff2cd0e3b4218f26c412966a68f5669d8a768fa0c3e98356`
- Feste Bytevektoren: `49919ba844631ac6783fbcf6f0e7aac651a9444260e36c6799237717e841a242`

Die echte PSA-Annahme desselben Korpus und die aktuellen SDK-/PR-Builds sind
bei Erstellung dieses Commits noch offen. Ein lokales SDK mit abweichendem Pin
zählt nicht als qualifizierter PSA-/SDK-Nachweis. Nachgelagerte Ergebnisse
werden dem veröffentlichten Commit im PR und in #93 zugeordnet.

## Anschließende Eigentümerarbeit

Autorisierte Schlüsselzuordnung, aktuelle Veranstaltung/Run/Paket-/Stations-
bindung und Epochen-/Typ-/Subjectrechte, Entzug, Stream-/Replay-/Konfliktprüfung,
Abhängigkeiten und erneute Prüfung unmittelbar am dauerhaften Commit bleiben
erforderlich. Die ursprünglichen Bytes müssen dafür erhalten bleiben.
Receiptprüfung benötigt einen eigenen Codec und eine Korrelation mit tatsächlich
dauerhaft bekannten Aufträgen; bis dahin bleibt sie unverfügbar.

D1-Anwendungsreducer, F5a, Ressourcen-/Latenz-/Stackqualifikation und reale
Mehrboxabnahme bleiben offen. Kein dritter Abnahmefall vor P3 und Freeze.

**Gesonderter Nutzerbeschluss, 31.10.2026:** Die manuelle Ersatzfreigabe ist
ausdrücklich zur versionierten Ausarbeitung und Umsetzung beauftragt: bewusste
Bestätigung durch eine an der Zielbox aktuell berechtigte Spielleitung,
ausgewiesener lokaler T2-Ersatzbeleg und gemeinsame einmalige fachliche Freigabe
mit denselben Wirkungen wie das Original. Ein verspätetes Original darf keine
zweite Wirkung auslösen. Quelle: Antwort „Manuelle Ersatzfreigabe umsetzen
(empfohlen)“ in Sitzung `01a192a7-694c-73b2-b7b4-05d67b091cf5` auf den konkret
beschriebenen Vorschlag. Sprachversion, Besitzeradapter und atomarer D1-Pfad
gehören zum anschließenden Arbeitspaket; dieser D0-Codec implementiert sie nicht.

Die neue Kryptografieimplementierung wird als eigener prüfbarer PR vorbereitet;
die ausdrücklich vorbehaltene menschliche Mergefreigabe nach
[Mergepolicy](../quality/merge-policy.md) wird dadurch nicht ersetzt.
