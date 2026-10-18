# D0 v1: Format, Ursprung, Rechte und Migration

**Status: mit PR #72 übernommener D0-Vertrag; keine neue Firmware-/App-Funktion.**
Stand/Basis/Auftrag wie im [D0-Hauptvertrag](d0-vertrag-v1.md).
Bestehende [Wire v2](../led/ble-wire-v2.md), Profil-v1-Schemata, BLE-Requests,
HMAC-Domänen und NFC-Testfixtures bleiben bytegleich.

## 1. Neues Ereignisformat · D0-FORMAT

Formatname `larp.event.v1`. Der Vertrag transportiert eine unveränderliche
COSE_Sign1-Nachricht mit eingebettetem CBOR-Payload. Dieselben vollständigen Bytes
über ESP-NOW und WLAN; Relays ändern den signierten Teil nicht. Fragmentnummer,
Hoplimit und Transportkennung gehören zu einer separaten D2-Hülle. Sie sind
keine Ereignis-ID und verleihen keine Autorität. D0 definiert keine neue BLE-ID.

Technische Auswahl für diesen Vertrag: **COSE_Sign1 mit ES256/P-256/SHA-256**,
keine Algorithmusverhandlung oder Rückfall auf unsignierte/Gruppenschlüssel-
Nachrichten. Grundlage: [RFC 9052 §4.2/4.4](https://www.rfc-editor.org/rfc/rfc9052.html#section-4.2),
[RFC 9053 §2.1](https://www.rfc-editor.org/rfc/rfc9053.html#section-2.1)
und [CBOR RFC 8949 §4.2.1](https://www.rfc-editor.org/rfc/rfc8949.html#section-4.2.1).
Dies ist eine Protokollauswahl, kein Nachweis einer verlinkten COSE-Bibliothek.

COSE-Sign1 Tag 18 ist obligatorisch. Geschützte Header enthalten genau `alg=-7`
und `kid` als 32-Byte-Digest des kanonischen öffentlichen COSE_Key; ungeschützte
Header sind leer. Schlüssel hat EC2/P-256, `alg=-7`, x/y je 32 Byte; sein Hash
bezieht sich auf genau die Map `{1:2, 3:-7, -1:1, -2:x, -3:y}` in deterministischem
CBOR. `kid` ist nur Suchhilfe im **lokal autorisierten** Register, kein Vertrauen.
`external_aad` sind exakt die ASCII-Bytes `larp.event.v1`; Sig_structure gemäß
RFC 9052, Signatur 64 Byte `R || S`, nicht ASN.1-DER. Bibliothek prüft gültigen
Kurvenpunkt, Parameter, Wertebereiche und Signatur; keine eigene ECC-Arithmetik.
Signaturprüfung erfolgt an originalen geschützten/Payloadbytes, niemals an einer
nachträglich semantisch gleich wirkenden Neuformatierung. Alternative gültige
Signaturen desselben Payloads erzeugen kein neues Ereignis.

Payload ist eine deterministische CBOR-Map mit ausschließlich Integerlabels.
Keine unbekannten/duplizierten Schlüssel, unbestimmten Längen, Floats,
Zusatz-Tags oder angehängten Bytes. Vor Signaturprüfung Gesamtlänge begrenzen;
Decoder darf keine unbegrenzten Strukturen allozieren. Diese strenge v1-Auswahl
ist enger als generisches CBOR/COSE. JSON unten ist nur lesbare Prüfdarstellung,
kein alternativ erlaubtes Signatur-/Transportformat.

| Label | Feld | Typ / normative Grenze |
| --- | --- | --- |
| 0 | `version` | unsigned 1 |
| 1 | `kind` | unsigned 1=`fact`, 2=`observation`, 3=`command`, 4=`receipt` |
| 2 | `event` | bstr16, nichtnull |
| 3 | `run` | bstr16, nichtnull |
| 4 | `origin` | bstr16, nichtnull; mit gepinntem Schlüssel verknüpft |
| 5 | `station` | bstr16 oder null; Facts/Spielkommandos verlangen Station, Zentrale darf nur stationlose Beobachtung/Quittung liefern |
| 6 | `generation` | unsigned64 >0 bei Station, sonst null |
| 7 | `bindingDigest` | bstr32 bei Station, sonst null |
| 8 | `packageDigest` | bstr32, SHA-256 der deterministischen Manifestbytes; Dateien über ihre Manifesthashes gebunden |
| 9 | `body` | eine der folgenden festen Arrays |

`fact`: `[epoch:bstr16, sequence:uint64>0, factType:uint, subject:bstr16,
value:bool, dependencies:[FactRef...], unusedRanges:[[first,last]...]]`.
`factType=1 task_solved`, `2 local_override`, `3 unused_range` sind die
einzigen v1-Typen. Typen 1/2 haben `value=true` und leere unusedRanges.
Typ 3 hat subject=origin, value=true, keine dependencies und 1..8 disjunkte,
aufsteigende Bereiche mit `0 < first <= last < sequence`. Historisch bereits
bekannte Facts in einem solchen Bereich erzeugen Konflikt statt Löschung.
Auch umgekehrt ist jeder später eintreffende Fakt an einer bereits als unbenutzt
geschlossenen Position ein Ursprungskonflikt, unabhängig von seiner Art.
Andere Regeln/Faktentypen benötigen eine neue ausgewiesene Fähigkeit/Version.
`FactRef=[event:bstr16, origin:bstr16, epoch:bstr16, sequence:uint64>0]`.
Höchstens acht unterschiedliche Abhängigkeiten, keine Selbstabhängigkeit;
event/run/Paketbezug der referenzierten Inhalte zusätzlich prüfen. Zyklische
oder nicht erfüllbare Abhängigkeiten bleiben ohne Wirkung und werden sichtbar.

`observation`: `[boot:bstr16, sequence:uint64>0, state:uint, challenge:bstr16|null]`.
`state`: 0 unknown, 1 ready, 2 running, 3 paused, 4 stopped, 5 fault.
Andere Statusfelder erhalten eigene Version; keine Kopie des gesamten
BLE-Status unter falscher Versionsnummer. Boot ist hier frisch pro Ursprungstart,
keine persistente Fact-Epoche. Aktive Challengeprüfung nach D0-FRESH zusätzlich.

`command`: `[commandId:bstr16, targetBox:bstr16, targetBoot:bstr16,
grant:bstr16, targetRevision:uint64, action:uint, resource:uint16]`.
action 1=`play_once`, 2=`light_once`; resource 1..65535 ist ein vorab im Paket
zulässiger boxlokaler Ressourcen-/Effektverweis. Kein Start/Reset/Rechteauftrag
und kein pauschales Ausführen beliebigen fremden Profilcodes. Targetboot/-grant
müssen noch gültig und an denselben Ursprung/Schlüssel/Kontext gebunden sein.

`receipt`: `[ref:FactRef|commandId:bstr16, digest:bstr32, targetBox:bstr16,
receiptId:bstr16, stage:uint, reason:uint]`. `targetBox` ist der Adressat der
Rückmeldung, origin der bestätigende Knoten. stage 1 transport_received,
2 durable_received, 3 applied, 4 effect_observed, 5 rejected, 6 deferred,
7 effect_unknown; reason 0 none, 1 context, 2 trust, 3 gap, 4 capacity,
5 expired, 6 conflict, 7 effect_uncertain. Eine stage-4-Aussage setzt einen
eigenen beobachteten Gerätebeleg voraus, kein bloßes Senden.
Erlaubte Kombinationen: Stufen 1..4 nur mit reason=0; rejected mit reason=1..6,
deferred mit reason=3/4, effect_unknown mit reason=7. Andere Kombinationen sind
Formatfehler. Stufen 5/6/7 sind eigene Ergebniszweige, keine höheren Erfolgsstufen.
`ref` und `digest` müssen gemeinsam den ursprünglichen offenen/belegten Auftrag
oder Fakt bezeichnen; beim Command ist `digest` ebenfalls SHA-256 seiner
deterministischen Payloadbytes. Numerischer Stufenvergleich ersetzt keine
fachliche Ergebniszusammenführung nach D0-ACK.

Fact-Digest ist SHA-256 der ursprünglichen deterministischen Payloadbytes.
Gleiche Fact-ID bei anderem Digest ist Konflikt, auch bei gültiger Signatur.
Ein Relay kann nur eine eigene `transport_received`-Bestätigung abgeben, niemals
die `applied`-Bestätigung einer Zielbox. Stage/Grund einer Quittung gehören zum
signierten Inhalt. Quittungen sind keine dauerhaften Spieltatsachen und verbrauchen
keine Fact-Sequenz; ihre eigene ID dient nur der Rückmeldungsdeduplizierung.

**Grenzen v1:** gesamte getaggte COSE-Nachricht höchstens 2048 Byte; Tiefe
höchstens 8 inklusive Hülle; höchstens acht Abhängigkeiten/unusedRanges.
Das ist ein Abwehrlimit, keine vorhandene RAM-/ESP-NOW-Kapazitätszusage.
2048 Byte dürfen nicht als einzelnes ESP-NOW-Paket vorausgesetzt werden.
D2 muss kleineren nutzbaren Transferumfang, begrenzte Reassemblierung, Timeout,
Queuebudget und Fairness real nachweisen. Passt die Umsetzung nicht, Fähigkeit
nicht anbieten; keine stille Kürzung/signierte Neuinterpretation.

## 2. Lokales Ursprungsvertrauen · D0-TRUST

Ein Zielregister bindet je Veranstaltung `kid`, öffentlichen Schlüssel,
originBox, erlaubte Stationsbindung/Epochen, Faktentypen/Objekte und getrennte
Erlaubnisse für Beobachtungen, Kommandos und Quittungen. Fact-Erlaubnis gewährt
keine GM-Rechte, kein Geräteprovisionieren und keine Kommandoberechtigung.
Allein eine passende Signatur oder ein bekannter Transportpeer genügt nicht.
Eine Zentrale erhält keine Station-/GM-Autorität durch ihre Netzrolle.

Einführung: Ursprung erzeugt privaten Signaturschlüssel auf der Box; öffentliche
Zuordnung über einen unabhängig authentifizierten Einrichtungspfad prüfen.
Kein Trust-on-first-use aus Advertising, MAC, QR-Text oder selbst signiertem
Funkpaket. Jede Zielbox übernimmt ihren Vertrauensdatensatz ausdrücklich durch
eine dort aktuell berechtigte Spielleitung, mit Ausgangsstandprüfung, neuer
lokaler Vertrauensrevision und dauerhaftem Rücklesen gemäß #28. Einrichtung
ohne vorhandenen Zugang nutzt den Einrichtungsschlüssel; letzter Zugang und
Recovery aus dem bestehenden Konzept bleiben unangetastet.

Der private Signaturschlüssel bleibt im normalen Betrieb auf der Ursprungsbox, nicht im Paket, Handyexport,
NFC-Testtag, Status oder Journal. Ein Handy darf öffentliche Datensätze vermitteln,
aber keine öffentliche Zuordnung allein als bereits authentifiziert erklären.
Transportverschlüsselung/Vertraulichkeit ist separat: Signaturen verschlüsseln
keine Spielinhalte. V1-Nachrichten enthalten keine Geheimnisse oder unnötigen
Personendaten. Für vertrauliche Inhalte braucht D2 vor Freigabe einen zusätzlichen
standardisierten Schutzvertrag; COSE_Sign1 allein liefert ihn nicht.
Chipgebundene private Vollflash-Sicherungen können Schlüsselmaterial enthalten;
sie unterliegen dem bestehenden geschützten Rückweg, nicht dem öffentlichen
Paketformat. Ihre Wiederherstellung hebt die Rollback-/Epochenregel D0-ID nicht auf.

Rotation: neuen Schlüssel vorbereiten und Besitz unabhängig prüfen, je Zielbox
neuen Datensatz bewusst übernehmen. Kein Schlüssel aus dem Funk darf sich selbst
zum Nachfolger erklären. Übergangsschlüssel nur ausdrücklich je Ziel zugelassen;
kein stiller gleichzeitiger universeller Zugang. Alte ungeprüfte Nachrichten
nach lokaler Sperre abweisen. Muss Historie unter altem Schlüssel übernommen
werden, braucht es eine bewusst erlaubte, begrenzte Migrationsfreigabe mit
Epochen/Sequenzbereichen; Absenderdatum ist keine Berechtigung.

Entzug: lokal neue Vertrauensrevision dauerhaft übernehmen; schon kopierte
Eingänge vor Anwendung erneut dagegen prüfen. Unbekannte Entzüge sind offline
nicht durchsetzbar; je Ziel bestätigt/ausstehend/unklar anzeigen. Bereits
angenommene Tatsachen werden dadurch nicht rückwirkend gelöscht. Ein kompromittierter
zugelassener Ursprung kann eigene falsche Fakten signieren: Kryptografie prüft
Herkunft, nicht Wahrheit eines Rätselerfolgs. Betroffene Bindung sperren und
lokale Folgeentscheidungen sichtbar prüfen; kein automatischer globaler Rollback.

## 3. Entscheidungs- und Implementierungstore

| Tor / Eigentümer | Vor Implementierungs-/Produktfreigabe nachweisen |
| --- | --- |
| D0-G1 · #28/D2 | Gepinnte COSE-Verarbeitung/Bibliothek auswählen und Herkunft/Lizenz prüfen; unabhängig erzeugte ES256/COSE-Goldenbytes und Manipulations-/Kurven-/Parameter-/Framinggegenfälle. Separaten versionierten Challenge-/Grant-Kontrollvertrag nach D0-FRESH vor dessen Fähigkeiten implementieren und prüfen. Keine Freigabe aus dem Vertragsmodell. |
| D0-G2 · #28/D1 | Schlüsselentropie und sichere Aufbewahrung, authentifizierter öffentlicher Fingerprintabgleich, atomare Vertrauensänderung, Recovery/Flashrestore-Sperre und Entzug am Commitpunkt implementieren und prüfen. |
| D0-G3 · D1/#31 | Fact-Allocator, Reservierungen/unusedRanges einschließlich umgekehrter Konflikte, Journal/Replay/Outbox, gleiche-ID-Konflikt, atomare Anwendung; Stream-/Latch-Erhalt über Run-/Paketwechsel und historischer Abgleich/zugelassener Bootstrap bei Kompaktierung; reale Stromausfälle separat. |
| D0-G4 · D2/#31 | 2048-Byte-Grenze in RAM/Stack/Flash prüfen, gepinnte SDK-Transferlimits/Fragmentierung/Kanal/Retry/Queues und BLE-Koexistenz nachweisen. Vorhandene Funkprobe ist kein neuer Codecbeleg. |
| D0-G5 · #30/D3/D4 | Paketmanifest/-hashes, Fähigkeiten, Profil-/Ressourcenanschluss, explizite lokale Aktivierung und verteilter Referenzlauf. Keine Verpflichtung zu zentraler Erreichbarkeit. |

Die Auswahl des Protokolls und seiner fachlichen Regeln liegt in D0 vor.
Diese Tore sind **Implementierungs-/Betriebsnachweise**, keine heimlich bereits
erledigten Arbeiten. Der tatsächliche SDK-ECDSA-Konfigurations-/Link-/Budgetbeleg
ist offen; gelesene Header allein genügen nicht. Es wird weder eine neue
Bibliothek installiert noch der bestehende Frameworkpin geändert.

## 4. Versionen, Fähigkeiten und Migration · D0-MIG

| Ebene | Bisher | Neuer Anschluss / sichere Ablehnung |
| --- | --- | --- |
| BLE-Baugruppen v2 / configured v2 | unveränderte Header, Bytes, Domains, Lease/Request | Neuer Veranstaltungskontext nur unter neuer ausgewiesener Schnittstelle/Fähigkeit; kein Einfügen in reservierte alte Bytes. |
| Profil v1 / Profiljournal v1 | boxlokale Komponenten/Regeln und bestehende Persistenz | Ein neues Veranstaltungspaket v1 referenziert Hashes der unveränderten lokalen Profile. Externe Bedingungen brauchen später Profil v2; kein neuer Predicatewert unter v1. |
| Ereignisformat v1 | heute nicht vorhanden | COSE/ES256-Profil und exakte Bodyversion oben; unbekannte Version/Typen ablehnen, nicht als älteren Typ interpretieren. |
| Status / Fähigkeiten | vorhandene Schemata bleiben | Neue capability `event_contract_v1` nur nach implementierten Verifizierern/Allocator/Replay; zusätzliche Grenzen und `observation_v1`, `command_grant_v1`, `receipt_v1` getrennt ausweisen. Keine Fähigkeit aus Firmwarelabel ableiten. |
| NFC-Testformat | bestehender 59-Byte-Baugruppentest | Unverändert. Produktives Spielobjekt später eigener Typ/Version mit Objekt-ID, Veranstaltungs-/Durchlaufbindung, Autorität, Schreibkonkurrenz und Replayvertrag. UID/Flags sind kein signierter Boxfakt. |

Paketmanifest v1 benennt event/run, Paketformatversion, vollständige Stationsbindungen,
lokale Profil-/Ressourcenbytes mit SHA-256, Regel-IDs/-Generationen, externe
Abhängigkeiten, Offlinepolitik und zwingende Fähigkeiten/Versionsbereiche.
Sein Digest bezieht sich auf die unveränderlichen deterministischen Manifestbytes;
zugeordnete Dateien sind über deren Hashes gebunden. Das Manifest enthält seinen
eigenen Digest nicht. Ein passender Hash allein verleiht keine Veröffentlichungs-
oder Aktivierungsrechte: Paketquelle/Inhaltsstand werden über den authentifizierten
Verwaltungsweg geprüft und an jeder Zielbox bewusst bestätigt. Es enthält
weder private Schlüssel noch einen universellen
Veranstaltungsschlüssel. Paketveröffentlichung ist keine Zielaktivierung und
verleiht keine neue zentrale Administratorrolle. Konkretes Manifest-Codec und
Editor-/Verteiloberfläche liegen bei #30/D3, müssen diesen Inhalt erhalten.

Migrationsfolge: (1) Ziel kennt neue Fähigkeiten tatsächlich und weist seine
Grenzen aus; (2) Manifest/Profile/Ressourcen vollständig separat bereitstellen;
(3) Hashes, referenzierte Bindungen/Regeln und Offlinefälle prüfen; (4) an der
aktuell berechtigten Zielbox bei geeignetem gestopptem Spiel bewusst aktivieren;
(5) atomar alten oder neuen aktiven Kontext und dauerhaften Fortschrittsbezug
bestätigen. Der Streambeleg bleibt erhalten; im selben Durchlauf bleiben auch
bereits belegte Freigabe-Latches erhalten. Ein neues Ziel benötigt historische
Belege oder ausdrücklich zugelassenen Bootstrap gemäß D0-APPLY; ein hoher erster
Zähler ist kein neuer Nullpunkt. Teilerfolg/unklare Antwort löst keine
automatische Wiederholung aus.
Fremde/neue Run-ID erfordert ausdrückliche Durchlaufentscheidung; Verwaltungs-
Recovery bewirkt keinen Reset. Alte Ereignisse verbleiben im alten Kontext.

Downgrade: ein nicht unterstütztes aktives Format bleibt gesperrt; kein Löschen
oder automatisches Formatieren. Rückweg nur mit nachweisbar konsistentem Kontext
und neuer bewusster Entscheidung. Alte Programme dürfen unbekannte Daten nicht
als leeres Journal behandeln. Ein altes Flashabbild ist kein sicherer
Fortschrittsrollback und unterliegt D0-ID.

Die [Fixturefälle](fixtures/d0-v1.json) prüfen diese Grenzen als semantisches
Modell mit ausdrücklich simuliertem Verifizierergebnis. Echte COSE-/CBOR-Bytes,
Cross-Implementierungsvektoren und produktive Schema-/Codecimplementierung sind
D0-G1, nicht bereits durch JSON-Beispiele oder einen grünen Modelllauf bewiesen.
