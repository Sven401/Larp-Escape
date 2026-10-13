# Begrenzter Companion-Befehlsrahmen v1

**Stand:** 7. Oktober 2026. **Status:** schmaler Firmware-/Host-/Werkzeugpfad implementiert; tatsächlicher Funkadapter, Pairing, authentisierte Geräteidentität und Wireantworten offen. Dieser Rahmen trägt ausschließlich den [Befehl v2](command-codec-v2.md), kein vollständiges Profil. Die App konsumiert diesen Firmwarevertrag; Fremd-Appcode bleibt unverändert.

## Wireformat und Grenzen

Eine Transportwert-Nachricht enthält genau einen Rahmen; mehrere Rahmen dürfen nicht ohne äußere Wertgrenzen aneinandergehängt werden. Keine C++-Struktur oder Hostbytefolge direkt übertragen.

| Byteoffset | Größe | Bedeutung |
| --- | --- | --- |
| 0 | 1 | Rahmenversion exakt 1 |
| 1 | 1 | Typ exakt 1: vollständiger GameCommand-JSONvertrag v2 |
| 2 | 2 | message, unsigned little endian, 1..65535 |
| 4 | 2 | total, unsigned little endian, 1..1024 Dokumentbytes |
| 6 | 2 | offset, unsigned little endian, 0..total-1 |
| 8 | 1..120 | unveränderte Dokumentbytes dieses Stücks |

Gesamt 9..128 Byte; count wird aus der äußeren Transportwertlänge bestimmt. count darf total-offset nicht überschreiten. Keine leeren Stücke, NUL-Terminierung, Headerfeld für Rolle/Principal/Epoch, Prüfsumme oder automatische Endiannesskonvertierung von gepackten Typen. Eine unbekannte Version/ein unbekannter Typ wird ausdrücklich als Unsupported abgelehnt. Fehler erhält das Decoderziel unverändert; Frame.payload ist nur bis zur Änderung/Freigabe des Eingabepuffers geliehen.

Das Proben-/Rechnerwerkzeug benutzt höchstens 20 Byte (8 Header + 12 Nutzbytes), damit kleine äußere Werte darstellbar sind. Das ist eine gewählte Grenze des Tests. Ein tatsächlicher Funkadapter muss seine zulässige Wertgröße gesondert bestimmen und darf kleinere Nutzstücke verwenden. Der schmale [SDKdienstvertrag](gatts-service-sdk-v1.md) legt inzwischen die eigenen Dienst-/EingangsUUIDs fest. Tatsächliche MTU, Android-Berechtigungen, ESP-NOW-Koexistenz und Energiebedarf bleiben gesonderte Integrations-/Messaufgaben.

message korreliert nur die laufende Zusammenfügung innerhalb einer verifizierten Verbindung. Es ist **keine** GameCommand.request, kein Replayzähler und kein Identitäts-/Rechtenachweis. message darf nach Ende des laufenden Dokuments wiederverwendet werden; Boot/Lease/Request/Revisionen und die begrenzten Anwendungsreceipts entscheiden weiterhin über Ausführung/Duplikate. Ein Adapter darf pro aktueller GM-Verbindung nur eine Zusammenfügung bedienen.

## Zusammenfügung und Lebensdauer

CommandStream hat genau einen festen 1024-Byte-Puffer, keinen Heap und keinen vollständigen Profilpuffer. Ein alleiniger Transporteigentümer ruft receive/retry/poll/snapshot auf. Keine parallelen Callbacks, kein ISR-Port. Äußere Callbacks müssen Werte begrenzt kopieren/einreihen oder denselben Eigentümerkontext garantieren; kein Callback ruft Engine/Geräte auf.

Principal und connectionEpoch werden aus der **verifizierten Verbindung dieses Rahmens** als getrennte APIargumente beigefügt. Vor Rahmendecode/Kopie wird der aktuelle synchronisierte Authoritysnapshot gelesen; beim vollständig dekodierten Command prüft der tatsächliche FreeRTOS-Adapter Metadaten nochmals unter seiner kurzen Admission-Sperre. Der C++-Port stellt nur diese Grenze bereit und authentisiert selbst keinen Client.

