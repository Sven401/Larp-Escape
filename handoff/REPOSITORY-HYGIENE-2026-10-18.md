# Repository-Hygiene · 18. Oktober 2026

**Status: umgesetzt; Dokumentations-/Ignore-Korrekturen zur PR-Prüfung.**
Quelle: beauftragte Maintainer-Hygiene in Sitzung
`01a14c15-6145-7130-83e4-c9f22c8ae3ad`.
Geprüfte Zielbasis: `784984da05249c77525273c658405d2f3b8e8e24`,
frisch gegen Remote abgeglichen. [Maschinenlesbarer Ablesestand](repository-hygiene-2026-10-18.json).

## Übernahme und geschlossene Herkunfts-PRs

**Belegt:** PR #56 ist gemergt; seine Spitze `c03c490` ist Vorfahr des
Zielcommits, beide besitzen denselben Git-Baum. Herkunft und fachliche
Konfliktentscheidungen stehen im [Konsolidierungsbericht](KONSOLIDIERUNG-2026-10-17.md).
Alle acht PR-Heads vor Schließung erneut mit GitHub und den tatsächlichen
Remote-Branchspitzen abgeglichen. Keine neue PR-Spitze vorhanden.

| PR | Geprüfter Head | Pfade unverändert / weiterentwickelt / fehlend | Abschluss |
| --- | --- | ---: | --- |
| #40 | `0a1e6be` | 2 / 1 / 0 | geschlossen, durch #56 ersetzt |
| #43 | `5174b7b` | 6 / 3 / 0 | geschlossen, durch #56 ersetzt |
| #44 | `99b6b4b` | 2 / 2 / 0 | geschlossen, durch #56 ersetzt |
| #48 | `d1c7077` | 56 / 40 / 0 | geschlossen, durch #56 ersetzt |
| #49 | `e4b2ce0` | 66 / 33 / 0 | geschlossen, durch #56 ersetzt |
| #50 | `724f0bd` | 79 / 30 / 0 | geschlossen, durch #56 ersetzt |
| #51 | `cff0c99` | 74 / 33 / 0 | geschlossen, durch #56 ersetzt |
| #55 | `fba195f` | 5 / 1 / 0 | geschlossen, durch #56 ersetzt |

Methode: geänderte Pfade jedes Ausgangs-PRs gegenüber `42610abf` gegen den
integrierten Zielbaum prüfen. Gleiche Blobs sind bytegleich; unterschiedliche
Blobs sind allein kein Übernahmebeleg. Für sie wurden Herkunft/Deltas und die
V2-Konfliktentscheidungen aus #56 herangezogen. #48 ist in #51, #49 in #50
enthalten. Keine Quelle verschwindet durch das Schließen eines PRs.

Zusätzlich bytegleich im Zielstand: Mandat/Maintainer-Einstieg aus #40,
B0-Helfer und Tests aus #43, LED-Komponentenentscheidung aus #44,
Matrix-Scanner/Codec/Speicheradapter aus #50 sowie Audio-Ausgabe/UART aus #51.
Gemeinsames Routing, Androidsitzung und Wire-Fixtures wurden durch V2 ersetzt.
Jeder PR erhielt eine eigene Abschlussbegründung und den Link auf #56;
kein erneuter Merge, Retargeting oder Branchlöschen.

**Issues:** #25 auf den integrierten Softwarestand und die nächste Reihenfolge
abgeglichen. #53 als abgeschlossene Softwarekonsolidierung geschlossen.
#38 ebenfalls abgeschlossen: seine ausdrücklich offene Bedingung „belegte
Übernahme“ ist mit #56 und dessen Ziel-CI erfüllt. Historische Issueinhalte
bleiben als datierte Vorgängerfassungen erhalten. Geräte- und Gesamtprodukt-
aufgaben wurden nicht aufgrund begrenzter Softwarebelege geschlossen.

## Bestandsaufnahme und Vormerkungen

**Gemessen vor den Hygiene-Edits:** 15 registrierte Worktrees einschließlich
dieser Arbeitskopie, 16 lokale Branches, 25 tatsächliche Remote-Branches.
Die eigene neue Arbeitsbranch war zu diesem Zeitpunkt noch nicht gepusht.
Vollständige Branchnamen und Commitspitzen stehen im JSON; keine persönlichen
absoluten Klonpfade veröffentlicht.

Die ursprüngliche Fetch-Konfiguration aktualisierte nur Ziel- und
Maintainerbranch. Darum zusätzlich alle Remote-Branchspitzen explizit geholt;
keine Änderung der gespeicherten Fetch-Konfiguration. Erst danach die Anzahl
von Commits bestimmt, die kein Remote-Branch erreicht. Ein Cherry-pick oder
Tag allein wird dabei nicht mit identischer Remote-Branchhistorie verwechselt.

