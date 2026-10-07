# Ursprüngliche Callbackbindung und FreeRTOS-Verbindungseigentümer

Stand: 7. Oktober 2026, auf Firmwarebasis fb3a272. Lokaler Firmware-/Toolpfad; Produktionsanschluss, SDK-Gesamtherkunft und Geräteabnahme offen.

## Ausgeführter Pfad

`FreeRtosGattsConnection` besitzt genau eine aktive Callbackverbindung und zwei feste, unterschiedliche Attribute für Befehle und Profile. Die Endpointwerte werden beim Bau kopiert. Connect erzeugt über den tatsächlichen IdentityAccess-/Authorityport nur eine neue geschlossene Verbindung mit einmaligem lokalem Token. SDKnummer, Adresse, HCIhandle, Linkrolle und Sicherheitsflags verleihen keine Spielleitungsrechte.

Connect/Disconnect/Writeaufnahme und die zugehörige IdentityAccess-Administration haben genau einen serialisierten unmittelbaren Callbackeigentümer. Worker lesen nur unveränderliche Snapshots unter einer kurzen eigenen FreeRTOS-Sperre. Keine IdentityAccess-, Authority-, Clock-, SDK-, Queue-, Parser-, Datei- oder Spielarbeit wird unter dieser Sperre ausgeführt. Die bestehende atomare Authoritysperre schützt getrennt die Rollenepoch.

Die tatsächlichen SDK-Konverter `fromSdkGattsConnect` und `fromSdkGattsDisconnect` kopieren Interface/conn_id/Adresse aus den gepinnten Callbacktypen. Die isolierte SDKprobe verwendet diese Konverter und den echten Verbindungseigentümer; ihre ursprüngliche Fake-Liveness wurde dadurch ersetzt. Fakeverifizierung und Fakeposting bleiben ausdrücklich synthetisch, network=false.

## Originale Daten und Abbruch

`admitOriginalGattsWrite` und die Profilvariante kopieren alle Ereignisskalare und höchstens128 geliehene Bytes vor der ersten Lifecycle-/Authoritybeobachtung. Überlange/null Werte werden nicht gelesen. Danach erfassen sie die Originalbindung am unmittelbaren Callbackrand und verwenden die vorhandene Rawaufnahme, tatsächliche Eingangsqueue und ATT-Antwortqueue. Ein erneuter Rechtecheck entscheidet weiterhin über die Ausführung.

Disconnect invalidiert den Transport vor dem externen Policyaufruf. Späte lokale Arbeit benutzt `retireOriginal` mit ihrem bereits kopierten Verbindungstoken; sie darf beim Queueleeren keinen aktuellen Token anhand von SDKnummer/Adresse erwerben. Nach einer Wiederverbindung mit identischem conn_id und identischer Adresse bleiben alte Antworten und Eingangsframes ungültig. Ein Rollenentzug innerhalb derselben Verbindung lässt den originalen ATT-Fehlerkontext bestehen; er autorisiert keinen Befehl.

Ein unerwarteter zweiter Connect bei noch aktiver Verbindung schließt die alte lokale Bindung und Rechte, verwirft den neuen Aufnahmeschritt als Conflict und erhält den Zielwert. Das ist kein Wiederverbindungsersatz bei verlorenen SDKereignissen. Ein weiterer regulärer, geordneter Lifecycle kann danach wieder eine frische geschlossene Verbindung erzeugen. Kein automatischer Peer-/GMnachweis.

Reentrant Lifecycle-/Aufnahmeaufrufe desselben serialisierten Eigentümers bleiben Busy/false. Arbeiter dürfen nur `originalLive` und den Zeitport benutzen. Objekte, Endpointkonfiguration, IdentityAccess und Queues leben in der späteren Integration dauerhaft; ihre gemeinsamen Lebenszeiten und der alleinige Produzent sind Integrationsvoraussetzungen.

## Nachgewiesene vertikale Beobachtung

14 neue Hostfunktionen; finale Suite338/338 erfolgreich ohne Fixtureexport. Actual Callbackowner→Rawgate→Commandqueue/Stream→Applicationstart erzeugt13 Framekopien. Ursprünglicher Verbindungstoken2/Rollenepoch3; nach Disconnect und Wiederverbindung gleicher SDKnummer: Token5/Rollenepoch6. Alte ATT-Kopie wird verworfen, alte lokale Retirement ignoriert, alter Input unauthorized. Das Spiel läuft weiter; aktueller v2-Status gehört zur neuen Epoch6/Lease3/nextRequest1. Der Start hatte zuvor nextRequest2.

Die Fixtures `fixtures/gatts/lifecycle-observation.json` und `lifecycle-status-v2.json` stammen aus diesem tatsächlichen Hostpfad. Die unabhängige Gegenstelle `scripts/check_gatts_lifecycle.py` verwirft23 falsche Korrelationen. Es gibt keine neuen App-/Statuswirefelder. Geschlossene Verbindung kopiert eine originale ATT-Autorisierungsfehlermeldung und führt keinen Befehl aus. Andere Fälle prüfen Endpoint-/Kontextfehler, alte Arbeit, Reentranz, Borrowedbytes vor Sperrbeobachtung, NULL/129Byte/prepared/offset, ursprünglichen Zeitwert und den ersetzten Autoritätsproduzenten.

Ein optionaler Exportlauf hatte in einem bestehenden Profilapplyfall einen Dateiöffnungsfehler; dessen Ursache ist nicht bestimmt. Der finale Lauf ohne Export hat338/338/Exit0. Neue tatsächliche Fixtures wurden erzeugt und im unabhängigen Schema-/Gegenstellenlauf geprüft. Der Fehlversuch bleibt als lokaler Log getrennt erhalten.

