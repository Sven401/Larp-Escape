# Rollenarchitektur: Einführung und Prüfstand

Stand: **30.10.2026, Europe/Berlin**. Status: **Rollenpaket integriert;
Reviewer-App qualifiziert; tägliche Abschlussprüfung eingerichtet;
zwei vollständige Routine-Praxisdurchläufe noch offen.**
Quelle: Nutzerauftrag zur Rollenarchitektur, Zustimmung zur Umsetzung sowie
Folgeaufträge zur App-Qualifikation, täglichen Prüfung und Integration von #107.

## Integration und Verantwortung

[PR #107](https://github.com/Sven401/Larp-Escape/pull/107) wurde auf ausdrücklichen
Nutzerauftrag als Mergecommit
`f6ef9479627162682733047c8a122e8006178cfd` in
`plan/projektuebergabe-kuratierter-stand` übernommen.
Geprüfter PR-Head: `3d8b6d2c57f7cd1bbe75d1d7d01bfff685086457`;
Zielbasis unmittelbar vor Merge: `7585e1573240ac92da6e925f484a93429c219673`.
GitHub meldete vor Merge `APPROVED` und `CLEAN`; keine offenen Reviewthreads.
Merge mit erwarteter Head-SHA, ohne Umgehung des Branchschutzes.

PM koordiniert; Entwickler implementieren; Maintainer entscheidet technische
Grundsatzkonflikte; anderer Reviewer prüft und verantwortet zulässigen Merge
samt Integrationsnachkontrolle. [Rollen und Entscheidungswege](../docs/quality/rollenarchitektur.md),
[Mergepolicy](../docs/quality/merge-policy.md), [Fachmandate](../docs/quality/verantwortlichkeiten.md).
Vorhandene UI/UX-, LED-2.0- und A1/A2-Verträge wurden erhalten.
Führender Umsetzer: Hauptagent; unabhängiger Gegenprüfer: `rollen_review`.
Kein Geräteupload, Release oder Update von main.

## Nachweise

Am PR-Head wurden zehn lokale Testgruppen bestanden und unabhängig wiederholt;
60 lokale Dokumentziele und `git diff --check` waren erfolgreich. Zwei P2-Befunde
wurden korrigiert: Governance-Renames und die Trennung von Auto-Merge und
Abschlussnachkontrolle. Auch untergeordnete AGENTS-Anweisungen sind ausgenommen.
Kein verbleibender P1/P2-Befund in der unabhängigen Gegenprüfung.

Alle zwölf Pflichtchecks der sechs PR-Workflows am Head waren erfolgreich:
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/41126023916),
[LED](https://github.com/Sven401/Larp-Escape/actions/runs/41126023851),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/41126024015),
[ESP-NOW](https://github.com/Sven401/Larp-Escape/actions/runs/41126023893),
[native ESP-IDF](https://github.com/Sven401/Larp-Escape/actions/runs/41126024159),
[Rollen](https://github.com/Sven401/Larp-Escape/actions/runs/41126023960).

Die Pushprüfungen des tatsächlichen Mergecommits sind eigene Nachweise:
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/41131313048),
[LED](https://github.com/Sven401/Larp-Escape/actions/runs/41131312978),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/41131313421),
[ESP-NOW](https://github.com/Sven401/Larp-Escape/actions/runs/41131313087),
[native ESP-IDF](https://github.com/Sven401/Larp-Escape/actions/runs/41131312949),
[Rollen](https://github.com/Sven401/Larp-Escape/actions/runs/41131312905).
Nach Merge live zurückgelesen: **alle sechs Push-Workflows und alle zwölf
Prüfjobs erfolgreich**. Der Git-Baum des Mergecommits ist identisch zum
geprüften PR-Head (leerer Baumdiff); die eigenen Pushläufe wurden trotzdem
gesondert gelesen. Ergebnisse nicht von PR auf Merge übertragen.
Keine neue Geräte-, Spiel-, Accessibility- oder Nutzerabnahme.

## Reviewer-App: tatsächlich qualifiziert

App `larp-escape-reviewer`, App-ID **5547487**, Installation **177026169** für
dieses Repository. Eigenständige Botidentität gegenüber Autor/Pusher `Sven401`.
Der unabhängige Reviewer gab [Review 5718701450](https://github.com/Sven401/Larp-Escape/pull/107#pullrequestreview-5718701450)
am exakten PR-Head ab. Mit `Contents: read` akzeptierte GitHub den Review,
zählte ihn aber nicht zur erforderlichen Genehmigung (`REVIEW_REQUIRED`).

Nach Änderung auf `Contents: write` durch den Nutzer und gesonderter Bestätigung
an der Installation meldete GitHub `APPROVED` und `CLEAN`. Die bestehende
Genehmigung wurde anerkannt; erneutes Senden war nicht erforderlich.
`Pull requests: write`, `Actions/Checks: read` sind ebenfalls bestätigt.
Appdefinition und Installationsrechte getrennt prüfen: Die Definition war
bereits geändert, während die Installation zunächst weiterhin nur lesen durfte.

Kurzlebige App-Testtokens wurden widerrufen. Keine Schlüssel oder Tokens in
Projektdateien, Journal oder Ausgabe. Eine andere KI-Instanz allein bleibt
keine zweite GitHub-Identität; fachliche Gegenprüfung und technische Anrechnung
sind getrennte Voraussetzungen. Jede neue Revision braucht aktuelle Prüfung.

## Tägliche Abschlussprüfung

Auf ausdrücklichen Folgeauftrag eingerichtet und zurückgelesen:
**Larp-Escape tägliche Abschlussprüfung**, Status **ACTIVE**, täglich
**18:00 Uhr, Europe/Berlin**. Lokale Automatisierungskennung:
`larp-escape-t-gliche-abschlusspr-fung`. An diesen Chat gebundener Heartbeat;
Konfigurationsstand ist kein Nachweis eines bereits erfolgten geplanten Laufs.

Sie prüft offene Abschlussvorgänge, aktuelle CI, bereits unabhängig genehmigte
Routine-PRs, Integrationsnachkontrolle und belegten gesamten Issueabschluss.
Keine neue Produktentwicklung oder Selbstfreigabe. Governance, Security,
destruktive Migrationen, main, Releases und Geräteuploads bleiben ausgenommen.
Unveränderte Zustände bleiben still; Meldung nur bei relevantem Ergebnis,
Fehler oder neuer nötiger Entscheidung. Bestehender Fachaudit und
Fortschrittsabgleich behalten unveränderte Mandate.

Der zunächst vorgeschlagene stündliche Auftrag wurde automatisch abgelehnt und
nicht eingerichtet. Er ist verworfen; die tägliche Prüfung ist ausdrücklich
beauftragt und aktiv. Auto-Merge allein ersetzt weiterhin keine Nachkontrolle.

## Offene Erprobung und Rückweg

Noch offen: zwei kleine reale Routinepakete (Dokumentation und Code) jeweils
von Anforderung/Issue über passenden Spezialisten, Selbstprüfung, anderen
Reviewer, aktuelle CI und Merge bis Integrationsnachkontrolle und gesamtem
Auftragsabschluss durchführen. Rollenpaket und diese Governance-Fortschreibung
zählen nicht als Routinepiloten. Keine künstlichen Änderungen nur für den Test.

PM benennt je Paket Ziel, Besitzer, Reviewer, Grenzen und Abnahmefälle.
Nach zwei Piloten Agentenaufrufe, Review-Rückläufe, Wartezeiten bis Merge,
Nutzereingriffe, Wiedereröffnungen und verfügbaren Ressourcenverbrauch bewerten;
unbekannte Werte offen lassen. Kein dauerhafter Zeitgewinn bisher nachgewiesen.

Rückweg: täglichen Abschlussauftrag bei Fehlverhalten pausieren; betroffene
Auto-Merges stoppen, Belege erhalten und eigene Revisionen gezielt prüfen.
Schutzregeln nicht zum Erzwingen eines Merges lockern. Main und persönliche
Hooks bleiben unberührt. Die [historische Erstfassung](https://github.com/Sven401/Larp-Escape/blob/3d8b6d2c57f7cd1bbe75d1d7d01bfff685086457/handoff/ROLLENARCHITEKTUR-2026-10-30.md)
bewahrt Ausgangsbestand, erste Blockaden und frühere Prüfgrenzen.
