# Aktives Profil und bearbeitbarer Entwurf

Stand: 6. Oktober 2026. `DeviceRuntime::ProfileController` besitzt zwei private volle Profilspeicher und den Übernahmekern `Runtime`. Import, Einrichtung und Export laufen im Rechnerwerkzeug über diesen Controller; das separate ESP32-Bauziel instanziiert denselben Speicher-/Übernahmepfad. Dauerhafte Speicherung und Companion-Transport sind weitere Schritte.

## Speicherbesitz

`beginStorage()` reserviert vor Workerstart genau zwei `Profile`-Objekte mit `new (std::nothrow)` und prüft den Erfolg. Bei Fehler werden die unvollständigen Startup-Zuweisungen freigegeben. Ein erfolgreicher Aufbau wird nicht wiederholt. Der Controller und seine Kanäle haben Prozesslebensdauer; kein Zerstören bei noch laufenden Tasks. Im normalen Dienst, Import, Export, Spiel und Profilwechsel entstehen keine weiteren vollen Profilspeicher und keine wiederkehrenden Controller-Heapzuweisungen.

Die beiden Objekte sind private `unique_ptr`-Speicher. Öffentliche Methoden geben weder Profilzeiger noch mutable Referenzen heraus; Runtime ist ebenfalls privat. Ein aktiver Speicher wird nie vom Importer beschrieben. Während der Übernahme sind aktives Profil und Kandidat gesperrt, damit Runtime für Einrichtung/Rückkehr unveränderliche Quellen behält. Gerätebesitzer bekommen weiterhin nur kopierte Konfigurationen.

| Zeitpunkt | Aktiver Speicher | Entwurf |
| --- | --- | --- |
| Nach Speicheraufbau | Keiner | Einer der zwei Bereiche darf beschrieben werden |
| Nach validiertem Import | Unverändert | Vollständig validiert, neue Entwurfsrevision |
| Übernahme läuft | Für Rückkehr erhalten | Für Runtime gesperrt |
| `Applied` | Bisheriger Entwurf | Bisheriger aktiver Bereich wird inaktiv; bei erster Übernahme der andere Bereich |
| `RolledBack` / `Failed` | Vorheriger aktiver Bereich erhalten, falls vorhanden | Kandidat wieder bearbeitbar; kein heimliches Vertauschen |

Die Slots werden bei erfolgreicher Übernahme durch Indexwechsel vertauscht; keine volle Profilkopie. Der vorherige aktive Bereich wird beim nächsten Entwurf überschrieben. Das ist **kein dauerhaftes last-good-Profil** und kein Schutz vor Stromausfall.

## Import direkt in den inaktiven Bereich

`GameConfiguration::readDraftProfile(reader, draft)` benutzt denselben begrenzten Parser und dieselbe semantische Validierung wie der bisherige Import, schreibt jedoch direkt in einen exklusiv besessenen **inaktiven** Bereich. 128 KiB Dokumentgrenze, acht Containerstufen und Modellkapazitäten bleiben unverändert.

Ein Fehler darf teilweise Inhalte hinterlassen. Der Controller markiert den Entwurf vor dem Import ungültig und gibt ihn erst nach vollständigem Erfolg frei. Ein solcher fehlgeschlagener Entwurf kann weder aktiviert noch exportiert werden. Eingabe und Entwurf müssen bei der niedrigen Parser-API getrennte Bereiche sein.

Das bisherige `readProfile(reader, scratch, destination)` delegiert an denselben Parser und behält seinen Vertrag: getrennte Bereiche, Ziel erst nach Erfolg kopiert, Ziel bei Fehler unverändert. Dieser Vertrag braucht weiterhin zwei Bereiche für den Aufruf. Der neue Controller nutzt die direkte Entwurfsvariante und braucht dadurch beim bereits aktiven Profil keinen dritten vollständigen Bereich.

## API und Versionen

