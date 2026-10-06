# Dauerhafte Profile: Zwei-Datensatz-Journal

Stand: 6. Oktober 2026. Implementiert: hardwarefreier Journalkern und synchronisierter Rechner-Dateispeicher. ESP32-Adapter über vorhandenes LittleFS 1.10.0 an die synthetische FreeRTOS-Probe angeschlossen und gebaut. Geräte-/Stromausfallabnahme offen.

## Besitz und Ablauf

`ProfileJournal` verwendet denselben `ProfileController` mit zwei privaten RAM-Profilen. Kein drittes Profil, vollständiger JSON-Puffer oder neue Heapallokation im Journal. Backend besitzt genau einen offenen Stream und zwei dauerhafte Datensätze. Ein Anwendungskontext koordiniert Journal, Controller und Backend; kein paralleler Zugriff oder Wiederaufruf aus Backendcallbacks, kein Taskmutex. Externes Ändern der Dateien während dieser Eigentümerschaft ist nicht unterstützt.

1. Controller-Speicher vor Workerstart reservieren; Backend ausdrücklich öffnen/mounten.
2. `restoreDraft(expectedDraftRevision)` prüft beide Datensatzhüllen und lädt das neueste vollständig validierbare Dokument in den inaktiven RAM-Entwurf. Erfolg ist `Restored`; es wird kein Gerät/Spiel gestartet. Erst anschließend `activateDraft` anfordern und `Applied`/`Ready` abwarten.
3. `Empty` gibt es ausschließlich bei zwei fehlenden Dateien. Erst dann Factory/Import als erstes Profil verwenden. Vorhandene, aber unbrauchbare Datensätze ergeben `Corrupt`, unbekannte Hüllenversionen `Unsupported`, gleiche gültige Generationen `Ambiguous`. Kein automatisches Löschen/Formatieren.
4. Nach erfolgreicher Geräteübernahme `saveActive(expectedConfigurationRevision)` bei gestopptem Spiel. Nur `Applied`/`Ready` mit Hardwarefreigabe erlaubt. Erst zählt/hashes ein Exportdurchlauf, anschließend schreibt ein zweiter denselben Export in den anderen dauerhaften Platz. Bytes und CRC müssen übereinstimmen; kein vollständiger Puffer.
5. Header, Nutzdaten, Abschlussmarker und Backend-Synchronisierung müssen erfolgreich sein. Nur `Saved` erhöht die bestätigte dauerhafte Generation und vertauscht den ausgewählten Platz. Der bisher ausgewählte Datensatz bleibt unverändert. Danach kann das Spiel gestartet werden.

Import-/Exportlimits und semantische Prüfung bleiben identisch zum Profilmodell: maximal 128 KiB JSON, acht Containerstufen und feste Modellkapazitäten. Jeder tatsächlich begonnene Entwurfsimport konsumiert dessen Revision, auch bei Fehlschlag. Auf beiden Plattformen sind Dateioperationen synchron und können blockieren; noch kein inkrementeller Companion-/Hintergrundspeicher. Stdio und LittleFS benötigen eigenen Heap, Stack und Flash-/Latenzbudget.

## Fehler und Wiederanlauf

Die Hüllenprüfung prüft Header-CRC, Grenzen, vollständige Nutzdaten-CRC, passenden Abschlussmarker und exaktes Dateiende. Restore sortiert vollständige Datensätze nach unabhängiger Journalgeneration und versucht JSON-/Semantikimport in dieser Reihenfolge. Beschädigte oder semantisch ungültige neueste Daten erlauben den älteren validen Stand. Der zweite Lesedurchlauf prüft Headergleichheit, CRC, Abschluss, Dateiende und Schließen erneut; erst danach liefert der Leser EOF an den Parser. Damit wird ein fehlgeschlagener Abschluss nicht als gültiger Entwurf freigegeben.

Echte Lese-/Schließfehler brechen die Auswahl mit `IoFailure` ab: der neueste Stand kann dabei unbekannt sein. Eine unterstützte ältere Hülle kann bei einer unbekannten anderen Version geladen werden, Schreiben bleibt dann gesperrt. Auf spätere Generationen wird nicht zurückgezählt, auch wenn ein höherer vollständiger Datensatz semantisch abgewiesen wurde. Generation 0 ist ungültig; uint32-Grenze blockiert weitere Speicherung, kein Umlauf.

`restoreDraft(..., true)` lädt ausdrücklich einen gültigen Stand älter als die zuletzt gewählte Generation. Dies bereitet einen Boot-Rückfall nach fehlgeschlagener Geräteinitialisierung vor; es ersetzt weder die tatsächliche Park-/Reinitialisierung noch deren Abnahme. Ohne frühere Auswahl kein solcher Rückfall.

