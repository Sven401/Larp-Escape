# Begrenzte ausgehende Spielantwortframes v1

**Stand:** 7. Oktober 2026. **Status:** tatsächlicher Encoder, Queueauswahl,
Sender, Host- und synthetischer Probenpfad. Physischer Bearer und Appempfang offen.

## Wireformat

Der achtbytegroße Header hat dieselben Felder wie
[Befehlsframes](command-framing-v1.md), aber Typ2 für GameReply-JSON v2.
Version1, message/total/offset sind unsigned little-endian uint16.
total ist1..511; der tatsächliche Antwortencoder besitzt512Byte inklusive NUL.
Ein Wert hat9..128Byte, davon1..120Payloadbytes. Auf dem Wire gibt es keinen
abschließenden NUL-Byte und keine Principal-/Epochfelder. Ein Transportwert
enthält genau einen Frame. Der Commanddecoder und die Eingangsqueue weisen
Typ2 weiterhin als Unsupported ab; Replydecoder weist Typ1 ab.

decodeReplyFrame borgt Payloadbytes bis zur Änderung des Eingabepuffers,
Fehler lässt das Decoderziel unverändert. Gemeinsam benutzte innere
Header-/Grenzimplementierung, getrennte Typ-/Dokumentgrenzen; unveränderte
Befehlsreferenzframes. Der Rahmen selbst authentisiert keine Identität.

## Senderbesitz und Fortschritt

Ein Transporteigentümer benutzt ReplyFrameSender. Er übernimmt höchstens eine
bereits tatsächlich erzeugte RoutedGameReply aus takeCurrentReply, prüft deren
aktuelle GM-Bindung und friert den tatsächlichen encodeGameReply-Ausgabestand
im festen512Byte-Puffer ein. Keine Profil-/Heapallokation.
load meldet Loaded, Busy, Unauthorized, InvalidReply oder MessageExhausted;
ein aktiver gültiger Stand wird bei Busy nicht überschrieben.

service(now,maximumFrameBytes) prüft erneut die ursprüngliche Principal/Epoch
und liefert pro Aufruf höchstens einen Frame an IReplyFrameBearer.
Wertgrenzen9..128 werden ausdrücklich geprüft. Busy erhält exakt denselben
Offset, Datenstand und absoluten Fristbeginn. Erst Copied des Bearers bewegt
den Offset. Completed bedeutet ausschließlich: alle Frames wurden an diesen
Bearer kopiert. Es bedeutet keinen Telefonempfang, keine GATT-Indication-
Bestätigung und keine Appverarbeitung.

Der Bearer muss begrenzt ohne Wartezeit kopieren und unmittelbar an seinem
eigenen synchronisierten Verbindungspunkt dieselbe verifizierte Verbindung
prüfen. Er darf weder einen recycelten Connectionhandle noch eine neue Epoch
für die alte Antwort benutzen. Bei tatsächlichem späterem SDK-Senden braucht
auch eine weitere Bearerqueue diese Prüfung. Für GATT-Indications sind realer
Congestion-/Confirmationablauf und Pufferlebensdauer getrennt umzusetzen.

Entzug oder2000ms seit load verwirft verbleibende Fragmente, auch bei Busy
und Uhrumlauf. Unavailable verwirft die Antwort. Eine bereits vorher kopierte
oder gesendete Antwort lässt sich nicht rückwirkend ungesendet machen;
ihre Metadaten bleiben unverändert. Der Anwendungslauf wird dadurch nicht
gestoppt. Verlorene Antwort: Client korreliert vollständiges Boot/Lease/request,
liest frischen Zustand oder wiederholt ausschließlich den unveränderten
Befehl innerhalb des bestehenden Requestvertrags.

## Nachrichtenkennungen

message läuft innerhalb derselben verifizierten ConnectionEpoch von1 bis65535.
Jeder erfolgreich geladene Antwortstand verbraucht eine Kennung, einschließlich
später verworfener/abgelaufener Antworten. Der nächste uint32-Zählerstand65536
wird als MessageExhausted abgewiesen und niemals als uint16-Null/Reuse versendet.
Nur eine tatsächlich neue gültige Authorityepoch setzt den Zähler auf1.
Der Bearer muss dann die alte Clientzusammenfügung verwerfen und den neuen
Verbindungsstatus herstellen; Sender selbst vergibt keine Rechte/Epochen.

message ist ein Assemblierungsschlüssel, kein Spielrequest, Replay- oder
Authentisierungsnachweis. Die komplette GameReply enthält weiterhin die
ursprünglichen Boot-/Lease-/Request- und Revisionsdaten. Duplicate kann
historische Werte enthalten; es ersetzt keinen frischen Status.

## Vertikaler Nachweis und Gegenstelle

