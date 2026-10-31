# D1-Speicherkern: Umsetzung und Nachweise

Stand: **31.10.2026, Europe/Berlin**. Sitzung
`01a192db-a6ca-7fd3-a7eb-b4f4fdcd8126`. Auftrag: eigener geprüfter PR aus #60,
Firmware-Speicherteil gegen bestehenden D1-Vertrag, ohne Gerätezugriff oder Merge.
[Implementierter Speichervertrag und Anschlussstellen](../docs/veranstaltung/d1-firmware-speicherkern.md).

## Grundlage und Zuständigkeit

**Ausgelesen:** #25/#60, D0/D1, integrierte D1-A-Rechnerreferenz und geltende
Maintainer-/Rollenregeln am 31.10.2026. Arbeitsbeginn auf `8345d65a62a19b4c5236cd5b07207c3b50391676`.
Die parallele Reviewarbeit hat #117 → #121 → #122 integriert. D1 übernimmt #122
konfliktfrei im eigenen Branch; geprüfter Codecommit
`e0b3e5efb0d38d51dc8e98d20cde93857c140a12`, Eltern eigener Umsetzung
`ba7435c` und Zielbasis `b8472542230c5834d9664e98a89eb46c1cba6e57`.

Eigener verwalteter Worktree `d1-firmware-speicherkern`, Branch
`codex/d1-firmware-speicherkern`. Hauptcheckout, fremde lokale Änderungen und
parallele Modul-/Planerarbeiten unverändert. Kein Geräte-, COM-, Funk- oder
Flashzugriff. Keine Issue-Schließung und kein Mergeauftrag.

Federführung/Plattform und Adapter: `/root`; Kernumsetzung: `/root/d1_core`;
Kernprüfungen: `/root/d1_tests`. Unabhängiger Vertrags-/Speicherreview:
`/root/d1_contract_review`; zweite Gegenprüfung `/root/d1_final_review`.
Nach deren Korrekturen neue unabhängige Abschlussprüfung durch
`/root/d1_release_review`. Eigene Testimplementierung gilt nicht als unabhängige
Freigabe. Varianten: hardwareneutraler C++11-Kern, native ESP-IDF-6.1-Prüffirmware
mit echtem Partitionsadapter, synthetischer SDK-Adapterlauf, bestehende Legacy-
Host-/Firmwarequellen. Keine neue Plattform-/SDK-Version oder Partitionstabelle.

## Ergebnis und korrigierte Befunde

**Implementiert:** 256-KiB-Transaktionsablage mit 63 unveränderlichen Records,
3072 Nutzbytes je Record, maximal acht gemeinsame Fact-Vergaben, fester
Ursprungsbindung, Vorgangs-Replay und unverändertem Zähler bei Wiederholung.
Expliziter Leermedienaufbau, getrennte Beleganker, vollständiger Wiederanlaufscan,
Instanzsperre bei unklarem Schreiben; keine Löschung, Kompaktierung oder stiller
Rückfall auf ältere bestätigte Fortschritte. Konkreter nativer ESP-Port wird in
der Prüffirmware eingebunden, beim Start aber nicht geöffnet.

**Korrektur geprüft:** Zwei gezielte Gegenproben zeigten zunächst einen Fehler:

1. Eine bereits bestätigte Instanz konnte ein vollständig verschwundenes Medium
   als leer ansehen und neu initialisieren. Der erste Fix sperrte bekannten
   Leerverlust; die dauerhafte Regression schlug zuvor fehl und bestand danach.
2. Der andere Reviewer reproduzierte den Umweg über `recover(wrongOrigin)`:
   Der Fehlversuch verlor den bekannten RAM-Prüfstand; danach waren Leererkennung
   und Neuinitialisierung möglich. Auch `initialize` auf bestehendem Medium
   konnte diesen Prüfstand verlieren. Der korrigierte Kern erhält Ursprung,
   Revision, Vergabestand und Kettenende über fehlgeschlagene Öffnungen hinweg,
   unabhängig von `ready`. Beide Folgefälle sind dauerhaft geprüft und dürfen
   weder erneut schreiben noch Nummern vergeben.

Vorläufige Testlaufprobleme wurden vor Abschluss korrigiert: Unity im
PlatformIO-Hostpfad hat kein aktiviertes uint64-Assertionformat; die Tests
verwenden deshalb einen direkten uint64-Wertvergleich statt des Unity64-Formats. Alle 32-Byte-Digestwerte einschließlich
Null sind zulässig; nur Identitäten müssen nichtnull sein. Veraltete Testannahmen
wurden berichtigt. Frühere rote Zwischenläufe sind keine grünen Endnachweise.

