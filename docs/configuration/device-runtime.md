# Komponentenplanung und Geräteaufträge

Stand: 6. Oktober 2026. Implementiert: hardwarefreie Planung, revisionskorrelierter Broker, statischer FreeRTOS-Kanal und synthetische Ausführungsbeispiele. Die Produktionsfirmware benutzt weiterhin die feste Verdrahtung und den alten Spielcontroller. Dieser Schnitt initialisiert keine realen Komponenten und bietet noch keine Companion-Verbindung oder persistente Profilübernahme.

## Komponenten aus einem Profil planen

`DeviceRuntime::compilePlan` validiert zuerst das gesamte Profil. Erst bei Erfolg verändert es den Zielplan. Der Plan speichert IDs, Typen, Tabellenindizes, Besitzer, I2C-Busse, Matrixabhängigkeiten, LED-Gesamtzahl und den gemeinsamen Funkkanal; keine geliehenen Profilzeiger.

| Gerätebesitzer | Komponenten | Kopierte Aufträge |
| --- | --- | --- |
| NFC | NFC/SPI | Kartenlesung |
| I2C | IO-Expander und davon abhängige Matrix | Kristalllesung |
| Audio | UART-Audio | Track, Wake |
| LED | LED-Streifen | Vollständiger aufgelöster Effekt einschließlich 16 Farben |
| Peer | ESP-NOW-Peers | Aktivierungsanforderung |

Die Initialisierungsfolge führt erst IO-Expander, dann unabhängige Komponenten und zuletzt abhängige Matrizen auf. Sie ist unabhängig von der JSON-Reihenfolge. Der I2C-Besitzer richtet die geplanten Busse vor diesen Schritten ein. Bereits vorhandene Profilprüfungen sichern Pins, Buszuordnungen, UART-Exklusivität, Matrixendpunkte und IDs. Zusätzlich lehnt die Planung unterschiedliche feste ESP-NOW-Kanäle ab: mehrere Peers teilen einen Funkkanal. Kanal 0 folgt dem ausgewählten Kanal.

Ein realer Gerätebesitzer muss sämtliche benötigten `Component`-Einstellungen vor der Freigabe des Profils in seine eigene Konfiguration übernehmen. Der Plan ist eine Abhängigkeits- und Besitzbeschreibung, kein Treiberregister mit fertig konstruierten Geräten. Insbesondere mehrere FastLED-Controller, GPIO-Laufzeitwahl, SPI-/UART-Initialisierung und kontrollierte Treiberdeinitialisierung sind noch offen.

## Broker zwischen Spiel und Besitzern

`DeviceRuntime::Broker` implementiert den `Session::IProfileDevices`-Port. Anwendung und Broker gehören genau einem Anwendungskontext. Der Kanal kopiert Werte und ruft niemals zurück in Broker oder Session. Keine Hardwareabfrage und kein Queue-Warten im Anwendungstick.

- Ein Ticket enthält Generation, fortlaufende Sequenz, Profilrevision, Zustandsrevision, Komponenten-ID und Abfrageaktions-ID. Die Sequenz läuft nicht über; bei uint32-Maximum werden weitere Aufträge abgewiesen. Profilwechsel benutzen eine strikt neuere Generation.
- Der Broker prüft für jedes Kommando den aktiven Komponententyp und Besitzer. Zum Beispiel ist ein IO-Expander kein Matrix-Leseziel, obwohl beide dem I2C-Besitzer gehören.
- Pro Komponente höchstens eine anstehende logische Leseanforderung. Wiederholte Abfrage desselben Tickets ergibt `Pending`. Konkurrierende andere Abfrageaktionen derselben Komponente werden abgewiesen; es gibt keine zusätzliche Fairnessgarantie für solche Profilkombinationen.
- Nur passende terminale Antworten werden verbraucht. Fremde/alte Tickets und doppelte Antworten erhöhen `staleReplies`. Nach Zustands-/Profilwechsel gelangen alte Lesedaten nicht in den Spielfortschritt. Bereits angenommene Ausgaben dürfen ihre Arbeit noch abschließen.
- `service()` verarbeitet höchstens acht Antworten und läuft vor jedem Sessiontick. Ein Owner liefert den Abschlusszeitpunkt aus derselben Millisekundenuhr. Ein rechtzeitiger Abschluss kann am späteren Tick verbraucht werden; ein Abschluss nach der Frist ergibt `Unavailable`. Fehlende Antworten laufen ebenfalls in einen sichtbaren Timeout.
- Die vorläufige Antwortfrist ist `17 * cardTimeoutMilliseconds + 100 ms`. Sie deckt rechnerisch eine volle Owner-FIFO plus Ausführung ab, **sofern** alle betreffenden Jobs entsprechend begrenzt sind. Matrixscan, Scheduling, Audio und echte Treiber müssen ihren eigenen Zeitvertrag noch erfüllen. Das ist keine gemessene Reaktionsgarantie.
- Angenommene Play-Aufträge reservieren Audio-BUSY bereits vor dem Start durch den Audio-Besitzer. Freigabe erst nach einer Beobachtung derselben Generation, deren `processedSequence` das Play bestätigt. Der Besitzer hält Audio-BUSY bis zum tatsächlichen Abschluss wahr. Ein roher BUSY-Pin genügt hierfür nicht.

