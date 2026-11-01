# Partitionslokale Generationreservierung v1

Stand: 01.11.2026, unabhängige Hostgegenprüfung und gemeinsame Regression
abgeschlossen. Neuer nativer SDK-/CI- und Geräte-Powerlossnachweis dieses
Folgestands ausdrücklich noch offen. Dieses Paket ergänzt
nur eine dauerhafte positive uint64-Reservierung. Es erzeugt keine Local-Eingabe,
Folge, Entprellung, Stations-/Devicezuordnung, Controlfreigabe oder Running-/GM-
Autorität. Der vollständige Originaleingangsbesitzer bleibt offen.

## Eigentümer und Aufruf

`Esp32LocalGeneration::instance()` ist der einzige Prozessbesitzer. Konstruktion,
Copy und Move sind nicht öffentlich. Sein erster Versuch wird vor externen
Aufrufen verriegelt; Erfolg und Fehler sind für diese Prozesslebensdauer final.
Neuer RulesProgressOwner, neuer Handle oder erneuter Aufruf starten keinen Retry.
Der gesamte Zugriff gehört in den exklusiven Startup-/Ownerworker, nicht ISR
oder konkurrierende Tasks. Das ist kein allgemeiner SMP-Integrationsnachweis.

Beide nativen `app_main` rufen die Reservierung unmittelbar nach erfolgreichem
`Esp32RulesBootClock::beginEarlyStartup()` auf: Startupentropie ist bereits aus.
Fehlt die Counterbasis, erscheint nur ein Hinweis zur fehlenden Localgeneration;
Queue-/Kommunikations-/Einrichtungsfähigkeit wird dadurch nicht pauschal beendet.
Ein Clock-Startfehler bleibt dagegen die bereits bestehende frühe Startsperre.

Die nativen Proben führen nun tatsächlich NVS-Initialisierung und bei vorhandenem
gültigem Counterrecord einen Counterwrite aus. Die frühere Annahme eines reinen
Clock-/Queue-Starts ohne Flashöffnung gilt dafür nicht mehr. Eine Uploadfreigabe
folgt daraus nicht. Die Progresspartition wird von diesen Startcalls nicht geöffnet.

## Fester Record

64 Bytes, keine Struct-Paddingdarstellung:

| Offset | Größe | Inhalt |
| --- | --- | --- |
| 0 | 8 | ASCII `LARPLG01` |
| 8 | 2 | Little-Endian Version 1 |
| 10 | 2 | Little-Endian Länge 64 |
| 12 | 4 | Nullreservierung |
| 16 | 8 | positiver Little-Endian uint64-Höchststand |
| 24 | 16 | vollständige ursprüngliche Clockboot128 |
| 40 | 20 | Nullreservierung |
| 60 | 4 | Little-Endian CRC32 über Bytes 0..59 |

Unbekannte Version, ungleiche Länge, nichtnull Reservierung, CRCfehler,
Generation0 oder Boot0 sind ungültig. Der Codec leert fehlerhafte Ausgaben.
CRC ist ausschließlich Korruptionserkennung, keine Authentifizierung oder
Anti-Rollbackgarantie. Der Record ist bewusst nur physisch partitionslokal;
kein gespeicherter Wert wird als tatsächliche Device-/D0-/Stationsidentität ausgegeben.

## Tatsächlicher NVS-Schreibvertrag

Bestehende Partition `larpkeys`, Adresse `0x310000`, Größe `0x6000`, 4096-Byte-
Löschseiten; exakte Daten/NVS-Partition, unverschlüsselt und nicht readonly.
Abweichung bleibt geschlossen. Keine Partitionsänderung oder Migration.
Ausschließlich Namespace `larp-local-v1`, Schlüssel `boot-v1`.

Der konkrete private Adapter besitzt mit dem Prozessbesitzer eine gemeinsame
statische Sperre für die gesamte Software-CAS-Folge:

1. Partition prüfen und explizit `nvs_flash_init_partition("larpkeys")` aufrufen.
2. Readonly öffnen, exakte Länge/Bytes lesen und gültige Counterbasis prüfen.
3. Bei Missing/Corrupt/UINT64_MAX schließen; keinen Counterrecord erzeugen.
4. Nachfolger `alt+1` mit aktuellem Clockboot einfrieren; bisherigen Handle schließen.
5. Readwrite öffnen und erwarteten vollständigen Record erneut vergleichen.
6. Exakt diesen Schlüssel per `nvs_set_blob` schreiben und `nvs_commit` bestätigen.
7. Writer schließen, neuen Readonly-Handle öffnen, vollständige bytes identisch lesen.
8. Erst danach kopierte Reservierung veröffentlichen und Sperre lösen.

NVS selbst besitzt kein natives compare_exchange; die gemeinsame exklusive
Sperre ist notwendig. Ab dem ersten `nvs_set_blob`-Aufruf bedeuten Fehler immer
`Uncertain`, auch bei möglichem vollständigem Write. Diese Session bleibt
blockiert; es gibt keinen Deinit-/Reopen-/Erasuretry im Wrapper. `nvs_close`
ist eine void-API und liefert keinen separat überprüfbaren Durabilitätsbeleg.
Readback über einen neuen Handle ist nur Software-Readback, kein Kaltstart.
`Unavailable` bedeutet ebenfalls nicht pauschal NoWrite: etwa wenn nach bereits
bestätigter Reservierung die Clockdomäne ausfällt, wird die Nummer nicht exponiert.

