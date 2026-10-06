# Begrenzter Spielleiter-Befehlscodec v2

**Stand:** 6. Oktober 2026. Decoder, kanonischer Encoder und vollständige Nachrichtenaufnahme in statische FreeRTOS-Queue implementiert. Quellen-/Buildbelege getrennt von nicht ausgeführter JSON-/Fehler-/Geräteabnahme. Netzwerkanmeldung und Transport fehlen weiterhin. [Steuervertrag](configured-v2.md), [Nachrichtenschema](game-command-v2.schema.json).

## Eingabe und Zielerhalt

`decodeGameCommand(bytes, count, destination)` liest genau die angegebene vollständige Nachricht. Maximal 1.024 Bytes einschließlich Leerraum/Escapes, ohne optionalen Aufrufer-NUL. Der Parser benutzt kein strlen am Eingabepuffer, prüft die Grenze vor dem ersten Lesen und liest nie bytes[count]. Keine Fragmente, Reader-IO, Heapallokation oder Rekursion. Der Aufrufer hält mindestens count Bytes lesbar und unverändert, bis die Funktion zurückkehrt. Größere Nachricht/null Quelle/leer werden vor Parse abgelehnt.

Genau ein flaches JSON-Objekt, alle acht Felder erforderlich: schemaVersion/boot/lease/request/expectedConfigurationRevision/expectedStateRevision/kind/target. Reihenfolge beliebig; JSON-Leerraum ausschließlich Space/Tab/CR/LF. Nach dem Objekt nur solcher Leerraum; weitere Objekte, Nutzdaten oder mitgezählter NUL werden abgelehnt. Kein BOM, keine nested Werte, Kommentare, zusätzliche/duplizierte Felder oder trailing comma. Ein kleiner exklusiver GameCommand-Kandidat wird erst nach vollständigem syntaktischen/strukturellen/inhaltlichen Erfolg ins Ziel kopiert; Fehler erhält destination unverändert. Dies macht keine Behauptung über Gültigkeit der Referenzen im aktiven Profil.

Schlüssel/kind/boot sind begrenzte druckbare ASCII-Wiretexte, höchstens 32 dekodierte Zeichen pro Token. Unterstützte Namen können gültige gleichwertige JSON-Escapes verwenden (Quote/Backslash/Slash und druckbares ASCII aus vierstelligen Unicode-Escapes). Steuerzeichen, NUL, Nicht-ASCII, Surrogate, unvollständige/unbekannte Escapes werden abgelehnt. Das schränkt keine unterstützten Namen ein: alle acht Namen/sechs Befehlsnamen und Hexzeichen sind ASCII. Escaped gleicher Schlüssel fällt in denselben Feldmaskenplatz und ist als Duplikat erkennbar; kein zweites Privilegfeld durch andere Schreibweise. Keine Unicode-/Großkleinschreibnormalisierung. Unicode-Text in vollständigen **Profildokumenten** bleibt im bestehenden Profilreader unterstützt.

## Zahlen und Semantik

Wirezahlen werden ausschließlich als vorzeichenlose Dezimalzifferntokens geschrieben: 0 oder erste Ziffer1–9 gefolgt von Ziffern. Keine führenden Nullen, Vorzeichen einschließlich -0, Brüche oder Exponenten. Vor Multiplikation/Addition wird der Bereich geprüft, keine float-/strtod-/Überlaufkonvertierung. Diese lexikalische Festlegung ist strenger als JSON-Schema type=integer (das mathematisch ganzzahlige JSON-Zahlen beschreibt); der Schema-$comment und dieser Vertrag benennen die zusätzliche Wiregrenze. Ein Appserializer muss tatsächliche ganzzahlige Zifferntokens ausgeben.

schemaVersion exakt 2; beide erwarteten Revisionen 0..4.294.967.295; lease/request 1..4.294.967.295; target 0..65.535. Boot exakt 16 große Hexstellen und nicht nur Nullen. Kind ausdrücklich start/stop/pause/resume/change_state/take_transition; explizite Zuordnung zu C++-Enumwerten, keine Client-Enumzahl. ChangeState/TakeTransition target nicht null; alle übrigen target exakt0. Nach dem Decode prüfen GameControl/Application/Engine weiterhin Anmeldung, aktuelle Lease/Boot, Reihenfolge/Wiederholung, erwartete zwei Revisionen, Ready/Hardware/Speicher und tatsächliche Profilrechte.

Beispiel einer kanonischen Nachricht (synthetische Bootkennung, kein reales Zugangsangebot):

```json
{"schemaVersion":2,"boot":"1234567890ABCDEF","lease":1,"request":1,"expectedConfigurationRevision":2,"expectedStateRevision":0,"kind":"start","target":0}
```

## Ausgabe und Diagnostik

`encodeGameCommand` schreibt dieselben acht Felder mit explizitem Befehlsnamen, Hex-Boot und kanonischen unsigned Ziffern. Kein Heap; maximal 512 Bytes einschließlich NUL, written ohne NUL. Ungültige Boot-/Lease-/Request-/Kind-/Targetwerte werden abgelehnt. Erfolg besagt ausschließlich, dass ein darstellbarer Befehl geschrieben wurde; keine Profil-/Rechte-/Revision-/Queuezusage. Fehler clears written=0 und vorhandenen ersten Byte NUL wie andere Encoder. Die Eingabegrenze 1.024 lässt Leerraum/Escapes zu und ist unabhängig von der512-Byte-Grenze der kanonischen Ausgabe.

