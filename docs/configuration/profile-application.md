# Gemeinsamer Ablauf für Einrichtung, Speicherung und Spielleitung

Stand: 6. Oktober 2026. ProfileApplication implementiert und im Rechnerwerkzeug (--journal/--restore-journal) sowie in der ESP32-FreeRTOS-Probe angeschlossen. Reale Produktionsgeräte und src/main.cpp bleiben weitere Integration; kein neuer Upload/Geräte-/Ausfallnachweis.

## Besitz und Betriebszustände

Die Anwendung besitzt ProfileController und ProfileJournal privat. Zwei vollständige Profile bleiben einmalig reserviert; es gibt keine mutable Controller-/Journal-/Profilreferenz nach außen. Backend und kopierte Gerätekanäle werden vorher aufgebaut und leben länger als die Anwendung. Mount/Formatierung gehören dem Backend; eine fehlende/nicht mountbare Partition wird nicht automatisch formatiert. Alle APIs gehören dem einzigen Anwendungskontext. Für externe Tasks sind weiterhin kopierte Anfragen und Statusmailbox nötig.

| Phase | Verhalten |
| --- | --- |
| Empty | Noch kein Startversuch |
| PendingActivation | Entwurf aus Speicher/Factory/Import fertig, weitere Mutationen gesperrt; nächste service(now) startet Einrichtung mit frischer Zeit nach dem synchronen IO |
| Applying | Bestehender kontrollierter Gerätewechsel mit Park-/Resume-/Rückkehrvertrag |
| Saving | Einrichtung Applied/Ready, Spiel gestoppt; nächste service bestätigt dauerhafte Speicherung |
| Ready | RAM-Profil und bestätigter Speicherstand zugeordnet; Spielleitung kann ausdrücklich starten/steuern |
| Fault | Start gesperrt, Details und aktiver RAM-Stand sichtbar; kontrollierter neuer Restore/Entwurf möglich |

service verarbeitet die begrenzten Runtime-Antworten und führt genau einen Spieltick aus, wenn Ready und laufend. Kein zusätzlicher tickGame-Aufruf durch den Einsteiger. IO bleibt synchron bei gestopptem Spiel und kann blockieren; keine Behauptung einer vollständig zeitbegrenzten Servicefunktion während Import/Mount/Save/Bootrückfall. Nach IO wird ein neuer Zeitpunkt vor Beginn der Gerätefrist verwendet. pause gilt weiterhin als laufendes Spiel; große Dokument-/Einrichtungsvorgänge verlangen stop.

## Start und Konfigurationswechsel

begin(ExistingOnly) verlangt ein wiederherstellbares gespeichertes Profil. begin(InitializeEmpty, Factory, optionalReader) verwendet Factory/Import ausschließlich bei zwei fehlenden Datensätzen. Beschädigte vorhandene Daten werden nicht still ersetzt. Vor Workerstart kann begin den Entwurf vorbereiten; erst nach Kanal-/Taskstart service aufrufen. Eine bereits bestätigte gespeicherte Konfiguration wird nach erfolgreicher Einrichtung ohne neue Speicherung freigegeben.

createDraft/importDraft und writeDraft/writeActive verwenden erwartete Entwurfs-/Konfigurationsrevisionen. activateDraft sperrt den Kandidaten und koordiniert Einrichtung → Speicherung → Ready. Es startet kein Spiel. start/stop/pause/resume/changeState/takeTransition verwenden die vorhandenen erwarteten Revisionen und Graphregeln. Start/Steuerung sind nur bei Ready, bestätigter RAM-/Speicherzuordnung und Hardwarefreigabe möglich. Fehlgeschlagene Kandidateneinrichtung mit bestätigter Rückkehr zum zuvor gespeicherten RAM-Profil erlaubt einen ausdrücklichen Spielleiterstart des alten Standes; kein automatischer Neustart nach Fehler.

Nach fehlgeschlagenem Boot eines gespeicherten Profils wird einmalig ein älterer gültiger Datensatz eingerichtet. Erst erfolgreicher Apply führt zur erneuten Speicherung als neueste Generation; der ausgewählte ältere Datensatz bleibt bis dahin geschützt. Ohne passenden Rückfall oder bei weiterer fehlgeschlagener Einrichtung bleibt Fault. restoreStored erlaubt einen ausdrücklichen Wiederherstellungsversuch mit erwarteten Entwurfs-/Konfigurationsrevisionen. Ein ausdrücklich älterer Stand wird nach Einrichtung ebenfalls erneut gespeichert, damit der nächste Boot diesen erfolgreichen Stand auswählt.

## Unklare Schreibresultate

Bei Savefehler bleibt das neue RAM-Profil möglicherweise eingerichtet, seine dauerhafte Bestätigung fehlt. Die Anwendung sperrt Spielstart und zeigt StorageRejected/Fault. Wiederherstellung ist eine neue kontrollierte Operation, keine Behauptung, der vorige Schreibversuch habe sicher nichts verändert.

JournalSnapshot trennt selected slot und protectedSlot. Nach WriteUncertain bleibt der zuvor bestätigte Platz **über Rescans derselben Journallaufzeit hinweg** geschützt. Ein vollständiger neu lesbarer Datensatz darf bei fehlgeschlagener Synchronisierung nicht zum Schutzwechsel führen. selectedUnconfirmed und writeUncertain sind sichtbar; erneute Speicherung schreibt weiterhin in den anderen Platz. Erst Confirmed verschiebt den Schutz. So kann ein Retry nicht den alten bestätigten Datensatz zerstören, während der andere lediglich aus Schreibcache lesbar ist. Diese Korrektur wurde aus dem Codepfad abgeleitet, nicht mit Ausfalleinspeisung/echtem Stromverlust abgenommen.

