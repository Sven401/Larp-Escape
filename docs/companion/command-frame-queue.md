# Kurzer Empfangscallback und feste FreeRTOS-Framequeue

**Stand:** 7. Oktober 2026. **Status:** tatsächlicher Queue-, Kern-, Host- und
synthetischer Probenpfad. SDK-Verbindung, GATT-Antworten und RF nicht eingerichtet.

## Besitz und Grenzen

FreeRtosCommandFrameQueue besitzt vier statische Kopien von
CopiedCommandFrame: maximal128 Wertbytes plus vertrauenswürdige Principal,
Epoch, ursprüngliche Empfangszeit und tatsächliche Länge. begin erfolgt vor
Tasks/Callbacks und ist idempotent; kein Zurücksetzen gepufferter Frames.
Kein Heap oder Profil, ausschließlich task-kontextbezogene FreeRTOS-Queue-
Aufrufe mit Wartezeit0. Dies ist keine ISR-API.

Der künftige SDK-Adapter prüft zuerst Service, Connection und Characteristic-
Handle und ordnet die verifizierten Metadaten außerhalb der Clientbytes zu.
copyWrite weist Prepared Writes und GATT-Offset ungleich0 ab; dieser Offset ist
vom inneren Frameoffset zu unterscheiden. Nur aktueller verifizierter GM darf
kopieren. Tatsächlicher Framecodec prüft Version, Typ, Header- und Nutzlängen,
Gesamtlänge und Offsets. Im Callback werden keine JSONdaten, Profile, Speicher
oder Spielaktionen verarbeitet.

Copied bestätigt ausschließlich die unveränderte feste Framekopie.
QueueFull ist ausdrücklich keine positive Aufnahme. Vollständiges CommandQueued
und spätere Executed-Spielantwort sind getrennte spätere Ergebnisse. Dieser
Schnitt versendet noch keine GATT- oder Appbestätigung. Der Arduino-Wrapper
bestätigt unter Umständen vor seinem Anwendungscallback; ein künftiger Adapter
muss die tatsächliche Aufnahmeantwort selbst richtig zuordnen.

## Transportowner und absolute Frist

dispatchCommandFrame übernimmt höchstens einen Frame pro Aufruf und führt ihn
durch den tatsächlichen CommandStream, Codec und Commandqueueadapter. Eine
leere Queue führt stream.poll aus: Entzug und Fristablauf wirken auch ohne neue
Daten. Kein implizites QueueFull-Retry und kein zusätzlicher Anwendungstick.
Bei QueueFull wiederholt der Owner ausdrücklich dieselben vollständigen Daten
innerhalb der verbleibenden Frist oder meldet Ablauf.

Die receive-Überladung mit receivedAt und now verwendet die **ursprüngliche
vertrauenswürdige Empfangszeit** des ersten Frames als Fristbeginn. Beide Zeiten
stammen aus derselben monotonen uint32-Millisekundenuhr. Queuewartezeit zählt zu
den absoluten2000ms. Bereits2000ms alte oder zukünftige Zeitwerte werden vor
Zusammenfügen/Admission abgewiesen. Ein späteres Stück, eine Wiederholung oder
QueueFull verlängern den ersten Beginn nicht. Die bisherige direkte
receive-Variante setzt receivedAt=now; ihr Vertrag bleibt erhalten.
Uhrumlauf wird mit unsigned Differenzen behandelt; keine beliebig alte
über einen vollen Uhrumlauf aufbewahrte Queue oder fremde Uhr ist zulässig.

## Entzug und Reconnect

Die kurze Authoritybeobachtung beim Kopieren ist kein atomarer
physischer Zugangs- oder Ausführungsnachweis. Ein späterer Entzug kann eine alte
bereits kopierte Nachricht in der Queue belassen. CommandStream prüft Authority
erneut, und die vollständige Commandaufnahme prüft nach JSONdecodierung nochmals.
Lifecycleentzug erfolgt über den synchronisierten Authorityzustand, nicht als
verlierbares Ereignis in dieser vollen Datenqueue. Eine alte Epoch darf weder
an einen recycelten Connectionhandle umetikettiert werden noch aktuelle neue
Teilnachrichten löschen oder ergänzen.

## Vertikaler Nachweis

Acht zusätzliche Hostfunktionen verwenden die tatsächliche FreeRTOS-Adapter-
Implementierung mit deterministischen kopierenden Ports: Outerwrite-/Frame-
Fehler, fehlender Zugang, Uninitialisiert-/False-Zielerhalt,128Byte-Kopie nach
Quellwiederverwendung, vier volle Plätze/FIFO/idempotentes begin, 20Byte-Frames
durch gespeicherte Anwendung und aktuelle Executed-Antwort, Entzug/Reconnect,
Wartezeit1900ms und Gesamtablauf2000ms, Erfolg1999ms einschließlich Uhrumlauf,
zu alte/zukünftige Zeit, leerer Ownerpoll ohne automatisches Retry oder
zusätzlichen Spieltick und Entzug nach erster Authoritybeobachtung.

Die vorhandene Werkzeuggegenstelle --frame-command produziert denselben
Framevertrag und13 kanonische Referenzwerte. esp32-profile-runtime-probe
führt synthetische Frames nun durch diese tatsächliche Queue und denselben
Ownerdispatch; kein SDKcallback und kein zusätzlicher Hardwarezugang.
66/66 gezielte Hostfunktionen erfolgreich, terminal Exit0, ohne Fixtureexport;
%TEMP%/larp-frame-queue-final-host.log. Alle vier finale Builds SUCCESS/Exit0,
%TEMP%/larp-frame-queue-final-build.log: Produktion55.148 RAM/1.228.417 Flash,
alte Probe21.480/291.673, Profilprobe99.728/387.449 plus57.272 Profilheap und
Allocator/FS. Tatsächliche Werkzeuggegenstelle nach Build erneut bytegleich:
13 Befehlsframes,1732 unveränderte Katalogbytes. Architektur86/201, Header55,
Format147/clang19.1.7, achtv2+Katalog schema-valid und Generatorabgleich.

CopiedCommandFrame144 Byte; tatsächliches Proben-ELF frameQueue672,
CommandStream1048 und Companionchannel1252 Byte. Eigene Xtensa-Frames:
dispatch192, receive mit Empfangszeit112, direkte receive-Weiterleitung48;
weitere Kette, Queue-/SDK-Sperren und Caller kommen hinzu. Keine gemessene
Callbackzeit, SMP-Eigenschaft, physische Empfangsrate oder Taskstackreserve.

Kein Upload, Appausführung, Cppcheck dieses Schnitts oder GATT-/RF-Abnahme.
Verifizierte Identität/Pairing, tatsächliche Wireantworten und ein längerer
Profilstream bleiben offen. Produktionsmain verwendet weiterhin den alten
Einstieg; neue Module werden dort noch nicht zur neuen Anwendung montiert.
