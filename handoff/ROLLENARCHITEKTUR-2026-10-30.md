# Rollenarchitektur: Auftrag, Einfuehrung und Nachweise

Stand: 30.10.2026. Status: Umsetzung auf eigenem Branch; Integration und
vollstaendiger autonomer Mergebetrieb noch nicht belegt.
Quelle: Nutzerauftrag Rollenarchitektur und Zustimmung zur Umsetzung.
Basis: `7585e1573240ac92da6e925f484a93429c219673`.

## Auftrag und Abnahme

Ziel: getrennte operative und technische Verantwortung; unabhaengiger Reviewer
schliesst zulassige Routine-PRs selbststaendig ab. PM, Maintainer, Reviewer,
Android, Firmware und Game Design als duenne Projektagenten ergaenzt; vorhandener
UI-Agent, LED-Rolle2.0, Fachmandate und UI-Skill bleiben erhalten.
Funktionale Produktquellen, Geraetezugang und Releases sind nicht Teil des Pakets.
Fuehrender Umsetzer: Hauptagent; andere Gegenpruefung: Subagent `rollen_review`.

Abnahmefaelle: eindeutige Rollen/Entscheidungen; kleine UI-Korrektur ohne Vollprozess;
unabhaengiger Review; aktuelle Pflichtchecks; keine Selbstfreigabe; keine
Routinefreigabe fuer Governance/Security/Architektur/destruktive Migration/main;
Merge/Nachkontrolle/gesamter Issueabschluss getrennt. Rollen und Workflow stehen
kanonisch in [Rollenarchitektur](../docs/quality/rollenarchitektur.md), Bedingungen
und Pflichtchecks in [Mergepolicy](../docs/quality/merge-policy.md).

## Beobachteter Bestand und Umsetzung

Der geoeffnete Checkout e7ec0f9 und der lokale Integrationsbranch waren veraltet;
der Remote-Head wurde ueber GitHub und Fetch abgeglichen. Aktueller eigener
Worktree basiert auf7585e157; fremde Aenderungen bleiben im Ausgangscheckout.
Offener PR#106 verbessert bereits den Projekteinstieg; sein Inhalt wird nicht
als integriert behauptet und nicht parallel kopiert. Bei Zusammenfuehrung
AGENTS und Verantwortungsmatrix gezielt abgleichen; keine Rollen zuruecksetzen.
PR#105 bleibt separater BLE-Entwurf.

Live gelesen: Repository-Metadaten, Branches, Ruleset Main, offene PRs, aktuelle
Checks/Reviews von#106; lokal die beiden bestehenden Automatisierungsauftraege.
Figma-Datei3uQpZfwREhQ9cHk80jMtRw war lesbar; aktive Pilot- und Archivseite
bestaetigt. Historische56-Screen-/Komponenten-/Navigationsnachweise wurden nicht
vollstaendig erneut ausgefuehrt. Keine Designaenderung und keine native UI-Abnahme.

Die bisherigen CI-Namen waren teils generisch; ESP-NOW hatte einen PR-Pfadfilter.
Zwoelf eindeutige Checknamen werden jetzt ohne PR-Pfadfilter erzeugt; Pushfilter
bleiben. Bestehende Build-/Testkommandos und Pins unveraendert. Neuer Rollencheck
fuehrt die Policy-Negativtests und Agenten-/Jobkonsistenz aus.

## Pruefstand und Grenzen

Lokal: zehn unittest-Pruefgruppen erfolgreich, mit Unterfaellen fuer jede
Pflichtpruefung und fehlende Pflichtfelder; git diff --check und 60 lokale Dokumentziele erfolgreich.
Kein YAML-Parser lokal installiert; Job-/PR-Eingangskonsistenz getestet,
tatsaechliches Einlesen der sechs Workflows wird durch GitHub-CI belegt.
Keine neuen SDK-/Android-/Geraetetests lokal. GitHub-CI wird am veroeffentlichten
Head gesondert bewertet. Die JSON-Pruefhilfe ist rein lesend, kein GitHubclient
und keine vertrauenswuerdige automatische Freigabequelle.

