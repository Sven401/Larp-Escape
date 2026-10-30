# Review und begrenzte autonome Merges

Status: Nutzerauftrag vom 30.10.2026. Gilt ausschließlich für
`plan/projektuebergabe-kuratierter-stand` in `Sven401/Larp-Escape`.
[Einführungsstand und noch fehlende Einrichtung](../../handoff/ROLLENARCHITEKTUR-2026-10-30.md).

## Befugnis und Ausnahmen

Ein anderer Reviewer soll einen freigegebenen Routine-PR selbstständig bis zum
Merge und zur Nachkontrolle führen. Er braucht keinen erneuten manuellen
Mergeanstoß. Diese Befugnis ersetzt keine fehlende technische Voraussetzung.

Ausdrücklich menschliche Freigabe brauchen: neue Produkt-/grundlegende
Architekturentscheidungen, Zugang/Schlüssel/Kryptografie/Vertrauensgrenzen,
destruktive oder nicht zuverlässig rücknehmbare Migrationen, Releases, main,
Geräteuploads außerhalb eines konkreten Geräteauftrags sowie Governance:
AGENTS, Rollen/Skills, Mergepolicy, CI/Pflichtchecks, Reviewidentitäten,
Schutz-/Berechtigungsregeln und Änderungen an deren Prüfhilfen.
Die vorliegende Rollen-/Governanceänderung ist vom Nutzer zur Umsetzung
beauftragt; ihre eigene unabhängige Prüfung wird dadurch nicht ersetzt.
Bei Zweifeln klassifiziert Reviewer mit Fachrolle; nicht still auf Routine setzen.

## Bedingungen unmittelbar vor Merge

- Richtiges Repository und exakter Zielbranch; PR offen, kein Draft, keine
  Konflikte. GitHub-Mergefähigkeit muss eindeutig sein; unbekannt bedeutet warten.
- Bestätigter Umfang und alle für diesen Umfang nötigen Abnahmen vorhanden.
  Softwareintegration darf als ausdrücklich begrenztes Paket erfolgen;
  erforderliche Geräte-/Produktabnahme bleibt für das Gesamtziel offen.
- Fachliche Gegenprüfung durch anderen Bearbeiter, der nicht am geprüften Diff
  implementiert hat. Bei Reviewkorrekturen neuen unabhängigen Reviewer einsetzen.
- Mindestens eine von GitHub anerkannte Genehmigung eines berechtigten Kontos,
  das weder PR-Autor noch letzter Pusher ist; am aktuellen Head. Ein zweiter
  Subagent unter demselben Konto erfüllt das nicht. Keine gefälschten Approvals,
  keine Token-/Kontoumgehung. Erforderliche Codeowner-Freigaben ebenfalls prüfen,
  falls später eingerichtet; aktuell ist keine CODEOWNERS-Datei vorhanden.
- Alle Reviewthreads aufgelöst, keine offenen Änderungsanforderungen. Rechte,
  Branchschutz und eventuell zusätzliche Regeln frisch lesen und einhalten.
- Alle zwölf unten genannten Pflichtchecks erfolgreich. Fehlend, skipped,
  neutral, cancelled, pending oder failure ist kein Bestehen. Doppelte Namen
  oder nicht zuordenbare Ergebnisse nicht durch einen beliebigen grünen Lauf ersetzen.
- CI-Lauf, Workflowdatei, Job, GitHub-Actions-App, Ereignis `pull_request`,
  PR-Head und Zielbasis zusammen prüfen. Pushläufe allein genügen nicht.
  Den von GitHub erzeugten PR-Prüfmerge der aktuellen Basis prüfen; bei neuem
  Head Freigabe erneuern, bei neuer Basis Integrationsprüfung erneuern.