Tatsächlicher gespeicherter Start → GameControl → aktuelle FreeRTOS-Replyqueue →
encodeGameReply → ReplyFrameSender → kopierender geprüfter Hostbearer →
unabhängiger Frame-/JSONvergleich. [Referenzframes](fixtures/reply-frames-v1.txt)
tragen206 unveränderte JSONbytes in18 maximal20Byte-Frames; daraus entsteht
exakt [reply-executed.json](fixtures/v2/reply-executed.json).

scripts/check_companion_fixtures.py implementiert eine unabhängige lesende
Rechnergegenstelle: Upperhex/äußere Grenze, v1/Typ2, positive Kennung, Gesamtlänge,
Offsets, einheitliche Nachricht und vollständiges Ende. Elf Varianten mit
Lücken, Duplikaten, Vertauschung, fehlendem Ende, falschen Typ-/Headerfeldern,
ungültigem Hex und Überlänge werden abgewiesen. Kein Berechtigungsnachweis.

Neun neue native Funktionen prüfen gemeinsame Codecgrenzen, tatsächliche
Antwortrekonstruktion/Golden, Busy/Exactframe/Frist, unterschiedliche Wertgrößen,
Busy-Zielerhalt, Reconnect, Entzug vor und nach Bearercopy, Uhrumlauf,
ungültige Antwort und alle65535 Kennungen plus Erschöpfung und neue gültige
Epoch. Der Zählerlauf benutzt synthetisch unveränderte Zeit; kein Geschwindigkeits-
oder Funklastnachweis. Erstes neues Prüfset hatte zwei falsche Epochannahmen
im alten Fixturehelper, der immer epoch1 submit verwendete; aktuelle Epoch
wird jetzt tatsächlich aus dem Kanal übernommen. Kein Firmwarefehler daraus.

esp32-profile-runtime-probe enthält einen festen synthetischen Bearer,
aktuelle Bindungsprüfung und höchstens einen Outboundframe pro Loop. Er
verarbeitet keine Funkverbindung und erhält keine Peerbestätigung.
Antwortverlust stoppt den dortigen Anwendungslauf nicht. Kein neuer GameTick.
75/75 native Funktionen erfolgreich (finaler Hostlauf Exit0), vier finale Builds
SUCCESS/Exit0: Produktion55.148 RAM/1.228.473 Flash (93,7%),
alte USB-Probe21.480/291.729, Profilprobe100.424/390.213 Byte.
Profilprobe benötigt zusätzlich57.272 Byte einmaligen Profilheap samt
Allocator-/Dateisystemkosten; neue Sender/Bearer besitzen keine Profile.
Produktionsmain ist weiterhin der alte Einstieg. Die beiden alten
Builds liegen im finalen Neuaufbau jeweils56Flashbytes über dem vorigen
Beleg; dieser Unterschied ist kein Beleg eines Anschlusses des neuen Pfads.

Tatsächliches Probe-ELF: ReplyFrameSender548Byte, synthetischer Bearer148Byte,
Framequeue672Byte. RoutedReplyFrame140Byte im Xtensacompiler. Eigene
Compilerframes load48/service192, gemeinsamer encodeFrame32/decodeFrame48
plus Wrapper/Ketten/Callerbuffer; keine gemessene Gesamtstackreserve.
Architektur89Dateien/207Includes, Header57, Format151 bestanden.
Acht v2-Fixtures plus Profilkatalog schema-valid, Generatorabgleich und
unabhängige Antwortframegegenstelle gültig. Tatsächliche CLI nach finalem
Build weiterhin13 bytegleiche Befehlsframes und1732 Katalogbytes.
Goldenvergleiche normalisieren ausschließlich CRLF und abschließende
Zeilenenden; Dokument-/Framebytes bleiben unverändert.

Keine neue ESP32-Ausführung, SMP-/RF-/Android-/Peerbestätigungsprüfung,
Remote-CI oder Cppcheck dieses Schnitts. Frühere physische USB-Messungen
betreffen ausschließlich die damalige Diagnoseprobe.

Wireaufnahmeantworten für CopiedFrame/Receiving/CommandQueued, Statusframing,
Identität/Pairing, tatsächlicher SDK-/Androidadapter und Profiltransfer bleiben
offen. Die App kann diesen schmalen Framevertrag und die Fixtures übernehmen;
dieser Nachweis führt keinen fremden Appcode aus.

## Folgevertrag vom 7. Oktober 2026

[Aufnahmeantworten v1](command-receipts-v1.md) übertragen den tatsächlich
verarbeiteten Streamstand als festen18Byte-Typ3-Wert mit ursprünglicher
Verbindungsbindung. Callback-Copied/Outerwritefehler bleibt separat;
keine behauptete GATT-/Peerbestätigung oder Spielausführung.