## Gemessene Softwareprüfungen

- Kernsuite: 23 Fälle, darunter 8193 Abbrüche an jeder programmierten Bytegrenze
  (4096 Header, 4097 Beleg/Record), 64 Lesefehlerpositionen und fünf
  Synchronisierungsfehler. Wiederanlauf öffnet dieselben modellierten NOR-Bytes.
- Konkreter Adapter im synthetischen SDK: fehlende/falsche/verschlüsselte/
  schreibgeschützte Partition, Bounds, Erased-only, Teilwrite, fehlerhaftes
  Rücklesen, zwei vollständige Journalcommits, Replay und beide obigen
  Wiederanlaufregressionen. Die Stub-API enthält absichtlich keine Löschfunktion.
- D1-A unverändert: 45 Tests, 36 tatsächliche Kindprozessabbrüche und Wiederöffnung
  derselben SQLite-Dateien. D0: 111 semantische Vertragsfälle. Beides erfolgreich;
  keine kryptografische Verifizierung oder physische Stromausfallgarantie.
- Architekturguard: 153 Dateien/320 direkte Includes. Headerprüfung: 90 getrennte
  C++11-Header. Formatbaseline: 343 Dateien, clang-format 19.1.7. Zusätzliche neue
  Adapter-/Testdateien separat mit demselben Formatter geprüft.

Reproduzierbare neue Prüfeingänge:

```powershell
python -m platformio test -e host-regression -v
cmake -S . -B private/host-build
cmake --build private/host-build
ctest --test-dir private/host-build --output-on-failure
```

Der lokale CMake-Lauf verwendet MinGW 5.1 und die vorhandene gepinnte Unity2.6.1
mit `MinGW Makefiles`; native Linux-/SDK-Qualifikation bleibt ein eigener
CI-Nachweis. Der vollständige lokale CMake-Endlauf auf `e0b3e5e` bestand alle acht Testsuiten.
Unabhängiger Abschlussreview `/root/d1_release_review`: keine offenen P1/P2;
eigener CTest-Lauf mit beiden D1-Suiten, 23 Kernfällen und 1112 Adapterassertions
erfolgreich. Die vollständige PlatformIO-Endregression auf demselben Codecommit bestand
16 Suiten mit insgesamt 173 Fällen; Laufdauer 4 Minuten 47 Sekunden. Die PR-CI gilt jeweils
nur für ihren zurückgelesenen Head/Basis/Prüfmerge, nicht automatisch für einen
späteren Integrationsstand.

**Gemessen, MinGW32:** `sizeof(ProgressJournal)=5192`, Request104, Record120,
Adapter12 Byte. Im Kern enthalten: 4096-Byte-Arbeitspuffer und 63 feste
Vorgangskennungen; keine Heapallokation. Adapter verwendet zusätzlich einen
128-Byte-Lesepuffer auf dem Stack. Diese Objektgrößen sind keine ESP32-Stack-,
Queue- oder Gesamtspeicherabnahme. [Logische I/O-Obergrenzen](../docs/veranstaltung/d1-firmware-speicherkern.md#begründete-softwarebudgets)
begrenzen den Umfang; reale Latenz, Schreibverstärkung und Verschleiß bleiben offen.
UINT64-Maximum ist mit63 Records/acht Facts regulär unerreichbar (maximal504);
geprüft sind gefälschte Zählerketten und unpassende Maximal-Erwartungswerte,
kein tatsächlicher Durchlauf bis2^64−1.

## Offene Integration und Geräteabnahme

D1-B2 ist damit nur als begrenzter Speicherbaustein bearbeitet. D0-Verifizierer,
kanonischer Transaktionscodec, semantischer Fact-/Stream-/Latch-Reducer,
vollständiger Mehrregelplan, intertask Rechte-/Profilfence und F5a-Gerätebesitzer
bleiben beim jeweiligen Folgeauftrag. Der parallel gestartete Mehrregelplaner
wird hier weder kopiert noch vorweggenommen. Wiederholte gespeicherte Vorgänge
sind geprüft; vollständiger D0-Ereigniskonflikt-/Replay-/Kompaktierungsschutz
ist **kein** Ergebnis dieses Primitives.

Produktpartition/Layoutmigration, echte Neustarts und Stromunterbrechungen,
Verschleiß, Speicherlast, Geräteeffekte und zeitliche Budgets bleiben offen.
Erfolgreiche Host-/SDK-Builds ersetzen diese Abnahmen nicht. Ein zurückgespieltes
vollständiges altes Flashabbild bleibt ohne monotonen Anker die D0-Rollbackgrenze.
#60 bleibt offen; dieser PR darf das Gesamtissue nicht automatisch schließen.