- Offset0 startet ein neues Dokument mit fester message/total/Principal/Epoch und Erstzeit.
- Folgende neue Stücke müssen genau bei received beginnen. Höchstens count Bytes werden kopiert; das Aufruferarray darf danach sofort wiederverwendet werden.
- Ein vollständig innerhalb des bereits kopierten Bereichs liegendes, bytegleiches Stück ist DuplicateFragment. Keine Kopie und keine Verlängerung der Frist. Teilüberlappung/Gaps/erstes Stück mit Offset>0: OutOfOrder, vorhandenes Dokument bleibt erhalten.
- Andere message: Busy, bisheriges Dokument bleibt erhalten. Dieselbe message mit anderer total oder ein widersprüchliches vollständig überlappendes Stück: FragmentConflict, bisheriges Dokument wird verworfen.
- InvalidFrame/UnsupportedFrame ändern eine gültige laufende Zusammenfügung nicht. Bad Principal/Epoch bei weiterhin gültiger aktueller Verbindung ändern sie ebenfalls nicht.
- Fehlt aktuelle GM-Authority, ist sie suspendiert oder wechselt sie, wird vorhandene Zusammenfügung verworfen. Neue gültige Verbindung darf ausschließlich neu ab Offset0 beginnen. Alte Metadaten dürfen die neue Zusammenfügung nicht verlängern oder löschen.
- Absolute Frist 2000 ms ab erstem aufgenommenem Stück, einschließlich Warteschlangenretry. Bei elapsed>=2000 wird vor Kopie/Retry verworfen und Expired geliefert. Dieser Aufruf nimmt keine neue Nachricht auf; nächster Aufruf darf neu bei Offset0 beginnen. Keine Verlängerung durch Bytes, Duplikate, andere message oder QueueFull.
- now ist der fortlaufende unsigned 32-Bit-Millisekundenzähler derselben Uhr. Subtraktion ist modulo32; Uhrumlauf geprüft. Eigentümer muss poll auch ohne Eingabe aufrufen und darf nicht einen ganzen Zählerzyklus aussetzen oder rückwärts springende Uhren liefern. Nach Suspend werden Bytes bei nächstem Owneraufruf unzugänglich; kein behaupteter sofortiger physischer Nullierungs-/Abbruchnachweis.

Wenn received==total, wird **erst dann** der vollständige tatsächliche JSONdecoder/Queueadapter benutzt. Queued leert die Zusammenfügung, die Anwendung hat noch nicht ausgeführt. DecodeRejected leert sie, verbraucht keine Spielanfrage. QueueFull behält exakt dieses vollständige Dokument; retry(now) oder ein bytegleiches wiederholtes bereits kopiertes Stück versucht eine einzelne Aufnahme erneut. poll macht keine Aufnahme und keinen GameTick. Andere Terminalfehler verwerfen die Zusammenfügung.

Verliert der Client eine Queued-Bestätigung, muss er das gesamte unveränderte JSON erneut ab Offset0 schicken. Ein isoliertes Endfragment nach geleerter Zusammenfügung ist OutOfOrder. Der bestehende Anwendungsreceipt erkennt das komplette Wiederholen und verhindert erneute Ausführung. Eine neue GameCommand.request darf erst nach bestätigtem/verifiziertem nextRequest gebildet werden.

## Ergebnisse und Vertrauensgrenze

CommandStreamResult ist eine **lokale kopierte Adapterausgabe**, noch kein kodiertes BLE-/Netzwerk-ACK. outcome unterscheidet Receiving, DuplicateFragment, Queued, QueueFull, DecodeRejected, InvalidFrame, UnsupportedFrame, Unauthorized, Busy, OutOfOrder, FragmentConflict, Expired, Idle und Uninitialized. message/total/received zeigen lokalen aktiven bzw. erfolgreich vollständig bearbeiteten Stand; nach Ablauf/Zugangsentzug sind sie null. Fehler beziehen sich teilweise auf den bisherigen aktiven Rahmen, nicht auf unvalidierte Clientkorrelation. Niemals blind an einen fremden Principal weiterleiten oder eine GameCommand.request aus einem Fragment erraten.

submission ist nur aussagekräftig, wenn submit tatsächlich erreicht wurde. Queued/QueueFull/DecodeRejected enthalten ein tatsächlich erreichtes JSONdecoderresultat; Default decode=None bei anderen Pfaden besagt keine Prüfung. Antworten aus der Anwendungsqueue tragen weiterhin originale Principal+connectionEpoch. Der spätere Router muss alte Verbindungen verwerfen und aktuelle Daten ausschließlich an verifizierte Ziele routen.

Kein Frame-CRC ersetzt sichere Anmeldung/Transportintegrität. Ein verifizierter Transport muss Integrität und Metadatenherkunft belegen. Unvollständige/fehlerhafte Dokumente wirken niemals auf Engine oder aktives Profil.

## Tatsächlicher vertikaler Pfad und Nachprüfung

pio test -e host-configured-control: tatsächlicher Encoder → Rahmen → CommandStream → JSONdecoder → tatsächlicher FreeRtosGameControlChannel gegen kopierende RTOS-Fakes → GameControl → gespeicherte ProfileApplication → Engine → kopierte Spielantwort. Frühe lokale Queued-Bestätigung und spätere tatsächliche Executed-Antwort werden getrennt geprüft. Synthetische Besitzer und MemoryStore sind keine Hardware-/SMP-/Dauerhaftigkeitsabnahme.