| Worktree-Branch | Versioniert geändert / ungetrackt | Nur lokal erreichbare Commits gegenüber Remote-Branches | Ignorierte Dateien | Vormerkung |
| --- | ---: | ---: | ---: | --- |
| `codex/maintainer-uebernahme` | 1 / 0 | 0 | 4852 | behalten: fremde Änderung |
| `codex/audio-controls-completion` | 0 / 0 | 0 | 1537 | Entfernungskandidat nach Artefaktprüfung |
| `codex/audio-test-alternative-local` | 0 / 0 | 2 | 1684 | behalten: unveröffentlichter Entwurf |
| `codex/baugruppen-konsolidierung` | 0 / 0 | 0 | 9091 | Entfernungskandidat nach Artefaktprüfung |
| `codex/integration-vorbereitung` | 0 / 0 | 0 | 0 | entbehrlicher Checkout vorgemerkt; Branch erhalten |
| `codex/nfc-gemeinsamer-test` | 0 / 0 | 0 | 1716 | Entfernungskandidat nach Artefaktprüfung |
| `codex/b0-konsolidierung` | 0 / 0 | 0 | 5 | Entfernungskandidat nach Artefaktprüfung |
| `codex/b0-pruefgrundlage` | 0 / 0 | 0 | 6347 | Entfernungskandidat nach Artefaktprüfung |
| `codex/b0-reproduzierbare-pruefgrundlage` | 0 / 0 | 0 | 1 | Entfernungskandidat nach Artefaktprüfung |
| `codex/ble-led-versuch` | 0 / 0 | 1 | 295 | behalten: unveröffentlichter Entwurf |
| `codex/produkt-led-effekte` | 0 / 0 | 0 | 2504 | Entfernungskandidat nach Artefaktprüfung |
| `codex/wake-input-test` | 0 / 0 | 0 | 2271 | Entfernungskandidat nach Artefaktprüfung |
| `codex/wled-led-standard` | 0 / 0 | 0 | 0 | entbehrlicher Checkout vorgemerkt; Branch erhalten |
| `codex/nfc-test-preparation` | 0 / 0 | 0 | 1332 | Entfernungskandidat nach Artefaktprüfung |
| `codex/repository-hygiene-2026-10-18` | 0 / 0 | 0 | 0 | aktuelle Hygienearbeit |

Die zwei unveröffentlichten Entwurfszweige bleiben ausdrücklich erhalten:
`codex/audio-test-alternative-local` (zwei Commits) und
`codex/ble-led-versuch` (ein Commit). Der Hauptcheckout hat eine fremde
Änderung an `.vscode/extensions.json`; unverändert belassen.

**Zur späteren Entfernung vorgemerkt:** die acht geschlossenen PR-Branches
(Maintainer, B0, LED-Entscheidung, Audio-Grundlage/-Endstand, Matrix-Grundlage/-
Endstand, Integrationsvorbereitung) sowie der gemergte Konsolidierungsbranch.
Lokale und Remote-Namen können abweichen; #43 lokal `codex/b0-konsolidierung`,
remote `codex/b0-pruefgrundlage`. Sie werden erst nach erneutem Abgleich
und ausreichender Sicherung einzigartiger Artefakte entfernt. Der aktive
Hygienebranch bleibt bis zum Abschluss seines PRs erhalten.

Weitere historische Branches, Archivbranches, `main`, `SvensBranch` und
`Starwarscon` sind keine automatisch freigegebenen Löschkandidaten.
Kein Branch, Worktree oder ignoriertes Artefakt wurde in dieser Hygiene gelöscht.
Ignorierte APKs/Binärdateien und ZIPs sind vorhanden; ihre Anzahl beweist weder
Entbehrlichkeit noch gesicherte Wiederherstellbarkeit. Vor einer späteren
Entfernung privaten Rohbestand und regenerierbare Builds getrennt beurteilen.

## Erhaltener Fortschritt und ausgeschlossene Eingaben

**Erneut geprüft:** sechs Quellbäume, sechs veröffentlichte annotierte
Sicherungs-Tags unter `evidence/2026-10-16/*` und 43 Dokumentblobs einschließlich
Größe, Git-Blob-ID und SHA-256 stimmen mit dem [Manifest](baugruppen-nachweise.json)
überein. Die Originalbelege und acht Ergebnis-IDs bleiben erhalten.
Keine Behauptung einer neuen Sicherung privater APKs/Flashabzüge.

