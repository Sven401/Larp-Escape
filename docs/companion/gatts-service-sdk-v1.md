# Einmalige GATTS-Diensteinrichtung mit bestätigten Originalhandles

Stand: 7. Oktober 2026, Firmwarebasis `4b2c228`. Kleiner Firmware-/Toolpfad nach AGENTS.md; Hostmodell und tatsächlicher ESP32-Compile-/Linkanschluss. Kein ausgeführter Bluetoothstart oder Gerätebeleg.

## Verbindlicher schmaler Dienstvertrag

`GattsServiceDefinition.h` und `fixtures/gatts/service-definition.json` definieren einmalig:

| Element | Kanonischer Wert |
|---|---|
| Eigene SDK-Appregistrierung | 0x4c45 / 19525; kein Peer-/GMnachweis |
| Primärer Dienst, Instanz0 | `407e6a1d-1cc2-43fd-856b-4aff85a20001` |
| Befehlseingang | `407e6a1d-1cc2-43fd-856b-4aff85a20002` |
| Profileingang | `407e6a1d-1cc2-43fd-856b-4aff85a20003` |
| Reservierte Handles | 5: Dienst + je Deklaration/Wert für zwei Eingänge |
| Eingangsgrenze | 128Byte; normale Writeeigenschaft, Anwendung antwortet |
| SDK-Schreibpermission | `ESP_GATT_PERM_WRITE_ENC_MITM`; ersetzt keine eigene Peerverifizierung/GMzuordnung |

UUIDs werden für ESP/BLE vollständig in little-endian128Bit-Reihenfolge kopiert. Tatsächliche numerische Handles kommen ausschließlich aus bestätigten SDKereignissen;32/34/36 sind Hostbeispielwerte, keine auf Funk fest verdrahteten Handles. Befehl-/Profilframing bleiben die bestehenden begrenzten Verträge. Noch keine Status-/Spiel-/Profilantwort-Characteristic, CCC oder Readdefinition. Appcode wurde nicht geändert; die zwölf vorhandenen v2-Vertragskopien bleiben getrennt.

## Geordnete Einrichtung

`Esp32GattsService` hat genau einen Callbackproduzenten und einen getrennten Worker. `begin()` auf dem Worker muss vor SDKinitialisierung erfolgen. Der öffentliche SDKcallback wird über `fromSdkGattsService` auf die ausgewählten Reg/Create/AddChar/Startskalare und kopierte16Byte-UUIDs reduziert; kein geliehener Zeiger gelangt in die Queue. Vier statische kopierte Plätze, zeroWait. Der einmal erfasste Producer und der Worker werden nie neu gebunden. Ein verlorener/übervoller/aus falscherTask kopierter ausgewählter Callback sperrt den Versuch. Bereitschaft prüft den Fehlerflag sofort, bevor der Worker den Fehler verarbeitet.

Der frühe gepinnte private Registrierungswrapper veröffentlicht zusätzlich einen monotonen, nicht wiederverwendbaren Registrierungswert mit Interface. Derselbe Wert wird bei der öffentlichen Ereigniskopie eingefroren. Er bezeichnet nur diesen eigenen Quellanschluss/SDKlauf, keine kryptografische Identität. Jede spätere Worker-/BTUprüfung vergleicht ihn mit der ursprünglichen unveränderlichen Registrierung; ein anderes Objekt mit derselben Interfacezahl kann ihn nicht übernehmen. SDK-/Taskinstanzen dürfen in diesem Schnitt nicht gestoppt, ersetzt oder wiederverwendet werden.

Workerphasen: Regbestätigung → echte `esp_ble_gatts_create_service`-Anforderung → Createbestätigung → echte `esp_ble_gatts_add_char` für Befehl → AddCharbestätigung → AddChar für Profil → Profilbestätigung → original gebundene Handleinstallation auf BTU → bestätigte Installation → echte `esp_ble_gatts_start_service`-Anforderung → passende erfolgreiche Startbestätigung → Ready.

Höchstens ein kopiertes Ereignis und ein SDK-/Konfigurationspost pro Workerdurchlauf. Alle UUID-/Instanz-/Primary-/Interface-/Service-/Handle-/Reihenfolge-/Erfolgsskalare werden geprüft. Identische unmittelbar wiederholte Bestätigungen posten nichts erneut und verlängern keine Frist. Anders geordnete oder ältere nicht unmittelbar identische relevante Ereignisse sperren den Versuch; keine heuristische Wiederzuordnung. Die SDK-APIrückgabe bestätigt nur deren lokale Aufnahme. Startposting allein ergibt kein Ready.