14 zusätzliche Framing-Testfunktionen (39 insgesamt) umfassen Bytefolge/Decoderzielerhalt, Grenzlängen, Eingabepuffer-Wiederverwendung, partielle/duplizierte/widersprüchliche Stücke, Lücken/Überlappung, Busy/Timeout/Uhrumlauf, Reconnect, Rechteentzug bei QueueFull und nach letzter Rahmenbeobachtung, kontrollierten Retry/Antwortverlust, malformed JSON ohne Verbrauch, 1024 Bytes und 1-/120-Byte-Nutzstücke.

pio run -e profile-tools baut einen schmalen Firmwareproduzenten. Anschließend:

~~~text
.pio/build/profile-tools/program.exe --frame-command docs/companion/fixtures/v2/command-start.json
~~~

Das Werkzeug prüft mit dem **tatsächlichen** Decoder (max1024 Byte), kanonisiert mit dem tatsächlichen Encoder, erzeugt message1 und schreibt pro 20-Byte-Rahmen eine Großhexzeile. Ungültiges Dokument liefert Fehler/Exit1 vor Frameausgabe. Hex ist eine lesbare Werkzeugdarstellung; auf Funk werden dekodierte binäre Werte gesendet. [Referenzhexzeilen](fixtures/command-frames-v1.txt) werden unabhängig im Hostfall durch den tatsächlichen Encoder erzeugt/verglichen. Textzeilen erlauben Systemzeilenenden; JSONcontent bleibt bytegenau, nur letzte LF/CRLF wird normalisiert.

Die isolierte esp32-profile-runtime-probe benutzt denselben CommandStream und einen wiederverwendbaren 20-Byte-Ausgabepuffer für synthetischen Start. Kein zusätzlicher GameTick. Nur Build, kein Upload oder Funkzugang. Produktion src/main.cpp bleibt bisheriger Einstieg. Wire-ACK/Reply-/Statusframing, Identität/Pairing und authentisierter Funkadapter sind der nächste Schnitt; Profiltransfer benötigt eigenen längeren begrenzten Streaming-/Integritätsvertrag.


## Ressourcen- und abschließender Buildbeleg

39/39 Hostfälle ohne Fixtureexport erfolgreich/Exit0; lokales Log
%TEMP%/larp-command-frame-host-final.log. Der tatsächlich gebaute Produzent
liefert 13 Referenzrahmen/153 rekonstruierte kanonische JSONbytes; leeres Objekt,
lexikalischer Float und 1025-Byte-Dokument jeweils Exit1 vor jeder Frameausgabe.
Acht JSON-Fixtures weiterhin gegen die drei kanonischen Schemata gültig.

Vier finale Builds SUCCESS/Exit0: Produktion55.148 RAM/1.228.417 Flash (93,7%),
alte USB-Probe21.480/291.673, neue Profilprobe99.056/386.469 Byte plus57.272
einmaliger Profilheap/Allocator/FS. Produktion bleibt alter Einstieg. Die neue
Probe wächst statisch gegenüber016fe40 um1064Byte; keine gemessene Boardreserve.

Xtensa/-Os: CommandFrame16, CommandStream1048, CommandStreamResult20 Byte.
Eigene Frames: Framedecode64, Encode48, Streamreceive112, Retry/Poll48,
Refresh/Submit32 Byte; Caller/Decoder/FreeRTOS-/SDKketten zusätzlich.
Standalone-Gesamttypmessung mit Adapter scheiterte an zwei fehlenden
SDK-Includepfaden; reine Typmessung danach erfolgreich, Adapterobjektgröße
1252Byte separat aus tatsächlichem Proben-ELF, 20Byte Framepuffer. Keine Runtime-/SMP-/Stackabnahme.

Architektur83 Dateien/193 Includes, 53 unabhängige C++11-Header,
Format139/clang-format19.1.7; diffcheck sauber. Kein Cppcheck dieses Schnitts,
Upload, Remote-CI oder Funk-/Androidabnahme. Vollständiger Backend-Ausfall-,
Zähler-/Fuzzing- und Feldbetriebnachweis bleibt offen.

### Kopierte Callbackaufnahme

Der neue [Framequeuevertrag](command-frame-queue.md) trennt die feste
Callbackkopie vom alleinigen CommandStream-Owner. Im Queuepfad zählt die
ursprüngliche Empfangszeit des ersten Frames zur unveränderten absoluten2000ms-
Frist; weder Queuewartezeit noch erneute Stücke verlängern sie. CopiedFrame ist
keine vollständige CommandQueued- oder Executed-Spielbestätigung.
