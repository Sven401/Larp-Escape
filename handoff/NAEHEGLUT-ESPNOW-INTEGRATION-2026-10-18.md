# Näheglut/ESP-NOW: Integrationsabschluss

Stand: **18. Oktober 2026**. Quelle: ausdrücklicher Nutzerauftrag, PR #76
bei grüner CI in `plan/projektuebergabe-kuratierter-stand` zu mergen und
Projektstand/Übergabe fortzuschreiben. Operativer Plan: [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25).

## Übernahme und CI

**PR #76 integriert:** [`1646bc37d43f29f0fb48b3cf13adf597dfdc2cf4`](https://github.com/Sven401/Larp-Escape/commit/1646bc37d43f29f0fb48b3cf13adf597dfdc2cf4).
Frischer Fetch bestätigt Eltern `42d986e` + `8e4c9af`; der vollständige
Dateibaum `34f689dbeb55c72d104b1b9b007b7e2a969f3e18` ist identisch zum PR-Head.

**PR-CI am Head `8e4c9af`: alle 9 Jobs erfolgreich**, unmittelbar vor Merge
frisch abgelesen: [Quality, 3](https://github.com/Sven401/Larp-Escape/actions/runs/39152095609),
[LED/Android/Kamera, 4](https://github.com/Sven401/Larp-Escape/actions/runs/39152095574),
[NFC, 2](https://github.com/Sven401/Larp-Escape/actions/runs/39152095496).
Die anschließenden Push-Läufe am tatsächlichen Merge und der reine
Dokumentationsabschluss werden getrennt im [Projektplan #25](https://github.com/Sven401/Larp-Escape/issues/25)
fortgeschrieben; der obige PR-Nachweis wird nicht auf spätere Commits umetikettiert.

Der erste geprüfte Head `6b7e12f` scheiterte im Linux-Kameratest an
`-Werror=misleading-indentation`: Die Warteschleife und die folgenden
Anweisungen standen in einer Zeile. `8e4c9af78dee1c4aa325ff18697a131d08e5bb9d`
klammert ausschließlich diese Testschleife und trennt die Folgeanweisungen.
Keine Änderung an Effekt-, Funk- oder Produktcode. Lokal erneut bestanden:
9 Camera-Fälle, 7 NearGlow-Gruppen einschließlich 2002 Werte-/Kurvenfällen,
10 RSSI-Laborfälle. Warnungen bleiben in CI Fehler; keine Prüfung abgeschwächt.

## Erhaltene Ergebnisse und Grenzen

[Testanbindung und ursprünglicher Arbeitsstand](NAEHEGLUT-ESPNOW-2026-10-18.md),
[Versuchsbericht mit Diagramm](../docs/espnow/naeheglut-result-2026-10-18.md)
und [Papierkalibrierung](../docs/led/paper-camera-calibration-2026-10-18.md)
bleiben die kanonischen Fachbelege. Die Geräteversuche wurden mit `e26bd93`
durchgeführt, nicht mit einem späteren Integrationscommit.

- Reale Funkquelle im isolierten Labor belegt: 444 Ergebnisse, Veraltung,
  Wiederkehr und gezielter Stop ohne automatischen Wiederanlauf.
- 6730/6732 nominal nachgerechnete TX-Frames exakt; zwei Übergangsabweichungen
  bleiben ungeklärt. Optisches Dunkelkriterium nicht vollständig erfüllt;
  Nutzerurteil, Feinstufen und lückenlose Zeitbewertung bleiben offen.
- Kontinuierliche Rückkehr bis zum Nahplateau nicht vollständig aufgezeichnet.
- Originalinstallation nach den Versuchen vollständig wiederhergestellt und
  verifiziert. Diese Integration umfasst keinen weiteren Gerätezugriff.
- Produktadapter, Spielwertvertrag, Reichweiten-, Dauer-, elektrische und
  Gesamtspielabnahme bleiben offen. CI ist keine optische Freigabe.

## Konkrete Fortsetzung

Bei einem Folgeauftrag zuerst den aktuellen Integrationsstand und #25 lesen.
Für weitere Lichtprüfung Nutzerbeobachtung ergänzen und die veränderte
Schwarzbasis gezielt untersuchen; Schwellen nicht nachträglich anpassen.
Für Produktintegration den Spieladapter anhand der vorhandenen
[Architektenrolle](../docs/led/rollen/led-effekt-architekt.md) entwerfen.
Keine dieser Folgearbeiten wird durch diesen Mergeabschluss begonnen.

Dokumentationsabschluss auf `codex/naeheglut-integrationsstand` im vorhandenen LED-Worktree unter
`C:/Users/seide/.codex/worktrees/led-effekt-architekt/Larp Escape`.
Der Hauptcheckout und seine fremde `.vscode/extensions.json` bleiben erhalten.
