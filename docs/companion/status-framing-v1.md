# Begrenzte Statusframes v1

**Stand:** 7. Oktober 2026. **Status:** tatsächliche aktuelle Mailbox, Encoder,
Sender, gemeinsame Versandpriorität und unabhängige Toolgegenstelle.
Physischer Bearer, Androidempfang und Echtzeitabnahme offen.

## Wireformat und unveränderlicher Stand

Der achtbytegroße Header folgt den Befehls-/Antwortframes: Version1,
Typ4, message/total/offset als uint16 little-endian. total ist1..6143.
Der tatsächliche Statusencoder besitzt6144Byte inklusive NUL; NUL wird
nicht übertragen. Ein Wert hat9..128Byte, davon1..120Payloadbytes.
Command-/Replydecoder weisen Typ4 ab; Statusdecoder weist Typ1/2/3 ab.
Fehler erhält das Decoderziel. Payload ist bis zur Änderung des Eingabepuffers
geborgt. Header- und Grenzprüfungen sind dieselbe innere Codecimplementierung.

Ein alleiniger Transporteigentümer liest die tatsächliche aktuelle Mailbox
über readCurrentStatus in seinen festen RoutedConfiguredStatus-Scratch.
StatusFrameSender prüft die originale Producer-Principal/ConnectionEpoch
gegen aktuellen GM-Zugang und friert encodeConfiguredStatus in einem festen
6144Byte-Puffer ein. Während der Übertragung wird dieser JSONstand weder mit
neueren Mailboxwerten noch einem neuen Lease/Profil/Spielzustand vermischt.
Weitere load-Aufrufe melden Busy und erhalten den aktiven Stand. Der Sender
besitzt keine Profile und allokiert keinen Heap. Appstatus muss vollständig
zusammengefügt und anhand Boot/sample/Lease/Revisionsdaten eingeordnet werden;
ein fertig übertragener Snapshot ist keine Zusage eines aktuellen Moments.

Der gemeinsame Bearervertrag IReplyFrameBearer/RoutedReplyFrame wird auch für
Typ4 verwendet. Originale Principal/Epoch bleiben nur vertrauenswürdige lokale
Metadaten, niemals JSON-/Headerrechte. Jeder service-Aufruf prüft erneut den
Zugang; der Bearer muss an seinem eigenen synchronisierten Kopier-/Sendepunkt
dieselbe aktuelle verifizierte Verbindung prüfen. Weitere SDKqueues dürfen
alte Fragmente nicht auf recycelte Handles oder neue Epochen umetikettieren.
Copied/Completed bedeutet nur Bearerkopie, keine Telefonzustellung,
GATT-Indication-Bestätigung oder Appverarbeitung.

## Zeit-, Speicher- und Fortschrittsgrenzen

* Vor load und vor der ersten erfolgreichen Kopie muss der produzierte Stand
  jünger als1000ms sein. Die Zeit ist dieselbe vertrauenswürdige uint32-Uhr wie
  sampledMilliseconds; vorauseilende Zeit erscheint alt und wird abgewiesen.
  Die Grenze ist eine Politik von vier250ms-Publikationsperioden, keine
  gemessene Echtzeitgarantie. Busy/Antwortvorrang darf noch ungesendete alte
  Snapshots nicht später beginnen lassen.
* Nach der ersten Kopie bleibt der ursprüngliche Snapshot unverändert bis
  Abschluss oder spätestens30000ms seit load. Es gibt keine Verlängerung
  durch Busy, andere Wertgrößen, Antwortvorrang oder neue Mailboxwerte.
  Die30000ms sind ein eigenständiges festes Statusbudget; die2000ms für
  Befehle/Spielantworten werden nicht als Statusdurchsatzannahme übernommen.
* service sendet höchstens einen begrenzten Frame; Busy erhält exakt Offset,
  Daten und Startfrist. Unavailable, Entzug oder Ablauf verwerfen den Rest.
  cancel verwirft ausdrücklich den aktiven Stand. Bereits kopierte Werte
  lassen sich nicht rückwirkend ungesendet machen. Spielbetrieb läuft weiter.
