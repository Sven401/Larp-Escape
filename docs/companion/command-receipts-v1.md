# Aufnahmeantworten für verarbeitete Befehlsframes v1

Stand: 7. Oktober 2026. Tatsächlicher Queue-/Stream-/Codec-/Host-/Probenpfad;
physischer Transport und Appanschluss offen.

## Bedeutung

Nach einem tatsächlichen dispatchCommandFrame-Aufruf beschreibt die Antwort
das Eingangsfragment und den beobachteten Streamstand. Callback-Copied
bedeutet eine Kopie in die Framequeue und bleibt hier ein lokales Ergebnis.
Receiving/DuplicateFragment bestätigen Teilbytes. Queued bestätigt vollständiges
decodiertes JSON in der Commandqueue; GameControl entscheidet später über
Revisionen und Ausführung. QueueFull hält das vollständige Dokument bis zur
ursprünglichen Frist für ausdrückliches Retry. GameReply v2 ist die spätere
Spielentscheidung. Bearer-Copied bedeutet keine GATT-/Telefon-/Appbestätigung.

## Fester Wirewert

Genau18Bytes, kein JSON/NUL/fragmentierter Standardheader. Typ3 unterscheidet
den festen Wert von Commandtyp1/Replytyp2. uint16 unsigned little-endian;
Principal/Epoch bleiben verifizierte lokale Verbindungsmetadaten.

| Bytes | Feld |
|---|---|
| 0 | Version1 |
| 1 | Typ3 |
| 2 | Ergebniscode1..13 |
| 3 | Reserviert0 |
| 4..5 | Eingangsmessage |
| 6..7 | Eingangsgesamtbytes |
| 8..9 | Eingangsoffset |
| 10..11 | Eingangspayloadbytes |
| 12..13 | Beobachtete Streammessage |
| 14..15 | Streamgesamtbytes |
| 16..17 | Streamempfangsbytes |

Codes: 1Receiving,2DuplicateFragment,3Queued,4QueueFull,5DecodeRejected,
6InvalidFrame,7UnsupportedFrame,8Unauthorized,9Busy,10OutOfOrder,
11FragmentConflict,12Expired,13Uninitialized. Explizite Abbildung statt
C++-Enumserialisierung. Unknown/Idle/reservierte Werte abweisen.

Inputmessage positiv, total1..1024, count1..120, offset/count vollständig
innerhalb total. Streammessage0 verlangt total=received=0; sonst total1..1024
und received<=total. Receiving/Duplicate: gleiche Nachricht, positive partielle
Bytes. Queued/QueueFull/DecodeRejected/Uninitialized: gleiche vollständige
Nachricht. Busy: andere positive Streammessage. Conflict: gleiche Message,
gegebenenfalls anderes total, Stand vor Verwerfen. OutOfOrder: leer oder
dieselbe Nachricht. Decoderfehler erhält Ziel; Encoderfehler written0/erstes
Zielbyte0. Kleine Antwort enthält keine JSONdecoder-Fehlerdetails.

## Eigentümer und Verlust

FrameDispatchResult hält ausschließlich Eingangsskalare und Principal/Epoch;
kein geliehener Payloadpointer. copyCommandReceipt läuft im alleinigen
Transporteigentümer, prüft aktuelle Principal+Epoch+GM und bietet höchstens
eine begrenzte Nullwartekopie an. Bearer prüft genau dieselbe Verbindung
erneut am eigenen synchronisierten Beobachtungspunkt; weitere SDKqueues
ebenso. Kein relabeln alter Antworten an neue/recycelte Verbindungen.

Busy/Unavailable hält keine eigene Receiptqueue, macht Verarbeitung nicht
rückgängig und löst keinen Retry/Anwendungstick aus. Leerer Poll bietet keine
Antwort an. Callback-QueueFull/Outerwritefehler, tatsächliche Congestion und
Peerbestätigung muss der kommende physische Adapter ergänzen. Lokaler
Arduino-GATT-OK-Zeitpunkt vor Characteristiccallback ersetzt Fachantwort nicht.

Client hält eine Commandnachricht offen, unterscheidet Input von Busy-Stream
und verwendet Messagekennung nur für identische Wiederholung; geändertes
Dokument braucht neue Kennung, keine Wiederverwendung innerhalb derselben
Verbindung. Diese Clientregel ist keine serverseitige Kennungsregistratur
oder Replay-/Authentisierungsnachweis.

Ohne vollständige Spielantwort bleibt Ausführung ungewiss. QueueFull:
identisches letztes Fragment ausdrücklich erneut anbieten. Verlorene Queued-
Antwort und geleerter Stream: vollständiges unverändertes JSON ab Offset0
wiederholen. GameControl dedupliziert über vollständige Boot/Lease/request-
Identität; keine neue Requestkennung allein aus Unsicherheit. Äußere Felder
ersetzen Spielkorrelation nicht.

## Nachweis

83/83 finale native Funktionen ohne Fixtureexport erfolgreich/Exit0, acht neue.
Echte Framequeue/Stream/Commandqueue/Application:13Befehlsframes erzeugen
[13Aufnahmeantworten](fixtures/command-receipts-v1.txt),12Receiving/1Queued
vor erstem GameControlservice. nextRequest bleibt1/Spiel gestoppt; erst spätere
Spielantwort startet. Busy-Verlust und vollständige identische Wiederholung
ohne doppelte Ausführung, volle Queue/letztes Fragmentretry, DecodeRejected,
andere Busy-Message/Lücke/Konflikt, Queuefrist/Uhrumlauf, Reconnect/Bearerentzug
geprüft. Unabhängige Pythongegenstelle korreliert tatsächliche Quellen und
weist15fehlerhafte Varianten ab.

Erster Compilelauf: Prüfhilfe vor ihren Abhängigkeiten sortiert, durch
Reihenfolgekommentar korrigiert. Nach Semantikerweiterung Codec-Testwerte
an legitime partielle/vollständige Werte angepasst; ein erster Fall war
falsch konstruiert, übrige82 erfolgreich. Keine Produktionsfehler daraus.

Synthetische Probe kopiert eine Aufnahmeantwort je Diagnosebefehlsfragment;
Replysender höchstens einen Spielantwortframe pro Loop. Begrenzte
Diagnosebefehlschleife ist kein SDKcallback. Transportowner soll höchstens
einen Eingangsframe je Arbeitsschritt bearbeiten. Kein neuer Spieltick,
Funk-/App-/Peerbestätigungsnachweis.

Vier finale Builds SUCCESS/Exit0: Produktion55.148RAM/1.228.473Flash,
alteUSBprobe21.480/291.729, Profilprobe100.424/391.097 plus57.272Profilheap/
Allocator/FS. Kein zusätzlicher statischer Receiptpuffer; bestehender Bearer
wiederverwendet. CommandReceipt16/Position8/FrameDispatchResult40Byte im
Xtensacompiler. Eigene Frames copyReceipt208/dispatch208/encode32/decode48/
valid32 plus Ketten/Caller/SDK, keine gemessene Gesamttiefe.
Architektur91/210, Header58, Format154 bestanden; v2/Katalogschema,
Generator und beide unabhängigen Framegegenstellen gültig.
Tatsächliche CLI nach finalem Build unverändert13Befehlsframes/1732Katalogbytes.
Kein Upload/SMP/RF/Android/Remote-CI oder Cppcheck dieses Schnitts.
Statusframing, Identität/GM-Zuordnung/Pairing/SDK, Profiltransfer,
Produktionsanschluss und physische Abnahmen offen.
