# Fachaudit-Betrieb: kurzer Änderungscheck, gezielte Vertiefung

**Stand 28.10.2026:** Vom Nutzer nach dem ersten Wochenlauf zur Verschlankung
beauftragt. Die bestehende Automation `larp-escape-fachaudit` bleibt ein
aktiver Wochenauftrag, Montag **10:00 Europe/Berlin**. Nur ihr Arbeitsauftrag
wird angepasst; Fachmandate, Prüfpflichten, Zielchat und Schreibgrenzen bleiben
erhalten. Repositorybasis dieses PRs ist
`f81956faffa4d4c6b0198fdbd0c626b21e137cd4`.
Die Dokumentation ist bis zum Merge ein PR-Stand; die ausdrücklich beauftragte
Promptumstellung wird unten getrennt belegt.

## Ablauf und Auswahl

1. **Kurzcheck, höchstens fünf Minuten einschließlich Laufnachweis,
   Rücklesen und Abschlussabgleich:** Remote-Head, relevante Änderungen
   seit dem letzten erfolgreichen Eingangs-/Delta-Vergleich, aktuelle Projektplanung,
   ausgelöste Wiedervorlagen und Primärhinweise tatsächlich eingesetzter
   Komponenten prüfen. Bereits gelesene unveränderte Git-Blobs mit Stand
   wiederverwenden. Quellen nach Risiko/Anwendbarkeit wählen und ausgelassene
   Eingänge sichtbar lassen; kein Vollscan aller Bibliotheken.
2. **Nur bei Anlass vertiefen:** Relevanter Hersteller-/Advisoryhinweis,
   betroffene Code-/Pin-/Vertragsänderung, fälliger Vertrags-/Normauslöser oder
   eine konkrete Wissenslücke, von der eine anstehende Entscheidung abhängt.
   Anlass und Priorität belegen. **Höchstens eine vertiefte Frage und 30 Minuten
   insgesamt**, einschließlich Kurzcheck, Gegenprüfung und Protokoll.
3. **Ohne Anlass früh enden:** Ergebnis ist „kein Vertiefungsanlass in den
   geprüften Eingängen“, keine Aktualitäts-, Sicherheits- oder Produktfreigabe.
   Kein eigener Fachreview oder Subagent für diesen Routinefall; Reviewstatus
   `nicht erforderlich` ist kein durchgeführter Review.

Die starre Rotation A3–A4–A5–A6–A1–A2 und das frühere Sechswochenziel entfallen.
A1–A6 bleiben eine Themenkarte, keine Terminpflicht. A4 ist nach dem A3-Lauf
eine sichtbare Wissenslücke, aber kein automatischer nächster Auftrag.
Feste Normeditionen weiterhin quartalsweise auf Revision/Errata abgleichen.
Echte Vertrags-/Abnahmeauslöser gelten zusätzlich. Mehrere fällige Fragen
priorisieren; verdrängte Fragen und Normprüfungen mit Auslöser offen halten.
Budgetende oder fehlende notwendige Quelle erzeugt einen unvollständigen
Prüfstand; nicht durch alten Cache als aktuell ausgeben.

## Fachreview und wirksame Übernahme

Die [Fachmandate](verantwortlichkeiten.md), [AGENTS](../../AGENTS.md) und
[bestehenden Prüfpflichten](../quality-checks.md) bleiben unverändert verbindlich.
Für jede Vertiefung und jeden neuen handlungsrelevanten fachlichen Schluss
führendes Mandat, anderen Gegenprüfer, Variante und kanonisches Ziel nennen.
Die Gegenprüfung muss tatsächlich erfolgen; ohne anderen Bearbeiter bleibt
der Entwurf ungeprüft. Kurzlebige Rechercheagenten gezielt dafür einsetzen.
Bei Advisories, Zugang, Kryptografie oder geänderten Vertrauensgrenzen passende
Security-Expertise zuordnen; ein Rollenname allein ist kein Qualifikationsbeleg.

Der Audit liefert Entwürfe. Der separat beauftragte Maintainer entscheidet
über Annahme, Zurückstellung oder Ablehnung und führt angenommene Folgearbeit
in die vorhandene Fachreferenz, Prüfung oder Implementierung bis PR.
Ein neuer Entwurf braucht ein bestehendes Arbeitspaket beziehungsweise ein
konkretes kanonisches Ziel, Verantwortlichen und überprüfbaren nächsten Schritt.
Keine automatische Annahme, kein zweites Issue-Backlog unter `outputs`.
Produkt-/Releaseentscheidungen und Gerätefreigaben bleiben beim Nutzer.

