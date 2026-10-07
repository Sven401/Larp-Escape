# Gepinnter früher GATT-Quellanschluss und originale SDK-Antwort

Stand: 7. Oktober 2026, Firmwarebasis27fd693. Isolierter Firmware-/Toolpfad, keine Produktionsfreigabe oder Geräteabnahme.

## Tatsächlicher Anschluss

`Esp32GattsSource` verbindet die vorhandene IdentityAccess-Policy, originale FreeRTOS-Lifecyclebindung, Command-/Profilqueues und ATT-Antwortqueue mit dem gepinnten privaten Bluedroidpfad. Ausschließlich das Ziel `esp32-gatts-abi-probe` aktiviert den Linkerwrapper `--wrap=GATT_Register`. Die bestehende Produktion bleibt separat.

Vor SDKstart wird genau ein dauerhaftes Objekt für eine eigene 16-Bit-Appregistrierung (Probe0x4c45) eingerichtet. Der Wrapper ersetzt beim ersten passenden `GATT_Register` nur Connect-/Requestcallback der kopierten siebenfeldrigen Tabelle. Unveränderte andere Anwendungen und fünf andere Callbackzeiger bleiben erhalten. Kein zweiter Registrierungsversuch/Rebind. Fehlende Callbacks/fehlgeschlagene Registrierung liefern0. Registrierung erteilt keine Peer-/GMrechte.

Der tatsächliche Rückgabewert bindet Interface, BTUtask und SDKthreadinstanz. Auf dieser selben Quelltask müssen einmalig zwei unterschiedliche, tatsächlich erzeugte Attributhandles installiert werden, bevor irgendein Verbindungscallback eintrifft. Der vorhandene64Byte-Lifecycleowner wird im eigenen ausgerichteten Speicher per placement-new gebaut; keine Heapallokation. Endpointkonfiguration und Owner werden nie ersetzt. Zu frühe Verbindung sperrt die spätere Einrichtung dauerhaft; keine nachträgliche Übernahme einer bestehenden Verbindung.

Connect/Disconnect werden vor dem ursprünglichen SDKcallback aufgenommen. Dieser Callback wird danach außerhalb eigener Sperren weitergeleitet. Seine spätere BTCqueue darf ausfallen, ohne unseren bereits erfassten Ursprung neu zuzuordnen. Alle WRITE/EXECcallbacks der ausgewählten Anwendung werden direkt am Quellrand konsumiert; kein zweiter Eingang über BTA/BTC. READ/MTU/CONF bleiben beim ursprünglichen Callback. Die Anwendung ist ausschließlich für diese zwei Eingangsattribute vorgesehen; unbekannte Attribute erhalten in diesem Schnitt keine eigene Antwort, sie werden ohne Wiederzuordnung verworfen. CCC-/Status-/Readanschluss bleibt offen.

Private `conn_id` bleibt vollständig erhalten: oberesByte=TCBindex, unteresByte=GATTinterface. Beispiel257=0x0101 darf nicht als BTCconn_id1 in `GATTS_SendRsp` gelangen. Originale Adresse kommt aus dem vorherigen unmittelbaren Quellconnect im selben Akteur; keine späte SDKadress-/Handleabfrage. Requestskalare und höchstens128 inline geliehene Bytes werden vor Clock-/Authority-/Queuebeobachtung kopiert. Null/überlange Daten werden nicht gelesen. EXEC/Prepare haben keinen Retentionspuffer und werden ausdrücklich als nicht unterstützt beantwortet, sofern eine ursprüngliche Verbindung besteht. Die bestehende Rechte-/Frameprüfung entscheidet über tatsächliche Aufnahme.

## Feste Antwortplätze bis zur wirklichen SDK-Verarbeitung

Ein Worker übernimmt einen vollständigen `RoutedWriteResponse` unverändert und reserviert höchstens einen von vier festen Plätzen. Jeder Platz bleibt belegt bis seine BTUcallbackverarbeitung endet. Zustände frei/eingereiht/inBearbeitung verhindern Reentranz; unter der kurzen eigenen Sperre werden nur lokale feste Werte kopiert. Kein Clock-/Policy-/SDK-/Parser-/Spiel-/Dateiaufruf unter eigener Sperre.

Der tatsächliche SDKport benutzt `osi_thread_post` mit Queueindex0 und Timeout0. Kein BTC-/BTAantwortposting und kein eigener Heap im Postingadapter. Bei Poolvoll/SDKqueuevoll/andererSDKinstanz wird der Platz freigegeben und die Ausgabe verworfen, ohne Retry oder Rücknahme des bereits kopierten Eingangs. Die gepinnte OSIqueue kopiert Funktion/Kontext nach Wert; False bedeutet, dass kein Callback eingereiht wurde. Ein erfolgreicher Post kann schon vor der Rückkehr konsumiert werden.