Unabhaengige Gegenpruefung: erste Quellenpruefung erkannte drei alte aktive
Merge-/Maintainerzuordnungen und die Governance-Ausnahme; in diesem Paket
korrigiert. Diff-Gegenpruefung fand zwei P2-Luecken: Rename-Herkunft und fehlende
Fortsetzung fuer Nachkontrolle/Issueabschluss. Rename/Copy pruefen nun alten
und neuen Pfad, fehlende Herkunft sperrt; zehn Testgruppen bestehen.
Die Abschlussfortsetzung ist ausdruecklich als offene Betriebsgrenze benannt.
Nachpruefung durch denselben unabhaengigen Bearbeiter erfolgt: zehn
Testgruppen selbst ausgefuehrt, korrigierte Dateien samt Handoff/PROMOTION
gelesen; keine verbleibenden P1/P2-Befunde. Git-Diffzugriff des Gegenpruefers
war eingeschraenkt; direkte Dateilektuere und Basisvergleich erfolgten.
Live-Administrationsaenderungen nicht nochmals unabhaengig per API geprueft.

Administrativer Lesezugriff ueber vorhandene lokale GitHub-Anmeldung erfolgreich;
keine Zugangsdaten in Dateien, Logs oder Projekt abgelegt. Als Collaborator
wurde ausschliesslich Sven401 gefunden. Eine zweite KI-Instanz besitzt damit
keine unabhaengige GitHub-Genehmigungsidentitaet. Deren Bereitstellung durch den
Kontoinhaber und ein realer Genehmigungs-/Auto-Merge-Pilot bleiben erforderlich.
Keine leere Genehmigung erzeugen und keine Schutzregel dafuer umgehen.

Live eingerichtet und ueber Administrations-API zurueckgelesen: Auto-Merge ist
aktiv; ausschliesslich der Integrationsbranch ist mit zwoelf GitHub-Actions-
Pflichtchecks (App15368), strikter Basis, einer Genehmigung, Verwerfen alter
Genehmigungen, Freigabe des letzten Pushs, aufgeloesten Threads und
Admin-Durchsetzung geschuetzt. Forcepush und Loeschung nicht erlaubt. Main
wurde nicht veraendert. Vorher war der Integrationsbranch ungeschuetzt und
Auto-Merge aus. Vorher-/Nachher-Snapshots lokal unter private/rollenarchitektur,
keine Zugangsdaten enthalten; nicht versioniert. Bestehende PRs muessen vor
Merge die neuen CI-Namen liefern, etwa durch gepruefte Uebernahme der neuen
Workflowdefinitionen; fehlende Namen bleiben blockierend.

Auto-Merge setzt nur den freigegebenen Merge fort. Ein eigener stuendlicher
Reviewer-/PM-Abschlussauftrag wurde von der automatischen Freigabepruefung
abgelehnt: gesonderte Zustimmung zur dauerhaften Hintergrundarbeit fehlt,
der genehmigte Plan enthielt keine neue Pollingautomation. Es wurde kein
Abschlussauftrag eingerichtet und kein Umweg versucht. Fuer Nachkontrolle und
Issueabschluss bleiben beauftragte Sitzungen noetig; dauerhafter Betrieb ist
offen. Bestehender Fortschrittsabgleich und Fachaudit unveraendert.

Noch kein erfolgreicher Routine-Merge-Pilot, kein nachgewiesener dauerhafter
Zeitgewinn und keine erfundene Ressourceneinsparung. Neben der zweiten
Reviewidentitaet benoetigt Hintergrund-Nachkontrolle ausdrueckliche Zustimmung.

## Migration, Wiedervorlage und Rueckweg

1. Eigenen PR pruefen und CI am exakten Head auswerten.
2. Kontoinhaber richtet eine separate berechtigte Reviewidentitaet ein. Reviewer
   prueft, dass ihre aktuelle Genehmigung von GitHub tatsaechlich anerkannt wird.
3. Kleinen Dokument- und begrenzten Code-Routinefall durch die gesamte Kette
   inklusive Nachkontrolle fuehren. Governance dieses Pakets bleibt vorbehalten.
4. Nach zwei Piloten Review-Ruecklaeufe, Wartezeit, Nutzereingriffe und verfuegbaren
   Agentenaufwand bewerten. Unbekannte Werte offen lassen.

Reviewer besitzt die technische Mergeblockade, PM verfolgt Bereitstellung und
Piloten bei naechster Projektfortsetzung. Vor autonomen Merges neuen Quellstand
und Schutzregeln lesen. Rueckweg: betroffene Auto-Merges deaktivieren, eigenen
Rollencommit gezielt zuruecknehmen, Belege erhalten; Schutzregeln nur auf einen
konkreten genehmigten Ruecknahmeauftrag aendern. Persoenliche Hooks unveraendert.