| Methode | Wirkung |
| --- | --- |
| `createDraft(Standard/Hint, expectedDraftRevision)` | Werkprofil im inaktiven Bereich erzeugen und validieren |
| `importDraft(reader, expectedDraftRevision)` | Direkt in den inaktiven Bereich lesen; Importbefund zurückgeben |
| `activateDraft(expectedDraftRevision, expectedConfigurationRevision, now)` | Validierten Kandidaten sperren und kontrollierte Geräteübernahme anfordern |
| `service(now)` | Begrenzten Runtime-Dienst ausführen und terminale Übernahme im Slotbesitz abschließen |
| `writeDraft(writer, expectedDraftRevision)` | Nur validierten, ungesperrten Entwurf serialisieren |
| `writeActive(writer, expectedConfigurationRevision)` | Aktiven Bereich serialisieren, auch als Quelle späterer Speicherung |
| `storage()` | Wertkopie von Revisionen, Slotverfügbarkeit und letztem Importbefund, keine Karten-UIDs |
| Spielmethoden | An private Runtime weiterreichen; während Bearbeitung/Übernahme gesperrt |

Die **Entwurfsrevision** ist ein eigener uint32-Zähler. Sie steigt vor jeder zugelassenen beschreibenden Operation, auch wenn deren Import scheitert, und beim terminalen Abschluss einer Übernahme. Bei Ablehnung vor Bearbeitungsbeginn bleiben Speicher und Revision erhalten. Ein überholter erwarteter Wert führt zu `StaleRevision`, keine Zählerüberläufe. Aktivierung reserviert die noch benötigte Abschlussrevision vor der Annahme.

Die **Konfigurationsrevision** steigt nur bei erfolgreicher neuer Einrichtung; **Dokumentrevision** kommt aus dem JSON; **Gerätegeneration** steigt auch bei Rückkehr. Siehe [Übernahmevertrag](activation.md). `Queued` ist Annahme, nicht bereits Einrichtungserfolg. Der Aufrufer muss `service()` fortsetzen und terminalen Status lesen. Die Ergebnis-Unterfelder sind jeweils für den durchgeführten Import, die Factoryvalidierung oder den tatsächlichen Runtime-Aufruf bestimmt; Standardwerte unbenutzter Unterfelder sind keine zusätzliche Freigabe.

Diese Methoden gehören genau einem Anwendungskontext. Der Reentranzschutz verhindert erneute Änderungen durch synchron aufgerufene Reader-/Writer-Callbacks, ist aber **kein Mutex für andere Tasks**. Reader und Writer sind synchrone, begrenzte Quellen/Senken; Bytes eines Writers dürfen nur bis zum Rücksprung geliehen bleiben.

## Gegenwärtige Zeitgrenze

Import, Factoryerzeugung und vollständiger Export laufen synchron und sind deshalb nur bei **gestopptem** Spiel zugelassen. Pause ist weiter ein laufendes Spiel. Während einer Geräteübernahme werden sie ebenfalls abgewiesen. So verzögert ein großer Dokumentvorgang keinen laufenden Spieltick; echte Treibertasks können vorhandene Jobs weiter beenden.

Der Controller liefert noch keinen inkrementellen Parser und keinen Hintergrundimport für das laufende Spiel. Für echte Appübertragung müssen Transportempfang, Bearbeitung, Aufrufbudget und Anmeldung der Zustandsänderungen begrenzt werden. Ein beliebig blockierender Reader/Writer verletzt den Vertrag. Parser-, Export- und Factorydauer sind auf dem ESP32 noch nicht gemessen.

## Rechnerbedienung und editierbare Ergebnisse

```powershell
pio run -e profile-tools -j 2
.pio\build\profile-tools\program.exe --import docs/configuration/standard-profile.json --activate docs/configuration/setup-profile.json --active-export ACTIVE.json
```

Der Einrichtungsmodus liest das Ausgangsprofil und den Kandidaten jeweils direkt in den privaten Entwurf. Vor Aktivierung des Kandidaten wird das alte Spiel gestoppt. Ohne Importdatei dient Standard als Werkprofil; `--example` wählt das Hinweiswerkprofil. `--active-export` ist nur mit `--activate` zulässig. Für diesen Export stoppt das Werkzeug das neue Spiel wieder und schreibt die tatsächlich aktive Konfiguration in die angegebene Datei. Schreib-/Schließfehler führen zu Fehlerstatus; eine dabei eventuell teilweise geschriebene Datei ist zu verwerfen.

JSONL enthält Speicherrevisionen, Übernahmestatus und Ergebnisnamen ohne Karten-UIDs. Die separate Profil-Exportdatei enthält bewusst die vollständige Konfiguration einschließlich der Rollen-UIDs. Synthetische Besitzer initialisieren keine physische Hardware. Ein Sessionstop allein entleert keine angenommenen Ausgabejobs.

