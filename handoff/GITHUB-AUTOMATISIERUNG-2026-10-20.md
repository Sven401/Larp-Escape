# GitHub-Automatisierung: Prüfung und Umstellung

**Status: belegt, 20.10.2026, Europe/Berlin.** Nutzerauftrag: bestehenden
GitHub-Abgleich pausieren, letzte Änderungen prüfen, auf das aktuelle
Projekt-Repository umstellen, ausschließlich lesend testen und danach aktivieren.

Die lokale Codex-Automatisierung wurde in dieser Reihenfolge angepasst.
Nach erfolgreichem Probelauf um 22:56 Uhr wieder `ACTIVE`; Name, ID und tägliche
09:00-Regel erhalten. Die gelesene Konfiguration bestätigt den neuen Auftrag
und die Bindung an den aktuellen Projektchat. Der nächste geplante Lauf wurde
noch nicht beobachtet; die effektive Scheduler-Zeitzone ist nicht eigens belegt.

## Quelle und Arbeitsbasis

- Repository: [Sven401/Larp-Escape](https://github.com/Sven401/Larp-Escape).
- Maßgeblicher Integrationsbranch: `plan/projektuebergabe-kuratierter-stand`.
- Abschließend live geprüfter Remote-Head:
  `97a2584cc108ebb9c0f667fc46f898d163e00144` (PR #88).
- Einstieg: [AGENTS.md](../AGENTS.md), [Maintainer](MAINTAINER.md),
  [technische Übergabe](README.md), passende Fachverträge und Quellcode.
- GitHub-Issues bleiben verbindlich für Planung, Prioritäten und Abnahmekriterien.
  Repository und commitbezogene Nachweise bestimmen den technischen Ist-Stand.
  Die frühere separate KI-Werkbank und ihre Dashboard-Skripte werden nicht mehr
  als Betriebsgrundlage verwendet.

Die Ausgangsprüfung verwendete `5b85965`. Während der Prüfung wurde #88 integriert.
Die Änderung wurde erkannt, der Vergleich verschiedener Quellstände mit
`source-changed` abgewiesen und auf `97a2584` neu begonnen. Der neue Merge
ergänzt ausschließlich Setup-Dokumentation; Produktquellen bleiben gegenüber
der vorherigen Prüfgrundlage unverändert. Der eigene Dokumentationsbranch
baut auf diesem neuen Integrationsstand auf.

## Letzte Änderungen der bisherigen Automatisierung

Geprüft wurden die sieben protokollierten Heartbeat-Läufe vom 11., 12., 13.,
14., 15., 19. und 20.10.2026 im ursprünglichen Automatisierungschat, dessen
Einrichtungsdialog sowie die aktuellen Issues und verfügbaren Ereignisse.

**Befund: In diesen sieben automatischen Läufen keine GitHub-Schreibaktion.**
Die Läufe hielten wegen alter Arbeitskonfiguration beziehungsweise später
wegen Quellenkonflikt und fehlender Markierungen an. Der letzte Lauf am
20.10. blieb beim bereits bekannten Konflikt still. In den protokollierten
Heartbeat-Werkzeugen fanden sich 27 Issue-Leseaufrufe und eine Issue-Suche,
keine Mutation. Lokale Befehle dienten dem Lesen/Abgleich.

Die letzten dort belegten GitHub-Schreibaktionen stammen aus der ausdrücklich
beauftragten **Einrichtung am 10.10.2026**:

| Vorgang | Umfang |
| --- | --- |
| Neue Planung anlegen | 13 Issues: #25 und #26–#37 |
| Bestehende Inhalte/Zustände bearbeiten | 36 Updates: #1–#23 historisieren/schließen, #25 und #26–#37 fortschreiben |
| Projektoberfläche einrichten | Ansichten, Status, Prioritäten und damalige vorläufige Iterationen |

Diese Einrichtung ist von späteren automatischen Läufen zu unterscheiden.
Der PO-Plan ab 14.10. hat frühere Planungsstände teilweise ersetzt; sie werden
nicht zurückgeschrieben. Es wurde keine fehlerhafte automatische Änderung
gefunden, die zurückgenommen werden müsste.

**Aussagegrenze:** Prüfung des verfügbaren Chat-/Werkzeugverlaufs und des live
gelesenen GitHub-Stands; kein vollständiger organisationsweiter GitHub-Audit.
Die verfügbaren Issue-Ereignisse liefern keine vollständigen Body-Revisionsdiffs.
Änderungen anderer Sitzungen werden deshalb nicht dieser Automatisierung zugerechnet.
Private Chatprotokolle werden nicht ins Projekt kopiert.

## Installierter Ablauf

Der Auftrag ist als [geprüfter Prompt-Snapshot](github-sync-audit-2026-10-20/prompt.md)
dokumentiert. Verbindlich für die Ausführung bleibt die lokale Automatisierung
`larp-escape-github-projektstand`; der Snapshot startet selbst nichts.
SHA-256 des ausgelesenen Prompt-Strings:

`ed34b0a306432fef0b2500f31e6e43a5904bb6c42094183a5a7072f9d35cf9a0`

1. Tatsächlichen Remote-Head prüfen und Quellen genau an diesem Commit lesen.
   Ein alter lokaler Checkout oder offener PR gilt nicht als Integration.
2. Aktuelle Pakete aus #25, Issue-Verknüpfungen und der versionierten Matrix
   ermitteln. Keine feste Beschränkung auf #26–#37.
3. Konkrete Vorher-/Nachher-Vorschläge mit Quellen und Aussagegrenzen vorbereiten.
   Softwaretest, Build, Labor, Gerätetest und Produktfreigabe getrennt behandeln.
4. Nur einen eigenen Abschnitt zwischen
   `<!-- codex-project-sync:v2:start -->` und
   `<!-- codex-project-sync:v2:end -->` pflegen.
   Beim ersten tatsächlichen Sachupdate darf das vollständig fehlende Paar
   samt Abschnitt vor den unveränderten bestehenden Body gesetzt werden.
   Unvollständige, doppelte, archivierte oder später entfernte Marker sperren das Ziel.
5. Vor Schreiben frisch lesen; bei geändertem Body, Zeitstempel oder Zustand
   neu bewerten. Nach Schreiben zurücklesen und Fremdtext unverändert bestätigen.
   Inhaltlich gleicher Stand erzeugt kein Update.
6. Issue-Abschluss nur für den gesamten belegten Umfang; Boardstatus nur mit
   geprüftem Item/Feld/Option. Titel, Kriterien, Checkboxen, Prioritäten, Termine,
   Iterationen, Kommentare und Zuständigkeiten bleiben außerhalb des Auftrags.
7. Fachlicher Konflikt sperrt das betroffene Ziel. Eindeutige unabhängige Pflege
   darf weitergehen. Unveränderter Stand bleibt still.

Keine neuen Issues, Produktänderungen, persönlichen Hooks, Builds, Uploads,
Commits, PRs oder Merges durch diesen wiederkehrenden Auftrag.

## Ausschließlich lesender Probelauf

Durchgeführt mit GitHub-Leseaufrufen, lesender Boardansicht und einem lokalen
Prüfprogramm ohne Netzwerkclient, Zugangsdaten oder Schreibfunktion.
GitHub-Berechtigungen des Kontos wurden nicht verändert. Die vorübergehende
Ansichtsfilterung wurde nicht gespeichert.

**Ergebnis:** 23 aktuelle Produkt-/Plan-Issues zweimal verglichen; Body, Zustand
und Änderungszeit unverändert. Alle 16 Schutzfälle bestanden. Null Änderungen
an Issues, Kommentaren, Issue-Zuständen oder Boardfeldern während des Probelaufs.

| Geprüfter Vorschlag | Ergebnis |
| --- | --- |
| #25: integrierten GM-Piloten und Wartungs-/Integrationsbelege ergänzen | Eigener neuer Abschnitt möglich; Alttext vollständig erhalten, Issue offen |
| #33: begrenzten Status-/Frischefortschritt des GM-Piloten ergänzen | Eigener neuer Abschnitt möglich; offene Funk-/Mehrstations-/Dauerfälle erhalten |
| #37: Stationsvorbereitung und lesenden GM-Zugang ergänzen | Eigener neuer Abschnitt möglich; allgemeine Rechte und Licht-/Spielaktionen offen |
| #60: Board `Todo` → `In progress` | Ausgearbeiteter D1-A-Stand im offenen Review #73; kein Abschluss, nicht angewendet |
| #34: älteres Rückkehrkriterium gegen automatische GM-Wiederverbindung | Konflikt erkannt; gesamtes Issue bleibt unverändert |

Die Vorschläge sind Prüfbeispiele des neuen Ablaufs, keine bereits angewendeten
Änderungen und keine vollständige Liste künftiger Boardkorrekturen.
Die anderen 20 Issue-Bodies bleiben im geprüften Vorschlag unverändert.

[Maschinenlesbares Ergebnis mit Vorher-/Vorschlags-Hashes](github-sync-audit-2026-10-20/dry-run.json)
und [Herkunft, Ausführung und Grenzen des Prüfprogramms](github-sync-audit-2026-10-20/HERKUNFT.md)
halten die Prüfung nachvollziehbar fest.

Das Prüfprogramm kontrolliert die Textoperationen und Schutzfälle; es ist
kein produktiver Sync-Client und erzwingt nicht das Verhalten eines künftigen
Modelllaufs. Quellzuordnung und fachliche Aussagen wurden zusätzlich anhand
der genannten Repository-/PR-Belege geprüft. Der spätere Schreib- und
Rückleseweg sowie ein tatsächlicher geplanter Lauf sind noch nicht getestet.

## Offener fachlicher Punkt und Rückweg

[#34](https://github.com/Sven401/Larp-Escape/issues/34) verlangt im älteren
Kriterium, dass Rückkehr nicht automatisch verbindet. Der integrierte
[GM-Pilot](../docs/companion/spielleiter-stationspilot.md) erlaubt begrenzte
automatische Wiederverbindung mit frischer Prüfung; bewusste Pause bleibt erhalten.
Die passende fachliche Abgrenzung beziehungsweise Kriterienkorrektur benötigt
eine eigene Entscheidung. Kein stilles Umdeuten oder automatisches Schließen.

Einfacher Rückweg bei einer Abweichung: denselben bestehenden Auftrag in Codex
pausieren, neue Konfiguration und Laufbericht erhalten, betroffene Änderungen
vor einer gezielten Korrektur prüfen. Die veraltete KI-Werkbank-Konfiguration
nicht wieder aktivieren. Es wurden keine zusätzlichen Automatisierungen angelegt.