## Bekannte Fälle und Wiedervorlagen

Das [akzeptierte Startregister](audits/2026-10-24-probelauf-2/register.json)
enthält fünf Finding-IDs und den Hardwarefall #39/B7. Seine historischen
`proposed_rotation_position`-Felder sind kein aktueller Betriebsauftrag.
Spätere integrierte Entscheidungen und überprüfter Codezustand haben Vorrang;
lokale Entwürfe sind noch keine angenommenen Änderungen.

- Komponente, Regel und Variante bestimmen die Wiedererkennung; neue Quelle,
  Titel oder Release allein erzeugt keine neue ID.
- A1-01/A2-01 bleiben softwareseitig korrigiert; nur neue belegte Regression
  kann sie wieder öffnen. Softwarekorrektur und physische Abnahme trennen.
- A2-02 bleibt hinsichtlich Parser/Patch verifiziert; seine Geräteprüfung ist
  vor jeweiliger Adapterfreigabe ausdrücklich an A2-03 gekoppelt.
- A1-02, A2-03 und beide B7-Endpunkte unter #39 behalten ihre offenen Grenzen.
  #54 ersetzt #39 nicht.
- Ereignisgebundene Wiedervorlagen unverändert erhalten. Kein Kalenderdatum
  bedeutet weder erledigt noch automatisch überfällig; keine Frist erfinden.

## Kompakter Laufnachweis

Nur der Koordinator schreibt neue eigene Entwürfe unter
`outputs/fachaudit/<UTC-Startzeit>-<Commitkurzform>/` im verifizierten Repository;
dieser Bereich bleibt ignoriert. Vor jedem Schreiben Origin prüfen.
`lauf.json` ist Pflicht; `README.md` nur bei Vertiefung, relevanter Abweichung
oder nötiger Entscheidung. Keine Kopie des gesamten Befundregisters.

Der minimale Nachweis enthält Laufart und Solltermin/Kontrollschlüssel soweit
belegbar, Start/Ende, vollständigen Quellcommit und Vergleichsbasis,
`kein_anlass`/`vertiefung`/`unvollstaendig`, konkrete geprüfte Eingänge samt
Zugriffsfehlern, Änderungen oder Referenzen auf unveränderte Fälle, offene
Auslöser, Reviewstatus, nächste Prüfung und die unten genannten Messwerte.
Quellen erhalten URL, Revision/Publikationsdatum soweit verfügbar und Abrufdatum.
Ein Kurzcheck erneuert keinen vollständigen Fachprüfstand. Erfolgreich geprüfte
Eingänge dürfen ihren Eingangs-/Delta-Vergleichsstand fortschreiben, damit
unveränderte Diffs nicht erneut bearbeitet werden. Fehlgeschlagene Eingänge
behalten ihren letzten erfolgreichen Stand; offene Fälle und Auslöser bleiben
erhalten. Teilrecherche setzt den vollständigen Fachprüfstand nicht weiter.

Geschriebene Dateien zurücklesen. Vor Abschluss Remote-Head und wesentliche
Eingänge erneut abgleichen. Bei Änderung bleibt das Ergebnis seinem alten
Commit zugeordnet; Übernahme erst nach neuem Abgleich. Quellen-, Code-,
Softwaretest- und Gerätebelege getrennt führen.

## Messbarer Nutzen und Entscheidung nach vier Läufen

Ein geplanter Lauf zählt einmal über **Automation-ID plus Solltermin**,
auch bei verspätetem Start. Manuelle Piloten, Erstlaufkontrolle und Wiederholungen
desselben Solltermins zählen nicht zusätzlich. Vollständige, teilweise
prüfbare und abgebrochene Ausführungen getrennt erfassen. Der abgeschlossene
Lauf vom 26.10. zählt als **1 von 4**, mit Kennzeichnung `vor Verschlankung`.
Bewertung nach vier mit Abschlussnachweis beendeten geplanten Läufen;
teilweise prüfbare Läufe zählen gekennzeichnet mit, Abbrüche werden separat
geführt und zählen nicht als abgeschlossen. Keine umfassende
Wirksamkeitsbehauptung aus dieser kleinen Stichprobe.

