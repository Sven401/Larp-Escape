# DG-Pilot v1: Schritte und segmentierte Endantworten

Stand 2026-10-10. **C++-Hostpfad mit synthetischen Komponenten und einem
Host-Referenzempfänger**. Basis ist Controltransport `3209275` mit Diagnoseowner
aus `510857f`. Dieser Schnitt aktiviert weder GATT noch echte Komponenten. Die
vorherigen [Controlrahmen](diagnostic-transport-host.md) bleiben unverändert und
melden weiterhin nur ihren eigenen synthetischen Controlumfang. Es gibt noch
keinen gemeinsamen produktiven Dispatcher oder Androiddecoder für diese Bytes.

## Schrittanforderung: exakt 80 Bytes

Integer sind Little Endian, IDs bleiben in derselben Bytefolge wie im
Controlvertrag. `DG`, Version 1, Art 4 sind ein eigener Pilotraum; kein
GameCommand-v2-Opcode wird umgedeutet. Der Decoder prüft exakte Länge vor jedem
Feldzugriff, akzeptiert nur zwei Operationen und allokiert keinen Heap.

| Offset | Bytes | Bedeutung |
| --- | --- | --- |
| 0 | 4 | ASCII `DG`, Version 1, Art 4 |
| 4 | 4 | Nachricht 1..UINT32_MAX |
| 8 | 16 | Geräte-ID, nicht null |
| 24 | 8 | Boot-ID, nicht null |
| 32 | 16 | Lauf-UUID v4 |
| 48 | 16 | Grant-UUID v4 |
| 64 | 4 | Schritt 1..UINT32_MAX |
| 68 | 4 | Timeout 1000..10000 ms |
| 72 | 1 | 0 WakeCapture, 1 LedSingle |
| 73 | 1 | Wake: Kanal 1; LED: 0 |
| 74 | 1 | Index 0 |
| 75 | 3 | Wake: `[0,0,0]`; LED: `[16,0,0]` |
| 78 | 2 | Wake: 0; LED: 500 ms |

Nicht verwendete Felder müssen null sein. Clientbytes enthalten keine Authority.
`StepTransport.receive` prüft Originalverbindung und kopierte vollständige
Authority mit `IdentityAccess.route` und übergibt erst danach an den tatsächlichen
`Control`. Dessen Grant-/Lauf-/Geräte-/Bootprüfung und bootweite Schrittgeschichte
bleiben maßgeblich. Senden oder lokale Aufnahme erzeugen keine Endantwort.

`Control.submitAt` ergänzt die ursprüngliche Empfangszeit: Frist ist
`receivedAt + timeoutMs`, tatsächlicher Start ist `now`. Warteschlangenzeit wird
nicht erstattet. Empfang vor Grantbeginn, aus der Zukunft oder nach Ablauf wird
abgewiesen. LED-Aufnahme benötigt noch **mehr als** 500 ms, weil am absoluten
Deadlinezeitpunkt bereits Timeout gilt. Das bisherige `submit` delegiert mit
`receivedAt=now`. Jede verwendete Zeit ist vertrauenswürdige monotone Gerätezeit,
höchstens 2^53−1; Appzeiten werden nicht als Gerätezeiten interpretiert.

## Endergebnis: 104 bis 744 Bytes

Die ersten 80 Bytes enthalten die vollständige kanonische ursprüngliche
Schrittanforderung einschließlich Nachricht. Danach folgen:

| Offset | Bytes | Bedeutung |
| --- | --- | --- |
| 80 | 8 | startedDeviceMs |
| 88 | 8 | endedDeviceMs |
| 96 | 1 | Outcome: 0 Completed, 1 Failed, 2 Cancelled, 3 Rejected |
| 97 | 1 | Reason: 0 None, 1 OwnerError, 2 Unsupported, 3 PermissionDenied, 4 Expired, 5 Busy, 6 Cancelled, 7 Overflow |
| 98 | 1 | activeHigh: 0/1 |
| 99 | 1 | restoredOff: 0/1 |
| 100 | 1 | Renderaufrufe: 0..2 |
| 101 | 1 | Flankenüberlauf: 0/1 |
| 102 | 1 | Flankenanzahl: 0..64 |
| 103 | 1 | Evidenzprofil exakt 1: synthetisch, Versorgungsspannung nicht verfügbar |
| 104 | 10 × Anzahl | je Flanke: Zeit LE64, Kanal 1..4, Flags Bit 0 rawHigh/Bit 1 active; restliche Bits null |

Länge ist exakt `104 + 10 * Anzahl`. Wake trägt keine Render-/Aus-Belege. LED
trägt keine Flanken und keinen Flankenüberlauf; Completed braucht zwei
Renderaufrufe und bestätigtes `restoredOff`. Completed entspricht Reason None;
alle anderen Outcomes benötigen einen Fehlergrund. Flankenzeiten sind monoton
nicht fallend innerhalb Start/Ende, Pegel und aktive Polarität müssen passen.
Ein Wake-Überlauf bleibt im Rohbeleg erhalten und verhindert im übergeordneten
Diagnosevertrag ein bestandenes Urteil. Der Codec bewertet keine Nutzerbeobachtung.
Ohne Versorgungstelemetrie wird später JSON `null` erzeugt, kein erfundener Wert.

## Segmentierung und Empfänger