CommandDecodeResult enthält Fehlerklasse, bekanntes Feld oder none und nullbasierten Byteoffset. EOF=count; eine nachträgliche Semantikprüfung kann die Position nach dem Wert/ganzen Dokument melden. Es werden keine Eingabetexte/UIDs/Authangaben in Diagnostik kopiert. Fehlendes Feld meldet erstes fehlendes nach festem Feldverzeichnis, duplicate das bekannte Feld, unknown kein vom Client vorgegebenes Diagnosewort. decoder_errors sind getrennt von GameReplyOutcome: fehlerhaftes JSON war noch nicht in der Spiellogik, hat keine Anfrage verbraucht und erzeugt keine Engineantwort. Der spätere Transport braucht einen begrenzten lokalen Fehlerantwortvertrag und darf bei unzuverlässiger Korrelation keine fremde Request-ID erraten.

## FreeRTOS-Aufnahme

`FreeRtosGameControlChannel::submitJson(bytes,count,authenticatedPrincipal)` darf der vertrauenswürdige Transporteigentümer für eine vollständig empfangene Nachricht benutzen. Das Principalargument stammt von der authentifizierten Verbindung, niemals aus Clientbytes. Ein JSON-Feld principal/role/auth wird als unknown abgelehnt, keine Clientrechte übernommen. Mit tatsächlichem Netzwerk-/Paarungscode muss diese Metadatenherkunft erst belegt werden; der öffentliche C++-Port selbst ist keine Authentisierung.

| Aufnahme | Bedeutung |
| --- | --- |
| Uninitialized | Queueadapter noch nicht gestartet; kein Parse |
| MissingPrincipal | null vertrauenswürdige Identität; kein Parse |
| DecodeRejected | Kandidat nicht vollständig gültig; keine Queueaufnahme |
| QueueFull | gültig dekodiert, kopierte Anfrage nicht aufgenommen; ursprüngliche Nachricht für Retry behalten |
| Queued | vollständige typed Werte kopiert; ursprüngliche Bytes können freigegeben werden; Ergebnis folgt separat |

GameSubmission.decode ist nur nach tatsächlich erreichtem Parser aussagekräftig; Default error=None bei fehlender Initialisierung/Principal beweist keinen Decode. Nur Aufnahmeoutcome entscheidet über Queued. Die Aufnahme ruft weder Controller noch GameControl aus dem Callback auf. Decoder/Kopien warten nicht auf Geräte; xQueueSend wartet null Ticks. Die spätere Transportframing-/Chunk-/Reconnect-/Backpressure-/Sicherheitslogik bleibt offen.

Die isolierte Profilprobe verwendet einen einmalig reservierten 512-Byte-Ausgabepuffer. Ihr schon bestehender lokaler synthetischer Start geht nun über Encoder → complete submitJson → Decoder → statische Anfragequeue → GameControl → Application/Engine. Principal 1 bleibt lokaler vertrauenswürdiger Probenbesitzer, kein Netzclient. Es wird kein zweites Spieltick eingeführt. Dieser Pfad wird kompiliert, **nicht hochgeladen oder ausgeführt**. Produktion verwendet weiterhin den bisherigen Einstieg und keine solche Verbindung. Neue Tests/Assertions, Rundläufe/Invalid-/Duplicate-/Counter-/Queue-/Rechteausführung und Geräte-/Timing-/Stack-/Energieabnahme wurden nicht durchgeführt.

## Aktueller Quellen-/Buildbeleg

Vier finale Builds `pio run -e profile-tools -e az-delivery-devkit-v4 -e esp32-runtime-probe -e esp32-profile-runtime-probe -j2` terminal Exit0/alle SUCCESS; lokales Log %TEMP%/larp-command-codec-final-build.log. Produktion 55.148 RAM/1.228.385 Flash (93,7 %), alte USB-Probe 21.480/291.673, neue 97.896/383.933 plus 57.272 Profilheap/Allocator/FS. Gegenüber db52732 kostete der einmalige Proben-Ausgabepuffer 512 Byte RAM; Queuegrößen/Controlobjekt unverändert. Produktion nutzt weiterhin den alten Einstieg, zukünftige Funkreserve nicht belegt.

Xtensa/-Os: GameCommand 28/CommandDecodeResult 8 Byte; decoder 176, string 48 / number 32 und kanonischer Encoder 64 Byte eigene Frames. Callees/SDK/snprintf und Aufrufer zusätzlich, keine gemessene Gesamtstack-/Laufzeitreserve. Architektur 76 Dateien/181 Includes, 49 unabhängige C++11-Header, Format 125/clang-format19.1.7; ausgewählte Cppcheck 2.1 warning/performance/portability für Reader/Encoder/Controlintegration ohne Befund; git diff --check sauber. Keine neue Testsuite/Assertions/Codec-/Queue-/Fehlerausführung, Upload oder Remote-CI. Der Nutzer hat ESP-Firmware/Schnittstellendesign dieser Arbeit zugeordnet; App-Seite konsumiert diese Verträge, Appcode wurde hier nicht geändert.
