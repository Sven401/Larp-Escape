# D2: begrenzte Übergabe an den späteren D1-Worker

**Status: Softwareanschluss zu [#61](https://github.com/Sven401/Larp-Escape/issues/61),
keine produktive D1-/Funkfreigabe.** Datum: 31.10.2026. Ausgangsbasis
`111b2a8aba6342025acc938aff5ba310baaa7c78` im kuratierten Integrationsbranch.
Der genaue Prüfhead und die anschließende CI stehen im zugehörigen PR.
Auftrag: aktuellen D1-Stand prüfen und den nächsten begrenzten D2-Anschluss liefern.
PM: Root; führende Implementierung/Plattform: `d2_tests`; unabhängiger
Plattform-/Security-Grenzreview: `d2_review`. Sitzung
`01a192dd-375d-7863-baa8-b46b6ff92452`. Keine allgemeine Sicherheitszertifizierung.

## Geprüfte Anschlussgrenze

**Ausgelesen:** [D1-PR #128](https://github.com/Sven401/Larp-Escape/pull/128),
Head `27884df501355597ed518bcc5896c53b23ece3b6`, ist inzwischen bei `111b2a8`
integriert. Der [Speicherkernvertrag](d1-firmware-speicherkern.md) und
`ProgressJournal` stimmen überein: synchrone Transaktionsablage, exklusiver
Speicherbesitzer, noch kein semantischer D1-Reducer/Recordcodec. Ein
`Journal::Committed` beweist weder dauerhafte fachliche Annahme noch Anwendung.
Die bestehenden [D2-Portaufrufe](d2-transportkern.md) müssen dagegen begrenzt und
nicht wartend bleiben. Direkte Journal-Aufrufe aus `poll` wären kein zulässiger
Anschluss; positive D2-Ergebnisse aus bloßem Journalerfolg wären unbelegt.

[`EventDurableWorker`](../../include/Companion/EventDurableWorker.h) implementiert
deshalb ausschließlich den kopierenden, flüchtigen Übergang. Der Kern kennt
keinen Flashport und ändert weder D1-Daten noch dessen Schnittstellen. Er nimmt
an keiner Produktaktivierung teil und bietet keine neue Fähigkeit an.

## Ausführbares Verhalten

- Alle drei `DurablePort`-Operationen verwenden vier **gemeinsame** Plätze:
  `mayPublish`, `accept` und `recordReceipt`. Originalbytes bis 2048 Byte sowie
  Fact-, Ziel- und sämtliche Quittungsfelder einschließlich Grund werden kopiert.
  Ein zu großer/leerer Eingang wird abgewiesen, nicht abgeschnitten.
- Der Produzent erhält zunächst `Unavailable`. Eine identische Wiederholung
  erzeugt keinen zweiten Auftrag. Der Worker übernimmt mit `take` eine eigene
  Kopie; seine spätere typisierte `complete`-Antwort bestätigt noch nichts beim
  Produzenten. Erst dessen Wiederholung mit identischer Operation, sämtlichen
  Metadaten, Länge und **allen Originalbytes** konsumiert das Ergebnis einmal.
  Derselbe Digest bei anderen Bytes reicht nicht. Danach ist eine neue Prüfung
  nötig; positive Ergebnisse sind kein dauerhafter Berechtigungscache.
- Pending, laufende Arbeit und fertige Ergebnisse belegen denselben Platz.
  Voll bedeutet `Unavailable` plus `HandoffStatus::Full`; keine Verdrängung.
  `outstanding()` umfasst alle drei Zustände. Der Eigentümer muss Eingaben bei
  Rückstau behalten und erneut vorlegen. Nicht mehr abgeholte Ergebnisse werden
  nicht selbsttätig gelöscht; Recovery verlangt den geordneten Fenceablauf.
- `take` bedient belegte Warteplätze zyklisch, höchstens einen pro Aufruf.
  Falsche Ergebnisarten, unbekannte Enumwerte, doppelte/verspätete Tickets und
  nicht mehr laufende Aufträge werden abgewiesen. Eine falsche Completion
  vernichtet die noch mögliche richtige Antwort nicht.
- `serviceOne` ist ein ausführbarer, synchroner Dispatcher **im separaten
  Speicherworker-Kontext**. Er gibt die kopierte Anfrage typgetreu an einen
  übergebenen semantischen `DurablePort` weiter. Ein Defaultport liefert nur
  `Unavailable`. D2-Produzentenmethoden rufen diesen Dispatcher niemals auf.
  Eine produktive semantische D1-Implementierung wird hier nicht mitgeliefert.

## Fence, Lebensdauer und Nebenläufigkeit

Alle Zugriffe brauchen externe Serialisierung; die Klasse enthält keine Locks,
Tasks, ISR-Anbindung oder Thread-Sicherheit. Die spätere Threadintegration muss
`take`/`complete` jeweils unter derselben Koordination wie die Produzentenseite
aufrufen und nur die kopierte Anfrage außerhalb dieses Bereichs bearbeiten.
`serviceOne` ist eine Hilfe für bereits serialisierte Worker-Ausführung und
installiert diese Koordination nicht. Der reale Journalaufruf darf keinen
Spiel-/Funkcallback und keinen zeitkritischen Tick blockieren.

Die Konstruktion verlangt eine ausdrücklich vom Eigentümer gelieferte,
nichtnull `incarnation`. Default/null bleibt inaktiv. Dieser Wert darf nicht
wiederverwendet werden, solange alte Aufträge/Antworten überleben könnten.
Erzeugung und Lebenszyklusnachweis sind offene Koordinatorpflicht; hier wird
kein Zufalls-/Persistenzverfahren erfunden. Tickets binden diese Inkarnation,
Generation und monotone Auftragsfolge. Kopieren und Verschieben der Instanz
sind gesperrt. Vor Zerstörung müssen alle Objektzugriffe beendet sein; eine
neue Instanz bekommt eine frische Inkarnation. Bei Neustart muss der Koordinator
alte Workerkanäle leeren.

`invalidate()` erhöht die Generation und verwirft ausschließlich flüchtige
Aufträge/Ergebnisse. Die Auftragsfolge wird nicht zurückgesetzt. Der Eigentümer
muss dies **vor** Sichtbarwerden jeder Änderung ausführen, die eine vorbereitete
Antwort entwertet: Rechte-/Kontextwechsel, D1-Konfliktsperre, Backend-Recovery oder
Verlust der Speicherbereitschaft. Die neue Generation kann keine alte positive
Completion übernehmen. Zählerüberlauf sperrt die Instanz; kein Wrap.

Das stoppt **keinen bereits laufenden Flashwrite** und ist keine fertige
taskübergreifende D1-Rechtefence. Der semantische Worker muss `isCurrent(ticket)`
und die tatsächlichen Rechte/Revisionen unmittelbar am Commitpunkt unter
derselben Koordination prüfen; relevante Änderungen bis zum geklärten
Schreibausgang ordnen. `isCurrent` gilt nur für laufende Aufträge und verleiht
selbst keine fachliche Berechtigung. Keine D1-Daten werden durch Invalidation
gelöscht oder zurückgerollt.

## Budget und Softwareprüfstand

**Gemessen, Windows-MinGW/C++11:** Bridge 9624 Byte, eine `WorkRequest`-Kopie
2376 Byte. Compilezeitgrenzen: Bridge höchstens 12 KiB, Anfrage höchstens 3 KiB;
vier globale Plätze, je höchstens 2048 Nutzbytes, keine Heapallokation. Jeder
Produzentenaufruf vergleicht höchstens vier begrenzte Anfragen und kopiert
höchstens eine. `serviceOne` benötigt zusätzlich eine Anfrage auf dem Stack.
D2-Core, Worker/D1, Verifizierer, SDK und zusätzliche Transportkopien sind
getrennte Budgets; diese Zahlen sind keine kombinierte ESP-Ressourcenabnahme.

Die [18 neuen Hostfälle](../../test/test_event_durable_worker/test_main.cpp)
sind mit `-std=c++11 -Wall -Wextra -Wpedantic -Werror` erfolgreich ausgeführt:
Defaultverweigerung, volle gemeinsame Queue, Originalbytes/alle Metadaten,
kopierte Lebensdauer, einmalige Ergebnisse aller Arten, falsche/stale/doppelte
Completions, vertauschte Ergebnisse, Generation/Inkarnationswechsel,
semantische Konfliktsperre, 2048-Byte-Grenze und 4096 Wiederverwendungen.
Die drei tatsächlichen D2-Core-Pfade warten im Integrationstest jeweils auf
den explizit simulierten semantischen Worker. Keine Testantwort gilt als
Kryptografie-, Journal-, Flash- oder Gerätebeleg. Der uint64-Maximalüberlauf
ist im Code abgefangen, wurde nicht durch 2^64 reale Wechsel erprobt.

**Weitere lokale Messungen:** vollständiger PlatformIO-Hostlauf mit den ersten
15 Workerfällen: 18 Suiten, 214/214 erfolgreich. Nach Ergänzung der drei
abschließenden Gegenfälle: neue Suite 18/18 sowohl über PlatformIO als auch
im strengen Einzelbuild erfolgreich. Architekturgrenze: 157 Dateien/330 Includes;
92 eigenständige C++11-Header; 111 D0-Modellfälle. Alle erfolgreich.
Die drei neuen C++-Dateien wurden mit clang-format 19.1.7 formatiert und geprüft.
Native SDK-/PR-CI ist ein nachfolgender, separat im PR zugeordneter Nachweis.

Reproduzierbare Prüfwege:

```text
python -m platformio test -e host-regression -f test_event_durable_worker -v
python -m platformio test -e host-regression -v
cmake -S . -B private/host-build
cmake --build private/host-build
ctest --test-dir private/host-build -R event-durable-worker --output-on-failure
python scripts/check_architecture.py
python scripts/check_headers.py
python scripts/check_d0_contract.py
```

Die bestehende Host-/Native-CI erhält den Quell-/Testanschluss; Workflows und
Pflichtchecks bleiben unverändert. Die unabhängige Gegenprüfung und weitere
commitbezogene Ergebnisse werden im PR festgehalten.

## Offene produktive Abhängigkeiten

Der nächste Anschluss braucht einen semantischen D1-Worker: verifizierte D0-
Originaldaten und echte Digests, kanonische Recordkodierung, dauerhaftes
Fact-/Stream-/Latch-/Quittungsreduzieren, aktuelle Rechtefence, Recovery und
Outboxrekonstruktion. Erst dessen belegte Ergebnisse dürfen `Publication::Durable`,
`Acceptance::Durable` oder `ReceiptCommit::TargetDurable` liefern. Die vorhandene
Journalablage allein liefert diese Semantik nicht. Die D1-Rechnerreferenz bleibt
fachliche Quelle; D1-/Planer-/F5a-Schnittstellen wurden nicht umgestaltet.

D0-Ursprungsverifizierer, Challenge/Grant, reale Transportadapter/Fragmentierung,
Funkunterbrechung am Gerät, BLE-Koexistenz, kombinierte Last, Flashlayout und
physische Stromausfälle bleiben offen. Kein Geräte-/COM-/Funkzugriff, Upload,
Wireformat, Bibliothekswechsel oder SDK-Migration in diesem Schritt. #60/#61
werden nicht durch den begrenzten Anschluss abgeschlossen.