Die endgültige BTUcallbackprüfung verwendet dieselbe Task wie der frühe Quellcallback: originale Generation/Verbindung/Interface/Adresse/Attribut und absolute lokale1000ms-Policy (Alter<1000, ursprüngliche Zeit nie erneuert). Danach erfolgt direkt `GATTS_SendRsp(packed_conn_id, trans_id, status, nullptr)` in diesem Akteur. Die Originalgeneration liegt bis zu dieser endgültigen Verarbeitung im eigenen festen Platz. Wiederverbindung mit derselben SDKnummer/Adresse verwirft alte eingereihte Antworten. GMentzug innerhalb derselben Verbindung verhindert spätere Ausführung, lässt aber den ursprünglichen Aufnahme-/Fehlerantwortkontext bestehen.

True beim Worker bedeutet ausschließlich lokales SDKposting. Rückgabestatus von `GATTS_SendRsp` löst keinen Retry aus und behauptet keine Zustellung oder Spielausführung. Die SDKinterne ATTpaketerzeugung kann allokieren; vollständige SDKcallbacks und Funkarbeit besitzen keine hier gemessene Zeit-/Heapgarantie.1000ms ist lokale Auswahlpolicy, weder ATTtimeout noch gemessene Funklatenz.

## Private ABI und Quellenbelege

`SdkGattSourceAbi.h` enthält nur die benötigte private C ABI, abgeleitet aus Apache-2.0-ESP-IDFv4.4.6. `BOOLEAN` ist tatsächlich bool; UUID20Byte, Write/Request608Byte mit inline600Byte-Wert und Boolfeldern an606/607, Response610Byte. Keine geratenen Anwendungscallbacktypen verwenden. Compileasserts prüfen Version/Extent/Layout. Der isolierte Buildguard akzeptiert ausschließlich das geprüfte ESP32-`libbt.a` SHA256 `1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787`; unbekannte Binarys verlangen erneuten Audit. Keine installierte SDKdatei wurde geändert.

Offizielle Quellbasis: `https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/` mit relativen Pfaden:

- `common/include/bt_common.h`: echte Scalar-/UUIDdefinitionen.
- `host/bluedroid/stack/include/stack/{bt_types.h,btm_ble_api.h,gatt_api.h}`: echte Interface-/Transport-/Write-/Response-/Callbacktypen.
- `host/bluedroid/stack/gatt/include/gatt_int.h`: gepackte Verbindungscodes.
- `host/bluedroid/stack/gatt/{gatt_api.c,gatt_sr.c,att_protocol.c}`: Registrierung kopiert Callbacktabelle; direkte Antwort prüft SDKverbindung/trans_id, verarbeitet anschließend synchron; Nullpayload ist für normales Write-ACK zulässig, Fehler benötigen keine Erfolgsechokopie.
- `host/bluedroid/bta/gatt/bta_gatts_act.c`: ursprünglicher Connect-/Requestbridge und Registrierung.
- `host/bluedroid/stack/btu/btu_task.c`: BTUdispatcher auf OSIqueue0.
- `host/bluedroid/osi/{osi_thread.c,include/osi/thread.h}`: tatsächliches zeroWait-Valueposting.

`scripts/check_gatt_source_abi.py` prüft12 konkrete Quellhashes und extrahiert echte Typdefinitionen in eine temporäre Referenz. Unabhängiger C++11-Compilervergleich: acht Größen/Ausrichtungen,16 Feldpositionen und exakte Scalar-/Booltypen des Connectcallback. Die Hashliste ist im Werkzeug versioniert; keine Netzabhängigkeit im Prüflauf. Reproduktion nach lesendem Bezug der genannten Dateien: `python scripts/check_gatt_source_abi.py --source-dir <SDKquellen> --compiler <g++>`. Nicht aus diesem Quellvergleich eine ausgeführte Funk-/SMP-/Geräteprüfung ableiten.

## Host-/Toolbeleg

11 neue Hostfunktionen, tatsächlicher Adapter mit deterministischen C-/RTOSports; finale Suite349/349/Exit0.13 originale Schreibframes→Commandstream→tatsächlicher Anwendungsstart. Im Modell13 SDKposts und13 spätere direkte GATTaufrufe mit gepacktem257 und Transaktionen1–13; SpielRunning/nextRequest2. Die unabhängige Gegenstelle prüft die tatsächliche Fixture `fixtures/gatts/source-posting-observation.json` und verwirft17 falsche Beobachtungen. Fakeverifizierung und Fake-SDKqueue/-sender bleiben sichtbar Hostmodell.

Weitere tatsächlich geprüfte Fälle: bereits gepostete alte Ausgabe nach gleicher Verbindung/Adresse, volle vier Plätze, fehlgeschlagenes Posting/Platzwiederverwendung, Alter999/1000 und Clockwrap, falsche Task/SDKinstanz, reentrant derselbe konsumierte Platz, unmittelbare synchrone Konsumption, lokaler SDKfehler ohne Retry, GMentzug mit Kopier-ACK ohne Eingangsausführung, Null/129Byte/Prepare/EXEC/unbekanntesAttribut, unabhängiger Profilpfad, einmalige/frühe/ungültige/falscheTask-Endpointkonfiguration, Scalar-/Byte-/Antwortkopie vor externer Mutation und Wrapperweitergabe erst nach ursprünglichem Disconnect.

