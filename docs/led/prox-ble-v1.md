# PROX-BLE/1: begrenzter nativer Näheglut-Devanschluss

Stand 31.10.2026. Vertragsbasis für den beauftragten Dev0.2-Prüfstand, keine
Produkt-TLS-/D0-/P0-Freigabe. Firmware und Android implementieren diesen Vertrag
getrennt; Hardware-/BLE-/Funkabnahme ist ein eigenes Tor. Der bestehende native
TLS-Bearer und LED-V2-Nachrichten werden nicht verändert.

## Transport und Zugang

Service `6c790101-2f50-4d5a-a7f4-35a478c4b101`, Eingang
`6c790102-2f50-4d5a-a7f4-35a478c4b101` (ATT Write Request), Ausgang
`6c790103-2f50-4d5a-a7f4-35a478c4b101` (Notify/CCCD). Eine aktive Verbindung je
Station; ein Handy hält zwei unabhängige Verbindungen zu den beiden Stationen.
Benachrichtigungen vor jedem Protokollstart abonnieren. Kein Dauer-Scan nach
Verbindungsaufbau. Anzeigename `Larp NG`; Scanantwort-Herstellerdaten `0xffff`
enthalten die 16 öffentliche Device-ID-Bytes; dies ist nur ein Suchhinweis,
keine Authentisierung. Die HMAC-Challenge prüft die tatsächliche Identität.

Die Fragmentheadersyntax entspricht LED-V2: 20 Byte ATT-Nutzlast bei MTU23,
8 Byte Fragmentheader. PROX verwendet einen eigenen begrenzten 256-Byte-
Assembler/Encoder; der bisherige LED-V2-128-Byte-Typ bleibt unverändert. Genau ein vollständiger Auftrag je Richtung wird geordnet
übertragen. Framingfehler, Assemblytimeout und volle Queue sperren diese
BLE-Verbindung; sie stoppen weder autonomen Core noch Funk-/LED-Betrieb.
ATT-Schreiberfolg ist nur Transportannahme, kein fachliches Ergebnis.

`BLE-ACCESS` exportiert ausschließlich nach explizitem vertrauenswürdigem USB-
Auftrag ein individuelles Stationscredential (frische Zufallswerte, eigener
NVS-Eintrag). Es ist strikt vom ESP-NOW-Paargeheimnis/PMK/LMK getrennt:

```json
{"schemaVersion":1,"protocol":"LARP-PROX-BLE/1","deviceId":"32 kleine Hexzeichen","secret":"64 kleine Hexzeichen","name":"Larp Näheglut","stationMac":"AA:BB:CC:DD:EE:FF"}
```

App speichert das Secret wie bestehende Zugänge im Android-Keystore-gestützten
Tresor. Keine Secrets in Telemetrie, normalen INFO-Antworten, Logs oder
Kennlinienexporten. Fehlende/beschädigte BLE-Credentials werden nicht still
ersetzt. Kein BLE-Zugriff darf ARM/STOP oder Funkproben auslösen.

## Nachricht und HMAC

Alle Mehrbytezahlen little endian; int8 im Zweierkomplement. Jede vollständige
Nachricht beginnt mit `version=1:u8, type:u8, totalLength:u16`; totalLength umfasst
Header und abschließenden 32-Byte-HMAC. Unbekannte Typen/Längen/Flags abweisen.
HMAC ist SHA256. Domänen sind unveränderte ASCII-Bytes ohne Nullterminator,
unmittelbar vor den angegebenen Bytes (keine impliziten Trenner).

Kontext `C = deviceId16 || bootNonce16 || linkNonce16 || clientNonce16`.
Bootnonce ist frisch je Boot, Linknonce je BLE-Verbindung, Clientnonce frisch je
Appverbindung. Nie Sessionresume. Accesskey ist das individuelle Credentialsecret.
Sessionkey `HMAC(accesskey, "LARP-PROX-BLE/1/session" || C)`.
Session-ID sind erste16 Bytes von
`HMAC(accesskey, "LARP-PROX-BLE/1/id" || C)`.

