# Begrenzte ATT-Empfangsantworten über FreeRTOS

Stand: 7. Oktober 2026, nach Firmware6748fb4. Lokal implementiert und im Host geprüft. SDK-Verbindungszuordnung, tatsächlicher SDK-Sender und Funkabnahme sind offen.

## Grund für diesen Schnitt

Der Raw-Zugang besitzt bereits einen Vertrag für eine begrenzte Antwortkopie. Ein direkter Aufruf des gepinnten ESP-IDF4.4.6-Senders erfüllt diesen Vertrag nicht: `esp_ble_gatts_send_response` übergibt Arbeit an BTC, dessen Übergabe Speicher anfordert und mit maximaler Warteoption postet. OSI verwendet dafür eine begrenzte FreeRTOS-Queue und `portMAX_DELAY`. Queue0 mit konfigurierter Länge0 wird durch die Standardkapazität100 ersetzt. Dies ist eine Quellcodeprüfung; das Verhalten des eingebetteten Binär-SDKs wurde nicht gemessen.

Primärquellen, Tagv4.4.6:

- [GATTS-API, Zeilen289–312](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/host/bluedroid/api/esp_gatts_api.c)
- [BTC-Kopie und Posting](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/common/btc/core/btc_task.c)
- [OSI-Queue, Standardkapazität und Warteoption](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/common/osi/thread.c)
- [OSI-Vertrag](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/common/osi/include/osi/thread.h)

Ein SDK-Post muss deshalb später in einem getrennten Arbeitskontext erfolgen. Der Spielbesitzer und der Raw-Callback bleiben frei von diesem Aufruf. Dieser Schnitt stellt die Kopie und einen einzeln aufrufbaren Dispatch bereit; er erzeugt noch keinen SDK-Worker-Task.

## Vertrag und Ablauf

`RoutedWriteResponse` erhält intern `receivedAt`, die ursprüngliche Callbackzeit. Wirewerte, Appschema und Befehlsfrist bleiben unverändert.

`FreeRtosGattsResponseQueue` implementiert den vorhandenen `IWriteResponseBearer`: vier statische Plätze, gesamte skalare Antwort vor jeder externen Beobachtung eingefroren, keine geliehenen Pointer. `begin` läuft einmal vor den Arbeitern; erneutes `begin` leert keine Nachrichten. Das Objekt lebt für die gesamte Anwendung.

| Grenze | Regel |
|---|---|
| Callbackkopie | höchstens ein `xQueueSendToBack` mit Wartewert0 |
| Antwort | gültiger fester ATT-Code, Originaltoken, Interface, Peeradresse und ausgewähltes Attribut |
| Alter | bei Kopie, Auswahl und Dispatch kleiner1000ms seit `receivedAt` |
| Queue | vier Werte; Voll führt zu `responseCopied=false` |
| Auswahl | höchstens vier Receives mit Wartewert0; höchstens eine frische Antwort; false erhält Zielwert |
| Dispatch | höchstens eine Auswahl und ein Submitteraufruf; Reentranz wird als Busy abgewehrt |
| Postfehler | Antwort verbraucht, kein Wiederholen; bereits kopierter Eingang bleibt erhalten |

Die1000ms sind eine lokale Auswahlpolicy. Sie sind weder gemessene Funklatenz noch ATT-Timeout und begrenzen die Dauer eines späteren SDK-Aufrufs nicht. Zukunftszeit wird durch die modulare Altersprüfung verworfen; der uint32-Umlauf ist im Host geprüft. Kein Queue- oder Workerzugriff erneuert die Zeit.

`IGattsResponseContext` muss die ursprüngliche SDK-Verbindung synchronisiert und ohne Warten beobachten. `IGattsResponseSubmitter` muss Originalbindung und Alter am tatsächlichen Postingrand erneut prüfen. Eine Verbindungsnummer allein überträgt unseren Generationstoken nicht an das SDK. Serialisierung bis zur SDK-Verarbeitung bleibt eine Voraussetzung des noch fehlenden echten Ports; der Fake belegt diese Voraussetzung nicht.

## Rechte und Aussage einer Antwort

ATT bestätigt hier nur die erfolgte Eingangskopie beziehungsweise deren Fehler. Die Berechtigung wird am Befehls-/Profilpfad geprüft. Der Antwortport verlangt eine lebende ursprüngliche SDK-Verbindung, kein Spielleitungsrecht für eine Fehlerantwort. Nach Rechteentzug kann deshalb eine zuvor erfolgte Kopie noch bestätigt werden; die Anwendung verwirft den alten Befehl weiterhin. Die eingefrorene ursprüngliche Authority wird nie zur aktuellen umbeschriftet.

