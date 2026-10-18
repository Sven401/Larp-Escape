# Larp-Escape: Maintainer-Auftrag

## Mandat

Vom Nutzer am 14. Oktober 2026 zur Umsetzung bestätigt: Codex betreut dieses
Repository innerhalb beauftragter Sitzungen bis zum fertigen Pull Request.
Dazu gehören Priorisierung, Umsetzung, Prüfung, Dokumentation sowie eigene
Branches, Commits, Pushes und Pull Requests. Merges, Releases und grundlegende
Produktentscheidungen bleiben beim Nutzer und benötigen dessen konkreten Auftrag.
Eine regelmäßige Hintergrundüberwachung ist gesondert zu vereinbaren.

## Einstieg und Arbeitsbasis

- Vor der Arbeit Arbeitsbaum, Branch, HEAD und Remote prüfen. Der vereinbarte
  Integrationsbranch ist `plan/projektuebergabe-kuratierter-stand`; Änderungen
  auf eigenen Arbeitsbranches vorbereiten und PRs gegen diesen Branch richten.
  Eine Integration nach `main` ist ein eigener Auftrag.
- [Maintainer-Einstieg](handoff/MAINTAINER.md) und
  [technische Übergabe](handoff/README.md) lesen, danach nur die für die Aufgabe
  passenden Verträge und Belege. Aussagen am aktuellen Code und Prüfstand messen.
- Die neue Maintainer-Vereinbarung und neue Projektkenntnisse werden in diesem
  Repository geführt. Bestehende technische Dokumentation verlinken statt
  duplizieren. Die frühere separate KI-Werkbank ist historische Belegquelle;
  ihre alten Fortsetzungsaufträge und Arbeitskonfigurationen nicht automatisch
  übernehmen. Projektfakten gehören nicht in persönliche globale Skills.
- Archivbranches sind Quellenlager. Entwürfe vor einer Wiederaufnahme fachlich
  bewerten. Die am 13. Oktober 2026 verworfene Diagnose-App und ihre paketweise
  Fortsetzung sind keine aktuellen Arbeitsaufträge.

## Änderungen und Nachweise

- Fremde Änderungen erhalten. Vor jedem Commit Status und Diff kontrollieren,
  nur ausdrücklich benannte Dateien oder eigene Hunks stagen; kein `git add -A`
  oder `git add .`. Unabhängige Arbeiten in eigenen Worktrees durchführen.
- Bestätigte Erkenntnisse knapp mit Status, Quelle, Datum und geprüftem
  Commitstand dokumentieren. Vermutungen und offene Prüfungen sichtbar benennen.
- Die dokumentierten, zur Änderung passenden Prüfungen ausführen. Hosttests,
  Builds, Uploads und beobachtetes Geräteverhalten sind getrennte Nachweise.
  Frühere grüne Ergebnisse ersetzen keine Prüfung des aktuellen Standes.
- Vor Geräteuploads den in der Hardwaredokumentation bezeichneten chipgebundenen
  Flash-Rückweg abgleichen. Die Maintainer-Übernahme selbst beauftragt keinen Upload.
- Abschlussberichte nennen Änderungen, geprüften Commit, Prüfergebnisse und
  verbleibende Einschränkungen sowie bei Veröffentlichung den PR-Link.

## UI/UX-Arbeit

Bei beauftragtem UI/UX-Entwurf, Bedienablaufplanung oder UI-Review den
[Rollenvertrag](docs/ui-ux/ROLLE.md) und den projektgebundenen Skill
`.agents/skills/ui-ux-design/SKILL.md` nutzen. Projektagent: `ui_ux_designer`
unter `.codex/agents/`. Der [Box-Pilot](docs/ui-ux/BOX-PILOT.md) ist der
erste Anwendungsfall. Sichtprüfung, Softwaretests und Geräteabnahme getrennt
belegen; Grundsatzentscheidungen bleiben beim Nutzer.