Beobachtete normale Rechnerbedienung mit Hinweis-, Zusatz- und Setupkandidaten: jeweils Entwurfsrevision **1** nach Ausgangsimport, **2** nach erster Aktivierung, **3** nach Kandidatenimport, **4** nach zweiter Aktivierung. Beim Kandidatenimport bleibt die aktive Dokumentrevision 3 erhalten; danach aktiv 4/5/6. Alle drei aktiven Exporte geschrieben und als JSON gelesen. Setup enthält weiterhin LED-Pin 27/31 LEDs, Audio-UART 2/Lautstärke 19 und Expanderadresse 0x22. Kontrollablauf initial 27/insgesamt 54 Operationen und Konfigurationsrevision/Generation 1 → 2 bleiben erhalten. Kein Speicherfehler-/Rollback-/Reentranz-/Schedulingnachweis daraus ableiten.

## Ressourcen und nächste Schritte

Xtensa-/Hostcompiler: `ProfileController` **11.152 Byte**, einschließlich Runtime und kleiner Speicherverwaltung; zusätzlich zwei Profile **57.272 Byte Heap**. `ProfileStorageSnapshot` **72**, `DraftResult` **76 Byte**. Zwei Profile + Controller + FreeRTOS-Kanal + fünf Besitzerkonfigurationen mindestens **103.864 Byte** vor Stacks/Treibern/Funk/Persistenz. Den eingebetteten Runtime-/Session-/Broker-/Planbedarf nicht nochmals addieren. Native Aktivierung legt keinen dritten Profile-Scratchbereich mehr an.

Frische Xtensa/-Os/-fstack-usage-Übersetzung: größter einzelner Controllerframe **304 Byte** (`admission`, inklusive Statuskopie), Importframe **176**; direkter Parseraufruf **1.024** und größte Rollenlistenfunktion **1.264 Byte**. Verschachtelte Frames, Validator und Framework kommen hinzu; keine gemessene Gesamtstackreserve. Worker parsen keine Profile.

Nächste Schritte: versionierte dauerhafte Speicherung mit überprüften vollständigen Dokumenten und last-good-/Stromausfalljournal; begrenzter Apptransport und Controllerkommandos mit Anforderungs-ID/Zugriffsprüfung; echte Gerätebesitzer mit Lesefehler-/Stilllegungsvertrag. Android-Editor/Spielleitung und reale Stack-/Zeit-/Energie-/Recoveryabnahme bleiben offen.


## Beobachteter Bau- und Quellenstand

Alle vier Bauziele erfolgreich: `pio run -e profile-tools -j 2` und `pio run -e az-delivery-devkit-v4 -e esp32-runtime-probe -e esp32-profile-runtime-probe -j 2`.

| ESP32-Ziel, 6. Oktober 2026 | Statisches RAM | App-Flash / 1.310.720 Byte |
| --- | ---: | ---: |
| Bisheriges Produktionsspiel | 55.148 | 1.227.445 / 93,6 % |
| Bisherige USB-Probe | 21.480 | 291.673 |
| Profilcontroller-/Übernahmeprobe | 94.784 | 309.581 |

Zusätzlich 57.272 Byte Profilspeicher auf dem Heap, ohne Allocatorverwaltung. Die neue Probe verwendet privaten Controller, erzeugt erst Standard und nach Spielstop das Hinweisprofil im inaktiven Slot; synthetische Eingaben kommen als kleine Wertkopien aus denselben Domain-Werkdefinitionen. Fünf Besitzer mit je 4.096-Byte-Stack, keine GPIO-/Bus-/Audio-/LED-/Funkinitialisierung. **Nicht hochgeladen oder am Board ausgeführt.** Die alte geprüfte USB-Diagnose bleibt erhalten.

Architektur **66 Dateien / 159 Includes**, **44** unabhängige reine C++11-Header, Format **111 Dateien** mit clang-format 19.1.7; ausgewählte Cppcheck-2.1-Analyse für Controller/Reader/Einrichtungswerkzeug/main ohne Befund. Native Quellen mit Warnungen als Fehler, `git diff --check` ohne Befund. Frameworkwarnungen weiter sichtbar. Keine neue Testsuite/Assertions oder Unity-Läufe, keine Fehlerpfad-/Zähler-/Rollback-/Reentranzabnahme, kein Upload oder Remote-CI. Die bereits verfügbaren positiven Rechner-/Exportbelege ersetzen diese Prüfungen nicht.