Ab dem Versuch einer destruktiven Öffnung des freien Platzes führen Fehler zu `WriteUncertain`; weitere Schreibversuche verlangen einen neuen Restore/Scan. Ein bereits vollständiger neuer Datensatz kann trotz unklarer Synchronisierungs-/Schließmeldung vorhanden sein. Daher keine Behauptung, er sei sicher nicht übernommen; nach Neustart Auswahl nach vollständigen geprüften Bytes. Ein vorhandener Entwurf nach `ControllerRejected` gehört weiterhin dem Controller, ist aber kein erfolgreicher Journal-Restore. Rückgabe der jeweiligen Operation ist maßgeblich; nicht verwendete Detailergebnisse sind keine zusätzlichen Urteile.

**Aktivierung und Speicherung sind zwei sichtbare Schritte.** Das neue Profil kann im RAM eingerichtet sein, bevor seine Speicherung bestätigt ist. Bei Stromverlust in diesem Fenster wird der vorige dauerhafte Stand geladen. Speichern verworfener/noch einzurichtender Kandidaten ist ausgeschlossen. Ein `Saved` verspricht das Synchronisierungsergebnis des Backends, keinen hier gemessenen physischen Stromausfallschutz. Noch offen: Produktionskoordinator/Status mit RAM- und Speicherrevision, Startfehler-Rückfall in der Produktion, explizite sichere Erstformatierung, Profilemigration, Fehler-/Stromausfallabnahme und Verschleißstrategie. Häufige unveränderte Speicherung wird nicht automatisch dedupliziert.

## Hüllenformat v1

Binäre Hülle mit festem Little-Endian und CRC-32/ISO-HDLC (Polynomial 0xedb88320, Start/Xorout 0xffffffff), **keine native C++-Objektablage**. CRC erkennt Beschädigung, ist keine Zugriffssicherung/Signatur.

| Teil | Bytes | Inhalt |
| --- | --- | --- |
| Header | 32 | `LARPPRF1` (8), Version 1 (u32), Journalgeneration (u32), JSON-Länge (u32), JSON-CRC (u32), reserviert 0 (u32), CRC der ersten 28 Bytes (u32) |
| Profil | 1–131072 | UTF-8-JSON gemäß bestehendem validiertem Profilschema |
| Abschluss | 24 | `LARPCMT1` (8), gleiche Generation/JSON-CRC/Länge (je u32), CRC der ersten 20 Markerbytes (u32) |

Datei genau `32 + Länge + 24` Byte, kein Anhang erlaubt. Profil-Dokumentrevision, Controller-Konfigurationsrevision, Gerätegeneration und Journalgeneration sind unterschiedliche Zähler. Nach Neustart beginnen flüchtige Controllerzähler erneut; Companion braucht vor externem Steuerzugang weiterhin Bootidentität/Request-ID.

## Rechnergebrauch

Ein vorhandenes eigenes Verzeichnis angeben; das Werkzeug verwendet nur `profile-a.larp` und `profile-b.larp` darin. Vollständige Dateien enthalten Rollen-UIDs wie der Profilexport; Statuszeilen geben keine Roh-UIDs aus.

```powershell
pio run -e profile-tools -j2
New-Item -ItemType Directory C:\Temp\LarpProfiles
.pio\build\profile-tools\program.exe --activate docs/configuration/setup-profile.json --journal C:\Temp\LarpProfiles --active-export C:\Temp\active.json
.pio\build\profile-tools\program.exe --restore-journal C:\Temp\LarpProfiles --active-export C:\Temp\restored.json
```

Bei `--journal` lädt ein vorhandenes Journal den Ausgangsstand; `--import`/`--example` initialisieren nur ein leeres Journal. `--restore-journal` ist ein separater exklusiver Modus und startet nach synthetischer Einrichtung kein Spiel. Der Rechner speichert erst nach den bestätigten synthetischen Ownerantworten. Dateibackend: `fflush`, Windows `_commit(_fileno(...))`, `fclose`; jeder Rückgabewert ausgewertet. Kein ESP32-Treiberbeleg und keine gemessene Stromausfallfestigkeit.

## ESP32-Speicheradapter und Herkunft

`lib/Esp32ProfileStore` verwendet die installierte SDK-Komponente LittleFS **1.10.0**, kein neues heruntergeladenes Paket. Direkte VFS-Registrierung am festen Pfad `/larp-profiles`, Partitionlabel `spiffs`; bereits registrierte/fehlende/nicht mountbare Partition führt zum Fehler. Mountflags Format/Grow=false; keine Format-, Erase- oder Imageuploadfunktion. Die App besitzt diesen Mount über die Prozesslebenszeit. Bestätigung erfordert `fflush`, POSIX `fsync(fileno(...))` und `fclose` mit Erfolg.

