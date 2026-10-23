# Fachaudit-Betrieb: vorbereiteter Wochenlauf

**Status: vorbereitet und pausiert angelegt; keine Aktivierung beauftragt.** 24.10.2026, Europe/Berlin.
Auftrag: zweiter unveränderter Audit-Probelauf auf integriertem Stand, danach
regelmäßige Automatisierung vorbereiten. Die Ausführung des Probelaufs und
die anschließende Vorbereitung sind getrennte Arbeitsschritte.
[Zweitlauf und Wiederholungsnachweis](audits/2026-10-24-probelauf-2/README.md).

Automation: **Larp-Escape Fachaudit**, ID `larp-escape-fachaudit`, Typ Heartbeat,
Status **PAUSED**, gebunden an diesen Audit-Chat. Anlage und exakter
Promptvergleich wurden am 24.10.2026 zurückgelesen. Der bestehende aktive
GitHub-Abgleich bleibt unverändert.

## Inhalt und Zuständigkeit

Eine chatgebundene Automatisierung koordiniert zeitlich begrenzte Fachrecherche.
Die [verbindlichen Fachmandate](verantwortlichkeiten.md) bleiben unverändert.
Pro Frage benennt der Koordinator das führende Mandat und einen anderen
Qualitätsgegenprüfer. Bei konkreten Security-Fragen ist passende Expertise
gezielt nötig; ein Titel ersetzt sie nicht. Der separat beauftragte Maintainer
entscheidet über angenommene Folgearbeit und führt sie bis PR.

Der [vollständige gespeicherte Auftragsentwurf](fachaudit-automation-prompt.md)
enthält Auswahl, Quellenprüfung, Deduplizierung, Bericht und Grenzen. Ein
Schedulerlauf ist hier noch nicht nachgewiesen. Die 30 Minuten sind ein
Arbeitsbudget im Auftrag, keine technisch erzwungene Abschaltung.

## Vorbereitete Terminierung

- Eine chatgebundene Automatisierung; kein dauerhafter Agent je Bibliothek.
- Vorschlag aus dem bestehenden Entwurf: Montag 10:00, Europe/Berlin.
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

Die am 24.10. gelesene Windows-Zone ist `W. Europe Standard Time`
(Berlin, Sommer-/Winterzeit). Bei unveränderter Zone wäre der nächste
Montagstermin **26.10.2026 10:00 MEZ / 09:00 UTC**. Ein pausierter Auftrag
hat noch keinen ausgeführten Termin. Vor Aktivierung den vom Scheduler
angezeigten nächsten Termin und die Zeitzone abgleichen; eine Regel ohne
explizites TZ-Feld beweist diese Zuordnung nicht.

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

Vor einer gesonderten Aktivierung die Vorbereitung integrieren, gespeicherten
Prompt gegen die geprüfte Fassung lesen und nächsten Termin/Zeitzone prüfen.
Die ersten geplanten Läufe kontrollieren; Sandbox-/Netzzugriff und tatsächliche
Schedulerzustellung sind durch den manuellen Zweitlauf nicht bewiesen.

Rückweg: denselben Auditauftrag pausieren; Quellenberichte und Entscheidungen
erhalten. Die verbindlichen Fachmandate, vorhandene CI und der separate
GitHub-Abgleich bleiben bestehen. Keine zweite Automation als Ersatz anlegen.
Nutzungsbewertung und Übernahmegrenze stehen in [PROMOTION](../../PROMOTION.md).
