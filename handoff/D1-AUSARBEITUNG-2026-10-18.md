# D1-Ausarbeitung und Rechnerprüfstand · 18. Oktober 2026

**Status: D1-A als Vorschlag ausgearbeitet; Firmware-/F5a-Anschluss D1-B und
physische Speicher-/Betriebsabnahme D1-C offen. #60 ist nicht abgeschlossen.**
Quelle: Nutzerauftrag „D1 (#60) auf Basis des integrierten D0-Vertrags ausarbeiten“
ohne direkten Gerätezugriff. Sitzung `01a14f48-8628-7930-a8b6-78f3b955a13b`.
Basis nach frischem Fetch: `dead12040c196ff74ab4be17ba71451a9351a676` auf
`plan/projektuebergabe-kuratierter-stand`. Eigener Branch
`codex/d1-fortschritt-journal` in sauberem separatem Arbeitsbaum.
Der konkrete Ausarbeitungs-/CI-Head ist im zugehörigen PR ausgewiesen; die
Version dieses Berichts und der Referenz ist gemeinsam commitgebunden.

## Ergebnis und Quellenabgleich

[D1-Persistenz-/Wiederanlaufvertrag](../docs/veranstaltung/d1-persistenz-v1.md),
[ausführbare Rechnerreferenz](../scripts/d1_journal_model.py) und
[Tests](../scripts/tests/test_d1_journal.py) konkretisieren den integrierten D0-Vertrag:

- Vergabe pro Fakt statt Nummernreservierung: eigener Fortschritt, Sequenz,
  Vorgangskorrelation, vollständiger Fakt und Outbox in einer Transaktion.
- Dauerhaften Empfang und vollständige fachliche Anwendung trennen. Latch,
  Zustandsänderung, Folgefakten und Effektvorbereitung atomar; wartende Eingänge
  nach Neustart erneut gegen Abhängigkeiten und aktuelle Rechte prüfen.
- Persistente Konfliktsperren, Lücken und historische Kontexte; bounded
  Kompaktierung erhält Fortschritt, Replayboden, unklare Wirkung und noch nötige
  Belege. Kein unbegrenztes Offline-/Kapazitätsversprechen oder stiller Altreset.
- Vor physischem Auftrag erst `attempted` dauerhaft festhalten; unklarer
  Wiederanlauf wird `unknown` und erzeugt keine automatische Wiederholung.
- Konkreter Koordinator-/Rechtefencevertrag für F5a, Quoten und Verschleißplanung;
  Produktionsformat/-backend und Firmwareanschluss bleiben ausdrücklich offen.

Gegengelesen: D0/D0-G1–G5, VA-01, Veranstaltungszugang, #60 und Projektplan #25;
`ProfileJournal.h/.cpp`, Profiljournal-/Fehlervertrag, `ProfileApplication.h`,
`GameControl.h`, Laufzeitvertrag und bestehende Quality-CI. Der vorhandene
Profilrückfall ist kein Fortschrittsrollback. Keine Änderung dieser Produktkerne,
bestehender D0-/Wire-/Profil-/NFC-Formate oder des Frameworkpins.

## Kriterium und tatsächlich prüfbarer Anteil

| Kriterium von #60 | Rechnernachweis / offene Grenze |
| --- | --- |
| Keine wiederverwendete dauerhafte ID, Lücken statt Maximum | Datei wieder öffnen, identischen Vorgang erneut nachlesen, UINT64_MAX, lokale Vergabe/Outbox nach Prozessabbruch; geordnete Lücke, `unused_range` in beiden Konfliktrichtungen und historischer Stream. Echte D0-Bytes/Schlüssel und ESP-Speicher: D1-B/C. |
| Abgestimmter Eingang/Anwendung/Folgeausgang, keine zweite Freigabe | Persistentes `pending`, Mehrregelplan mit vollständigem Rollback, Ersatzfreigabe plus spätes Original, andere Regelgeneration, Entzug nach Vorbereitung und direkt vor Commit; atomarer Latch/Counter/Outbox/Intent. Keine RTOS-/Produktprofilintegration behauptet. |
| Unterbrechungen, voller/beschädigter Speicher, Kompaktierung, alte Kontexte | 36 echte Prozessabbrüche an benannten logischen/SQLite-Grenzen; Fact-/Stream-/Effekt-/Vorgangs-/Bytequoten, tatsächliche SQLite-Seitenerschöpfung, Format-/Prüfdigest-/Kanonizitätsfehler; 40 Kompaktierungszyklen, alte Run-/Paketbindung. Verschleiß und elektrische Stromunterbrechung nicht gemessen. |
| Physische Wirkung von Annahme trennen, unklar nicht wiederholen | Vorbereiteter Intent ist kein Effekt; Claim vor Übergabe, Neustart nach Claim ergibt `unknown`, kein zweiter Claim; simulierte Beobachtung separat. Kein Gerät oder realer Effekt im Test. |

## Lokale Prüfung und CI-Anschluss

Verfahren: `python -m unittest discover -s scripts/tests -p test_d1_journal.py -v`.
**Gemessen: 44 Tests erfolgreich**; der veröffentlichte Head und dessen CI stehen
im PR. Die Suite enthält **36 tatsächlich beendete Kindprozesse** mit anschließendem
Öffnen derselben Datenbank. Abbruchpunkte: Vergabe, Latch/Intent, Kompaktierung,
vorbereiteter Zustand, geschriebener Snapshot, vor Commit und nach Commit.
Geprüfte Vorgänge: Veröffentlichung, Anwendung, Ersatzfreigabe, Kompaktierung,
Kontextwechsel, Effektclaim und Zielquittung. Vor Commit gilt der vorige Zustand,
danach der vollständige neue; beim unterbrochenen Claim dessen unbekannte Wirkung.
Das ist keine byteweise Fehlereinbringung in SQLite, kein Betriebssystemabsturz
und kein Abschalten eines Speichermediums.

Gemessene lokale Umgebung: Python 3.12.6, SQLite 3.45.3, Windows.
Zusätzlich erfolgreich auf dieser Basis: 111 bestehende D0-Modellfälle sowie
Architekturguard (145 Dateien/302 direkte Includes). Neun Dateien auf UTF-8 und
Python-Syntax sowie 121 relative Dateilinkziele und Diff geprüft.
Der neue Quality-Schritt `d1-journal` führt die Referenzsuite auf frischem
Windows-Runner aus; aktuelle PR-CI getrennt vom früheren grünen Merge `dead1204`.

SQLite und komplette JSON-Zustandskopien dienen ausschließlich als Referenz auf
dem Rechner. Policy-/Profilbeobachtungen, D0-Decodierung und Modelldigests sind
simuliert. Keine SQLite-Abhängigkeit, Produktionstask oder Speicherpartition
wurde in Firmware eingebaut. Kein Handy-/ESP-/Kamera-/COM-/Funkzugriff, Upload
oder neue Stromausfallabnahme; parallele Produktarbeit bleibt unangetastet.

## Nächster konkreter Schritt

Den Vorschlag und Rechnerkern im PR fachlich prüfen. Danach D1-B1–B4 aus dem
Vertrag beauftragen: echten D0-Verifizierer/#28, begrenzten Ziel-Speicherport und
F5a-Koordinator anschließen und Softwarebudgets nachweisen. D1-C bleibt eigener
Geräteauftrag. Ein Merge dieses Vorschlags allein schließt #60 nicht ab.
