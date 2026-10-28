# LED-Effektarchitekt: Integrationsabschluss

Stand: **18. Oktober 2026**. Status: **PR #74 integriert; Produktanbindung und
optische Abnahme offen**. Quelle: ausdrücklicher Nutzerauftrag, #74 in
`plan/projektuebergabe-kuratierter-stand` zu mergen und Projektstand/Übergabe
zu aktualisieren. Operativer Plan: [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25).

## Übernahme und Nachweis

[PR #74](https://github.com/Sven401/Larp-Escape/pull/74) wurde per Mergecommit
[`d4dd7c67cd6ba0e1903d8bf6c5df256c49fe5c3e`](https://github.com/Sven401/Larp-Escape/commit/d4dd7c67cd6ba0e1903d8bf6c5df256c49fe5c3e)
integriert. Unmittelbar danach frisch gefetcht und lokal geprüft:

- Erster Elterncommit: `dead12040c196ff74ab4be17ba71451a9351a676` (integriertes D0/#72).
- Zweiter Elterncommit: `40d962188635f6a7fc2d766523c4a714fd20eaf3` (geprüfter #74-Head).
- Vollständiger Dateibaum bei Merge und PR-Head:
  `f4836732d352596ad0ac35f3c79d85177ccb63c4`; identisch, keine Konfliktauflösung.
- PR #71/`e7ec0f9` bleibt gepinnte Fachquelle. Seine Produktänderungen wurden
  nicht zusätzlich integriert; `main` wurde nicht verändert.

Die folgende Fortschreibung betrifft nur diese Übergabe und die beiden
Projekteinstiege. Ihr veröffentlichter Abschluss wird in #25 nachgeführt.

## Nutzbare Ergebnisse

- [Projektgebundene Rolle 2.0](../docs/led/rollen/led-effekt-architekt.md):
  sichtbares Verhalten, nachvollziehbare Berechnung, parallele Aufträge/Mischung,
  Prüfplanung und klare Grenzen zur Produktintegration.
- [Ausführbare Live-Anleitung](../docs/led/interactive-development.md) und
  [Laborvertrag](../firmware/led/LIVE-NEARGLOW.md): gemeinsamer Effektkern,
  serielle Parameteränderung und paralleles Kamerafeedback. Zwei unabhängige
  Instanzen, Hintergrund, ungültige Eingänge, gezielter Stop/Neustart und ADD32.
- [Näheglut-Livebericht](../docs/led/naeheglut-live-results-2026-10-18.md):
  reale Farb-/Kurveniteration, zwei vollständig aufgezeichnete Folgeläufe
  und verifizierte Wiederherstellung des ursprünglichen Flashinhalts.
  Arbeitsvariante RGB (24,8,0), Gain 1000, Smoothstep 1000 ms; Nutzerurteil offen.
  Die ursprünglichen Quellstände, Binärhashes und Messbelege bleiben dort maßgeblich.
  Der Integrationscommit war nicht Grundlage dieser früheren Aufnahme.

## Getrennte Prüfstände

| Gegenstand | Stand und Ergebnis |
| --- | --- |
| PR-Head `40d9621` | 9/9 Jobs erfolgreich: [Quality, 3](https://github.com/Sven401/Larp-Escape/actions/runs/39144614921), [LED/Android/synthetische Kamera, 4](https://github.com/Sven401/Larp-Escape/actions/runs/39144614948), [NFC, 2](https://github.com/Sven401/Larp-Escape/actions/runs/39144614933). Vor Merge frisch ausgelesen. |
| Tatsächlicher Merge `d4dd7c6` | Eigene Push-Läufe: [Quality, 3 Jobs](https://github.com/Sven401/Larp-Escape/actions/runs/39145590308) und [LED/Baugruppen, 4 Jobs](https://github.com/Sven401/Larp-Escape/actions/runs/39145590228). Abschließend ausgelesener Status und anschließender Dokumentationsstand: [Projektplan #25](https://github.com/Sven401/Larp-Escape/issues/25). |
| Laborgerät und Kamera | Historische Teilbelege aus dem verlinkten Livebericht; kein neuer Geräte-, Kamera-, Funk- oder Uploadlauf bei dieser Integration. |
| Produkt-/optische Abnahme | Offen. Erfolgreiche Rechnerprüfung, Ausgabeaufzeichnung und Flash-Rückweg ersetzen diese Abnahmen nicht. |

NFC- und ESP-NOW-Push-Workflows wurden wegen ihrer Pfadfilter für diesen Merge
nicht ausgelöst. Ihre früheren Prüfungen werden nicht als neue Merge-Prüfung gezählt.
Die Dokumentationsfortschreibung wurde unabhängig durchgesehen; UTF-8, alle
77 lokalen Markdownziele und der Diff sind geprüft.

## Offene Punkte und nächster Schritt

1. Den konkreten Spieleinsatz und die reale Nähequelle festlegen: Wertebereich,
   Normalisierung, Frische und Ausfallbedeutung. Bestätigtes sichtbares Ziel bleibt
   ruhiges, bei Nähe anschwellendes warmes Gold auf allen 23 LEDs.
2. Auf Grundlage der integrierten Rolle den Produktadapter ausarbeiten:
   Eigentümer/Epochen/Generationen, Pause und Profilwechsel, Auftragsgrenzen und
   Überlast, produktseitige Mischung/Prioritäten sowie nötiger Spiel-/App-Anschluss.
   Das isolierte LIVE-Laborprotokoll ist dafür kein freigegebener Produktvertrag.
3. Farbe, Helligkeit und Übergang am realen Licht durch den Nutzer beurteilen;
   danach kombinierte Produkt-, Dauer- und elektrische Prüfungen getrennt planen.

Diese Folgearbeit ist durch den Mergeauftrag nicht begonnen. #29/#31/#37 und
die übrigen Produktabnahmen bleiben offen. D1-A/PR #73 bleibt separat in Review;
#60/D1-B/C, D2–D4 und die Zugangsspur #27 → #28 → #37 werden nicht abgeschlossen.
Für die nächste Sitzung zuerst den aktuellen Integrationsbranch und #25 lesen;
frühere grüne Prüfläufe nicht ungeprüft auf neue Inhalte übertragen.

Die Statusfortschreibung wird im vorhandenen sauberen LED-Worktree auf
`codex/led-architekt-integrationsstand` vorbereitet. Der Hauptcheckout bleibt
auf `codex/led-resonanzkreis`; seine fremde Änderung an
`.vscode/extensions.json` wurde nicht bearbeitet.