`BrokerSnapshot` ist eine Wertkopie mit begrenzten Zählern und Anzahl anstehender Reads. Karten-UIDs bleiben ausschließlich im internen Lesepayload. Der Brokerstatus ist noch kein veröffentlichter Companion-Status; Transport, Zugriffsprüfung und externe Anforderungs-IDs fehlen weiterhin.

## Statischer FreeRTOS-Kanal

`lib/FreeRtosProfileChannel` verwendet die festgelegte ESP32-/ESP-IDF-v4.4-Werkzeugkette:

- Fünf FIFO-Queues, je **16** kopierte Aufträge; zusätzlich höchstens ein laufender Auftrag je Besitzer.
- Eine Antwort-FIFO mit **8** Einträgen.
- **16** einmalige, überschreibbare Komponentenmailboxes für Verfügbarkeit, Audio-BUSY, Wake-Eingänge, Generation, bearbeitete Sequenz und letztes Besitzerergebnis. Keine UID.
- Alle Queuepuffer und Kontrollblöcke gehören dem Prozesslebenszeitobjekt; `xQueueCreateStatic`. Kein Queue-Heap, kein implizites Löschen lebender Queues. Einmaliger Aufbau vor Taskstart; das Objekt darf nicht bewegt oder kopiert werden.
- Alle Queuezugriffe mit Wartezeit null. Nicht für ISR-Aufrufe. Ein Anwendungsproducer, genau ein Worker je Besitzer, genau ein Writer je Komponentenmailbox. Die Klasse erzeugt die Worker nicht selbst.
- Ausstehende Arbeit wird **vor** dem Queuepublizieren gezählt. Ein schneller Consumer kann daher keinen ungezählten Auftrag abschließen. Volle Auftragsqueue liefert Ablehnung und erhöht den Zähler.
- `take` gibt keinen weiteren Auftrag heraus, solange der Worker sein Ticket nicht abgeschlossen hat. `complete` prüft das vollständige Ticket, Auftragsart und terminale Leseantwort. Doppelte/falsche Abschlüsse werden abgewiesen. Eine volle Antwortqueue liefert `ReplyDropped`, zählt den Verlust und beendet den Job; der Broker meldet später fehlende Daten als Timeout.

**Übernahmebarriere:** Alle Gerätebesitzer müssen zunächst autonome Treiberarbeit anhalten/abbrechen und anschließend `parkOwner` bestätigen. Parken gelingt nur ohne angenommene Arbeit für diesen Besitzer. Leere Queues allein reichen nicht: laufende Aufträge bleiben mitgezählt. `quiescent()` wird nur wahr, wenn alle fünf Besitzer geparkt und alle Jobs beendet sind. Dann kann der Broker einen erfolgreich kompilierten, unveränderten Plan mit neuer Generation übernehmen. Besitzer übernehmen ihre neue Konfiguration und Generation vor `resumeOwner`. Alte Mailboxwerte werden durch die Generation erkannt; alte Antworten werden beim nächsten `service` verworfen.

Dieser Kanal liefert die Bausteine der Barriere. Koordinator, Abbruchprotokoll, Treiberwechsel, Timeout der Barriere, Wiederherstellung des vorherigen Profils und last-good-Speicherung sind noch **nicht** implementiert. Ein Owner darf sich nicht als geparkt melden, solange sein Treiber selbständig weiterarbeitet. Ein `complete` bei Audio ist erst nach abgeschlossener/abgebrochener Gerätearbeit erlaubt, auch wenn vorher schon Start/BUSY publiziert wird.

