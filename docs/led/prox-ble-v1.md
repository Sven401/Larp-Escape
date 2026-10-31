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


### Oeffentliche Abbruchdiagnose (USB)

`BLE-DIAG` ist ein separates, lesendes USB-Kommando. Bestehendes `INFO` und
`BLE-INFO` behalten ihr Schema. Antwort: `type="ble-diag"`, `protocol=1`,
`reason`, `epoch`, `atMs` (lokale Boot-Millis), `messageType`, `offset`, `ageMs`,
`sdkCode`. Alle Werte sind oeffentliche Metadaten; keine Payload/Nonce/Schluessel.
Der erste lokale Abbruchgrund eines Links bleibt bis zum naechsten lokalen
Abbruch gespeichert, auch nach Disconnect/Reconnect. Grund 0 bedeutet keinen
aufgezeichneten lokalen Abbruch; ein entfernter Disconnect setzt keinen Grund.
Zeitwerte sind uint32 mit Wrap, nicht Handyzeit. Alter/Offset/Typ/SDKcode sind
nur bei passenden Bearer-Ursachen belegt, sonst 0.

| reason | Bedeutung |
|---|---|
| 1 | unspezifischer Owner-Abbruch |
| 2 / 3 | Ingress- / Egress-Queue voll |
| 4 | absolutes Egress-Nachrichtenalter erreicht |
| 5 / 6 | Frame-Encoding / Transferzaehler erschoepft |
| 7 / 8 | Notify-Busy-Frist / anderer Notify-SDKfehler |
| 9 / 10 | Notify abbestellt / Hostfehler |
| 11 | Challenge-Erzeugung oder Versand |
| 12 / 13 | Ingress zu alt / Assembly ungueltig |
| 14 / 15 | Proof oder AuthOK / Command ungueltig |
| 16 | Result-Erzeugung oder Versand |
| 17 / 18 | unauthentisierte 5s-Frist / Telemetrie-Erzeugung oder Versand |

Die Diagnose lockert keine Frist: Telemetrie bleibt maximal 1000ms ab signiertem
Geraetesnapshot im Egress, andere Nachrichten maximal 2000ms. Die Snapshotzeit
an Telemetrieoffset24 und Bearer-Zeit stammen beide aus `esp_timer_get_time()/1000`.
Ein erfolgreicher Notify-SDKaufruf bestaetigt nur die Uebergabe an NimBLE, keinen
Empfang der App. Die Diagnose ist kein Funk- oder Latenz-Messnachweis.


### ATT-Ressourcendiagnose

Grund19 in `BLE-DIAG` bezeichnet einen lokalen `mbuf_to_flat`-Fehler (SDKcode
oder -1 bei unerwarteter Kopierlaenge). Der Link wird dann lokal gesperrt.
`BLE-STATS` liefert separat `type="ble-stats"`, `protocol=1`, `epoch`,
`accessCalls`, `accessOk`, `accessAtMs`, `accessSize`, `accessCode`, `notifyCalls`,
`notifyOk`, `notifyBusy`, `notifyAllocFailures`, `minMsysFree`,
`minNotifyPoolFree`, `lastNotifyPoolFree`. Zaehler sind saturierend uint32 und
werden bei neuem Link zurueckgesetzt; sie bleiben nach Disconnect lesbar.
`accessCode` ist der ATT-Rueckgabecode des letzten eigenen Accessaufrufs.
Minima -1 bedeuten noch keine Beobachtung. Callbackzaehler erfassen nur den
zum eigenen Input zugelassenen Writepfad, keine CCCD/anderen Attribute.

MSYS ist die globale Anzahl freier Bloecke; ein positives globales Minimum
beweist keinen freien Block im passenden Pool. Der Notify-Poolwert stammt aus
dem tatsaechlich zugewiesenen mbuf (`om_omp->omp_pool->mp_num_free`), unmittelbar
vor `notify_custom`, nachdem dessen eigener Block bereits reserviert wurde.
Die Werte sind beobachtete Momentaufnahmen, keine reservierten Credits oder
garantierten Tiefstwerte zwischen Abtastungen. Bei Allocationsfehler ist der
passende Pool fuer diesen Versuch unbekannt. NotifyBusy umfasst ENOMEM/EBUSY;
NotifyOk ist nur SDK-Uebergabe, kein Funk-/Appempfangsnachweis.