Der Quellenport besitzt genau einen festen Konfigurationsplatz. Er kopiert Registrierungswert, Interface, ursprüngliche Callbackzeit, Service-/Befehl-/Profilhandles und den dauerhaften Antwortqueuebezug vor externen Beobachtungen. Posting auf tatsächliche BTUqueue0/timeout0 über den bestehenden gepinnten OSIport. Keine eigene Heapallokation. Der Quellenakteur prüft nochmals Task/SDKinstanz/Originalwert/Alter und installiert den vorhandenen64Byte-Verbindungseigentümer per placement-new genau einmal, bevor irgendein Verbindungscallback eingetroffen ist. Bereits zu früh eingetroffene Verbindung, Konflikt, falscheTask, abgelaufener oder abgewiesener Post erlauben keinen Rebind/Retry.

Ein voller/verlorener/später Einrichtungspfad bleibt gesperrt. Konfigurationsplatz/Callbackobjekte dürfen auch bei gescheitertem Worker nicht gelöscht werden, solange ein SDKpost möglich ist. Ein bereits geposteter, noch gültiger BTUjob kann nach Workerfehler seine lokale einmalige Installation abschließen; der fehlgeschlagene Worker startet/advertisiert dann nichts und meldet kein Ready. Vollständige SDKservicebereinigung und produktiver Wiederanlauf bleiben ein eigener späterer Schnitt.

## Eigene Fristen und Lebensdauer

- Gesamte Einrichtung: Alter seit erstem `begin()` kleiner30.000ms, bis einschließlich Startbestätigung. Duplikate aktualisieren diesen Beginn nicht.
- BTU-Handlejob: Alter seit ursprünglichem Profileingangscallback kleiner5.000ms, sowohl beim Post als auch beim endgültigen Konsum. Verifizierte Installation bleibt anschließend gültig; keine zeitabhängige Reinstallation.
- ATT-Aufnahmeantwort bleibt separat kleiner1.000ms; Befehls-/Statusfristen werden nicht für Einrichtung übernommen.

30.000/5.000ms sind lokale Startpolicies ohne gemessene SDK-/Funkgarantie. SDKcreate/add/start dürfen in ihrem eigenen internen Posting allokieren oder warten; deshalb erfolgt kein solcher Aufruf im kurzen Callback oder unter eigener Sperre. Der Worker prüft Fristen an seinen Aufrufgrenzen; dieser Schnitt begrenzt keine blockierende SDKfunktion in Echtzeit. Unsigned32Bit-Differenzen und Clockwrap sind im Host geprüft.

Dienst, Quellenport, Policy, Clock und Queues müssen über alle SDKcallbacks und Posts leben. Genau ein Versuch pro Prozesslebensdauer; kein Deregistrieren/SDKstop/retry. Kein Advertising oder SDKinit in dieser Probe. Produktive Einrichtung muss Advertising und künftige Authentisierung an aktuelles Ready binden; früher Connect erteilt keine GMrechte. Ready selbst erteilt keine Rechte. Echte Kryptografie und explizite persistente Spielleitungszuordnung fehlen weiterhin.

## Tatsächlich geprüfter Firmware-/Toolpfad

16 neue Hostfunktionen; finale Suite365/365 ohne Export. SDKdriverbody/Callbackkonverter/Worker/Quelleninstallation/Queues laufen gegen deterministische öffentlicheSDK-/RTOS-Cfunktionen. Echte Service-/Characteristic-APIparametrierung wird im Modell beobachtet. Installation wird im Quellakteur ausgeführt und muss vor Startposting bestätigt sein. Erst die richtige tatsächliche Startbestätigung setzt Ready. Danach öffnet das Modell die private Verbindung257, verwendet weiterhin sichtbar Fakeverifizierung plus explizite GMzuordnung und führt13 Originalframes über die erzeugten Befehlhandles in den tatsächlichen Anwendungskern; SpielRunning.

`service-setup-observation.json`: vier lokale SDKanforderungen,14 Quellposts (eine Einrichtung +13 spätere ATTantworten), Beispielhandles32/34/36,13 kopierte Frames, Ready/Running; RFinitialized/deviceMeasured jeweilsfalse. Die Beobachtung wurde durch den tatsächlichen Hostlauf erzeugt; final ohne erneuten Export verglichen. Unabhängige Pythongegenstelle prüft kanonische UUIDbyteordnung gegen C++, Grenzen und vorhandene Frameanzahl;58 falsche Definitionen/Beobachtungen werden abgewiesen. Beobachtung ist kein Funk-/Peer-/Kryptografie-/physischer Flashbeleg.

Weitere Fälle: lokale SDKpostfehler bei allen vier APIstufen, falsche UUID/Handle/Instanz/Primary/UUIDlänge, fehlende Originalregistrierung, falsches Interface/Reihenfolge, vier volle Ereignisplätze/falscherProducer, Workerreentranz/falscherWorker, unveränderte Eingangskopien bei Mutation, verlorene/fehlerhafte/falsche/zu frühe/zu späte Startbestätigung,30Sekundengrenze und Clockwrap,5Sekunden-Installationsgrenze, Connect vor Installation, Quelltask-/SDKwechsel, fixe einmalige Konfiguration und Originalwert eines anderen Ports.