| Typ | Gesamtlänge | Aufbau nach Header | HMAC |
| --- | ---: | --- | --- |
| `0x81` Challenge | 90 | deviceId16, boot16, link16, curveRevision:u32, capabilities:u16 (=1) | accesskey, `/challenge` + Bytes0..57 |
| `0x01` Proof | 52 | clientNonce16 | accesskey, `/proof` + C |
| `0x82` AuthOK | 88 | sessionId16, curveRevision:u32, curveDigest32 | sessionkey, `/auth-result` + Bytes0..55 |
| `0x02` GetCurve | 62 | sessionId16, request:u32, observedDeviceMs:u32, ttlMs:u16 | sessionkey, `/get-curve` + Bytes0..29 |
| `0x03` SetCurve | 74 | sessionId16, request:u32, observedDeviceMs:u32, ttlMs:u16, expectedRevision:u32, points8 | sessionkey, `/set-curve` + Bytes0..41 |
| `0x83` CurveResult | 102 | sessionId16, request:u32, code:u8, revision:u32, digest32, points8, durable:u8 | sessionkey, `/curve-result` + Bytes0..69 |
| `0x84` Telemetry | 145 | Tabelle unten | sessionkey, `/telemetry` + Bytes0..112 |

Alle Domänensuffixe oben enthalten das vollständige Präfix
`LARP-PROX-BLE/1`, beispielsweise `LARP-PROX-BLE/1/proof`.
Challenge wird nach CCCD-Abo gesendet. Ungültiger Proof beendet nur BLE.
AuthOK schaltet die passive Telemetrie frei (maximal2Hz). Erst frische,
verifizierte Telemetrie liefert observedDeviceMs für Verwaltungsaufträge.
Vorher GetCurve/SetCurve nicht senden. Die App prüft Boot/Link/Devicekontext,
Session-ID, authentisierte Telemetriefolge und Handy-Empfangsalter getrennt vom
Quellenalter. Verbindungsabbruch stellt keinen autonomen Stop oder Neustart dar.

Requests beginnen bei1 und steigen innerhalb einer Session strikt. Ein
identischer letzter Auftrag darf sein gecachtes Ergebnis erneut erhalten,
ohne NVS/Anwendung zu wiederholen; gleicher Request bei anderem Inhalt ist
Konflikt, ältere Requests stale. Maximal ein offener Verwaltungsauftrag.
TTL1..2000ms; lokales Empfangsalter `now-observedDeviceMs <= ttlMs`, zusätzlich
keine zukünftige Zeit nach unsigned Halbintervallvergleich. Deadline betrifft
Annahme, nicht eine zugesagte Flashdauer. Keine Uhrsynchronisation mit Handy.

## Kennlinie, Revision und Resultat

Genau vier Punkte zu je `rssi:int8, percent:u8`. RSSI -127..-1, strikt steigend;
Prozent0..100 nichtfallend (Plateaus zulässig). Standard
`(-90,0),(-75,30),(-60,60),(-40,100)`. Interpolation integer in level0..1000:
`10*y0 + (rssi-x0)*10*(y1-y0)/(x1-x0)`, nach unten abrunden; außerhalb klemmen.
Eingang bleibt der bisherige geglättete ganzzahlige RSSI; keine Meterangabe.
Ungültige Quelle blendet unabhängig von jedem Kurvenendpunkt nach null.

Digest ist SHA256 der genau10 Bytes `[1,4,x0,y0,x1,y1,x2,y2,x3,y3]`.
Fehlender Datensatz liefert Defaultrevision1 mit durable=false, ohne automatischen
Flashwrite. Explizites USB BLE-ACCESS prüft zuerst alle Datensätze und persistiert
fehlende Defaultkurve und App-Credentials mit Readback. Wiederholter Export liest
nur; korrupte Datensätze werden nicht ersetzt. Revision beginnt bei1, ist stationslokal persistent und darf nicht überlaufen.
SetCurve prüft CAS, komplette Kennlinie und Frist vor Speicherwirkung. Nur
bewusstes Übernehmen schreibt NVS, nicht Dragbewegungen. Erfolg erst nach
Commit und exaktem Readback, anschließend weiche Zieländerung im vorhandenen
NearGlow-Lauf; kein Scene.reset/ARM und keine Erneuerung der Quellenfrische.

CurveResult-Codes: 0 Erfolg, 1 falsche erwartete Revision, 2 ungültige Punkte,
3 abgelaufener Auftrag, 4 belegt, 5 Speicherfehler, 6 veralteter Request,
7 Requestkonflikt, 8 Revision erschöpft, 9 dauerhaft gespeichert aber nicht
im laufenden Core angewandt. Code9/durable1 meldet die neue gespeicherte
Revision; die angewandte Ausgaberevision bleibt separat/gegebenenfalls0. durable0/1 bezieht sich auf die
bestätigte Persistenz des mitgeteilten aktuellen Kurvenstands; nur code0 und
durable1 bestätigen erfolgreiche Übernahme. Fehler verändern keine Teilpunkte.
Bei unklarem Speicher-/ACK-Ausgang kein automatischer neuer Schreibauftrag:
frischen Zustand/Revision/Digest lesen und abgleichen. Es gibt keine atomare
Zwei-Stationen-Übernahme und kein implizites Rollback einer erfolgreichen Station.