Merge mit erwarteter Head-SHA ausführen, keine Administratorumgehung oder
Direktpush. Bestehende Mergehistorie des Integrationsbranches wird mit Mergecommit
fortgeführt. Auto-Merge erst nach unabhängiger Freigabe und Risikoprüfung aktivieren,
wenn lediglich Pflichtchecks ausstehen; GitHub übernimmt das spätere Warten.

## Pflichtchecks

Alle PR-Workflows laufen ohne Pfadfilter am PR-Eingang. Pushfilter dürfen bleiben.
GitHub Actions ist die zulässige Checkquelle; Workflowdatei und Checkname gehören
zusammen. Der neue Rollencheck prüft Konfiguration und Negativfälle, kein Fachreview.

| Workflowdatei unter `.github/workflows/` | Pflichtchecknamen |
| --- | --- |
| quality.yml | Quality / Host; Quality / Firmware; Quality / Android |
| led-product.yml | LED / Camera; LED / Protocol; LED / Firmware; LED / Android |
| nfc-probe.yml | NFC / Tests; NFC / Firmware |
| espnow-probe.yml | ESP-NOW / Firmware |
| native-idf.yml | Native / Qualification |
| role-policy.yml | Roles / Policy |

## Nachkontrolle und Fortsetzung

Reviewer liest nach Merge PR-Zustand, Mergecommit und Zielbranch zurück und
verfolgt die relevanten Pushprüfungen des Integrationsstands. Ein fehlgeschlagener
Integrationslauf erzeugt konkrete Folgearbeit; kein blindes Revert und kein
vollständiger Issueabschluss. PM prüft danach den ganzen Aufgabenumfang und
aktualisiert zugehörige Issues nur mit passenden Belegen.

Auto-Merge setzt den Merge nach Sitzungsschluss ereignisgesteuert fort,
sobald die Freigaben vorliegen. Es fuehrt keine Integrationsnachkontrolle und
keinen Issueabschluss durch. Ein separater, eng begrenzter
Reviewer-/PM-Abschlussauftrag benoetigt noch ausdrueckliche Zustimmung zur
dauerhaften Hintergrundarbeit; seine Einrichtung wurde automatisch abgelehnt.
Bis dahin erfolgen Nachkontrolle und Issueabschluss in beauftragten Sitzungen. Solange die unabhängige Reviewidentität nicht
bereitsteht, bleibt dieser Betrieb blockiert. Bestehender Fortschrittsabgleich
und Fachaudit bekommen keine erweiterten Rechte. Kein neuer Pollingauftrag ist eingerichtet.

## Einrichtung und Rückweg

Repositoryweit Auto-Merge aktivieren, ausschließlich den Integrationsbranch mit
zwölf Pflichtchecks, aktueller Basis, einer unabhängigen Genehmigung,
Verwerfen alter Genehmigungen, Freigabe des letzten Pushs, aufgelösten Threads,
Admin-Durchsetzung und ohne Forcepush/Löschung schützen. Main unverändert lassen.
Eine separate berechtigte Reviewidentität muss vom Kontoinhaber bereitgestellt
und praktisch qualifiziert werden. Kein Zugangsmaterial im Repository ablegen.

`scripts/merge_readiness.py` prüft einen normalisierten Lesesnapshot lokal und
ändert nichts an GitHub. Er unterstützt die Reviewerentscheidung, ersetzt weder
API-Rücklesen noch Branchschutz und kann nicht selbst vertrauenswürdige Belege
erzeugen. `files` enthaelt die vollstaendigen GitHub-Aenderungseintraege mit
`filename`, `status` und bei Rename/Copy `previous_filename`; beide Pfade werden
geprueft. Eingaben und Grenzen stehen im Modul. Tests unter `scripts/tests/`.

Bei Fehlverhalten zuerst Auto-Merge betroffener PRs deaktivieren, Vorgänge
bewahren und untersuchen. Rollenrevision gezielt zurücknehmen. Branchschutz
nicht zum Erzwingen eines Merges abschalten; konkrete genehmigte Rücknahme
mit vorher gesichertem Zustand separat durchführen.