Nur die eigene neue Fixture wurde im ersten erfolgreichen Lauf erzeugt; finale Suite ohne Export. Kein erneuter allgemeiner Fixtureexport. Der anfängliche eigene Compilerfehler (fehlende Strukturklammer) wurde vor erfolgreichen Läufen korrigiert; Log getrennt. Die unabhängige Compilerprüfung verwendet wie der bestehende Headerguard nur für ihren Unterprozess das Toolchainbin-Verzeichnis im Suchpfad.

## Grenzen und nächster Schnitt

SDKprobe armt den tatsächlichen Hook und prüft den uninitialisierten Ablehnungszweig, linkt die echten Registrierungs-/Posting-/GATTsymbole, startet aber keinen Bluetoothstack, keine Dienste oder Funk. Das Objekt, Policy, Queues und Antworten müssen über alle SDKcallbacks/Posts leben. Einmalige eigene Serverregistrierung; kein Deregistrieren/Stoppen/Neustarten des SDK bei anstehenden Werten. Der gecapturete BTUtask-/SDKinstanzbezug darf nicht recycelt werden. Diese Integration ersetzt keine solche Lebensdauerfreigabe.

Offen: tatsächlich erzeugte Service-/Attributhandles per original gebundenem Konfigurationsauftrag auf die Quelltask übertragen und Abschluss vor Connect nachweisen. Danach kryptografisch verifizierte Peeridentität mit expliziter persistenter GMzuordnung; Status-/Spiel-/Profilantwortausgabe am ursprünglichen Funklink, produktive Task-/Dienstinstallation und Androidanschluss. Produktionsreserve/reale Gerätebesitzer und physische RF/SMP/Stack/Heap/Zeit/Energie/Flashprüfungen bleiben getrennt offen. Kein neuer Upload.


## Finale Builds, tatsächlicher Link und Budget

Fünf Ziele final erfolgreich/Exit0 nach allen C++-Änderungen: Produktion55.148ByteRAM/1.229.077ByteFlash(93,8%), alte USBprobe21.480/291.881, Profilprobe107.560/406.865, SDKprobe41.984/1.016.421(77,5%) und Hostprofilwerkzeug. SDKprobe netto+15.512ByteRAM/+726.192ByteFlash gegenüber27fd693; sie zieht jetzt tatsächlich Bluedroid ein. Diese isolierten Werte nicht zur Produktion addieren: gemeinsame Bibliotheken überschneiden sich; ein kombinierter Produktionsanschluss ist nicht gebaut. Profilstartupheap bleibt57.272Byte plus Allocator/Dateisystem, weiterhin ungemessen. Bestehende Vendorwarnungen bleiben im Log.

Actual finales ELF: Quellenport356Byte inklusive64Byte-Verbindungsowner und vier52Byte-Postingplätzen; zusätzliche ATT-Antwortqueue272Byte; Wrapperregistrierung zusätzlich35Byte Einzelglobals vor Linkerpadding. Eigene Compilerframes: request224, connection64, originalLive64, submit80, consume80, registrationwrapper64, source/configuration32Byte. Darunter kommen bestehende Originalaufnahme240/Rawaufnahme336 und SDK-/FreeRTOS-Aufrufketten hinzu; keine Gesamtstackreserve oder SMP-/Latenzmessung.

Die finale SDK-BTAregistrierung lädt im ELF das Aufrufliteral0x4014bfc8 mit Wert0x400d48c8: tatsächliches `__wrap_GATT_Register`. Die finale eigene `consume`-Routine ruft unmittelbar `GATTS_SendRsp` auf, nachdem Original-/Task-/Altersprüfung abgeschlossen ist. Adressen gelten nur für diesen lokalen ELFstand. Compile-/Linknachweis, kein ausgeführter BTU-/RFnachweis.

Unabhängiger Quell-ABIvergleich positiv; zwei gezielt veränderte temporäre Quell-/Archivfixtures werden von den Guards abgewiesen. Kein installiertes SDK wurde für diese Negativprüfung verändert. Architektur124Dateien/267Includes,75 reine C++11-Header und245 Formatdateien erfolgreich. Zwölf App-v2-Paare frisch byte-/SHAgleich; keine Appänderung/-prüfung.

Lokale Tempbelege: `larp-gatts-source-host-final-20261007.log` (349/349 ohne Export), `larp-gatts-source-host-first-20261007.log` (anfänglicher Compilerbefund), `larp-gatts-source-host-second-20261007.log` (347/347, eigene neue Fixture erzeugt), `larp-gatts-source-builds-final-20261007.log`, `larp-gatts-source-abi-20261007.log`, `larp-gatts-source-counterpart-20261007.log`, Architektur-/Header-/Formatlogs und `larp-gatts-source-budget-20261007/` (nm, eigenes Compilerobjekt, tatsächliche SDKregistrierungs-/GATTaufrufe). Kein Upload, Portöffnen, SDKstart, Formatieren, Push, PR, Merge oder Remote-CI.