Quelle: lokale Frameworkfassung Arduino-ESP32 2.0.14, `tools/sdk/esp32/include/esp_littlefs/include/esp_littlefs.h`, sowie Symbole `vfs_littlefs_fsync`/`lfs_file_sync` in `libesp_littlefs.a`. Die [Upstream-Fassung v1.10.0](https://github.com/joltwallet/esp_littlefs/blob/v1.10.0/src/esp_littlefs.c) leitet VFS-fsync an lfs_file_sync und gibt Fehler zurück. Das ist eine Quellen-/Schnittstellenprüfung, keine vollständige Gleichheitsprüfung der vorübersetzten Bibliothek oder Geräteabnahme.

## Probenanschluss und Ressourcen

Die ESP32-Probe verlangt einen erfolgreichen Mount ohne Formatierung; bei fehlender/unbrauchbarer Partition bleibt sie gestoppt. Vor Workerstart wird der Entwurf wiederhergestellt oder nur bei leerem Journal das Standardprofil erzeugt. Nach vollständiger Einrichtung wird ein neuer RAM-Stand gespeichert, bevor die Probe das Spiel startet. Ein bereits geladener Stand wird beim Start nicht unnötig erneut geschrieben. Nach zehn Sekunden folgt der synthetische Hinweisprofilwechsel mit neuer Speicherung. Bei einem fehlgeschlagenen Boot des geladenen Standes einmaliger älterer Restore/erneute kontrollierte Einrichtung; bei Rollback, fehlendem Rückfall oder unbestätigter Speicherung bleibt das Spiel gestoppt. Dies ist implementierter Probenablauf, kein beobachteter Geräte-/Ausfallbeleg. Synthetische Karten/Kristalle stammen weiter aus Werksdefinitionen und emulieren keine beliebigen Profileingaben.

Journalzustand Xtensa: Objekt 24, Snapshot 16, Result 80 Byte. Eigene Compilerframes (-Os) Restore 704, Save 384, Hüllenprüfung und RecordReader je 64 Byte; Parser/Validator/Filesystem/Framework und verschachtelte Aufrufe kommen hinzu. Keine reale Stackreserve. Probe 94.864 Byte statisches RAM/368.353 Flash, Profilheap zusätzlich 57.272 plus Allocator/LittleFS/Stdio-Verwaltung. Produktion 55.148 RAM/1.227.733 Flash (93,7 %), noch ohne instanziierten konfigurierbaren Produktionspfad; alte USB-Probe 21.480/291.673. Die Linkerentfernung beweist keine spätere Funk-/Appreserve. Vor BLE und Produktionsanschluss Budget weiter offen.

Das gebaute Standardpartitionsschema enthält `spiffs` bei 0x290000 mit Größe 0x160000 und coredump ab 0x3f0000. Zwei maximale Datensätze brauchen 262.256 Byte plus Dateisystemmetadaten/Reserve. Das ist kein Beleg für Format/Inhalt/Partitionen des angeschlossenen Geräts. Kein Image- oder Firmwareupload ausgeführt.

## Belege und nächste Arbeit

Normaler Rechnergebrauch am 6. Oktober: leeres eigenes Journal → Standard gespeichert (Generation 1/Platz 0), Setup eingerichtet/gespeichert (2/1), neuer Prozess → Setup wiederhergestellt (2/1). Beide Aufrufe Exit 0 ohne stderr, aktive Exporte als JSON gleich, Profilrevision 6 und geänderte Setup-Pins/Bus/Adresse/LED-Zahl/Lautstärke erhalten. Lokal: `%TEMP%/larp-profile-authoring/journal-b10901f0/`, JSONL und editierbare JSON-Exporte. Fortsetzung mit Hinweisprofil → Generation 3/Platz 0, weiterer neuer Prozess lädt 3/0; aktuelle Exporte erneut gleich (Dokumentrevision 4), älteres Setup bleibt 2/1 (Revision 6). Alle vier normalen Aufrufe Exit 0 ohne stderr. Hüllendateien als Artefakte gelesen: 20.275/20.871 Byte. Keine Beschädigung, Abbruch-, Randfall-, Rollback-/Stromausfallsuite ausgeführt; keine neue Testsuite/Assertions, kein Upload oder Gerätecheck.

Alle vier Builds mit `pio run -e profile-tools -e az-delivery-devkit-v4 -e esp32-runtime-probe -e esp32-profile-runtime-probe -j2` am terminalen Ende erfolgreich (Exit 0), Log lokal `%TEMP%/larp-profile-journal-final-build.log`. Journal und Esp32ProfileStore sind in der neuen Probe verlinkt. Architekturprüfung 68 Dateien/163 direkte Includes, 45 unabhängige C++11-Header, Formatbaseline 115 Dateien/clang-format 19.1.7. Cppcheck 2.1 warning/performance/portability für Journal, Dateiadapter und Rechneranbindung ohne Befund; git diff --check sauber. Bestehende FastLED-/Frameworkwarnungen aus den Neubauten sichtbar. Kein Remote-CI-/Boardnachweis.

Nächster Schritt: tatsächlicher ESP32-Mount/Boot/Save-Rückfall nur mit konkreter Gerätefreigabe; hardwarefrei Produktionskoordinator und Companion-Status/Anfragen anbinden. App, reale Gerätebesitzer und Gesamtziel bleiben offen.