Jeder Endantwortframe hat 12 Headerbytes plus **1..116** Nutzbytes, höchstens
128 Bytes. Header: ASCII `DG`, Version 1, Art `0x84`, Nachricht LE32 an Offset 4,
Gesamtlänge LE16 an Offset 8, Byteoffset LE16 an Offset 10. Die Nachricht muss auch
mit den eingebetteten Anforderungsbytes übereinstimmen. Es gibt weder Padding noch
eine native C++-Struktur auf dem Draht.

`nextFrame` passt die Nutzlast an die übergebene Kapazität an. Maximal 744 Bytes
benötigen sieben 128-Byte-Frames. Die 104-Byte-LED-Antwort benötigt bei einer
20-Byte-Transportkapazität 13 Frames. **Anforderungen sind weiterhin unsegmentierte
80 Bytes**, Controlantworten 88 Bytes: Ein späterer Service braucht daher mindestens
88 Bytes effektive ATT-Nutzlast oder einen zusätzlichen geprüften Request-/Control-
Fragmentvertrag. Kleine Antwortsegmente allein erfüllen diese MTU-Anforderung nicht.

Der Sender puffert höchstens eine Endantwort (744 Bytes). Er erhält ausschließlich
die Antwort des tatsächlichen Owners und sendet jedes Segment einmal auf der
ursprünglichen Verbindung. Nach Transportfehler erfolgt kein automatischer Retry.
Gepufferte Completed-Antworten bleiben zusätzlich an genau ihren Lauf/Grant
gebunden: Abbruch oder ein inzwischen neuer Grant stoppt restliche Erfolgssegmente.
Ein vom Owner erzeugtes Cancelled-Ergebnis kann vor Ablauf noch separat geliefert
werden; der App-Reducer darf damit keinen bereits terminalen Schritt überschreiben.
Verlorene Segmente führen in der App zu unklarem Ausgang. Antwortausgabe muss wie
Aufnahme, Owner und Lifecycle in einer gemeinsamen Serialisierung liegen; eine
spätere SDKqueue muss die Originalbindung bei tatsächlicher Ausgabe erneut prüfen.

`ReplyAssembly.begin` erwartet eine ausdrücklich bekannte ursprüngliche Anfrage,
Nachricht, Verbindung, Authority und absolute lokale Empfangsfrist. Pro Frame wird
die aktuelle vertrauenswürdige Route erneut geprüft. Sender und Empfänger haben
jeweils ihre eigene Uhr; der Hosttest stellt keine Uhrensynchronisierung her.

Der Empfänger nimmt nur lückenlose aufsteigende Bytebereiche an. Vollständig
enthaltene bytegleiche Duplikate sind wirkungslose `Duplicate`; veränderte,
überlappende, vorauseilende, anders gebundene oder übergroße Segmente beenden den
Empfang. Duplikate verlängern niemals eine Frist. Nach vollständigem Empfang folgt
strenge Codecprüfung und Vergleich sämtlicher ursprünglicher Korrelations- und
Operationsfelder. Erst `take` liefert genau einmal das Ergebnis, nach erneuter
Frist-/Authorityprüfung. Späte Segmente können es nicht ersetzen. App-Disconnect
muss `retire` aufrufen; Rechteentzug schließt bereits über die aktuelle Route.

Timeout, Rückwärtszeit, Disconnect oder Entzug verwerfen restliche Sendebytes und
Empfangsfragmente. Der tatsächliche Owner beendet seinen Schritt/Grant entsprechend
dem jeweiligen Lifecyclegrund; nicht mehr lieferbare Ergebnisse werden verworfen,
ohne daraus einen Erfolg abzuleiten. Der Sender liefert ab absoluter Schrittfrist
keine Endantwort mehr; der Appzustand bleibt bei fehlender Antwort `timeout/unclear`.

## Prüfungen und nächster Schnitt

`python scripts/check_diagnostic_step_transport.py` kompiliert den tatsächlichen
C++11-Owner, Identitätskern, Codecs, Sender und Empfänger mit Werror. Ein unabhängiger
Pythonclient vergleicht alle 744 Wake- und 104 LED-Bytes. Die Hostfälle prüfen 64/65
Flanken, beide Operationen, kleine Segmente, falsche Korrelation, Oversize, Lücken,
bytegleiche/veränderte Duplikate, Zeitablauf, Queuezeit, Disconnect und Entzug.
Bestehende Owner- und Controltests bleiben eigene Runner. 756 neue Hostassertions
und die unabhängigen vollständigen Bytevergleiche bestanden. Größen der Hostobjekte:
Sender 896 Bytes, Empfänger 888 Bytes; temporäre Reply-Werte und Taskstack kommen
hinzu. Dies ist kein gemessener ESP32-Stack-/Timingnachweis.

Noch offen: Der synchrone `Admission`-Rückgabewert ist ein Hostadapterstatus und
hat hier keinen eigenen Wire-Acknowledgement-Vertrag; verlorene/abgewiesene
Anforderungen werden nicht als Geräteergebnis ausgegeben. Für Android sind
persistente Pending-Aufnahme vor Sendung, Byteparser/Reassembly gegen diese
Vektoren, unveränderte Unterbrechungszustände, unabhängige Nutzerbeobachtung und
Export zu verbinden. Danach benötigt der produktive Service konsistente
Capabilities, Authentisierung/F1-F2, echte Komponentenbesitzübergabe, garantierte
Ownerbedienung und das geprüfte Hardwareprofil. Bisherige GATTdefinition und
Produkt-main sind unverändert. Kein Produkt-GATT-/Androidlauf, Upload oder
Hardwaretest wird durch die Hostfälle belegt.
