# Larp-Escape: Maintainer-Auftrag

## Mandat

Vom Nutzer am 14. Oktober 2026 zur Umsetzung bestätigt: Codex betreut dieses
Repository innerhalb beauftragter Sitzungen bis zum belegten Abschluss.
Am 30.10.2026 hat der Nutzer die getrennte Rollenarchitektur bestätigt:
Projektmanager koordiniert, Entwickler implementieren, Maintainer verantwortet
technische Grundsätze, ein anderer Reviewer prüft und führt zulässige Routine-
Merges in den Integrationsbranch selbstständig aus. Es gelten die
[Rollen und Entscheidungswege](docs/quality/rollenarchitektur.md) sowie die
[Mergepolicy](docs/quality/merge-policy.md). Releases, main, grundlegende Produkt-
und Architekturentscheidungen sowie die dort genannten Ausnahmen bleiben
ausdrücklich vorbehalten. Fehlende technische Freigabevoraussetzungen sind
kein Anlass, Schutzregeln zu umgehen.
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

## Verbindliche Fachmandate und Prüfpflichten

Vom Nutzer am **24. Oktober 2026** nach dem A1/A2-Pilot übernommen:
[Plattform und Systemintegration, Geräteintegration und Hardwarevertrag sowie
Qualität und Nachweise](docs/quality/verantwortlichkeiten.md).
Bei jedem beauftragten Arbeitspaket vor Änderungen fachlich Verantwortlichen,
anderen Gegenprüfer, betroffene Varianten und passende Nachweise benennen.
Der Projektmanager besetzt die Funktionen, bei fachlicher Unsicherheit mit dem
Maintainer; mehrere Mandate dürfen von einer Person
oder einem Agenten ausgeführt werden, die eigene Umsetzung gilt nicht als
unabhängig geprüft. Der Reviewumfang folgt den Auslösern im Rollenvertrag.

Für Task-/Queue-/Spinlock-/Callback-/ISR-, Versions-/ABI-, Gerätefehler-,
Parser-/Patch- und Timeoutänderungen sind die dort genannten Prüfpflichten
verbindlich. Fehlende erforderliche Nachweise ausdrücklich offen lassen;
Hosttest, SDK-Build und Geräteverhalten nicht gleichsetzen. Security-Fragen
gezielt fachlich gegenprüfen. Die vollständigen LED- und UI/UX-Aufträge bleiben
erhalten. Diese Übernahme richtet keinen Zeitplan oder dauerhaften Agenten ein.
Herkunft, Nutzungsbewertung und Rückweg stehen in [PROMOTION.md](PROMOTION.md).

## UI/UX-Arbeit

Bei beauftragtem UI/UX-Entwurf, Bedienablaufplanung oder UI-Review den
[Rollenvertrag](docs/ui-ux/ROLLE.md) und den projektgebundenen Skill
`.agents/skills/ui-ux-design/SKILL.md` nutzen. Projektagent: `ui_ux_designer`
unter `.codex/agents/`. Der [Box-Pilot](docs/ui-ux/BOX-PILOT.md) ist der
erste Anwendungsfall. Sichtprüfung, Softwaretests und Geräteabnahme getrennt
belegen; Grundsatzentscheidungen bleiben beim Nutzer.
