# Gemeinsamer Baugruppenstand · 17. Oktober 2026

> **Nachtrag · 18. Oktober 2026:** PR #56 ist bei `784984d` in den
> kuratierten Zielbranch integriert; alle acht Push-CI-Jobs am Mergecommit
> sind erfolgreich. [Aktueller Einstieg](MAINTAINER.md) und
> [Hygienebericht](REPOSITORY-HYGIENE-2026-10-18.md).
> Der folgende Bericht bewahrt den damaligen Vorbereitungs-/Prüfstand;
> „Merge offen“ und „CI läuft“ sind historische Aussagen. Die gemeinsame
> Geräteabnahme bleibt offen.

**Status: implementiert und lokal vollständig softwaregeprüft; CI am
veröffentlichten PR-Stand läuft. Keine neue Geräteabnahme.**
Auftrag: Arbeitsplan aus der Zieldatei bis zum gemeinsam geprüften
Konsolidierungs-PR umsetzen, keine Merges oder Geräteuploads.
Codex-Sitzung: `01a146a9-aa36-71b0-9ea3-bddac4d2cab8`.
Produktplanung: [#53](https://github.com/Sven401/Larp-Escape/issues/53),
Vorbereitung: [#55](https://github.com/Sven401/Larp-Escape/pull/55).

## Basis und Herkunft

Ziel ist unverändert `plan/projektuebergabe-kuratierter-stand` bei
`42610abfef07c971d9902ea6ca5a350ed68981bf`. Arbeitsbranch
`codex/baugruppen-konsolidierung`, eigener Worktree. Hauptcheckout und seine
fremde Änderung an `.vscode/extensions.json` bleiben erhalten.
Alle übernommenen PRs bleiben offen und unverändert; kein Retargeting,
kein Mergecommit, kein Auto-Merge und keine Integration nach `main`.

| Quelle | Geprüfter Quellcommit | Übernahme |
| --- | --- | --- |
| Maintainer #40 | `0a1e6be68bf8e70f5a5184b3f6b3de1f94744b23` | Cherry-pick `d845524`; Einstieg/Mandat |
| B0 #43 | `5174b7bbac143d1359d15103c696c77ae47fc877` | Drei Commits als `4182841`, `9baa601`, `6741467`; vollständige Prüfgrundlage erhalten |
| LED-Entscheidung #44 | `99b6b4b33b3b7794e5b3b58d924d3bdb260ace9b` | Zwei Commits `d070e07`, `ab4fe10`; WLED nur externe Funktionsreferenz |
| NFC-Endstand #52 | `14d7a0dae4ae0e697ff609bb0e86996e9864a62d` | Ausgangspunkt des Arbeitsbranches; enthält #45/#46/#47 |
| Audio-Endstand #51 | `cff0c998cde05715c2117c17ecf42681b69d1ad4` | Delta gegen gemeinsamen Vorfahren `33bf31b`; enthält #48, #47 bereits im NFC-Stamm |
| Matrix-Endstand #50 | `724f0bdabed2bd51c170942f3e45bc2cecc34932` | Delta gegen `c0f76ee`; enthält #49 und früheren NFC-Code, nicht erneut eingespielt |
| Plan/Nachweise #55 | `fba195fda9bf96f17414ee7dfdb8ce355349521d` | `fe18aa6`, `b029647`; historische Quellen und Sicherungsreferenzen erhalten |

GitHub-Metadaten, frischer Fetch und lokale Vorfahren/Diffs wurden am
16./17. Oktober abgeglichen. Es gab gegenüber der Planaufnahme keine neue
fachliche PR-Spitze. Vollständige ursprüngliche Liste und Konfliktvorschau:
[Integrationsvorbereitung](INTEGRATION-2026-10-16.md).

## Konfliktentscheidungen

- **Funkvertrag:** [gemeinsame Version 2](../docs/led/ble-wire-v2.md).
  Audio `0x04`/`0x85`, NFC `0x05`/`0x86`; keine Längenheuristik.
  Immer 91 Byte Gesamtstatus: Wake/NFC-Fähigkeit, explizites Audioflag,
  eigene Audio- und Matrixfelder. Version/HMAC-Domänen gemeinsam geändert.
  Historische V1-Paare werden vor Bedienung abgewiesen.
- **Auftragsbesitz:** LED/Audio/NFC teilen eine Requestfolge, einschließlich
  NFC-Reservierung, Busy, Abschluss und Duplikatbehandlung. Keine überlappende
  Besitzerübergabe. NFC-Ergebnis besitzt weiter eine eigene Sitzungssequenz;
  daraus entstehen keine frischen Eingangs-/Audio-Beobachtungen.
- **Android-Zusammenführung:** `MainActivity`, `LedUiState`, `LedBleSession`
  und `LedProtocol` behalten Audio, Matrix und NFC gemeinsam. Audioresultat
  und NFCresultat haben eindeutige Decoder. Audio ist auch während NFC
  gegen neue Aufträge gesperrt. Nach NFC-Abschluss verwendet die App die
  gemeinsame nächste Requestnummer; kein automatisches Wiedersenden.
- **Datenerhalt:** Kein Wechsel von App-ID, Zugangs-/NVS-Schema,
  Wake-Anordnung, Matrixlauf-Dateiformat oder Speicherpfaden.
  Matrixklassifikation, Altdateilesung und NFC-Erstschreibkorrektur bleiben
  erhalten. Hosttests prüfen Dateiformate; es fand keine Installation und
  keine Prüfung der aktuell privaten Gerätedateien statt.
- **Firmware:** Ein LED-, Audio- und Wake-Besitzer im gemeinsamen Loop.
  NFC-Worker nur `larp-nfc-test`; Matrixscan nur `larp-matrix-test`, mit
  bestehendem historischem IO7-Opt-in. Gleichzeitige NFC-/Matrix-Testmakros
  sind verboten. Kein neuer kombinierter Hardwaremodus.
- **CI:** B0 bleibt erhalten, Android prüft zusätzlich Release-Build.
  Baugruppen-CI enthält Wire, LED/Wake/Matrix, Audio und NFC gemeinsam,
  alle drei Varianten sowie Quellen-/Exitcode-/Fehlerbelege. NFC-Probe
  bleibt unabhängig erhalten. Ergebnisse gleicher überlappender Suiten
  werden nicht addiert.

Diese Zusammenführung bringt die manuellen Baugruppenfunktionen in einen
gemeinsamen Softwarestand. Profilabhängige reale Spielausführung und
allgemeiner Rollen-/Bondzugang bleiben #27/#28/F5a/#30/F5b.

## Erhaltungsnachweis je Ergebnis-ID

Ziel aller Softwareübernahmen ist Implementierungscommit
`59e7311e4abc5ea81156a6b2631a744229e404ef` in [PR #56](https://github.com/Sven401/Larp-Escape/pull/56). Originale und
Sicherungs-Tags: [Nachweisregister](BAUGRUPPEN-NACHWEISE.md).

| ID / Quelle | Behandlung im gemeinsamen Stand | Neu nötiger Nachweis |
| --- | --- | --- |
| LED-01 / #45 `20518f5` | **übernommen:** eigene Effekte, Start/Stop/Effektende/Disconnect, Auswahl löst nichts aus; Ausgabe- und Wiretests | V2-Paarung und berührte LED-Funk-/Fehlerfälle #29/#37; keine alten Sichtbelege pauschal wiederholen |
| WAKE-01 / #46 `1040035` | **übernommen:** vier Rohpegel, Anordnung/Swap/Speicherung, eigene Aktualitätsfrist; Wake-/I2C-/App-Tests | Livewerte über V2; Chipmarking/B7 bleiben #39 |
| NFC-01 / #47 `33bf31b` | **als eigenständiger Prüfpfad erhalten:** USB-/HCE-Probe und PN532-Transaktionen unverändert | Kein neuer HCE-Pflichtlauf allein wegen BLE-Versionierung |
| NFC-02 / #47 `33bf31b` | **übernommen und unabhängige USB-Probe erhalten:** Tagfixture/UID-/Payload- und Rücklesevertrag | Gemeinsamer App-/Boxpfad unter V2 gezielt prüfen |
| NFC-03 / #47 `33bf31b` | **übernommen:** Erstbefüllung ohne Schreibretry, frischer neuer Lesekontakt, vollständiger UID-/Nachhervergleich | Kein behaupteter neuer Handy-/Taglauf; bestehende ReaderMode-Regressionen prüfen |
| MATRIX-01 / #50 `724f0bd` | **übernommen:** gespeicherter Sammellauf, Treffer/Zähler/Zeiten, neun unbestückte und vier Problemfelder getrennt; Codec-/Altdatei-/Klassifikationstests | V2-Status und C3/G3/H3/A8 in erhaltenem Lauf nach #54; kein Löschen/neuer Gesamtrundgang |
| AUDIO-01 / #51 `cff0c99` | **übernommen:** Play/Pause/Resume/Volume, 30-s-Grenze, Stop/Disconnect/App-Verlassen und kein Autostart; UART-/Besitzer-/App-Tests | Berührte Bedienung über V2; R3 kurzes natürliches Ende bleibt ausdrücklich ausgenommen |
| NFC-04 / #52 `14d7a0d` | **übernommen:** App-/Box-Rundlauf, einmaliger bestätigter Schreibauftrag, UID-Bindung, frische reine Abschlusslesung | V2 und gemeinsame Requestfolge; zweiter Lauf, falscher Tag/Schreibabbrüche #35 |

Keine Ergebnis-ID wurde fachlich verworfen oder als verloren behandelt.
Historische Bestätigung und neue gemeinsame Geräteabnahme bleiben getrennt.

## Softwareprüfung

**Gemessen am gemeinsamen Quellenstand von Implementierungscommit
`59e7311e4abc5ea81156a6b2631a744229e404ef`.** Die Prüfungen liefen während
der Vorbereitung; anschließend stimmten alle versionierten Arbeitsdateien mit
diesem Commit überein (`git diff --exit-code`). Die CI prüft den veröffentlichten
Checkout gesondert. [Maschinenlesbarer Prüfstand samt Ressourcen, Artefakthashes
und unveränderten Besitzer-/Datenquellen](konsolidierung-pruefstand-2026-10-17.json).

- B0 Host: 105 Regressionen, 509 konfigurierte Steuerungsfälle; separate
  echte Mbed-TLS-Suite 519 Fälle. Alle bestanden, Suiten überlappen.
- 221 offizielle Kryptografiedateien, acht native und zwei SDK-Negativfälle;
  Originaleingaben unverändert. 20 CI-Helfer, sieben Architekturfixtures,
  Architektur 145 Dateien/302 Includes, 86 unabhängige C++11-Header und
  Formatbaseline 309 Dateien mit clang-format 19.1.7 bestanden.
- 7347 native Wire-/HMAC-Assertions, gemischte Folge LED → Audio → NFC → LED,
  Überlappung/alte Antworten/Fehlrouting/Versionsbruch; Python-Goldenbytes
  gegen C++ und Kotlin. 16 LED-/vier SDK-Ausgabefälle, Wake-/Matrix-/I2C-Tests,
  elf Audio-/fünf UART-Fälle und NFC-Transport/86 Transaktionen bestanden.
- Android: 83 Core- und 72 App-JVM-Fälle, null Fehler/Skips; Debug- und
  Release-APK gebaut. Debug-Lint null Fehler, drei bestehende Warnungen
  (`OldTargetApi`, `DataExtractionRules`, `MissingApplicationIcon`) und
  eine Information (`AutoboxingStateCreation`). Release ist kein freigegebenes,
  signiertes Verteilungspaket.
- `larp-led-product`, `larp-nfc-test`, `larp-matrix-test` und `nfc-probe`
  sowie Spielprodukt, Runtime-/Profil-Runtime-/GATTS-ABI-Probe gebaut:
  alle acht ESP32-Ziele erfolgreich. Profilwerkzeug ebenfalls gebaut.

Die anfänglichen Androidfehler betrafen eine alte Fragmentanzahl (nun acht
bei 91 Byte) und einen positionsgebundenen Testkonstruktor nach Zusammenführung;
beide behoben und vollständigen Android-Aufruf erfolgreich wiederholt.
Temporärdatei-/Compilerfehler der lokalen Windows-Sandbox wurden durch
Ausführung mit den vorhandenen lokalen Werkzeugen außerhalb der Sandbox
behoben; sie zählen nicht als bestandene Produktprüfung.

## Ressourcen und Quellenbeleg

Statische Linkerwerte der gemeinsamen lokalen Builds; kein gemessener Heap,
Stack oder Energieverbrauch. RAM-Kapazität je ESP32-Ziel 532.480 Byte.

| Variante | RAM Byte | Flash Byte / Anwendungskapazität |
| --- | ---: | ---: |
| Spielprodukt `az-delivery-devkit-v4` | 55.148 | 1.229.077 / 1.310.720 |
| `esp32-runtime-probe` | 21.480 | 291.881 / 1.310.720 |
| `esp32-profile-runtime-probe` | 107.560 | 407.077 / 1.310.720 |
| `esp32-gatts-abi-probe` | 50.920 | 1.092.337 / 1.310.720 |
| `larp-led-product` | 42.504 | 1.134.909 / 3.145.728 |
| `larp-nfc-test` | 47.264 | 1.146.033 / 3.145.728 |
| `larp-matrix-test` | 42.552 | 1.137.029 / 3.145.728 |
| `nfc-probe` | 25.172 | 239.401 / 3.145.728 |

Der Spielbuild bleibt bei 93,8 % seiner kleineren Anwendungspartition.
Die Baugruppenpartition ist unverändert 3 MiB; deren Reserve ist keine
Ressourcenzusage für den späteren Gesamtspielbuild.

**Belegt:** sechs historische Quellbäume und 43 Dokumentblobs samt Größen,
Git-Blob-IDs und SHA-256 erneut gegen das Manifest geprüft. Matrix-Laufdaten,
Speichercodec/-adapter, NFC-Probe und Android-NFC-Pfad, LED-Ausgabebesitzer,
Audio-Ausgabe/UART und Wake-Anordnungsmodell stimmen bytegleich mit den
jeweils benannten Originalständen überein (Pfadliste im JSON).
Nur Transport-/Sitzungs-/Statusintegration ist gemeinsam neu zu beurteilen.
APKs und acht Firmware-Binärdateien sind lokal per Größe/SHA-256 zugeordnet;
Hashangaben sind keine Veröffentlichung oder Sicherung dieser Binärdateien.

## Restabnahme und nächster Schritt

**Offen:** gemeinsames V2-App-/Firmwarepaar am Gerät, echtes Funkverhalten,
physische Wirkung, dynamischer Heap/Stack/Queues/Latenz/Energie und Dauerbetrieb.
Build-RAM ist statischer Bedarf, kein Laufzeitverbrauch. Der alte Spielbuild
und die manuellen Baugruppenvarianten sind getrennte Firmwareziele.

Nach gesondertem Auftrag tatsächliches installiertes Paar und chipgebundenen
Flash-Rückweg abgleichen. Dann Matrix #54 im erhaltenen Lauf, LED #29/#37,
NFC #35 und Status/Sitzung #33/#34 gezielt prüfen. Originalberichte bleiben
maßgeblich für bisherige Beobachtungen. Diese Sitzung verbindet weder Handy
noch Box, schreibt keinen Flash und erteilt keine Produktfreigabe.

Backlog: #33 enthält getrennte Labor-/Baugruppenfristen; #31 eigenständige
F5a-Kriterien vor #30 und getrennte F5b-Betriebsabnahme. Keine Fertigstellungstermine
erfunden. Merge-Entscheidung bleibt beim Nutzer.