Ein neuer tatsächlicher Prozess reserviert immer den geladenen Höchststand+1,
auch bei zufällig identischen Boot128-Bytes. Kein Gleichheitsshortcut verbindet
zwei Prozesslebensdauern. Ein ungenutzter reservierter Wert darf verloren gehen;
er wird nicht zurückgegeben. Es gibt keinen Erstinitialisierer und keine
Fallbackbasis0/1. Fehlende Basis braucht den gesondert qualifizierten Einrichtungsweg.

## Gemeinsame Partition und Qualifikationsgrenzen

`larpkeys` enthält bereits `larp-led/access-v1`. Unser Wrapper nennt, liest oder
verändert diesen Schlüssel nicht. Namespaceisolation ist trotzdem keine physische
Fehlerisolation: NVSinit/GC/Recovery können intern gemeinsame Seiten umschreiben.
Es gibt kein `nvs_flash_erase`, keine automatische Initialisierung des fehlenden
Counterrecords und keine Reparatur-/Migrationsfunktion im neuen Adapter.

Der neue SDK-Komponentenkatalog kompiliert den tatsächlichen Backendpfad für
beide nativen Varianten. Hosttests verwenden nur explizite NVS-/SDK-Stubs und
vorbesetzte Records. Eigener strenger C++11/O2/Werror-Lauf: 16 Prozesse, 1424
Assertions; [Fälle und Reproduktion](../../test/fixtures/local-generation-own/README.md).
Der unabhängige Hostkorpus prüft tatsächlich dieselben sechs Produktquellen:
561 feste 64-Byte-Orakel, 68 frische Prozesse und 7152 Assertions. Enthalten sind
unter anderem alle SDK-Fehlerpunkte, mögliche Set-/Commitwirkungen, Reentry,
Clockfehler nach Write und mehrere Prozesse mit unveränderten Bootbytes bei
fortgeschriebenem simuliertem Medium. [Unabhängige Fälle und Reproduktion](../../test/fixtures/local-generation-review/README.md).
Die gemeinsame CMake-/nativeCTest-Verdrahtung verwendet denselben vollständigen
Runner ohne Kurzmodus. Der tatsächliche native SDK-/Folgestand bleibt separat
offen bis zum qualifizierten CI-Lauf.

Offen bleiben echte Stromausfall-/Verschleiß-/Kapazitäts-/NVS-RAMqualifikation,
Erstbereitstellung, tatsächliche Original-Control-/Inputbindung und geprüfte
Firmwarewechsel. Vollständiger alter Flashsnapshot kann auch den Counter
zurückrollen. Speicherverlust, unklare Basis oder ungeprüfter Firmware-Rückweg
öffnen keine automatische Fortsetzung desselben event/run. Die D0-Recoveryregel
für neue Ursprungsepoche ersetzt die fehlende Epoche im P0-Localschlüssel nicht.
Kein Geräteupload, Merge, Release oder Abschluss von #93 wird damit behauptet.

## Abgeschlossener Hostprüfstand dieses Folgestands

- Eigener tatsächlicher Backend-/Singletonlauf: 16 Prozesse, 1424 Assertions.
- Unabhängige Qualitykampagne (`quality_p1`): 561 feste 64-Byte-Orakel,
  68 wirklich frische Prozesse, 7152 Assertions; tatsächliche sechs
  Produktquellen vor/nach dem Lauf unverändert.
- Gemeinsamer kompletter CMake-Build und CTest: 46/46, 61,37 Sekunden.
- PlatformIO `host-regression`: 263/263 Testfälle, 109,855 Sekunden.
- Architekturguard: 192 Dateien / 420 direkte Includes.
- Standaloneheader: 110 Coreheader, zusätzlich beide neuen SDK-Header separat
  als einziger Include unter strengem C++11/Wpedantic/Werror.
- Formatbaseline: 382 Dateien mit clang-format19.1.7 sowie neue SDK-/Fixturedateien
  explizit; diff--check sauber. Neue Dokumentation strikt UTF8 ohne BOM.

Gezielte unabhängige Source-/SDK-/Lifecycle-Sicherheitsprüfung und abschließende
Wiring-/Dokugegenlese: `module_digest_review`, ohne bestätigten Sourcebefund.
Maintainer-Wiringprüfung: Root. Produktquellen blieben während der finalen
Hostläufe auf demselben gemeldeten Freeze; es gab keine nachträgliche
Funktionsänderung und keinen Gerätezugriff. Beide SDK-Varianten werden erst im
folgenden tatsächlichen CI-Lauf gegen die qualifizierte SDK-Version geprüft;
frühere grüne CI-Stände oder die SDK-Stubs qualifizieren diesen neuen Pfad nicht.
