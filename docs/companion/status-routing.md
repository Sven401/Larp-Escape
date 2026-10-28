# Konfigurierter Status mit ursprünglicher Verbindungsbindung

Stand: 7. Oktober 2026. Tatsächlicher Application-/FreeRTOS-Hostpfad,
JSONv2 unverändert; Statusframes und physischer Transport offen.

**Korrektur vom 24.10.2026:** Die Mailbox besteht aus einem gespeicherten Wert
und Verfügbarkeitsflag. Publikation, aktuelle Auswahl und rohe Diagnosekopie
liegen mit dem AuthorityState unter derselben kurzen Spinlock-Sperre.
Es gibt keinen FreeRTOS-Peek oder verschachtelten Queue-Lock. Die datierten
alten Stack-/Objektgrößen unten beschreiben ausschließlich den damaligen
Stand und sind keine Größenmessung dieser Korrektur.
[Aktuelle Hostprüfungen](../../test/test_configured_control/README.md)
trennen Kontextregel, Wertkonsistenz und Host-Threadkonflikte von weiterhin
offener ESP32-SMP-/ISR-/Latenz-/Geräteabnahme.

## Kanonischer Produzent

GameControl erzeugt ein RoutedConfiguredStatus mit kopierter Principal/
connectionEpoch und dem bisherigen ConfiguredStatus. Die Metadaten stammen
aus seinem eigenen bereits synchronisierten AuthorityState. Sie werden beim
Lesen nicht durch den aktuellen Kanalstand ersetzt. Daher bleibt ein alter
Lease-/Requeststand beim Rechtewechsel sichtbar als alter Produzentenstand.

Mailboxgröße ist weiterhin eins; sie kopiert die komplette Envelope.
GameControl besitzt einen wiederverwendeten Veröffentlichungswert, keine
zusätzliche große Statuskopie im publish-Stack. Nur sole application owner
publiziert; Adapter darf historische Bytes nicht mit neuer Epoch versehen.

## Aktueller Empfänger

FreeRtosGameControlChannel::readCurrentStatus prüft die verfügbare
Statuskopie unter der kurzen Authoritysperre ohne RTOS-Aufruf und akzeptiert nur die ursprünglich
gebundene aktuelle Principal+Epoch+GM. False erhält sein Ziel. Reconnect,
Entzug, Observer oder Suspend sperren alten Stand sofort; nach neuer
GameControlservice-Publikation wird der neue Lease-/Requeststand lesbar.

readStatus bleibt ausdrücklich Rohdiagnostik. Es kann ältere Daten zeigen und
wählt keinen aktuellen verifizierten Netzwerkempfänger. Status JSON enthält
weiterhin keine Principal-/Epochfelder; nach erfolgreicher Auswahl wird
status mit unverändertem encodeConfiguredStatus serialisiert.

Auswahl ist ein Beobachtungspunkt. Spätere Statusframe-/SDKqueues/Bearer müssen
unmittelbar vor ihren Kopier-/Sendebeobachtungen nochmals dieselbe Verbindung
prüfen. Kein relabeln recycelter Handles, kein rückwirkend ungesendet machen
bereits kopierter Werte. Keine automatische Spielunterbrechung bei Entzug.

Der erste aktuelle Leseport unterstützt GM. Allgemeine Observer-/öffentliche
Statusrechte sind noch kein Produktentscheid und werden dadurch nicht
implizit eingeführt. Diagnose ist kein transportseitiger Berechtigungsweg.

## Vertikaler Nachweis

Vier neue native Funktionen, insgesamt87/87 erfolgreich/Exit0:
alter Epoch1-Lease1 nach echter Authority3 bis neuer Producerpublikation
abgewiesen, dann Epoch3-Lease2; verzögerte alte Publikation/wrong Principal,
False-Zielerhalt; Entzug unmittelbar vor Auswahl, Observer/Suspend und
weiterlaufendes tatsächliches Spiel; Entzug direkt nach Producerobservation
behält alten Tag und wird bei aktueller Auswahl verworfen.

Die bestehenden tatsächlich erzeugten v2-Statusfixtures verwenden nun den
aktuellen Auswahlport und bleiben bytegleich. Encoder/Gegenstelle/Schema
erhalten genau denselben JSONvertrag. Keine Rollen/Identität aus ClientJSON.

Vier finale Builds SUCCESS/Exit0: Produktion55.148RAM/1.228.473Flash,
alteUSBprobe21.480/291.729, Profilprobe100.448/391.197 plus57.272Profilheap/
Allocator/FS. Tatsächliches Probe-ELF GameControl1336/Companionchannel1260Byte.
Xtensatypen ConfiguredStatus516/RoutedStatus524Byte. Eigene publish-Compiler-
frames480 plus32Byte-Wrapper; service208. Tatsächlich SDKkompiliertes
Adapterobjekt readStatus/readCurrentStatus jeweils560Byte entry-Frame,
zusätzlich Callerkopie/Queue-/Sperr-/Encoderkette; keine gemessene Gesamtreserve.
Aktueller Auswahlport ist im SDKobjekt gebaut, von der Probenloop noch nicht
benutzt oder physisch ausgeführt. Publisher-/Mailboxmetadaten dort gebaut.

Architektur91/210, Header58, Format155 erfolgreich. Achtv2+Katalogschema,
Generator und unabhängige Reply-/Receiptgegenstelle gültig. Tatsächliche CLI
nach finalem Build weiterhin13Befehlsframes/1732Katalogbytes bytegleich.
Anfrage-/Lease-/Samplecountermax und alle Statusalter-/SMP-Randfälle sind
dadurch nicht vollständig abgenommen.
Kein physischer Transport, RF-/SMP-/Android-/Stack-/Zeitnachweis oder Upload.
Produktionsmain bleibt der bisherige Einstieg. Nächster Schritt:
begrenztes Statusframing mit tatsächlicher Mailbox/Encoder-/Toolgegenstelle,
danach verifizierte Identität/GM-Zuordnung/Pairing/SDK und Profiltransfer.


## Folgeschnitt vom 7. Oktober 2026

[Statusframing v1](status-framing-v1.md) verbindet diesen tatsächlichen
aktuellen Auswahlport mit festem Encoder/Sender, originaler Bindung und
unabhängiger Gegenstelle. Die Profilprobe liest jetzt diesen Port über
globalen Scratch, hält einen unveränderlichen Stand und gibt Spielantworten
im gemeinsamen Versandsschritt Vorrang.99Hostfunktionen/vierBuilds
erfolgreich; physischer Bearer und Appintegration weiterhin offen.
