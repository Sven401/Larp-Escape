# Herkunft und Prüfung

**Status: am 20.10.2026 angewendet und geprüft.**
Entstanden im beauftragten Wechsel der vorhandenen GitHub-Automatisierung auf
das aktuelle Larp-Escape-Repository. Eigenes kleines Python-Standardbibliothek-
Prüfprogramm; keine fremden Codepakete, Kontozugänge oder GitHub-Schreibschnittstellen.

- [Bericht](../GITHUB-AUTOMATISIERUNG-2026-10-20.md)
- [Prüfprogramm](check_dry_run.py)
- [Tatsächliches Ergebnis](dry-run.json)
- [Installierter Prompt, datierter Snapshot](prompt.md)

## Wiederholen

Python 3.12 war lokal vorhanden. Die Schutzfälle benötigen keine Eingabedatei:

```powershell
python handoff/github-sync-audit-2026-10-20/check_dry_run.py --self-test
```

Der tatsächliche Probelauf verwendete zwei lokal gehaltene JSON-Snapshots
aus GitHub-Leseaufrufen und Boardablesung:

```powershell
python handoff/github-sync-audit-2026-10-20/check_dry_run.py before.json after.json
```

Format beider Eingaben:

```json
{
  "source_commit": "vollständiger zuvor verifizierter Integrationscommit",
  "issues": [
    {"number": 25, "title": "...", "body": "...", "state": "open",
     "updated_at": "...", "html_url": "..."}
  ],
  "board": {"60": "Todo"}
}
```

Für den datierten Vergleich sind alle 23 damaligen Issues erforderlich,
insbesondere #25, #33, #34 und #37. Ein neuer Lauf muss die Quellen erneut
prüfen und datierte Vorschlagstexte fachlich aktualisieren. Die Volltexte der
Snapshots werden nicht als zweite Planungsquelle versioniert. Ein späterer
Live-Snapshot kann abweichen und reproduziert dann nicht denselben Hash.

## Umfang und Grenzen

16 synthetische Fälle prüfen Erhalt von Fremdtext/CRLF, Wiederholung ohne
Änderung, Ersetzen des eigenen Blocks, konkurrierende Änderungen an Text,
Zeitstempel oder Zustand, unbestätigte Quelle, fachlichen Konflikt, entfernte,
fehlende, vertauschte, doppelte, eingebettete Marker sowie historische Issues.
Zusätzlich wurde ein tatsächlich gewechselter Remote-Head vor dem Neustart des
Livevergleichs abgewiesen. Alle 23 tatsächlichen Issue-Bodies, Zustände und
Zeitstempel waren zwischen den endgültigen Leseproben gleich.

Die Erkennung von Code-/Archivabschnitten ist eine einfache Prüfung für
die hier erfassten Fälle, kein vollständiger Markdown-/HTML-Parser.
Das Programm erzeugt Vorschläge und gibt JSON auf Standardausgabe aus.
Es schreibt keine Dateien und besitzt keinen API-Client. Es prüft weder
eine spätere Modellentscheidung noch einen produktiven Schreibweg.
Das Ergebnisfeld `github_mutations: 0` beschreibt diesen reinen Leseablauf;
es ist kein vom Programm erhobener globaler GitHub-Auditwert.
Die Zahlen wurden mit dem tatsächlich ausgeführten Werkzeugablauf abgeglichen.