* lastSample wird nur nach erfolgreichem Encoder/load gespeichert. Derselbe
  Boot/sample wird nach Abschluss oder Abbruch nicht erneut encodiert;
  AlreadySampled. Kleinere Samples sind StaleStatus, Bootwechsel innerhalb
  derselben Authorityepoch ist InvalidStatus. Ein echter Neustart erzeugt
  einen neuen Sender. Nach Abbruch kann der nächste neue Producerstand
  übernommen werden, nicht derselbe halbe Stand unmarkiert wiederholt werden.
* message verbraucht pro erfolgreich geladenem Snapshot eine Kennung1..65535,
  auch bei Abbruch.65536 wird MessageExhausted, niemals Null/Reuse. Nur eine
  tatsächlich neue gültige Zugangsepoch setzt Kennung und Samplehistorie
  zurück. Message ist Assemblierung, kein Sicherheits-/Spielrequestnachweis.

Bereits gemessene native Fragmentzahlen: Ready1716JSONbytes braucht143Frames
bei20Byte-Werten oder15bei128Byte. Running1683 braucht15bei128Byte.
Die gemeinsame maximale Codecgröße6143Nutzbytes bedeutet rechnerisch512/52
Frames bei20/128Byte. Diese Zahlen zählen Kopien bzw. Bytes im Hostmodell,
keinen Funkdurchsatz, keine MTUverhandlung und keine erfüllte30000ms-Funkfrist.

## Vorrang der tatsächlichen Spielantwort

serviceConfiguredFrames verwendet den tatsächlichen ReplyFrameSender-Zustand
vor dem Versandsschritt. Solange eine Spielantwort wartet, darf Status in diesem
ganzen Schritt nur Deferred/Abbruch liefern. Das gilt auch für den Schritt,
in dem die Spielantwort Completed meldet. Damit höchstens eine Spielantwort-
oder Statuskopie pro Schritt. Beide Sender prüfen Frist/Zugang weiterhin;
Deferral erneuert keine Frist. Caller nimmt aktuelle Replyqueuewerte zuerst
auf. Der Spielkern wird dadurch nicht zusätzlich getickt.

Die synthetische ESP32-Profilprobe benutzt genau diese Funktion, denselben
Statussender und die echte aktuelle FreeRTOS-Mailbox. Ihr fester kopierender
Bearer prüft aktuelle Principal/Epoch/GM, arbeitet ohne GATT/Funk.
Aufnahmeantworten im synthetischen Eingangsgenerator sind ein eigener Weg;
die Ein-Kopie-Grenze ist ausdrücklich der Reply-/Statusversandsschritt.

## Reproduzierbarer vertikaler Nachweis

Gespeicherte Application/echter GameControl → producergebundene FreeRTOS-
Mailbox → readCurrentStatus → echter encodeConfiguredStatus/StatusFrameSender
→ geprüfter kopierender Hostbearer → bytegenaue JSON-/Framefixtures →
unabhängige lesende Pythongegenstelle.

[Readyframes](fixtures/status-ready-frames-v1.txt) enthalten exakt
[Ready JSON](fixtures/v2/status-ready.json),143 Werte mit maximal20Byte.
[Laufstatusframes](fixtures/status-running128-frames-v1.txt) enthalten exakt
[Laufstatus JSON](fixtures/v2/status-running.json),15 Werte mit maximal128Byte.
Nur Zeilenenden der Golden-Texte werden normalisiert, keine JSON-/Framebytes.

scripts/check_companion_fixtures.py rekonstruiert unabhängig Version/Typ,
positive Kennung, Grenzen, gleiche Gesamtlänge/Kennung, lückenlose Offsets und
vollständiges Ende.34 fehlerhafte Varianten über beide Fixtures abgewiesen:
leere/unvollständige Folgen, Duplicate, Vertauschung, Extraende, Hexfehler,
Überlänge, falsche Typen/Version/IDs/Längen/Offsets, gemischte Nachricht oder
Gesamtlänge und6144-Byte-Deklaration.