Primärquelle: [ESP-IDF v4.4 FreeRTOS API](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/freertos.html), insbesondere statische Queues und Taskerzeugung. Die installierten `queue.h`, `task.h` und `portmacro.h` wurden ebenfalls gelesen. Dieser ESP32-Port definiert `StackType_t` als `uint8_t` und Taskstackgrößen in **Bytes**.

## Rechnerbedienung mit Warteschlangen

```powershell
pio run -e profile-tools -j 2
.pio\build\profile-tools\program.exe --import docs/configuration/standard-profile.json --play-queued docs/configuration/standard-queued-play.txt
.pio\build\profile-tools\program.exe --import docs/configuration/extended-profile.json --play-queued docs/configuration/extended-queued-play.txt
.pio\build\profile-tools\program.exe --import docs/configuration/standard-profile.json --play-queued docs/configuration/gamemaster-play.txt
```

Die gleichen Session-, Plan-, Ticket- und Brokerklassen laufen mit begrenzten synthetischen FIFO-Besitzern. Jeder Tick bedient zuerst deren angenommene Aufträge und liefert kopierte Antworten, danach läuft der Sessiontick. `settle NOW N` führt **1..8** solcher Mikroticks am gleichen Zeitpunkt aus; Profilintervalle und aktive Wartezeit werden dadurch nicht beschleunigt. Die editierbaren Beispiele verwenden vier Mikroticks. Ein physischer Taskplan wird dadurch nicht simuliert.

`device_request` protokolliert hier die synthetische Ausführung (`applied`), die Session die Queueannahme (`queued`). `channel` zeigt angenommene/abgewiesene Aufträge, Antworten, veraltete Antworten, Timeouts, Antwortverluste, ausstehende Reads und noch queued Ausgaben. Ein Sessionstop leert angenommene Ausgabejobs nicht automatisch. Am letzten Beispielschritt können deshalb noch Ausgabejobs in der FIFO stehen.

Regenerieren: `python scripts/generate_play_examples.py` erzeugt die unmittelbaren und die queued Varianten aus denselben Profilen. Das Werkzeug führt keine Assertions und keine Hardwareausführung durch. Der bestehende unmittelbare `--play`-Modus bleibt verfügbar.

## ESP32-Bauziel mit fünf synthetischen Tasks

`pio run -e esp32-profile-runtime-probe -j 2` baut einen separaten Einstieg, der Profil, Plan, Broker, Session, Kanal, statische Worker-Kontrollblöcke und fünf **4.096-Byte-Stacks** tatsächlich instanziiert. Fünf Besitzer laufen auf Core 0; der Arduino-Anwendungslauf bedient Broker/Session. NFC, Matrix, Audio, LED und Aktivierung sind synthetisch; keinerlei externe GPIO-, Bus-, Audio-, LED- oder Netzwerkinitialisierung.

Die Probe enthält nur einen automatischen Ausschnitt mit der ersten Profilrolle und 100-ms-Synthetikaudio. Sie ist keine komplette Sechs-Runden-Abnahme und keine Produktionsintegration. In diesem Arbeitsschritt wurde sie ausschließlich gebaut, **nicht hochgeladen oder am Board ausgeführt**. Der zuvor geprüfte USB-Diagnosestand bleibt am Board.

## Noch offen

1. Reale Besitzer-/Treiberadapter mit profilabhängiger Einrichtung, Abbruch/Stilllegung, Lebenszeichen-/Sample-Alterprüfung und kontrolliertem Produktionsanschluss. Die Mailboxes prüfen bislang Generation, nicht das Alter eines gecachten Samples.
2. Last-/Overflow-/Stale-/Timeout-/Generationswechselabdeckung und Legacyvergleich; normaler Rechnergebrauch beweist diese Randfälle nicht.
3. Persistente aktive/Entwurf-Slots, last-good-Profil, Fehler-/Stromausfallverhalten und vollständige Aktivierungsbarriere.
4. Dynamischer Companion-Status, authentifizierte Steuereingänge und direkter Appzugang ohne zusätzliche Netzwerkinfrastruktur.
5. Android-Editor/Spielleitung; reale Hardware-, Stack-, Latenz-, Energie- und Recoveryabnahme.