## Telemetrie: Bytes und Bedeutung

| Offset | Bytes | Inhalt |
| ---: | ---: | --- |
| 0 | 4 | Header |
| 4 | 16 | Session-ID |
| 20 | 4 | Telemetriefolge1..uint32max, je Link strikt steigend |
| 24 | 4 | Gerätemillis zum kopierten Snapshot |
| 28 | 2 | Flags siehe unten |
| 30 | 2 | autonomer Faultcode (bisherige Näheglut-INFO-Codes) |
| 32 | 2 | BLE-/Konfigurationsfaultcode, 0 gesund |
| 34 | 1 | letzter bestätigter roher RSSI, 0 falls nie gemessen |
| 35 | 1 | letzter gefilterter RSSI, 0 falls nie gemessen |
| 36 | 2 | targetLevel: bewertetes Helligkeitsziel0..1000 (bei invalid0) |
| 38 | 4 | lokales Alter der letzten gültigen Quellprobe; uint32max falls nie |
| 42 | 4 | Sequenz der zuletzt angenommenen eigenen Funkprobe; 0 falls nie |
| 46 | 6 | konfigurierte Peer-STA-MAC |
| 52 | 3 | zuletzt berechnetes RGB (laufende weiche Rampe) |
| 55 | 3 | zuletzt beim RMT eingereichtes RGB |
| 58 | 3 | zuletzt durch RMT-TX-Ende bestätigtes RGB |
| 61 | 4 | Gerätemillis der letzten RMT-Einreichung |
| 65 | 4 | Gerätemillis der letzten RMT-Endbestätigung |
| 69 | 4 | aktuelle gespeicherte/konfigurierte CurveRevision für CAS (durableKnown separat) |
| 73 | 32 | Digest der gespeicherten/konfigurierten Kurve (keine Output-Anwendungszusage) |
| 105 | 4 | Kennlinienrevision beim letzten RMT-Submit |
| 109 | 4 | Kennlinienrevision des letzten bestätigten RMT-Frames |
| 113 | 32 | Telemetrie-HMAC |

Flags: bit0 paired,1 armed,2 persistedArmed,3 sourceConfigDurableKnown,
4 sourceFresh,5 radioReady,6 txIdle,7 outputKnown,8 outputPending,9 black,
10 curveDurableKnown; Bits11..15 null. RGB-Übertragungsbestätigung ist keine
optische Messung. Stale RSSI darf als letzter Wert grau dargestellt werden,
niemals als frischer Abstand. Ein BLE-Ausfall erhält ein eigenes Handy-Alter;
er darf keine frische Quellmessung oder einen bestätigten Effekt erfinden.

## Nebenläufigkeit und Prüftore

Autonomer Owner bleibt alleiniger Besitzer von Core, Szene, RF und RMT.
NimBLE-Callbacks kopieren nur begrenzte Frames mit Verbindungs-/Epochennachweis.
BLE-Empfangszeit wird im Callback kopiert; Owner verzögert die Frische nicht
auf den Drainzeitpunkt. Eingangsframes ab2000ms Alter sperren den Link.
Ausgehende Telemetrie ist vom signierten Snapshot-Gerätemillis bis zum letzten
Fragment auf <1000ms begrenzt, andere Nachrichten auf <2000ms; jede
Überschreitung sperrt den Link, statt alte Daten scheinbar frisch nachzuliefern.
Das ist kein Uhrsync- oder exakter Einweglatenznachweis.
Eine getrennte BLE-Queue erschöpft/sperrt ausschließlich BLE, niemals den
ESP-NOW-Ereignispuffer. Owner erzeugt kopierte signierte Meldungen; Hosttask
sendet begrenzte Fragmente. Kein Queue-/SDK-Aufruf unter Snapshot-Spinlock.
Keine BLE-Trennung verändert armed/source freshness oder erzeugt Stop/Start.

Software-Koexistenz auf dem gepinnten SDK aktivieren; WiFi- und NimBLE-Task auf
getrennten Cores. Bestehende connectionless Powersavefenster nicht verändern.
Passive2Hz-Telemetrie und zwei parallele Handyverbindungen benötigen echte
S3-Last-/Funklücken-/Konfigurations-/Reconnect-Nachweise. Ein erfolgreicher
Host-/SDK-Build ersetzt diese nicht. Diese Vertragsfassung ist erst zusammen
mit passender App-/Firmwareprüfung als implementiert zu markieren.