Nach einem echten Geräteboot stammen gelesene vollständige Datensätze aus dem vorhandenen Flashstand. Rechnerprozessneustart beweist lediglich Lesen im neuen Prozess; OS-/Flashcaches und physische Stromausfallfestigkeit bleiben eigene Abnahme. Zugriffssicherung gehört weiterhin zum Companion-Vertrag; CRC ist keine Authentisierung.

## Sichtbarer Status und Grenzen

ApplicationSnapshot enthält Phase/Operation/Ergebnis, letzte Controller-/Journalergebnisse sowie gespeicherte Journalgeneration, Profildokumentrevision und zugeordnete flüchtige Konfigurationsrevision. storedMatchesActive vergleicht die bekannte Konfigurationsidentität innerhalb dieses Boots; gleiche frei vergebene Dokumentrevisionen beweisen keine gleichen Dokumente. Bei neuem Apply ist diese Zuordnung erst nach bestätigtem Save hergestellt. persistence liefert zusätzlich Auswahl/Schutz/unklare Resultate. Andere Snapshots bleiben UID-freie Wertkopien; Netzwerkcode darf diese Anwendung nicht direkt aus einem anderen Task abfragen.

begin/activate/restore liefern Queued als angenommenen Vorgang, Ready als terminale erfolgreiche Einrichtung/Speicherung. ControllerRejected/StorageRejected und die Snapshots liefern Details; nicht benutzte Teilresultate sind keine zusätzlichen Urteile. Der ergänzte [Companion-Kern v2](../companion/configured-v2.md) bietet Boot-/Requestidentität, Deduplizierung und ausdrücklich vergebene lokale Spielleiter-Leases über kopierte Ports. Vertrauenswürdige Netzwerkanmeldung, Wiredecoder und Transport bleiben offen. Die bestehende Status-v1-Bedeutung wird nicht verändert. Android und direkter Appfunk sind weitere Etappen.

## FreeRTOS-Ressourcen und Belege

Engine.running verwendet dieselbe Bedingung wie der bisherige Snapshot: state != Missing; paused bleibt running. Session/Runtime/Controller reichen dieses bool im Anwendungskontext weiter, keine Task-/Atomicsynchronisierung. Laufzustandsprüfungen brauchen keinen vollständigen SessionSnapshot mehr. Xtensa/-Os: Controller-Admission 32 statt 304 Byte eigener Frame; Journalrestore 528 statt 704; Runtime.activate 64 Byte. Journal.save bleibt384. Parser/Validator/SDK und verschachtelte Aufrufe kommen hinzu; keine reale Stackreserve.

Xtensa: ProfileApplication **11.212 Byte**, inklusive Controller (11.152) und Journal (24); keine doppelte Rechnung. ApplicationSnapshot24/Result84 Byte; JournalSnapshot trotz Schutzfeldern16 Byte. Eigene Frames begin208/restoreStored192/service208, darunter Journal528/384 und Parserketten. Zwei Profilheapbereiche57.272 Byte plus Allocator und IO-/Treiberverwaltung unverändert.

Normaler Rechnergebrauch: neues eigenes Journal → Standard eingerichtet/gespeichert (Konfigurationsrevision1/Journal1), Setup (2/2), neuer Prozess → gespeichertes Setup (flüchtig1/Journal2). storedMatchesActive true erst nach erfolgreichem Abschluss; exportierte JSON-Inhalte gleich, Setuprevision6. Aufrufe Exit0, stderr leer. Artefakte lokal %TEMP%/larp-profile-authoring/application-f980a393/. Keine Fehler-/Rollback-/Uncertain-/Counterausführung, Assertions, neue Testsuite oder Gerätetest daraus ableiten.

Finale Builds mit `pio run -e profile-tools -e az-delivery-devkit-v4 -e esp32-runtime-probe -e esp32-profile-runtime-probe -j2`: alle vier SUCCESS, terminaler Exit0. Log lokal %TEMP%/larp-profile-application-final-build.log. Probe94.880 RAM/370.233 Flash; Produktion55.148/1.228.273 (93,7 %), alte USB-Probe21.480/291.673. Produktion instanziiert weiter den bisherigen Einstieg, somit keine spätere BLE-/Appreserve ableitbar. Bekannte Framework-/FastLEDwarnungen sichtbar. Architektur70 Dateien/165 Includes, 46 unabhängige C++11-Header, Format117/clang-format19.1.7; Cppcheck2.1 warning/performance/portability für gemeinsame Anwendung, Journal, Controller, Runtime, Engine und Rechneranbindung ohne Befund. git diff --check sauber; keine neue Testsuite, Remote-CI, Ausfalleinspeisung oder Geräteprüfung. Im Probenloop sind vollständige Spielstatuskopien auf tatsächlichen Stop/sekündliche Meldung beschränkt. Historischer nächster Schritt (in Companion-v2-Kern inzwischen implementiert): dynamischer Companion-Status und kopierte Spielleiteranfragen mit Boot-/Requestidentität; vertrauenswürdiger Netzzugang, Wiredecoder und Transport bleiben offen; reale Gerätebesitzer, Produktionsanschluss, Geräte-/Stromausfall-/Timing-/Energieabnahme bleiben offen.
