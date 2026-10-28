# Projektfestlegungen: Übernahme und Korrektur

## 28. Oktober 2026 — Fachaudit nach Erstlauf verschlankt

- **Beauftragt:** Bestehenden Wochenauftrag verschlanken, Fachmandate und
  Prüfpflichten erhalten und messbare Nutzenkriterien verankern. Dokumentation
  in [PR #99](https://github.com/Sven401/Larp-Escape/pull/99) gegen den
  Integrationsbranch, Basis `f81956faffa4d4c6b0198fdbd0c626b21e137cd4`.
- **Beobachtete Anwendung:** Erster geplanter Lauf am 26.10.: 20 Minuten
  18 Sekunden Chatdauer, keine neuen/geänderten Findings und keine daraus
  angenommene Änderung; Dubletten, Zuständigkeiten und Wiedervorlagen in
  gesonderter Gegenprüfung bestätigt. Audit und Kontrolle stark verspätet,
  Ursache ungeklärt. Der Nutzen der manuellen Pilotkorrekturen wird dem
  Wochenbetrieb nicht zugerechnet. [Belege](docs/quality/fachaudit-betrieb.md#beobachteter-ausgangspunkt-und-umstellung).
- **Übernahme:** Kurzcheck höchstens fünf Minuten; nur bei belegtem Anlass
  eine Vertiefung, höchstens 30 Minuten insgesamt. Rotation/Sechswochenziel
  entfallen; echte Vertragsauslöser und quartalsweiser Normabgleich bleiben.
  Kompaktes Laufprotokoll statt Registerkopie, Fachgegenprüfung weiterhin
  bei Vertiefung. Keine neuen Rollen, globalen Regeln, Hooks oder Prüfwerkzeuge.
  Die aktive Promptumstellung ist im Betriebsbeleg getrennt vom noch offenen
  Dokumentations-PR nachgewiesen.
- **Nutzungsbewertung:** Nach vier abgeschlossenen geplanten Läufen, einschließlich
  26.10. als Ausgangsmessung, belegte Entscheidungen, angenommene Änderungen
  und geschlossene Nachweislücken getrennt zählen und Aufwand/Fehlalarme/
  offene Entwürfe gegenüberstellen. Frühere Betriebsbewertung bei Budget-
  oder wiederholten Quellenproblemen nach den
  [Zählregeln](docs/quality/fachaudit-betrieb.md#messbarer-nutzen-und-entscheidung-nach-vier-läufen).
  Die neue Kurzcheckdauer und Wirkung sind noch nicht erprobt.
- **Einfacher Rückweg:** Auf Nutzerauftrag denselben Auftrag pausieren oder
  den früheren Prompt aus `f81956f` wiederherstellen. Nachweise, Fachmandate,
  Prüfpflichten, CI und separaten GitHub-Abgleich erhalten. Kein automatischer
  Taktwechsel, keine zusätzliche Kontrollautomation.

## 24. Oktober 2026 — Integration geprüft und Fachaudit aktiviert

- **Beauftragt und erfolgt:** PR #97 nach zwölf erfolgreichen Checks in den
  Integrationsbranch übernommen; anschließend den vorhandenen Fachaudit nach
  Termin-/Zeitzonenprüfung aktiviert. Eigene Merge-CI auf `c582ed7` jetzt
  Quality 3/3 erfolgreich; [Prüfstand und Grenzen](handoff/MAINTAINER.md).
- **Rückgelesen:** `larp-escape-fachaudit` ACTIVE, geprüfter Prompt und Zielchat
  unverändert. Montag 10:00 Europe/Berlin; erster Solltermin 26.10.2026
  10:00 MEZ, vorgemerkter Start 09:58:11 MEZ. [Betriebsbeleg](docs/quality/fachaudit-betrieb.md)
  trennt Solltermin, Startversatz und noch nicht nachgewiesene Ausführung.
  Der bestehende GitHub-Abgleich bleibt unverändert.
- **Nutzungsbewertung bleibt offen:** Keine zusätzliche manuelle Auditprüfung
  durch die Aktivierung. Erste Ausführung kontrollieren, nach vier tatsächlich
  abgeschlossenen geplanten Läufen Nutzen/Takt bewerten. Befugnisse, Fachmandate
  und Geräte-/Security-Nachweisgrenzen bleiben unverändert.
- **Einfacher Rückweg:** Auf Nutzerauftrag denselben Wochenauftrag pausieren;
  Quellenberichte und Entscheidungen erhalten. Keine Ersatzautomation.

## 24. Oktober 2026 — Zweitlauf geprüft, Fachaudit-Auftrag pausiert vorbereitet

- **Beauftragt:** Zweiten unveränderten Audit-Probelauf am integrierten Stand
  durchführen, Dubletten/Zuständigkeiten/Wiedervorlagen prüfen und danach den
  regelmäßigen Betrieb vorbereiten. Keine Aktivierung beauftragt.
- **Beobachtete Anwendung:** [Zweitlauf mit eingefrorenem 2A/2B-Vergleich](docs/quality/audits/2026-10-24-probelauf-2/README.md)
  auf `cee968a7bf4c97eb68c9fe6f705bb5a06eef76b0`; fünf vorhandene Finding-IDs
  und #39/B7 wiedererkannt, keine neue/doppelte Finding-ID. 14 Quellenhashes,
  Auftragshash, Zuständigkeiten und sieben manuelle Gegenfälle gegengeprüft.
- **Nutzungsbewertung:** Historische offene Pilottexte öffnen korrigierte
  Software nicht erneut. Quellenzugang und Hardwaregrenze bleiben getrennt.
  Die ausdrücklich fehlende Geräte-Wiedervorlage von A2-02 wird in der
  Vorbereitung an das vorhandene A2-03-Adapterfreigabetor angeschlossen.
  Keine neue Frist oder physische Abnahme behauptet.
- **Vorbereitung nach Anwendung:** [Betriebsentwurf](docs/quality/fachaudit-betrieb.md)
  und [Prompt](docs/quality/fachaudit-automation-prompt.md) unabhängig geprüft.
  Datierter Startstatus und stille unveränderte Laufabschlüsse präzisiert.
  Genau eine chatgebundene Automation `larp-escape-fachaudit` mit **PAUSED**
  angelegt und Prompt zurückgelesen; bestehender GitHub-Abgleich unverändert.
- **Übernahmegrenze:** Kein neuer allgemeiner Skill, Hook oder Agentenstandard;
  Fachmandate unverändert. Manueller Replay ist keine automatische
  Dublettengarantie und kein Scheduler-/Sandbox-/Gerätenachweis. Vor Aktivierung
  Vorbereitung integrieren und nächsten Termin/Zeitzone prüfen. Nach vier
  geplanten abgeschlossenen Läufen Nutzen und Takt neu bewerten.
- **Einfacher Rückweg:** Denselben Auftrag pausieren; Entwürfe und Belege
  erhalten. Mandate, Produktprüfungen und GitHub-Abgleich bleiben bestehen.

## 24. Oktober 2026 — Fachmandate und Prüfpflichten verbindlich übernommen

- **Bestätigt:** Nach dem unten dokumentierten A1/A2-Pilot hat der Nutzer die
  Übernahme der Mandate und verbindlichen Prüfpflichten ausdrücklich beauftragt;
  anschließend sollen die Laufzeitbefunde korrigiert werden. Quelle: Folgeauftrag
  in Sitzung `01a16ab1-34d7-73b0-924a-fc2d245b9027`, Dokumentationsbasis
  `dc0e3f12b571c534ef258300a47eab8a917387df`.
- **Beobachtete Anwendung und Bewertung:** Der
  [Pilot](docs/quality/audits/2026-10-24-pilot/README.md) ordnete RTOS-Kontext-
  und Gerätefehlergrenzen konkreten Fachmandaten zu. Getrennte Gegenprüfung,
  Hostdiagnose und PN532-Regressionen lieferten überprüfbare Folgen. Das belegt
  einen nützlichen Einsatz, keine langfristige Effizienz oder Geräteabnahme.
- **Übernommen:** [Fachverantwortung](docs/quality/verantwortlichkeiten.md)
  mit Plattform/Systemintegration, Geräteintegration/Hardwarevertrag und
  unabhängiger Nachweisprüfung. [AGENTS.md](AGENTS.md) verlangt ihre Anwendung,
  benannte Verantwortliche/Gegenprüfer und passende Nachweise je Auftrag.
  Die LED- und UI/UX-Rollen behalten ihren vollständigen Auftrag; gemeinsame
  technische Grenzen erhalten gezielte Fachreviews. Security-Expertise wird
  für tatsächlich betroffene Fragen ausdrücklich zugeordnet.
- **Grenze:** Kein neuer Scheduler, dauerhafter Agent, globaler Skill oder Hook.
  Der Koordinator-/Automationsentwurf bleibt für Hintergrundbetrieb offen.
  Diese Übernahme schließt A1-01/A2-01 nicht ab; Laufzeitkorrekturen, aktueller
  Prüflauf und deren Review folgen gesondert. Historische Pilotbelege bleiben erhalten.
- **Einfacher Rückweg:** Die verbindliche Verankerung in AGENTS und die neuen
  Rollenanschlüsse nach ausdrücklicher Änderung des Auftrags zurücknehmen,
  Fachmandate als inaktiv kennzeichnen und Pilot-/Entscheidungshistorie erhalten.
  Rücknahme der Rollen aktiviert keine alte Automatisierung und entfernt keine
  fachlich weiterhin nötigen Tests. Änderungen an Prüfschritten oder Produktcode
  sind getrennt zu begründen und zu prüfen.


## 24. Oktober 2026 — Historischer Übernahmevorschlag nach dem A1/A2-Pilot

- **Ausdrücklich beauftragt:** Verantwortlichkeiten hinterfragen, ESP32/FreeRTOS
  und Adafruit manuell auditieren und konkrete Rollen-/Prüffolgen zeigen.
  Sitzung `01a16ab1-34d7-73b0-924a-fc2d245b9027`.
- **Beobachtete Anwendung:** [Pilot](docs/quality/audits/2026-10-24-pilot/README.md)
  auf Codebasis `e61d7642d035ccdcf8eca689d6c50a82bcf1560b`. Zwei Fachaufträge,
  getrennte Rollenprüfung, reale RTOS-Hostdiagnose, neue PN532-Regressionen.
- **Bewertung:** Plattform- und Geräteintegrationsmandat ordnen zwei konkrete
  Grenzprobleme zu; eine bekannte Parser-/Patch-Prüflücke wird geschlossen.
  Supportstatus und SPI-Bool-Grenze aktualisieren bestehende Fachreferenzen.
  Kein Nachweis langfristiger Zeitersparnis, keine externe Zertifizierung.
- **Übernahmevorschlag:** [Fachmandate](docs/quality/verantwortlichkeiten.md)
  bei passender beauftragter Arbeit nutzen; PR #95 enthält prüfbare Dokumente
  und Tests. Bestehende LED-/UI-Rollen bleiben durchgängig verantwortlich.
  Kein neuer Agentenprozess, Zeitplan, globaler Skill oder Hook eingerichtet.
  Die Diagnose ist ein datierter Befundbeleg und kein allgemeines CI-Gate.
- **Einfacher Rückweg:** neue Rollenverweise inaktiv setzen, Vorschlag und
  Befundhistorie erhalten. Neue PN532-Tests und einzelnen CI-Schritt getrennt
  zurücknehmen; dann Prüflücke ausdrücklich wieder öffnen. Firmwarecode,
  Bibliothekspins, bestehender Codex-Heartbeat und persönliche Hooks sind unverändert.


## 20. Oktober 2026 — GitHub-Abgleich auf aktuelle Projektquellen umgestellt

- **Ausdrücklich beauftragt:** bestehenden Heartbeat pausieren, letzte Änderungen
  prüfen, Quellen umstellen, lesend testen und danach wieder aktivieren.
- **Beobachtete Anwendung:** [Audit und Probelauf](handoff/GITHUB-AUTOMATISIERUNG-2026-10-20.md)
  an Integrationsbasis `97a2584cc108ebb9c0f667fc46f898d163e00144`;
  23 Issues verglichen, 16 Schutzfälle bestanden, keine Issue-/Boardmutation.
  Wechsel der Quellbasis während der Prüfung erkannt und neu abgeglichen.
- **Bewertung/Übernahme:** neue Quellenführung liefert aktuelle Teilfortschritte,
  bewahrt Alttext und isoliert den fachlichen Widerspruch #34. Der bestehende
  lokale Auftrag ist nach der Leseprobe wieder aktiv. Keine bewiesene
  langfristige Zuverlässigkeit; geplanter Lauf und produktiver Schreibweg
  bleiben unbeobachtet. Das Prüfprogramm bleibt datierter Nachweis und wird
  weder allgemeiner Standard noch ausführender Sync-Client.
- **Einfacher Rückweg:** denselben Auftrag pausieren, Konfiguration/Belege
  erhalten und betroffene Ergebnisse prüfen. Alte KI-Werkbank-Anweisungen
  nicht reaktivieren. Keine persönlichen Skills oder Hooks geändert.

## 18. Oktober 2026 — LED-Rolle 2.0 nach realer Iteration

- **Auftrag erweitert:** Nutzer verlangt Idee bis praktisch verfeinerte Umsetzung
  mit früher serieller/Kamera-Iteration. Die Beschränkung der Fassung 1.0 ist damit
  ersetzt; die historische Nutzungsbewertung bleibt unten erhalten.
- **Beobachtete Anwendung:** [Näheglut-Bericht](docs/led/naeheglut-live-results-2026-10-18.md)
  mit einem Firmwareimage und drei frühen Kameraläufen vor Commit: Farbauswahl,
  Kurvenvergleich, zwei Instanzen, Parameteränderungen, gezielter/globaler Stop.
  Vorschau-Lock im ersten Lauf aufgedeckt und behoben; zwei Folgeläufe vollständig.
- **Bewertung/Übernahme:** [Rolle 2.0](docs/led/rollen/led-effekt-architekt.md) und
  [interaktiver Weg](docs/led/interactive-development.md) sind ein nützlicher
  Projekteinstieg. Gemeinsamer Kern statt Kamera-Sonderalgorithmus, Vorschaufehler
  getrennt von Aufnahmebelegen, offene Aufträge beim Abschluss explizit und
  gestalterisches Feedback von metrischer/Produktabnahme getrennt. Eine Anwendung
  belegt keinen dauerhaften Zeitgewinn. Kein globaler Skill und keine Hookänderung.
- **Gültigkeit:** Sitzung `01a14f9a-35fb-72e1-9acb-132b0d16599d`, Basis `aa887f5`,
  Quell-/Binaryhashes und aktuelle Prüfwege im Bericht. Originalfirmware und voller
  Flash verifiziert wiederhergestellt. Produktadapter und optisches Nutzerurteil offen.
- **Einfacher Rückweg:** Rollen-/Anleitungsverweise im Maintainer-Einstieg inaktiv
  setzen; ursprüngliche CAL/RUN-Laborroute weiterverwenden. Neue LIVE-Route und
  NearGlow-Dateien lassen sich getrennt entfernen. Produkt-USB/BLE wurden nicht
  erweitert; die frühere Installation ist bereits wiederhergestellt. Fachquellen,
  Entwurf und historische Versuchsbelege erhalten.


## 18. Oktober 2026 — LED-Effekt-Architektenrolle nach erster Anwendung

- **Beauftragt und angewandt:** Projektgebundenes
  [Rollenprofil 1.0](docs/led/rollen/led-effekt-architekt.md) für Verhalten,
  Berechnung, Prüfplanung und den fachlichen Spiel-/Effektvertrag.
  Quelle: Nutzerauftrag und Antworten in Sitzung
  `01a14f9a-35fb-72e1-9acb-132b0d16599d`; Ausgangsbasis `dead12040c196ff74ab4be17ba71451a9351a676`.
- **Beobachteter Einsatz:** Kandidat 0.1 auf den neuen Näheglut-Entwurf angewandt;
  [Erprobungsbericht](handoff/LED-EFFEKT-ARCHITEKT-ERPROBUNG-2026-10-18.md)
  belegt 23 Modelltests und unabhängigen Review. Stop während Ausblenden,
  fortlaufende Zieländerungen, Gesamt-Aus und Ticket-/Instanzleben präzisiert.
- **Bewertung:** Quellenrouter und Pflichtfragen deckten konkrete
  Mehrdeutigkeiten auf; erste nützliche Anwendung, keine gemessene langfristige
  Zeitersparnis oder optische Freigabe.
- **Übernahme:** Rollenprofil bei beauftragter LED-Effektentwurfsarbeit als
  Einstieg nutzen. Keine Änderung an AGENTS, globalen Skills oder Hooks.
  Fachvertrag bleibt Entwurf; die einmalige Python-Probe wird nicht zum
  allgemeinen Produktwerkzeug oder CI-Standard befördert.
- **Einfacher Rückweg:** Verweise auf das Rollenprofil aus den beiden
  Maintainer-/Übergabeeinstiegen entfernen und Profil als inaktiv markieren.
  Fachquellen, Entwurf und Prüfbelege bleiben erhalten; keine Firmwareänderung
  muss rückgängig gemacht werden.

## 15. Oktober 2026 — Fehlinterpretation zur LED-Grundlage zurückgenommen

- **Bestätigt:** Der Projekt-Owner hat klargestellt, dass WLED ausschließlich
  eine externe Beispielimplementierung ist. Die verbindliche Grenze steht in
  [LED-Funktionalität: externe Beispielimplementierung](docs/led/implementation-reference.md).
  Quelle: Klarstellungsauftrag in Codex-Sitzung
  `01a13c85-2371-7bd1-8b80-37104cb58c04`.
- **Korrektur:** Codex hatte den vorherigen Auftrag in `54933ea` fälschlich als
  Einbindungsvorgabe dokumentiert und diese dem Owner zugeschrieben. Diese
  Deutung ist zurückgenommen; es handelt sich nicht um einen nachträglichen
  Wechsel der Produktentscheidung des Owners.
- **Durchgeführter Rückweg:** Übernahme-, Migrations-, Usermod- und
  Schnittstellenpflichten entfernt; den Zusatz zum Laufzeitvertrag
  zurückgenommen; die Quellen als externe Beispiele eingeordnet. App und
  Firmware wurden in diesem Dokumentationsbranch nicht verändert.
- **Bewertung:** Der Quellenvergleich ist als Lernmaterial nützlich. Daraus
  folgt keine Freigabe zur Nutzung als Projektkomponente. Abweichende
  Implementierungsbranches und installierte Software benötigen eine eigene
  Korrektur; dieser Eintrag behauptet deren Bereinigung nicht.

## 18. Oktober 2026 — UI/UX-Rolle und erster Box-Pilot

- **Ausdrücklich beauftragt:** Nutzer verlangt Einrichtung der vorgeschlagenen
  UI/UX-Rolle und Start des Box-Piloten, Sitzung
  `01a1505d-dfd4-7ad2-89cf-8d7be1b02240`. Die explizite Rollenübernahme ist die
  genehmigte Ausnahme vom vorherigen Erprobungstor; keine allgemeine Hookänderung.
- **Übernahme:** [Rollenvertrag](docs/ui-ux/ROLLE.md), Projektagent und
  projektgebundener Skill mit AGENTS-Verweis. Keine persönlichen globalen
  Projektfakten; kein neuer MCP-Server und keine Modellfestlegung.
- **Erste Anwendung:** [Brief und Zustandsmodell](docs/ui-ux/BOX-PILOT.md) auf
  `7e72411bb1c416b5864036c79a1e17d9cf46b5ac` gegen MainActivity, Repository,
  ViewModel, LED-Zustand und BLE-Sitzung geprüft. Zugangsprüfung, Frische,
  unklarer Ausgang und bewusste Wiederaufnahme getrennt. Kein verbundenes
  Android-Gerät, keine sichtgeprüfte App und kein Gerätetest.
- **Vorläufige Nutzungsbewertung:** Rollenverfahren macht Unterschiede zwischen
  Bestand, Architektur-Soll und Gestaltungsvorschlag prüfbar. Optische Qualität,
  Bedienbarkeit und dauerhafte Zeitersparnis sind noch offen; Nutzerbewertung
  folgt erst nach gewähltem Entwurf und Prototyp.
- **Einfacher Rückweg:** UI/UX-Verweis in AGENTS entfernen, Projektagent und
  projektspezifischen Skill deaktivieren/entfernen. Brief und Belege historisch
  erhalten. App-/Firmwarecode und persönliche Hooks wurden nicht verändert.

## 18. Oktober 2026 — UI/UX-Pilot bis zum interaktiven Entwurf

- **Beauftragt:** Nutzer bestätigt Veranstaltung/Station als Kontext und verlangt
  Figma plus gesamten interaktiven Pilotablauf; gleiche UI/UX-Sitzung wie oben.
- **Beobachtete Anwendung:** [Prototyp](design/box-pilot/README.md) mit getrennten
  Zustandsregeln, 21 nativen Figma-Frames, Browserbedienung und gemeinsamem
  Quellen-/Umsetzungsvergleich. Neun Regeltests und vier Packagingtests bestanden;
  Typprüfung, Build und Integrität der 28 Vorlagendateien erfolgreich.
- **Nutzungsbewertung:** Das Verfahren deckte einen Scroll-/Kopf-Überlauf und
  überdeckende Teststeuerung auf; beide in der Appanbindung korrigiert.
  Befehlsbestätigung und unklarer Ausgang sind sichtbar getrennt. Keine gemessene
  langfristige Zeitersparnis und keine native Nutzer-/Geräteabnahme behauptet.
- **Übernahmegrenze:** Kein Ausbau zum globalen Skill oder neuen allgemeinen
  Werkzeugstandard. Simulationskontrollen bleiben außerhalb der Produktoberfläche.
- **Rückweg:** Prototypordner separat entfernen/deaktivieren; Figma-Datei behalten
  oder gesondert archivieren. Android und Firmware bleiben unverändert.


## Native IDF-Quellenprüfung · 28.10.2026

**Projektwerkzeug, angewandt und bewertet:** `scripts/check_idf_sources.py`
wurde beim nativen Host-/SDK-Aufbau wiederholt ausgeführt; es erkannte einen
unvollständigen lokalen SDK-Checkout. Nach Wiederherstellung und korrekter
Git-Pfadbehandlung akzeptiert es den exakten6.1-Pin. Sieben Tests mit echten
isolierten Git-Repositories prüfen sauberen Stand sowie falschen HEAD,
fehlendes/verschobenes Submodul, tracked Haupt-/Untermoduländerungen und
falschen Gitroot. [Auftrag, Ergebnis und Grenzen](docs/migration/esp-idf-qualification-2026-10-28.md).

Nutzen: Versionsstring allein genügte nicht; qualifizierte Quellen müssen beim
Konfigurieren nachvollziehbar sein. Nur native Qualifikationsziele binden den
Wächter ein, keine globale Regel, kein Hook und keine Automation. Untracked
SDK-Dateien erfasst er nicht. Rückweg: native Qualifikation samt neuer CI und
Guard-Einbindung gemeinsam zurücknehmen; bestehende Legacypins bleiben erhalten.
Zukünftige SDK-Pinänderungen benötigen erneute Qualifikation, kein Guardlöschen
zum Erreichen eines grünen Builds. Quelle: Migrationsauftrag dieser Sitzung.