| Messwert | Zählregel / erforderlicher Beleg |
| --- | --- |
| Belegte Maintainerentscheidungen | Anzahl einzigartiger auditveranlasster Fälle mit Entscheidungsdatum, Rolle und Referenz; Annahme, Zurückstellung und begründete Ablehnung getrennt |
| Angenommene Änderungen | Einzigartige Fälle mit ausdrücklicher Annahme und Ziel; Umsetzung/Integration separat mit PR/Commit ausweisen |
| Geschlossene Nachweislücken | Einzigartige Fälle mit tatsächlich passendem neuen Test-, Mess- oder Entscheidungsnachweis und dessen Quelle |
| Offene Entwürfe | IDs, Empfänger und Alter seit Vorlage; unbekannte Entscheidung bleibt offen |
| Fehlalarme / Dubletten | Als handlungsrelevant vorgelegte, anschließend wegen falscher Anwendbarkeit/Behauptung beziehungsweise Dublette verworfene Fälle; begründete Prioritätszurückstellung ist kein Fehlalarm |
| Aufwand | Gemessene Sekunden bis Protokollabschluss; vollständige Chatdauer nur wenn verfügbar. Paralleler Agenten-/Token-/Kostenaufwand unbekannt lassen, wenn nicht gemessen |
| Betrieb / Quellenzugang | Soll-/Iststart und Verspätung, Laufstatus, vollständige Kurzchecks innerhalb fünf Minuten, Vertiefungen innerhalb 30 Minuten, notwendige fehlgeschlagene Quellenzugriffe |

Wirkungszähler **nicht addieren**: derselbe Fall darf in mehreren Phasen
erscheinen, innerhalb eines Zählers aber nur einmal. Quellenabrufe,
Rechercheagenten, Reviews und Befundentwürfe sind Tätigkeiten, noch keine Wirkung.
Eine begründete Entscheidung gegen eine Änderung kann nützen, benötigt einen
Entscheidungsbeleg. Unbekannte Werte sind `null`, nachgewiesene Abwesenheit
ist `0`. Keine behauptete Zeit-/Kostenersparnis ohne belastbaren Vergleich.

Die Bewertung nennt die absoluten Zahlen mit Belegen, Anteil vollständiger
Kurzchecks innerhalb fünf Minuten und Aufwand pro belegtem Wirkungsfall.
Dessen Nenner ist die deduplizierte Menge der Fälle mit belegter Folge,
nicht die Summe der drei Wirkungszähler; bei null Fällen nicht berechenbar. Alte und neue Betriebsform
getrennt darstellen; keine personelle Arbeitszeit aus paralleler Laufzeit
ableiten. Bei null belegten Folgen ausdrücklich **„Zusatznutzen bislang nicht
belegt“** festhalten und Fortsetzung, geringeren Takt oder ereignisbezogenen
Betrieb gegen konkreten Präventionsbedarf abwägen. Keine Findenquote;
ruhige Wochen beweisen weder Nutzlosigkeit noch Wirksamkeit.

Vorzeitige Betriebsbewertung bei einer Überschreitung von 30 Minuten,
zwei aufeinanderfolgenden Kurzchecks über fünf Minuten oder derselben
notwendigen unzugänglichen Quelle in zwei aufeinanderfolgenden Läufen.
Das sind Entscheidungsauslöser, keine Erlaubnis zur eigenständigen Änderung
von Zeitplan oder Befugnissen. Bereits gemeldete unveränderte Blocker bleiben still.

## Beobachteter Ausgangspunkt und Umstellung

Der [manuelle Pilot](audits/2026-10-24-pilot/README.md) und seine
[Softwarekorrekturen](runtime-fixes-2026-10-24.md) sind belegter Nutzen gezielter
Facharbeit; sie werden der später eingerichteten Automation nicht zugerechnet.

Erster regulärer Lauf am **26.10.2026** auf `f81956faffa4d4c6b0198fdbd0c626b21e137cd4`:
A3 mit zwei begrenzten Fragen, tatsächlicher Chatlauf **20 Minuten 18 Sekunden**,
keine neuen/geänderten Findings, kein daraus angenommener Änderungsauftrag,
keine geschlossene Geräteabnahme. Eine RMT-Fehlervermutung wurde am SDK-Quellcode
verworfen; die unveränderte WLED-Semantik frisch gegengeprüft.
Interne Qualitätsgegenprüfung und die separate Erstlaufkontrolle bestätigten
Dublettenbehandlung, Mandate und Wiedervorlagen. Das belegt einen brauchbaren
Ablauf, noch keinen dauerhaften Zusatznutzen des Wochenbetriebs.