Zwölf neue native Funktionen prüfen Codecgrenzen, tatsächliche Ready-/Start-
Mailbox und Golden, Ready auch mit128Byte-Werten, unveränderter alter Stand
nach echtem Start, tatsächliche Antwortpriorität einschließlich Completion,
Busy/erste-Kopie-Frische, absolute Frist, Uhrumlauf, ungültige Statuswerte,
Wertgröße9/8/129, cancel, Sample-/Boothistorie, Reconnect/neuer Lease,
Bearerentzug vor/nach Kopie/Observer/Unavailable sowie alle65535 geladenen
Kennungen mit Abbruch, Erschöpfung und neuer Epoch. Synthetische Zeit bleibt
im Zählerlauf unverändert; kein Geschwindigkeits-/Funklastbeleg.

Kein neuer Upload, RF-/SMP-/Androidlauf, physischer Stack-/Zeit-/Energienachweis
oder Produktionsanschluss. Identität/ausdrückliche GM-Zuordnung, SDKpfad,
Profiltransfer, echte Besitzer und Geräteabnahme bleiben die nächsten Schnitte.

## Abschlussprüfstand dieses Schnitts

99/99 native Funktionen erfolgreich/Exit0, einschließlich zwölf neuer
Statusfunktionen. Erstlauf mit gezieltem Fixtureexport sowie Abschlusslauf
ohne Export, keine alten Vertragsbyteänderungen. Lokale Beleglogs:
%TEMP%/larp-status-frames-host-export-20261007.log und
%TEMP%/larp-status-frames-host-final-20261007.log.
Unabhängige Pythongegenstelle3399Statusbytes/158Frames/34Fehlerfolgen;
bestehende Reply206/18/11 und Receipt13/15 weiter gültig.
Achtv2+Katalogschema/Generator und tatsächliche CLI13Frames/1732Bytes gültig.
Alle zwölf fremden App-v2-Snapshotpaare frisch bytegleich; keine Appprüfung.

Vier finale Builds SUCCESS/Exit0,
%TEMP%/larp-status-frames-builds-final-20261007.log:
Produktion55.148RAM/1.228.529Flash(93,7%),alteUSBprobe21.480/291.785,
Profilprobe107.168/393.225Byte plus57.272Profilheap/Allocator/FS.
Produktionsmain weiterhin bisheriger Einstieg. Die beiden alten Builds
haben jeweils56Flashbytes mehr als9dacc8e; kein Beleg des neuen Anschlusses.

Tatsächliches finales Probe-ELF: StatusFrameSender6196Byte,
globaler RoutedStatus-Scratch524Byte, zusammen6720Byte zusätzlich zum
vorigen Proben-RAM. Replysender548/Bearer148Byte unverändert.
Xtensa FrameStep2/RoutedValue140Byte. Eigene -Os/-fstack-usage-Frames:
cancel32/refresh48/load48/service192/Versandsschritt48,
encodeConfiguredStatus160, innere Frameencode32/decode48.
Tatsächlicher SDK-Probenloopentry864Byte; Adapterread560 plus Callerkopie/
Sperre/Encoderkette, keine gemessene Gesamtreserve. Messquellen lokal:
%TEMP%/larp-status-frame-budget-20261007 und SDKobjekt/ELF.
Architektur94Dateien/216Includes,60Header,159Formatdateien erfolgreich.
Vorhandene FastLED-/ArduinoUART-Warnungen bleiben neben grünen Builds.
Keine neue Cppcheck-/Remote-CI-/physische Abnahme.

Uhrvergleiche verwenden den bestehenden uint32-Millisvertrag modulo2^32;
regelmäßige Bearbeitung innerhalb eines vollständigen Uhrumlaufs ist eine
Voraussetzung. Feste Byte-/Schritt-/Fristgrenzen sind belegt; tatsächliche
Mikrosekundenbudgets und Funk-/FreeRTOS-Scheduling am Gerät noch offen.