## Beobachteter Stand am 6. Oktober 2026

Normale Bedienung mit dem gebauten Rechnerwerkzeug, keine neue Testsuite:

| Beispiel über `--play-queued` | Ergebnis |
| --- | --- |
| Standard | Sechs Rollen je drei Kristalle, dieselbe Trackfolge wie beim unmittelbaren Port, Bonus 900, genau ein synthetischer Aktivierungsauftrag, danach Idle und zurückgesetzte Runden/Historie |
| Zusatzprofil | Rolle 77, vier Kristalle, vierter Track 178, Rollen-Track 177, Zustand 88, orange Palette, Rückkehr nach Idle |
| Spielleiter | Start/manuelle Zustände/Pause/Fortsetzen/Stop, Zustandsrevisionen 1..6 |

Standard: **765** angenommene Aufträge, **90** Antworten; Zusatzprofil: **110 / 6**; Spielleiter: **6 / 0**. In diesen Abläufen jeweils null abgewiesene Aufträge, veraltete Antworten, Timeouts und Antwortverluste. Beim letzten Status jeweils keine anstehenden Reads, aber **ein noch queued Ausgabeauftrag**; das ist keine vollständige Stilllegungsabnahme. Der unmittelbare Standardmodus wurde ebenfalls regulär bedient und lieferte die gleiche Audiofolge und genau eine Aktivierung.

Alle vier Bauziele erfolgreich: `pio run -e profile-tools -j 2` und `pio run -e az-delivery-devkit-v4 -e esp32-runtime-probe -e esp32-profile-runtime-probe -j 2`.

| ESP32-Bauziel | Statisches RAM | App-Flash von 1.310.720 Byte |
| --- | ---: | ---: |
| Produktionsspiel (alter Controller) | 55.148 Byte | 1.226.053 Byte / 93,5 % |
| Bisherige USB-Probe | 21.480 Byte | 291.673 Byte |
| Neue Profil-/Queue-Probe | 102.400 Byte | 298.697 Byte |

Die neue Probe instanziiert den Ausführungspfad, jedoch nur **ein** Profil und synthetische Besitzer. Reale Treiber, zweiter Slot, Parsergebrauch, BLE und Persistenz kommen hinzu. Die Produktionszahlen bleiben ohne diesen Anschluss; Linkerentfernung ist keine zukünftige Reservefreigabe. Bestehende FastLED-/Arduino-ESP32-Warnungen sichtbar. Die neue Probe behandelt eigene Projektquellen als Warnungsfehler; eine bestehende `return`-Warnung in `esp32-hal-uart.c` bleibt Frameworkwarnung.

ESP32-Compiler-/ELFgrößen: Plan **492**, Broker **3.992**, FreeRTOS-Kanal **18.500**, fünf Probe-Worker einschließlich Stacks/Kontrollblöcken **23.280**, zusätzliche Probe-Mailboxwerte **320 Byte**. Command **180**, Reply **204**, DeviceObservation **20 Byte**. Das Queuespeicherbudget allein beträgt **16.352 Byte** für Payloadpuffer; Kontrollblöcke, Handles und In-flight-Tickets sind in der Kanalgröße enthalten. Doppelte vollständige In-flight-Payloads wurden durch Ticket/Art ersetzt. Zwei Profile + Session + Plan + Broker + Kanal mindestens **86.008 Byte**, vor Worker-/Treiber-, Parser-, Transport- und Speicherbedarf.

`-fstack-usage` mit Xtensa/`-Os`: größter einzelner Brokerframe **432 Byte** (`poll`), Planframe **128 Byte** (`compilePlan`); verschachtelte Aufrufe kommen hinzu. Keine gemessene Stackreserve oder Latenz daraus ableiten.

Architektur **57 Dateien / 139 direkte Includes**, **39** isolierte C++11-Header, Format **99 Dateien** mit clang-format 19.1.7; ausgewählte Cppcheck-2.1-Analyse (`warning,performance,portability`) von Plan/Broker/Play ohne Befund, `git diff --check` ohne Befund. Keine neue Unity-Suite ergänzt/ausgeführt, kein Upload, keine neue Boardmessung, keine Remote-CI.