Lokale Belege: `outputs/fachaudit/2026-10-26T18-25-59Z-f81956f/README.md` und
`lauf.json`, Audit-Chat `01a16ab1-34d7-73b0-924a-fc2d245b9027`,
Auditturn `01a17976-2384-70e0-b223-5b04539a6aec`.
Berichtabschluss 18:44:40 UTC, letzte Rücklese 18:45:39 UTC,
Chatabschluss 18:46:06 UTC; diese Zeitpunkte sind verschiedene Nachweise.

Sollstart 10:00 MEZ, tatsächlicher Start 19:25:47 MEZ; Nachkontrolle startete
19:46:48 statt 11:00 MEZ. Ursache ungeklärt, kein Abbruch.
Die einmalige Nachkontrolle wurde anschließend pausiert und entfernt.
Der neue Auftrag verspricht keine zeitkritische Sicherheitsalarmierung.
Lokale Ausführung braucht Rechner, App und Quellenzugriff
([Betriebsvoraussetzungen](https://learn.chatgpt.com/docs/automations)).

**Umgestellt und zurückgelesen am 28.10.2026, 18:33 UTC / 19:33 MEZ:**
`larp-escape-fachaudit` bleibt ACTIVE. Gespeicherter Prompt stimmt mit
[dieser Promptdatei](fachaudit-automation-prompt.md) überein
(UTF-8 ohne abschließenden Zeilenumbruch, SHA256
`0560956eec6f4ea7c8f04bb562cb43d2ad7958ded040e6d0cb31dd0637f7a506`).
Name, Wochenrhythmus, Zielchat, Benachrichtigungseinstellung und letzter
Laufzeitstempel blieben unverändert. TOML und Scheduler wurden nach dem
Werkzeugaufruf unabhängig zurückgelesen. Die Konfiguration des separaten
GitHub-Abgleichs blieb bytegleich.

Nächster Solltermin: **02.11.2026, 10:00 MEZ / 09:00 UTC, Europe/Berlin**.
Vorgemerkter Start: 09:59:10 MEZ; dies ist kein Ausführungs- oder
Zustellungsnachweis. Die Promptänderung ist bereits beauftragt und aktiv;
bis zum Dokumentationsmerge haben die im Prompt ausdrücklich geänderten
Betriebsregeln Vorrang vor den älteren Betriebsabschnitten am Integrationshead.
Fachmandate und Prüfpflichten gelten unverändert. Neue Kurzcheckdauer und
Nutzenwirkung sind noch nicht praktisch erprobt.

**Prüfung dieser Umstellung:** Getrennter Dokumentationsreview
(`responsibility_map`) und Verhaltensreview (`erstlauf_review`) im bestehenden
Chat; die vier Präzisierungen aus dem Verhaltensreview wurden zurückgeprüft.
136 lokale Verweise einschließlich Ankern geprüft, keine Fehler; Diff auf
fünf Dokumentationsdateien begrenzt. Keine erneuten Produkt-/Gerätetests;
PR-CI ist ein eigener Nachweis.

## Befugnisse, Meldungen und Rückweg

Lesen und eigene Laufentwürfe sind erlaubt. Keine Produkt-/Pin-/Fixture-/
Fach-/Rollen-/Hookänderungen, keine GitHub-Issues/Boards/Sync-Marker, fremden
Berichte oder anderen Chats ändern; keine Gerätehelfer, Installationen,
Commits, Pushes, PRs, Merges oder Releases durch den Wochenlauf.
Der ausdrücklich beauftragte Maintainerprozess behält sein separates Mandat.

Unveränderte nicht handlungsrelevante Ergebnisse bleiben still; melden bei
relevantem neuen Befund, wesentlicher Änderung, abgeschlossener offener
Folgearbeit oder neu nötiger Entscheidung. Keine neue Kontrollautomation.
Rückweg auf Nutzerauftrag: denselben Auftrag pausieren oder den früheren
Prompt aus dem Git-Stand `f81956f` wiederherstellen. Belege, Fachmandate,
CI und GitHub-Abgleich erhalten. [Bewertung und Übernahme](../../PROMOTION.md).