Root-Ignore ergänzt private Arbeits-/Artefaktverzeichnisse, lokale Worktrees,
Zugangsdateien, Signiermaterial und erzeugte Android-/Flash-Backups. Bestehende
Android-Regeln für Build, Gradlecache und lokale SDK-Konfiguration bleiben.
Der Provisionierungshelfer verlangt bereits ein neues Ziel außerhalb des
Repositories. Kuratierte JSON-/CSV-Belege unter `docs/` und `handoff/` sowie
editierbare Hardwarequellen bleiben versioniert; keine Dateien entfernt.
Ignore-Regeln sind eine Vorsorge, kein Schutz gegen explizites Force-Add oder
bereits getrackte Geheimnisse. Der begrenzte Quellencheck und seine Grenzen
stehen im Prüfabschnitt; keine allgemeine Sicherheitszertifizierung.

## CI am tatsächlichen Zielcommit

**Ausgelesen am 18.10.2026: acht von acht Jobs erfolgreich**, alle an
`784984da05249c77525273c658405d2f3b8e8e24`:

| Workflow | Ziel-Commit-Lauf | Abdeckung |
| --- | --- | --- |
| Quality | [38980094469](https://github.com/Sven401/Larp-Escape/actions/runs/38980094469) | B0, echte Kryptografie, konfigurierte Hosttests, Legacy-/ABI-Firmwareziele, Android unter Windows |
| LED product | [38980094470](https://github.com/Sven401/Larp-Escape/actions/runs/38980094470) | gemeinsames V2-Wire, LED/Wake/Matrix/Audio/NFC-Adapter, drei Baugruppen-Firmwares, Android unter Linux |
| NFC probe | [38980094423](https://github.com/Sven401/Larp-Escape/actions/runs/38980094423) | eigenständige NFC-Transaktionen und Probe-Firmware |

**Überschneidungen untersucht:** Android wird unter Windows und Linux geprüft;
das sind unterschiedliche Laufzeitumgebungen. NFC-Hosttests laufen sowohl
in der gemeinsamen Baugruppen-CI als auch im unabhängigen Probe-Workflow.
Letzterer kann bei reinen Probeänderungen separat starten und baut die Probe.
Für diesen begrenzten Hygiene-PR bleibt die Prüfabdeckung erhalten; kein
Workflow gelöscht und keine scheinbare neue Testanzahl durch Addition.
Eine spätere Laufzeitoptimierung braucht einen eigenen Abdeckungsabgleich.

## Schutzregeln und Merge-Ablauf

**Frisch ausgelesen:** Default-Branch `main`; Repository erlaubt Merge,
Squash und Rebase, Auto-Merge ist deaktiviert, automatisches Branchlöschen aus.
Das aktive [Ruleset Main](https://github.com/Sven401/Larp-Escape/rules/24680694)
gilt nur für den Default-Branch: Update-/Erstell-/Löschsperre, keine Bypassakteure,
lineare Historie, signierte Commits, ein Review, Code-Owner-/Last-Push-Freigabe,
aufgelöste Reviewthreads und Squash. Required-Checks-Liste leer.

Der kuratierte Zielbranch wird als ungeschützt gemeldet und passt nicht auf
dieses Ruleset. Die CI wird dort durch den sichtbaren Regelsatz nicht erzwungen.
**Vorgemerkt:** branchspezifische Review-/Check-Policy entscheiden; Main-Sperre
vor gesonderter Main-Integration klären. Schutzregeln unverändert gelassen.
Klassische Branch-Protection ist für die App-Verbindung mit 403 nicht lesbar;
daraus folgt keine Aussage über die Abwesenheit zusätzlicher Regeln.

## Prüfungen dieser Änderung und verbleibender Umfang

Dokumentationslinks, Diff und Ignore-Auswahl werden am Hygiene-Commit geprüft;
die Ergebnisse stehen im zugehörigen Hygiene-PR. Die B0-Helfer sind unverändert,
20 lokale Helfertests erneut erfolgreich. Begrenzter Quellencheck auf private
Schlüsselheader, bekannte Tokenformate und versehentlich getrackte private
Verzeichnisse/Buildpakete ohne Treffer; weder Git-Historie noch private
Rohartefakte sind damit vollständig sicherheitsauditiert.

Neue Firmware-/Androidbuilds und Geräteprüfungen wurden für diese reine
Dokumentations-/Ignore-Änderung lokal nicht durchgeführt. Die GitHub-Prüfungen
am veröffentlichten Hygiene-Head und PR-Prüfmerge werden gesondert bewertet.

**Weiter offen, außerhalb der Hygiene:** gemeinsames V2-App-/Firmwarepaar am
Gerät, #54, LED-/NFC-/Sitzungsfehlerfälle #29/#33/#34/#35/#37,
Chip/B7 #39, allgemeiner Zugang #27/#28 und Spiel-/Profilbetrieb #31/#30.
Audio-R3 bleibt ausdrücklich ausgenommen. Vor gesondertem Geräteupload
installiertes Paar und chipgebundenen Rückweg prüfen. Keine Geräte- oder
Produktfreigabe und keine Integration nach `main` in diesem Auftrag.
