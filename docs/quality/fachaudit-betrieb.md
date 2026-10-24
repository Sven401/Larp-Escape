# Fachaudit-Betrieb: aktivierter Wochenlauf

**Status: nach ausdrücklichem Folgeauftrag aktiviert; erste geplante Ausführung offen.**
24.10.2026, Europe/Berlin. Die zuvor pausierte Vorbereitung ist über
[PR #97](https://github.com/Sven401/Larp-Escape/pull/97) am Mergecommit
`c582ed7048cf7c386f240eca20e09142b86c7e00` integriert;
[geprüfter Integrationsstand und eigene Merge-CI](../../handoff/MAINTAINER.md).
Der [Zweitlauf](audits/2026-10-24-probelauf-2/README.md) bleibt ein eigener
manueller Nachweis mit damaligem Vorbereitungsstatus.

Automation: **Larp-Escape Fachaudit**, ID `larp-escape-fachaudit`, Typ Heartbeat,
Status **ACTIVE**, Zielchat `01a16ab1-34d7-73b0-924a-fc2d245b9027`.
Aktivierung am 24.10.2026 um 01:55:42 MESZ; Status, Zielchat und exakter
Promptvergleich danach erneut zurückgelesen. Der bestehende aktive
GitHub-Abgleich bleibt unverändert.

## Inhalt und Zuständigkeit

Eine chatgebundene Automatisierung koordiniert zeitlich begrenzte Fachrecherche.
Die [verbindlichen Fachmandate](verantwortlichkeiten.md) bleiben unverändert.
Pro Frage benennt der Koordinator das führende Mandat und einen anderen
Qualitätsgegenprüfer. Bei konkreten Security-Fragen ist passende Expertise
gezielt nötig; ein Titel ersetzt sie nicht. Der separat beauftragte Maintainer
entscheidet über angenommene Folgearbeit und führt sie bis PR.

Der [vollständige gespeicherte Auftrag](fachaudit-automation-prompt.md)
enthält Auswahl, Quellenprüfung, Deduplizierung, Bericht und Grenzen. Ein
Schedulerlauf ist hier noch nicht nachgewiesen. Die 30 Minuten sind ein
Arbeitsbudget im Auftrag, keine technisch erzwungene Abschaltung.

## Aktivierte Terminierung und Rücklesebeleg

- Eine chatgebundene Automatisierung; kein dauerhafter Agent je Bibliothek.
- Aktivierter Takt: Montag 10:00, Europe/Berlin.
- Maximal 30 Minuten und zwei vertiefte Fragen je Lauf.
- Standardmodell und Berechtigungen des Chats beibehalten.
- Erste Rotationsfolge nach Aktivierung: A3, A4, A5, A6, A1, A2.
  Dringende neue Hinweise und tatsächlich ausgelöste Wiedervorlagen haben
  Vorrang; verdrängte Gebiete bleiben sichtbar. Sechs Wochen sind ein
  Abdeckungsziel, kein nachgewiesener Betrieb.
- Feste Normeditionen quartalsweise auf Revision/Errata prüfen; eine
  verbindliche Projektnorm wird dadurch nicht ausgewählt.
- Nach vier tatsächlich abgeschlossenen geplanten Läufen Nutzen und Takt
  bewerten. Manuelle Probeläufe zählen dafür nicht.

Am 24.10.2026 wurden Automations-TOML und ausschließlich die beiden
zugehörigen Einträge der lokalen Scheduler-Datenbank lesend abgeglichen.
Die Windows-Zone `W. Europe Standard Time` umfasst Berlin einschließlich
Sommer-/Winterzeit. Der Sommerzeitwechsel am 25.10. ist berücksichtigt:

- Nächster Solltermin: **26.10.2026 10:00 MEZ / 09:00 UTC**;
  `next_run_nominal_at = 1793005200000`.
- Vorgemerkter Start: **26.10.2026 09:58:11 MEZ / 08:58:11 UTC**;
  `next_run_at = 1793005091000`.
- `last_run_at = null`: noch kein Laufzeitpunkt erfasst, keine Zustellung behauptet.

Der lokale App-Code (Version 26.1016.6573) erlaubt bei dieser Wochenregel
einen Startversatz von bis zu fünf Minuten in beide Richtungen. Die
beobachteten minus 109 Sekunden passen dazu; Solltermin und Vormerkung
wurden getrennt geprüft. Das ist keine minutengenaue Ausführungsgarantie.

Der gespeicherte Prompt stimmt exakt mit der verlinkten Projektdatei überein:
SHA256 `84216bd07c59a4912903a4829bbe44d6e6a3a5ae62a835beb00af51635de1943`
(UTF-8, ohne abschließenden Dokument-Zeilenumbruch). Der bestehende
GitHub-Abgleich bleibt ACTIVE, täglich 09:00, mit unverändertem Zielchat,
Änderungszeitpunkt und Prompt-Hash
`ed34b0a306432fef0b2500f31e6e43a5904bb6c42094183a5a7072f9d35cf9a0`.
In dieser Fortschreibung wurde kein Zeitplan oder Prompt geändert.

Lokale Aufgaben brauchen laufenden Rechner, App und zugängliche Quellen.
Die offizielle Dokumentation empfiehlt den manuellen Probelauf und die
Kontrolle der ersten Ausführungen:
[Scheduled tasks](https://learn.chatgpt.com/docs/automations?surface=app).
Verpasste oder unvollständige Läufe bleiben als Abdeckungslücke sichtbar.

## Zustand, Dubletten und Wiedervorlagen

Das [Startregister](audits/2026-10-24-probelauf-2/register.json) verweist auf
fünf bestehende Finding-IDs sowie den bestehenden Hardwarefall #39 mit zwei
B7-Endpunkten. Es ist ein commitbezogener Einstieg, keine zweite Projektplanung.
Neuere integrierte Entscheidungen sind maßgeblich; frühere lokale Laufentwürfe
verhindern Dubletten, dürfen aber keine angenommene Entscheidung vortäuschen.

Ein neuer Quellenabruf, Titel oder Release erzeugt keine neue Finding-ID,
wenn Komponente, Regel und Variante bereits erfasst sind. Softwarekorrektur,
Vertragsübernahme und offene physische Abnahme bleiben getrennte Zustände.
A1-01/A2-01 werden nicht wegen alter Pilotabsätze wieder geöffnet.
A2-02 bleibt hinsichtlich Parser-/Patchregression verifiziert; seine offene
Geräteprüfung wird ausdrücklich an das bestehende Adapterfreigabetor aus
A2-03 angeschlossen. Das ergänzt die Zuordnung, ohne eine neue Frist oder
Produktfreigabe zu erfinden.

Wiedervorlagen sind überwiegend ereignisgebunden. Kein Kalendertermin bedeutet
weder erledigt noch automatisch überfällig. Pro Lauf Ereignisse und Git-Diff
gegen den letzten geprüften Stand abgleichen. Noch nicht beauftragte Hardware-
oder Security-Folgearbeit erhält keine erfundene personelle Besetzung.

## Erlaubte Ergebnisse und Grenzen

Der Lauf darf Repository/GitHub und offizielle Quellen lesen und ausschließlich
eigene Berichte unter `outputs/fachaudit/<UTC-Startzeit>-<Commitkurzform>/`
im verifizierten lokalen Repository schreiben. Dieser Bereich ist bereits
ignoriert. Bestehende fremde Berichte werden nicht überschrieben. Jeder Lauf
erzeugt `README.md` und `lauf.json` mit Start/Ende, SHA, Quellenzugang,
Änderungen, IDs, Rollen, Wiedervorlagen, Auslassungen und Reviewstatus.
Ein lokaler Entwurf ist noch keine Integration.

Keine Produkt-, Pin-, Fixture-, Rollen-, Hook- oder GitHub-Planungsänderung;
keine Gerätehelfer, Installation, automatischen Commits/Pushes/PRs/Merges,
Releases oder Nachrichten in andere Chats. Der bestehende GitHub-Abgleich
bleibt unverändert. Der Audit schreibt nicht in seine Sync-Marker.

Nur ein Koordinator schreibt. Vor Abschluss erneut Remote-Head und Eingänge
prüfen. Bei Änderung bleibt der Beleg beim alten Commit und eine Übernahme
bis zum neuen Abgleich offen. Ein Quellenfehler wird nicht durch alten Cache
verdeckt. Unveränderte, nicht handlungsrelevante Ergebnisse bleiben still;
neue relevante Befunde, wesentliche Änderungen oder nötige Entscheidungen
werden gemeldet.

## Aktivierung und einfacher Rückweg

Die beauftragte Aktivierung ist nach Integration, Promptvergleich und
Termin-/Zeitzonenprüfung erfolgt. Die erste geplante Ausführung und ihre
Ergebnisse bleiben zu kontrollieren; Sandbox-/Netzzugriff und tatsächliche
Schedulerzustellung sind durch den manuellen Zweitlauf oder ACTIVE allein
nicht bewiesen. Nach vier tatsächlich abgeschlossenen geplanten Läufen
Nutzen und Takt bewerten; keine zusätzliche Überwachung wurde eingerichtet.

Rückweg: denselben Auditauftrag pausieren; Quellenberichte und Entscheidungen
erhalten. Die verbindlichen Fachmandate, vorhandene CI und der separate
GitHub-Abgleich bleiben bestehen. Keine zweite Automation als Ersatz anlegen.
Nutzungsbewertung und Übernahmegrenze stehen in [PROMOTION](../../PROMOTION.md).