Die anfänglichen eigenen Testhilfenfehler (doppelte ESP_OKdefinition und Includeordnung) sowie zu frühe Zugangserwartung/absolut statt relativ gesetzter Fristtest wurden korrigiert. Fehlversuche bleiben im Tempprüflog; maßgeblich ist der erfolgreiche finale Lauf. Kein allgemeiner Fixtureexport.

## SDKbasis und Grenze

Tatsächliche öffentliche Header und `libbt.a` des gepinnten Arduino2.0.14/ESP-IDF4.4.6 werden im isolierten ESP32ziel übersetzt/gelinkt. Primärquellen: [ESP-GATTS-API](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/api/include/api/esp_gatts_api.h), [GATTdefinitionen](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/api/include/api/esp_gatt_defs.h), [APIimplementierung](https://github.com/espressif/esp-idf/blob/v4.4.6/components/bt/host/bluedroid/api/esp_gatts_api.c). Öffentlicher Hoststub modelliert nur ausgewählte API-/Ereignisfelder, er beweist keine öffentliche SDK-ABIgröße. Gepinnter privater Quellenport bleibt beim bestehenden separaten ABIvergleich und Binaryguard.

SDKprobe prüft nur den uninitialisierten Ablehnungszweig, hat keinen registrierten öffentlichen Livecallback und startet keinen SDKdienst. Tatsächliche Connect-/Create-/Startcallbacks, Advertising/SMP, Runtimefristen/Heap/Stack/Funkdurchsatz/Energie/Flashdurabilität und Androidanschluss bleiben offene Abnahmen. Produktion verwendet weiter den bestehenden Controller. Nächster Schnitt: echte original gebundene kryptografische Peerprüfung und ausdrücklich persistente GMzuordnung, anschließend ursprüngliche Funkausgabe und produktive Eigentümer/Startinstallation.


## Finale Builds und tatsächlicher Speicher-/Linkbeleg

Alle fünf Ziele final erfolgreich/Exit0, nach allen C++-Änderungen: Produktion55.148ByteRAM/1.229.077ByteFlash(93,8%), alteUSBprobe21.480/291.881, Profilprobe107.560/406.865, SDKprobe42.416/1.020.869(77,9%) und Hostprofilwerkzeug. Nur die isolierte SDKprobe wächst gegenüber4b2c228: netto432ByteRAM/4.448ByteFlash. Getrennte SDK-/Produktionsbuildwerte ergeben keinen kombinierten Reservebeleg. Profilstartupheap weiter57.272Byte plus Allocator/Dateisystem, ohne Gerätemessung. Bestehende Vendorwarnungen im Log.

ActualfinalesELF: Dienstowner388Byte inklusive seiner vier44Byte-Ereignisplätze und statischer Queueverwaltung; Quellenport392Byte (+36) inklusive unverändertem64Byte-Verbindungseigentümer und vier52Byte-Antwortplätzen. Zusätzliche Registrierungsglobals4Byte-Zähler/8Byte-Sperre, Driver1Byte; bestehende ATTqueue272Byte. SDK-/Allocator-/Workerstackkosten sind darin nicht enthalten.

Eigene Compilerframes: Ereigniskopie128, Worker144, Ready64, Begin/Phasenpublikation/Snapshot32, Current48; Quellenjobpost/-status/-konsum64, Register-/Quelllesung32. SDKdriverframes Create112, AddCommand/AddProfile96 plus gemeinsamer Addhelper80, Start32. Bestehende Quellrequest224, Verbindung64, Antwortpost/-konsum80 bleiben. Darunter kommen Decoder/Policy/Queue/SDKketten hinzu; kein Gesamtstack-/SMP-/Zeitbeleg.

Finales ELF zeigt echte direkte Treiberaufrufe von `esp_ble_gatts_create_service`, `esp_ble_gatts_add_char` und `esp_ble_gatts_start_service`, sowie tatsächliche BTUkonfigurationsroutine mit erneuter Original-/Task-/Altersprüfung und einmaligem `configureEndpointsNow`. Privater Binaryguard akzeptiert den unveränderten Pin. Dies ist Compile-/Linknachweis; die SDKprobe führt diese Dienst-APIs nicht im initialisierten SDK aus.

Architektur124Dateien/267Includes,75 reine unabhängige C++11-Header und254Formatdateien erfolgreich; diff-check grün. Zwölf App-v2-Paare frisch byte-/SHAgleich, Appprüfung/-änderung/-Nachricht nicht Bestandteil. Tempbelege: `larp-gatts-service-host-final-20261007.log`, `larp-gatts-service-builds-final-20261007.log`, `larp-gatts-service-counterpart-20261007.log`, Architektur-/Header-/Formatlogs und `larp-gatts-service-budget-20261007/`. Frühere eigene Fehlversuche liegen getrennt in first/second/third/fourth-Hostlogs. Keine COMöffnung/Upload, SDKinit/Advertising, Formatierung oder Remoteoperation.
