# Entwicklungsstand und private Bereinigung

**Ausgeführt und geprüft · 20.–21.10.2026.** Dieser Beleg ergänzt die
[Bestandsaufnahme](ENTWICKLUNGSSTART-2026-10-20.md). Grundlage ist der ausdrückliche
Auftrag, #91 nach grüner CI zu integrieren, ungesicherte Quellen und private
Belege zu sichern und erledigte Chats und Worktrees aufzuräumen. Aktive Arbeit
und offene Produktentscheidungen bleiben erhalten.

## Integrierter Ausgangspunkt

[PR #91](https://github.com/Sven401/Larp-Escape/pull/91) wurde nach dem Abgleich
mit dem inzwischen integrierten #90 gemergt. Ziel bleibt
`plan/projektuebergabe-kuratierter-stand`.

- Geprüfter PR-Head: `776a15d351ba043d8781a8de1f963ee3ce2bf224`.
- Merge: `f174cee458d67163af06b2e38fc76473c38a5b5c`.
- Neun PR-Jobs erfolgreich: [Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39500214797),
  [LED product](https://github.com/Sven401/Larp-Escape/actions/runs/39500214778),
  [NFC probe](https://github.com/Sven401/Larp-Escape/actions/runs/39500214835).
- Merge-Dateibaum und geprüfter PR-Dateibaum sind identisch; beide Merge-Eltern
  wurden geprüft. Zusätzlich sind alle sieben Jobs der Push-Prüfungen am Merge
  erfolgreich: [Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39501307036)
  und [LED product](https://github.com/Sven401/Larp-Escape/actions/runs/39501306993).
  Das ist ein eigener Nachweis auf dem Mergecommit.
- Dokumentationsprüfung: zwölf geänderte Dateien, 261 lokale Linkziele,
  JSON-Syntax, Konfliktmarker und Diff geprüft, ohne Befund.

Ein zusätzlicher sauberer Checkout `.worktrees/kanonischer-start` liegt lokal
auf dem Integrationsbranch bei diesem Merge. Der zuvor zurückliegende lokale
Integrationsbranch wurde ausschließlich vorwärts aktualisiert. Der Hauptordner
bleibt wegen PR #71 und seiner vorhandenen lokalen Änderungen unverändert auf
dem Resonanzkreis-Branch. Vor neuer Arbeit immer den tatsächlich geöffneten
Checkout und den aktuellen Remote-Stand prüfen.

## Sicherung und Aufräumen

86 erledigte Chats wurden archiviert: 33 direkt abschließbare und 53 erst
nach bestätigter Quell-/Belegsicherung. Vor jedem Eingriff wurden Status,
letzter Turn und Änderungszeit erneut mit der Bestandsaufnahme abgeglichen.
Die zwölf bewusst erhaltenen lokalen Chats, bereits archivierte Chats und
der zusätzlich zugeordnete ältere Architektur-Chat blieben unangetastet.

Die private Archivkennung ist `cleanup-2026-10-20-01a15b13`. Vollständige lokale
Inventare, Dateihashes, Chat-IDs, Indexdateien, binäre Patches und private
Gerätebelege bleiben dort. Sie gehören nicht ins veröffentlichte Repository.
Der lokale Archivpfad ist zusätzlich im ursprünglichen Hauptcheckout unter
`private/maintenance/cleanup-2026-10-20/ARCHIVE.txt` verzeichnet.

Die Git-Sicherung umfasst acht Repositories einschließlich der historischen
KI-Werkbank und der eingebetteten EasyEDA-/GM-Companion-Quellen. Alle Refs,
Worktree-Heads und 18 zusätzlich nur über Reflogs nachgewiesene Commits wurden
in eigenständigen Bundles bewahrt. Wiederimport in leere Git-Repositories,
Objektprüfung und Refvergleich waren erfolgreich. Eine unabhängige Nachprüfung
bestätigte 212 von 212 Hashes der acht Bundles und 204 Metadaten-/Patchdateien.

60 abgeschlossene Arbeitskopien wurden vollständig auf demselben Laufwerk ins
private Archiv verschoben: 57 verknüpfte Worktrees abgemeldet und drei
eigenständige Klone verschoben. Die 165.007 Dateien mit insgesamt
12.243.157.325 Bytes wurden anhand vollständiger SHA-256-Manifeste
vor und nach der Verschiebung geprüft. Ignorierte Dateien und Caches blieben
erhalten. Git meldete ausschließlich den alten, bereits fehlenden Pfad ab;
die ursprüngliche Branchhistorie blieb erhalten.

Die privaten Bereiche des weiter benutzten Hauptcheckouts, die externen
Matrix-Belege, die editierbaren UI-/Präsentationsquellen und die beiden historischen
Hauptquellen erhielten zusätzliche Kopien mit SHA-256-Prüfung durch vollständiges
Wiedereinlesen. Die 16 gesicherten Bereiche umfassen 107.007 Datei-Vorkommen
und 10.644.528.644 Archivbytes. Darin überlappen sich Matrix-Teilbereiche;
die Zahlen sind nicht als eindeutige Dateien oder Datenmenge zu addieren.
Hauptlauf und Ergänzung sind datierte Momentaufnahmen, abgeschlossen am
21.10.2026 um 00:01:22 beziehungsweise 00:01:38 MESZ. Der gemeinsame Beleg
bestätigt vollständige Pflichtquellenabdeckung und alle Einzelprüfungen;
spätere Änderungen aktiver Quellen werden dort weitergeführt.
Die alten Hauptquellen bleiben an ihrem Ort. Regenerierbare
Build-/Abhängigkeitsordner des aktiven Hauptcheckouts sind kein Bestandteil
dieser ausgewählten Kopien und bleiben dort erhalten.

## Bewusst erhalten

- Resonanzkreis/PR #71, D1-A/PR #73 und LED-Entwurf/PR #89; ihre
  fachliche Bewertung, Konfliktauflösung und Integration sind eigene Aufgaben.
- Optische Näheglut-Abnahme, Produktanschluss, PCB-/Boxangaben und weitere
  offene Hardwareentscheidungen.
- Noch nicht gegen den heutigen Code abgeschlossene ältere Android-
  Import-/Export-Lebenszyklusbefunde sowie ein nicht erfolgreich abgeschlossener
  Handoff. Die zugehörigen Chats bleiben auffindbar.
- Der aktive GitHub-Heartbeat und sein aktueller Trägerchat. Seine geprüfte
  Arbeitsbasis ist der aktuelle Hauptordner mit explizitem Remote-Ref, nicht
  der archivierte Automations-Worktree; Definition und Termin wurden durch
  diese Bereinigung nicht verändert.
- Die veröffentlichten Archivbranches als Quellenlager, die frühere Sicherung
  vom 18.10.2026 und die 27 getrennten Evaluationsläufe.

## Wiederaufnahme

Für neue App-/Firmwarearbeit gilt der [Maintainer-Einstieg](MAINTAINER.md).
Wiederverwendbare ältere Arbeit wird über die Quellenzuordnung der
[Bestandsaufnahme](ENTWICKLUNGSSTART-2026-10-20.md) fachlich geprüft und gezielt
auf einem eigenen Branch übernommen. Ein Archiv ist keine Produktfreigabe und
kein automatisch wieder aktivierter Entwicklungsauftrag.

Das private Archiv enthält `WIEDERHERSTELLUNG.md` und maschinenlesbare Receipts.
Archivierte verknüpfte Worktrees haben absichtlich keine aktive Gitregistrierung;
ihre alten `.git`-Zeiger werden nicht ungeprüft benutzt. Der Wiederaufbau in
einem neuen Ordner aus Bundle, vollständigem Dateibaum und gestagtem Patch wurde
mit binären, ungestagten, gelöschten und unversionierten Dateien praktisch
geprüft. ZIP-Kopien werden anhand der im Receipt angegebenen Einträge inklusive
etwaiger Wiederholungsfassungen wiederhergestellt.

Diese Arbeiten verändern keine Produktentscheidung und sind kein neuer
Geräte-/Firmware-Abnahmenachweis. Das lokale Archiv liegt auf demselben
Datenträger und ist deshalb kein Schutz vor dessen Ausfall.