## SDK-Quellbefund und verbleibende Herkunftsprüfung

Gepinnte Quellen wurden lesend geprüft:

- [BTA-GATTS-Brücke](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/host/bluedroid/bta/gatt/bta_gatts_act.c): Anforderungsdaten werden mit conn_id/trans_id/Adresse an den BTCcallback übergeben; Connect/Disconnect folgen dem GATTverbindungscallback. Die Anforderungsbrücke verwendet selbst GATT_GetConnectionInfor. Sie enthält keinen eigenen Applicationtoken.
- [BTC-GATTS-Brücke](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/host/bluedroid/btc/profile/std/gatt/btc_gatts.c): gepinnte SDK-Callbacktypen und kopierte Werte bis zum unmittelbaren Appcallback; danach Freigabe. Transferfehler können Ereignisse verlieren.
- [BTC-Dispatcher](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/host/bluedroid/btc/core/btc_task.c): APIcall und APIcallback werden über den BTCthread abgearbeitet. Dies ersetzt keinen Nachweis vollständiger geordneter ursprünglicher Ereignislieferung und keinen Tokencheck beim späteren SDKsend.

Der neue Owner ist der konkrete lokale Anschluss am Callbackrand. Er setzt eine geordnete vollständige SDK-Lieferung voraus und kann alte, erst nach Wiederverbindung angelieferte rohe SDKdaten ohne ursprünglichen Token nicht selbst unterscheiden. Darum bedeutet der Hostnachweis keine bewiesene SDK-Gesamtherkunft. Insbesondere darf eine eigene Zwischenqueue keine rohen Events ohne Originalbindung speichern. Eine Bearer-/Workerintegration muss Herkunft und Ordnung vor diesem Rand und bis zur tatsächlichen SDK-Verarbeitung beweisen oder einen passenden Originaltoken im SDKpfad erhalten. Das spätere direkte send_response ist weiterhin kein ungesichert verwendeter Ausgang dieses Owners.

Nächster Schritt: konkrete SDK-Ingress-/Postingordnung mit Fehler- und Wiederverwendungsmodell schließen; gegebenenfalls muss der gepinnte SDKpfad selbst die ursprüngliche Generation mitführen. Danach echte kryptografische Peerattribution, Provisionierung und persistente ausdrückliche GMzuordnung. Produktionsmain, reale Besitzer, Flashreserve und physische RF/SMP/Zeit/Stack/Heap/Energie/Flash-/Androidabnahmen bleiben offen. Kein neuer Upload oder BLEstart.

## Finale Builds und Budget

Fünf finale Builds erfolgreich/Exit0 nach allen C++-Änderungen: Produktion55.148ByteRAM/1.229.077ByteFlash(93,8%), alte USBprobe21.480/291.881, Profilprobe107.560/406.865, SDKprobe26.472/290.229 und Hostprofilwerkzeug. Gegen fb3a272 wachsen nur die isolierte SDKprobe netto um64Byte statisches RAM und2.324Byte Flash. Profilheap bleibt57.272Byte plus Allocator/Dateisystem. Die vorhandene Arduino-UARTwarnung (return ohne Wert) erschien im Vendorcode; eigene Probequellen werden mit Werror gebaut.

Actual SDK-ELF: Verbindungseigentümer64Byte, IdentityAccess144, Kanal1264 und ATT-Queue272. Eigene Compilerframes: live64, connect144, close64, retire64, disconnect80, capture112, Original-Befehls-/Profilaufnahme jeweils240, setup752 und loop208Byte. Die bestehende darunterliegende Rawaufnahme benötigt zusätzlich ihre eigenen336Byte-Frames. Diese Zahlen sind keine Gesamtstackreserve, Zeit-/SMP-/RF-/Heapmessung. Kein neuer Queueplatz, keine Allokation oder SDKsend im Ownerpfad.

Architektur124Dateien/267Includes,75 unabhängige C++11-Header und239 Formatdateien erfolgreich. Die drei neuen Adapter sind ausdrücklich im bestehenden Formatguard enthalten. Zwölf App-v2-Paare frisch byte-/SHAgleich; keine Appänderung/-prüfung oder Fremdchatnachricht.

Lokale Tempbelege: `larp-gatts-lifecycle-host-final-20261007.log` (338/338 ohne Export), `larp-gatts-lifecycle-host-export-filefailure-20261007.log` (Dateiöffnungsfehler), `larp-gatts-lifecycle-counterpart-20261007.log`, `larp-gatts-lifecycle-builds-final-20261007.log`, Architektur-/Header-/Formatlogs und `larp-gatts-lifecycle-budget-20261007/summary.txt`. Reproduktion über die bestehenden PIO-Ziele, Guards und `scripts/check_companion_fixtures.py`.

Lesend bezogene BTAquellen in `larp-idf-446-lifecycle-source`: bta_gatts_main.c SHA256 `88A2C19AA864DBDB1C6914BBF9E80C4C7CEC813023194395C714348B48DCDAA1`, bta_gatts_act.c `E98946184F528686F8A95AF8A5E448F37C01D94B4D81B4DCBA4688CD545FE79D`, bta_sys_main.c `50F54343171CFD1126619603CA1CB049377CD54B2BB684863048BA830B0072B7`. Keine installierte SDKdatei wurde verändert. Keine neue physische/Androidabnahme, kein cppcheck oder Remote-CI.