Im exakt gepinnten SDK reserviert `ble_att_svr_rx_write` seine Antwort vor
unserem Accesscallback (`ble_att_svr_build_write_rsp` / `ble_att_svr_pkt`).
Dort kann ATT17 entstehen, ohne unseren Accesszaehler zu erhoehen. Ein Zaehler-
vergleich ueber den vollstaendigen Fehlerzeitraum kann diese Grenze eingrenzen.
`BLE_GAP_EVENT_NOTIFY_TX` wird bei Notifications im `ble_gatts_notify_custom`-
Pfad nach L2CAP-Uebergabe ausgeloest und ist kein Controller-/RF-TXende. Die
Diagnose aendert weder Sendefristen noch Ressourcenlimits oder Wiederholungen.


### Kooperativer ATT-Poolvorrat

Nach belegter Poolerschoepfung im Geraetelauf wird vor jeder Notification nach
der eigenen mbuf-Allokation die freie Anzahl im tatsaechlichen Pool geprueft.
Sind weniger als vier Bloecke frei, gibt der Sender ausschliesslich sein noch
nicht an NimBLE uebergebenes mbuf einmal frei und vertagt denselben Fragment-
versuch auf den naechsten Poll. `BLE-STATS.notifyReserveDeferrals` zaehlt diese
Schutzvertagungen getrennt von SDK-ENOMEM/EBUSY (`notifyBusy`). `notifyCalls`
zaehlt weiterhin Versuche inklusive Allokationsversuchen und Schutzvertagungen;
`notifyOk` zaehlt nur erfolgreiche SDK-Uebergaben. Der Vorrat vier ist eine
heuristische Restkapazitaet fuer ATT-Eingang/Antwort, keine atomare Reservierung
gegen andere Tasks. Keine Poolvergroesserung und kein neuer Command-Retry.

Offset, Transfer-ID und bestehende absolute Nachrichtenfristen bleiben bei
Vertagung erhalten. Reicht die Kapazitaet bis zum bisherigen Alterslimit nicht,
bleibt der sichtbare EgressAge-Abbruch erhalten. Bei zugewiesenem mbuf ohne
verwendbare Poolinformation wird das eigene mbuf freigegeben und der Link
sichtbar mit NotifyError/EINVAL gesperrt, nicht als Poolnull/Allocationsfehler
etikettiert. Sobald `notify_custom` aufgerufen wurde, konsumiert ausschliesslich
das SDK den Puffer auf jedem Rueckgabepfad; kein zusaetzliches eigenes Free.


### MTU-angepasste Transportframes

Der 8-Byte-Fragmentheader und die innere Message (maximal256 Byte einschliesslich
HMAC) bleiben unveraendert. Ein Frame hat 9..244 Byte. Sein Budget betraegt
`min(244, ausgehandelteATTmtu-3)`; bei unbekanntem MTU oder MTU23 gilt20.
Es darf kein Erfolg eines MTU-Requests vorausgesetzt werden. Firmware liest
`ble_att_mtu(handle)` ausschliesslich im Hosttask am aktuellen Link. Input
prueft sowohl244 als auch das aktuelle ausgehandelte Budget; Callbacks kopieren
maximal244 Byte in die begrenzte Ownerqueue. Die alte20-Byte-Zerlegung bleibt
vollstaendig gueltig. Der eigene PROX-Assembler hat weiterhin256 Byte Budget.

Die App handelt vor Discovery/CCCD eine groessere MTU aus (Ziel247) und
verlangt ein bestaetigtes erfolgreiches Ergebnis mit MTU>=156, sonst wird
der Livekanal nicht gestartet. Android kann stattdessen517 anfordern;
die Firmware bietet explizit preferredMTU256 an. Der Firmwaretransport bleibt
mit MTU23/20-Byte-Frames kompatibel. Bei MTU>=156 passen
145 Byte Telemetrie plus8 Byte Header in ein153-Byte-Frame. Bei Budget244
benoetigt die256-Byte-Maximalmessage zwei Frames (244 und28 Byte). Die Groesse
kann zwischen Fragmenten wechseln, die Offset-/Transfer-/Längenpruefung bleibt
streng. Framebudget<=8 oder>244 ist ungueltig. Fristen, Replay-/HMACpruefung,
Source-Loss und Ressourcenvorrat werden dadurch nicht gelockert. Vor jedem
Fragment gelten weiterhin das absolute Egressalter und der tatsaechliche
Poolvorrat; eine groessere ATT-Nutzlast ist kein TX-Credit.