Eine falsche Attributanfrage behält ihr angefragtes Attribut im Fehlerwert. Eine wiederverwendete Verbindung, ein anderes Interface, eine andere Peeradresse oder ein neu ausgewähltes Attribut schließen die alte Antwort. Peeradresse und lokaler Token sind hier Routingmerkmale; die tatsächliche kryptografische Peerzuordnung bleibt offen.

## Vertikale Belege

- Tatsächlicher Raw-Zugang → kopierte Befehlsqueue → neue Antwortqueue → einzeln aufgerufener Fakepost:13 Frames/13 Antworten, anschließend tatsächlicher Start durch den Anwendungsbesitzer.
- Tatsächlicher Raw-Profilzugang → Profilqueue/temporäre Quelle/private Importfreigabe/Apply → bestätigtes Hostjournal:172 Aufnahmeantworten; Entwurf, Übernahmeannahme und Saved bleiben getrennt.
- 18 neue Hostfunktionen prüfen unter anderem FIFO/Voll, fehlendes begin, sämtliche ATT-Codes, Zukunft/Umlauf/Frist, Einfrieren vor Beobachtung, Abbruch zwischen Beobachtungen, Handle-Wiederverwendung, Entzug, Reentranz, SDK-Postfehler und Eingangsbestand.
- Finale Suite:311/311 erfolgreich ohne Fixtureexport. `scripts/check_gatts_response_queue.py` prüft die tatsächliche Hostbeobachtung und verwirft18 veränderte Beobachtungen. Bestehende Wiregegenstellen bleiben gültig.
- `esp32-gatts-abi-probe` verwendet die tatsächliche statische Queue und denselben Dispatcher mit einem ausdrücklichen Fake-Kontext/Fakepost. Kein BLEstart, SDK-Post oder physischer Peerempfang.

Die Fixture `fixtures/gatts/response-queue-observation.json` beschreibt einen Hostlauf. `callbackPosts=0` wird bei jeder Raw-Aufnahme geprüft; `runningBeforeOwner=false` bleibt bis zur tatsächlichen Besitzerverarbeitung bestehen. Die Gegenstelle liest den vorhandenen13-Frame-Befehl separat. Sie macht aus dem JSON keine Funkmessung.

## Nächster konkreter Schritt

Den ursprünglichen SDK-Verbindungslebenszyklus und einen geordneten kopierten Ereignispfad schließen. Besonders Authcomplete liefert nicht alle Verbindungsmerkmale eines GATTSconnect. Alte Ereignisse dürfen nicht durch eine spätere Handle-/Adresssuche einer neuen Verbindung zugeordnet werden. Erst danach tatsächlichen Verifizierer, ausdrücklich bestätigte Spielleitungszuordnung und SDK-Sender anbinden. Hardware-, Zeit-, SMP-, Heap-/Stack- und Energieabnahmen bleiben gesondert offen.

## Finaler Build- und Speicherstand

Alle fünf Ziele erfolgreich: Produktion55148RAM/1229077Flash, alteUSBprobe21480/291881, Profilprobe107560/406613, isolierteSDKprobe26408/287313. Die Profilprobe benötigt zusätzlich57272Byte Startup-Profilheap plus Allocator/Dateisystem. Nur die SDKprobe wächst hier netto um280Byte statisches RAM und964Byte Flash. Produktionsmain verwendet weiterhin den bisherigen Controller.

Im tatsächlichen SDK-ELF: Antwortqueue272Byte einschließlich vier44Byte-Werten und statischer FreeRTOS-Verwaltung, Dispatcher12Byte, Fake-Kontext4Byte, Fake-Submitter52Byte. Einzelne Objektgrößen sind keine Nettosummen des Gesamt-ELFs. Neue Queue/Dispatcher verwenden keine eigene dynamische Allokation.

Eigene Compilerframes aus den tatsächlichen Buildobjekten: begin32, current32, copy80, take80, service80, Fakepost32, Raw-Befehlsaufnahme336/Raw-Profilaufnahme336, SDKsetup512 und SDKloop208Byte. Aufgerufene virtuelle Funktionen, RTOS und spätere SDK-Funktionen sind zusätzlich zu berücksichtigen. Dies ist keine Messung der Gesamtstackreserve oder Laufzeit.

Architektur124Dateien/267Includes,75unabhängige C++11Header und234Formatdateien erfolgreich geprüft. Zwölf bestehende App-v2-Paare frisch byte-/SHAgleich; Appcode und Appprüfungen gehören weiterhin der App-Sitzung. Kein Upload, physischer Flash-/Funklauf oder Remoteoperation.
