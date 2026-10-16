# Integrationsvorbereitung · 16. Oktober 2026

**Status: vorbereitet; gemeinsame Codekonsolidierung und Prüfung offen.**
Auftrag: Integration vorbereiten und Backlog priorisieren, **noch nichts mergen**.
Quelle: Nutzerauftrag vom 16.10.2026, Codex-Sitzung
`01a1468c-3ad1-7840-9439-076cf817ca52`.

Die verbindliche Produktplanung steht in [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25),
die Konsolidierungsarbeit in [#53](https://github.com/Sven401/Larp-Escape/issues/53).
Dieser Bericht hält ausschließlich technische Abhängigkeiten, Befunde und Prüftore fest.

## Basis und unveränderte Grenzen

Zielbranch: `plan/projektuebergabe-kuratierter-stand` bei
`42610abfef07c971d9902ea6ca5a350ed68981bf`, nach frischem Fetch geprüft.
Die folgenden PRs sind offen und gegen diesen Stand gerichtet. Kein Merge,
kein Auto-Merge, kein Produktcodewechsel, Upload oder Restore in diesem Auftrag.
`main` und bestehende Arbeitsbranches bleiben unverändert.

## Geprüfte Spitzen und tatsächliche Vorfahren

| PR | Head (Kurzform; vollständige SHA im JSON) | Inhalt / enthaltene PR-Spitzen |
| --- | --- | --- |
| [#40](https://github.com/Sven401/Larp-Escape/pull/40) | `0a1e6be` | Maintainer-Einstieg, separat |
| [#43](https://github.com/Sven401/Larp-Escape/pull/43) | `5174b7b` | B0, separat; ersetzt geschlossene Alternative #42 |
| [#44](https://github.com/Sven401/Larp-Escape/pull/44) | `99b6b4b` | Komponentenentscheidung, separat |
| [#45](https://github.com/Sven401/Larp-Escape/pull/45) | `20518f5` | LED_ONLY |
| [#46](https://github.com/Sven401/Larp-Escape/pull/46) | `1040035` | Wake; enthält #45 |
| [#47](https://github.com/Sven401/Larp-Escape/pull/47) | `33bf31b` | NFC-Grundlage; enthält #45/#46 |
| [#48](https://github.com/Sven401/Larp-Escape/pull/48) | `d1c7077` | Audio-Grundlage; enthält #45/#46 |
| [#49](https://github.com/Sven401/Larp-Escape/pull/49) | `e4b2ce0` | Matrix; enthält #45/#46 und NFC-Code bis `c0f76ee` |
| [#50](https://github.com/Sven401/Larp-Escape/pull/50) | `724f0bd` | Matrix-Sammellauf; enthält #49 |
| [#51](https://github.com/Sven401/Larp-Escape/pull/51) | `cff0c99` | Audio-Bedienung; enthält #48 und #47 |
| [#52](https://github.com/Sven401/Larp-Escape/pull/52) | `14d7a0d` | App-/Box-NFC; enthält #47 |

Prüfweg: aktuelle PR-Metadaten und lokale Git-Vorfahrenprüfung.
#49/#50 enthalten nicht den abschließenden NFC-Dokumentationscommit `33bf31b`.
Die Matrixkorrektur #49 meldet historisch noch keine Installation; der spätere
Sammellauf in #50 ist der jüngere Gerätebeleg. Beide Aussagen gelten für ihren Prüfstand.

## Geplante Integrationsfolge

1. Grundlagen #40, #43, #44 berücksichtigen und zur späteren Übernahme bereitstellen.
2. Gemeinsamen Stamm #45 → #46 → #47 übernehmen, sobald ein Mergeauftrag vorliegt.
3. #52 als nächsten NFC-Stand vorsehen.
4. Audio aus #48/#51, anschließend Matrix aus #49/#50 gezielt konsolidieren.
   Gemeinsame Vorfahren nicht doppelt einspielen. Endstände und Zwischen-PRs
   bleiben bis zur belegten späteren Übernahme offen; Draftstatus nicht automatisch ändern.

Das ist eine Reihenfolge zur fachlichen Bearbeitung, keine direkt ausführbare
Mergekette: Die Endstände kollidieren. Kein bestehender PR wurde retargetet,
um die vorgeschriebene Integrationsbasis zu umgehen. Vor jeder späteren
Übernahme HEAD/Basis/CI erneut lesen; die Vorschau ist an obige SHAs gebunden.

## Gemessene Konflikte

55 paarweise Baumvorschauen: **47 ohne Textkonflikt, 8 mit Konflikten**.
[Vollständiger maschinenlesbarer Bericht](integration-preview-2026-10-16.json)
mit Heads, Merge-Basen, Exitcodes und allen Konfliktpfaden.

| Paar | Konfliktpfade |
| --- | ---: |
| #48 / #49 | 10 |
| #48 / #50 | 10 |
| #48 / #52 | 8 |
| #49 / #51 | 10 |
| #49 / #52 | 10 |
| #50 / #51 | 10 |
| #50 / #52 | 10 |
| #51 / #52 | 8 |

Methode: `git merge-tree --write-tree --name-only LEFT RIGHT`, Exit 0/1.
Nur unverknüpfte Baumobjekte erzeugt; keine Commits, Index-, Ref- oder
Arbeitsbaumzusammenführung. **Keine kumulative Vorschau und kein gemeinsamer Build.**
Konfliktfreiheit ist weder semantische Kompatibilität noch Produktfreigabe.

## Fachliche Auflösung vor einem gemeinsamen Code-PR

Ausgangsbelege und Erhaltungsprüfung: [Baugruppen-Nachweisregister](BAUGRUPPEN-NACHWEISE.md).
Die acht dort benannten Teilergebnisse bleiben Projektfortschritt, auch wenn
ein Prüfpfad eigenständig bleibt. Der spätere Code-PR muss für jede ID die
Übernahme, Erhaltung oder begründete Zurückstellung mit Quell-/Zielstand ausweisen.
Ein offener Gesamtumfang setzt einen bestandenen Baugruppentest nicht zurück.

**Funkvertrag:** Im Quellvergleich bestätigt: Audio und NFC nutzen beide
Auftrag `0x04` und Antwort `0x85`, aber verschiedene Größen und HMAC-Domänen.
Audio erweitert Status auf 78 Byte, Matrix auf 82 Byte; beide beginnen ihre
Erweiterung an Offset 38. NFC nutzt zusätzlich Fähigkeitsbit `0x20` in Byte 37.
Betroffene Quellen: `LedProtocol.kt`, `LedBleSession.kt`, `LedWire.cpp`,
`ProductMain.cpp` und `docs/led/fixtures/wire-v1.json` an den obigen Heads.

Die Konsolidierung muss ein eindeutiges Nachrichtenrouting und Statusschema
festlegen, Firmware und Android gemeinsam anpassen und gemeinsame Python-/C++-/Kotlin-
Referenzbytes erzeugen. Vorhandene App-/Firmwarepaare dürfen keine fremde
Nachricht als gültigen Auftrag ausführen. Neue Typen oder ein versioniertes
Schema sind erst im Code-PR konkret festzulegen; dieser Plan ändert keinen Vertrag.
Eine gemeinsame Requestfolge, Sitzungs-/Revisionsbindung, Duplikatregeln und
Fähigkeitsprüfung müssen über alle Baugruppen hinweg erhalten bleiben.

**UI und Daten:** Änderungen in `MainActivity.kt`, `LedUiState.kt` und
`LedBleSession.kt` gemeinsam auflösen. Matrix-Sammellauf, neun deaktivierte
Felder und vier Problemstellen erhalten; NFC-UID-Bindung und getrennte
Box-/Handylesung erhalten; Audio-Pause/Fortsetzen/Lautstärke und Stopfristen erhalten.

**Varianten und CI:** Beide expliziten Umgebungen `larp-nfc-test` und
`larp-matrix-test` in `firmware/led/platformio.ini` erhalten; Standardprodukt
startet keine der beiden Prüfungen automatisch. Keinen kombinierten NFC-/Matrix-
Prüfmodus stillschweigend einführen. Workflowänderungen zusammenführen:
B0 aus #43, Audio-Tests, Matrix-Tests und NFC-Tests dürfen nicht verlorengehen.
MCP-B7-Eingangsopt-in ist weiterhin keine elektrische Eignungsprüfung.

## Prüfung und spätere Abnahme

Am 16.10.2026 ausgelesene GitHub-Check-Runs für #43/#49/#50/#51/#52:
sämtliche zurückgegebenen Jobs erfolgreich; konkrete Joblinks und SHAs im
[CI-Ablesestand](integration-checks-2026-10-16.json). Das sind Einzelzweigbelege
gegen die bisherige Basis. Testanzahlen nicht addieren: Suiten überschneiden sich.

Vor einem späteren Merge des konsolidierten Codes:

- B0 einschließlich echter Kryptografie, konfigurierter Hosttests, ABI und
  Herkunfts-/Fehlerbelegen am tatsächlichen gemeinsamen Commit ausführen.
- Wire-/HMAC-Tests und Goldenbytes für alle Baugruppen; gezielte Negativfälle
  gegen Audio-/NFC-Fehlrouting, falsche Größen/Versionen, alte Sitzungen,
  konkurrierende Requestnummern und unbekannte Fähigkeiten.
- Ausgabe-/Adaptertests für LED, Wake, Matrix, Audio und NFC vollständig ausführen.
- Android Protokoll-/App-Tests, Debug-/Release-Builds und Lint prüfen;
  kein Verlust der gespeicherten Zugänge, Anordnungen oder Matrixlaufdateien.
- Legacy-Firmware und Proben sowie `larp-led-product`, `larp-nfc-test`,
  `larp-matrix-test` und NFC-Probe bauen. Ressourcen je Variante messen;
  93,8 % Flash der bisherigen Spielfirmware ist keine Reserve des Gesamtprodukts.
- Quellen, tatsächlichen Prüfcommit, Artefakthashes und Grenzen dokumentieren.
- Spätere nutzergeführte Fehlerfallabnahme gemäß #29/#33/#34/#35/#37 und
  gezielte Matrix-Nachprüfung [#54](https://github.com/Sven401/Larp-Escape/issues/54).
  Geräteupload bleibt ein gesonderter Auftrag mit chipgebundenem Rückweg.

## Aktuelle Gerätebelege und Grenzen

- LED #45: drei Effekte und Auszustände begrenzt sichtbar bestätigt;
  Wechsel, Stop/Neustart je Effekt und Fehlerfälle bleiben offen (#29/#37).
- Wake #46: Live-Anzeige/Anordnung beobachtet; Chipmarking/B7-Eignung offen (#39).
- Matrix #50: 68 Koordinaten/110 Betätigungen; neun unbestückt, vier Probleme.
  Reparatur/Nachprüfung in #54; keine vollständige Hardwareabnahme.
- Audio #51: Bedien- und mehrere Abbruchfälle nutzerbestätigt;
  natürliches Ende eines kurzen Titels ausdrücklich ausgenommen.
- NFC #52: ein vollständiger App-/Box-Rundlauf bestätigt; zweiter Lauf,
  falscher Tag, Funk-/Schreibabbrüche und weitere Tagfamilien offen (#35).

Letzter dokumentierter Gerätestand ist das NFC-Paar aus #52; Audio wurde nach
diesem Lauf nicht zurückgespielt. Das ist ein Berichtsbefund, keine aktuelle
USB-/Handyabfrage dieser Sitzung. Kein erneuter Gerätetest in diesem Auftrag.
