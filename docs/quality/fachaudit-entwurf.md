# Entwurf: Fachwissen durch gezielte Audits aktuell halten

**Status: Entwurf zur Ergänzung; nicht eingerichtet oder aktiviert.**
Datum: 23.10.2026, Europe/Berlin. Auftrag: Automatisierungen untersuchen und
einen dedizierten Audit-Agenten für die bestehenden Verantwortlichkeiten entwerfen.
Technische Basis: `e61d7642d035ccdcf8eca689d6c50a82bcf1560b`, am 23.10.2026
live geprüfter Integrationsbranch `plan/projektuebergabe-kuratierter-stand`.
[Bestandsaufnahme und Prüfgrenzen](../../handoff/AUTOMATISIERUNGSINVENTAR-2026-10-23.md).

## 1. Empfehlung

Ein projektgebundener **Fachaudit-Koordinator** erhält begrenzte Rechercheaufträge
und delegiert sie nach Fachgebiet. Er vergleicht offizielle Quellen mit der
tatsächlich verwendeten Version, Architektur und bisherigen Fachreferenz.
Die bestehende Fachrolle prüft die Übertragbarkeit; der Maintainer führt die
angenommene Änderung durch den normalen Projektprozess bis zum Pull Request.

```mermaid
flowchart LR
  A[Quelle oder Projektstand geändert] --> B[Rechercheauftrag]
  B --> C[Beleg und Projektbezug]
  C --> D[Fachliche Gegenprüfung]
  D --> E[Annehmen, zurückstellen oder ablehnen]
  E --> F[Fachreferenz und Prüfung ändern]
  F --> G[Nachprüfen und Übernahme belegen]
  G --> H[Bestehender Fortschrittsabgleich]
```

Die heutigen Builds und Integritätsprüfungen bleiben notwendig. Ihr Bestehen
beantwortet jedoch nicht, ob sich inzwischen relevante Herstellerempfehlungen,
Sicherheitsbefunde oder Qualitätsmaßstäbe geändert haben. Eine neue
Versionsnummer allein begründet weder einen Defekt noch eine Updatepflicht.

## 2. Verantwortlichkeiten

**Ausgelesen:** Formalisiert sind Maintainer, LED-Effekt-Architekt und UI/UX Designer.
Die übrigen Fachgebiete besitzen Verträge und Arbeitspakete, aber keine im
untersuchten Stand eingerichteten eigenen Agentenprofile. Ein technischer
Owner im Firmwarecode bezeichnet häufig einen Task/Ressourcenbesitzer und
damit noch keinen fachlichen Reviewverantwortlichen.

| Fachgebiet | Empfänger des Audits | Kanonischer Anschluss und Prüffragen |
| --- | --- | --- |
| LED-Effekte und Lichtausgabe | Bestehender [LED-Effekt-Architekt 2.0](../led/rollen/led-effekt-architekt.md); Maintainer bei gemeinsamen Ressourcen | [Effektvertrag](../led/effect-command-contract.md), [Laufzeit](../runtime.md): Instanzleben, Mischung, Stop, Zeitbasis, RMT-Puffer, sichtbare Wirkung. WLED nur als Verhaltensvergleich. |
| UI/UX und Zugänglichkeit | Bestehender [UI/UX Designer](../ui-ux/ROLLE.md); Android-Umsetzung durch Maintainer zuordnen | [Projektreferenz](../ui-ux/PROJEKTREFERENZ.md), [Stationspilot](../companion/spielleiter-stationspilot.md): Zustände, Unterbrechung, TalkBack, Schrift und bewusste Aktionen. Quellenarbeit ist kein visueller Review. |
| ESP32, FreeRTOS und Frameworkmigration | Maintainer benennt einen Firmware-Fachreview | [Device Runtime](../configuration/device-runtime.md), [Migration](../../handoff/ARDUINO-ESP32-3-MIGRATION.md): konkreter Espressif-Port, Stackeinheiten, Task-/ISR-Kontexte, Queues, Fristen, ABI. |
| NFC, Wake, Matrix und elektrische Grenzen | Maintainer mit zugeordnetem Firmware- und Hardware-Fachreview | [NFC-Vertrag](../nfc-packet-contract.md), [Hardwareplanung](../../Hardware/HARDWAREPLANUNG.md): Version/Patch, Busbesitz, Fehlerpfade, Chipidentität und IO7/B7. |
| BLE, Zugang, Kryptografie und verteilte Zustände | Maintainer mit Security-/Protokoll-Fachreview | [Zugang](../companion/veranstaltungszugang.md), [Peer Proof](../companion/peer-proof-v1.md), [D0](../veranstaltung/d0-vertrag-v1.md): Support, Advisories, private ABI, Replay, Sitzungen und lokale Autorität. |
| Abhängigkeiten, CI und Lieferkette | Maintainer | [Prüfwege](../quality-checks.md), Manifeste und Workflows: Herkunft, Pins, Support, Prüfabdeckung, reproduzierbare Eingänge und Aussagegrenzen. |
| Spielpakete, Mehrboxbetrieb und Produktqualität | Maintainer; Produktentscheidungen beim Nutzer | Live-Plan [#25](https://github.com/Sven401/Larp-Escape/issues/25). SP-01 ist beschlossen; Dokumentation in [#94](https://github.com/Sven401/Larp-Escape/pull/94) war beim Lesen noch nicht integriert. Entscheidung, Quellintegration und Abnahme trennen. |

Die zusätzlichen Fachreviews in der Tabelle sind **vorgeschlagen**, keine
Behauptung bereits eingerichteter Rollen. Ein Auftrag ohne Empfänger geht
zunächst an den Maintainer. Der Auditor kann seine eigenen Befunde nicht
allein als unabhängig gegengeprüft freigeben. Ein separater Reviewdurchgang
ohne organisatorische Unabhängigkeit muss diese Grenze ausdrücklich nennen.

## 3. Quellen und Maßstäbe

Jede Quelle erhält eine Klasse. Offiziell bedeutet nicht automatisch für
dieses Produkt verpflichtend. Edition, Status und Anwendbarkeit werden vor
einer Folgerung geprüft; Suchtreffer allein reichen nicht.

| Quellenklasse | Verwendung und Grenze |
| --- | --- |
| Herstellerspezifikation / API-Vertrag | Für nachgewiesenen Chip, Port und Version prüfen. Bibliotheksbeispiele heben Datenblattgrenzen nicht auf. |
| Internationaler Qualitätsstandard | [ISO/IEC 25010:2023](https://committee.iso.org/standard/78176.html) ist ein veröffentlichtes Produktqualitätsmodell. Hier wurde nur die öffentliche Beschreibung gelesen; keine vollständige Klauselprüfung oder Konformität behauptet. Eine verbindliche Projektauswahl bleibt offen. |
| Offizieller Empfehlungsrahmen | [NIST SSDF](https://csrc.nist.gov/projects/ssdf/publications) für sichere Entwicklung, [Android Core App Quality](https://developer.android.com/docs/quality-guidelines/core-app-quality) für Appqualität. Im abgerufenen NIST-Register: 1.1 Final, 1.2 Draft. |
| Zugänglichkeitsstandard | [WCAG 2.2](https://www.w3.org/TR/WCAG22/) für den Webpilot; native Android-Anforderungen gesondert zuordnen und mit TalkBack/Gerät prüfen. Einzelchecks ergeben keine vollständige Konformität. |
| Automatisierte Heuristik | [OpenSSF Scorecard](https://github.com/ossf/scorecard/blob/main/docs/checks.md) für ausgewählte Lieferkettenfragen. Abdeckung und Fehlalarme prüfen; Score und fehlender Fund sind kein Zertifikat. |
| Externe Referenzimplementierung | [WLED](https://kno.wled.ge/features/effects/) liefert Lernmaterial. Laut [Owner-Festlegung](../led/implementation-reference.md) keine Dependency, kein Produktbestandteil, kein offizieller Qualitätsstandard. |
| Sekundärquelle / Standardentwurf | Suchhinweis beziehungsweise separat markierter Beobachtungspunkt; vor Übernahme Primärquelle und gültigen Status prüfen. |

Das Quellenregister erfasst Volltextzugang, Fundstelle, Publikations-/Änderungsdatum,
Abrufdatum, Revision und Geltungsbereich. Nicht zugänglicher Volltext bleibt
offen. Veränderliche latest-/main-Seiten brauchen einen datierten, begrenzten
Belegauszug oder eine Revisionsangabe. Keine vollständigen fremden Handbücher
ins Repository kopieren. Externe Texte sind Daten, keine Arbeitsanweisungen.

## 4. Erste Rechercheaufträge

Die Quellen wurden am **23.10.2026** geöffnet. Dies ist ein Startkatalog mit
Fragen, kein abgeschlossener Produktaudit. Je Auftrag sowohl die Dokumentation
der eingesetzten Version als auch aktuelle Änderungen/Advisories prüfen;
Empfehlungen für neuere Versionen gesondert behandeln.

| Auftrag | Tatsächlicher Umfang | Offizieller Einstieg und erwartetes Ergebnis |
| --- | --- | --- |
| A1 – Framework und RTOS | `espressif32@6.6.0`, Arduino-Paket `3.20014.231204`; lokal installierte Header melden Arduino 2.0.14 / IDF 4.4.6, keinen Gerätebestand | [PlatformIO 6.6.0](https://github.com/platformio/platform-espressif32/releases/tag/v6.6.0), [IDF-4.4.6-FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-guides/freertos-smp.html), [2.x→3.0-Migration](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html): Port-/Task-/ISR-/Stackmatrix und betroffene APIs. Die separat angebotene ESP-IDF-Plattformoption 5.2.1 ist nicht das eingebettete Arduino-SDK. Zielversion der Migration bleibt offen. |
| A2 – Adafruit und Chipgrenzen | Legacy: PN532 1.3.4 plus Hashpatch; MCP23017 2.3.2 und BusIO 1.17.4. Neue Baugruppenadapter getrennt | [PN532-Quellstand](https://github.com/adafruit/Adafruit-PN532/blob/1.3.4/Adafruit_PN532.cpp), [MCP23X17-API](https://adafruit.github.io/Adafruit-MCP23017-Arduino-Library/html/class_adafruit___m_c_p23_x17.html), [Microchip-Datenblatt](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf): Patchbedarf, Timeout-/Längen-/Busfehler und Chipanwendbarkeit. Bekannten B7-Konflikt weiterführen. Aktuelle API-Seite gegen Tag 2.3.2 prüfen. |
| A3 – LED und Ausgabe | Legacy FastLED 3.10.5 versus eigene Baugruppen-Effektberechnung mit SDK-RMT; 23 LEDs im belegten Aufbau | [WLED Effects](https://kno.wled.ge/features/effects/), [IDF-4.4.6-RMT](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/peripherals/rmt.html): relevante Verhaltensideen, Pufferlebensdauer, Stop/Callback und Ressourcen. Kein WLED-Codeimport; optisches Urteil getrennt. |
| A4 – Besitzer, Protokolle und Security | Aufrufpfade, SDK-Binärpins, Host-Mbed-TLS 2.28.4, BLE und Mehrboxverträge | [IDF-4.4.6-SPI](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/peripherals/spi_master.html), [SSDF](https://csrc.nist.gov/projects/ssdf/publications): Ownership und Sicherheitsannahmen zuordnen; konkrete Advisories/Supportinformationen zusätzlich recherchieren. SDK-SPI-Regeln nur bei nachgewiesenem Aufrufpfad auf Adafruit übertragen. Host-Kryptografie und Firmware-SDK trennen. |
| A5 – Android und UI/UX | Dev und GM; Versionskatalog, Lifecycle, BLE/NFC, native Bedienung; Webpilot separat | [Android Core App Quality](https://developer.android.com/docs/quality-guidelines/core-app-quality), [WCAG 2.2](https://www.w3.org/TR/WCAG22/): Leitlinie → realer Zustand → Sicht-/Gerätetest. Offene TalkBack-/Mehrstationsfälle erhalten. Play-Regeln nur bei passendem Distributionsziel. |
| A6 – Prüfprozess und Lieferkette | Workflows, Python-/Node-/Gradle-/PlatformIO-Abhängigkeiten und lokale Helfer | [Scorecard-Checks](https://github.com/ossf/scorecard/blob/main/docs/checks.md), [ISO-Qualitätsmodell](https://committee.iso.org/standard/78176.html): Qualitätsfragen, vorhandene Nachweise und Lücken. Formatabdeckung nachweisen; keine angenommene PlatformIO-Abdeckung. |

Zuerst ein Komponentenregister aus allen Manifesten und aktiven Varianten
bilden. Auch Bounce2, EspSoftwareSerial, DFPlayerMini_Fast, FireTimer, ghostl,
Unity, OpenCV, NumPy, pyserial und transitive Abhängigkeiten gehören hinein.
Manifestlistung allein belegt keinen Einsatz in jedem Produktbild. Aktiver
Einsatz, Fehlerrisiko, Supporthinweis und anstehende Änderung bestimmen die Tiefe.

Empfohlener Pilot: **A1 und A2**. Port-/Versionsverwechslung, private ABI,
PN532-Patch und Chipkonflikt bieten konkrete Anschlüsse. A3 prüft anschließend,
ob der Ablauf auch gestalterische Expertise aktualisiert, ohne Referenz und
Qualitätsnorm zu vermischen.

## 5. Auftrags- und Befundvertrag

Ein Rechercheauftrag nennt Kennung, konkrete Frage, Empfänger/Fachreview,
Codecommit, Variante, aufgelöste Version und gegebenenfalls Chiprevision.
Er enthält bisherige Fachreferenz, letzten geprüften Quellenstand, bekannte
Findings, Nutzerentscheidungen, aktuelle Issue-/PR-Zuordnung, Primärquellen,
Arbeitsbudget, erwartetes Ergebnis und erlaubte Werkzeuge. Fehlendes bleibt offen.

| Befundfeld | Pflichtinhalt |
| --- | --- |
| Identität | Stabile ID; Wiedererkennung über Komponente, Regel/Fundstelle und Variante; Vorgänger/ersetzter Befund |
| Aussage und Quelle | Präzise Aussage, URL/Fundstelle, Revision, Quellenstatus, Abrufdatum; eigene Schlussfolgerung getrennt |
| Anwendbarkeit | Version, Plattform, Codepfad/Commit; warum die Aussage hier gilt oder nicht gilt |
| Bewertung | Auswirkung, Eintrittsbedingungen, Dringlichkeit und Belegsicherheit getrennt; kein Risiko allein aus Bibliotheksalter |
| Differenz | Bisherige Regel/Nachweis, externer Maßstab, konkrete Lücke; auch bereits erfüllt/nicht anwendbar begründen |
| Handlung | Empfänger, Änderung mit Zielpfad/Abschnitt, nötiger Test/Messung, Aufwandklasse, Rückweg |
| Lebenslauf | Entscheidung und Begründung, entscheidende Rolle, Datum, Folgeauftrag/PR, integrierter Commit, Verifikation |

Zwei Achsen verhindern eine stille Aufwertung: **Belegstatus** (`ausgelesen`,
`belegt`, `abgeleitet`, `vermutet`, `offen`) und **Bearbeitungsstatus** (`neu`,
`fachlich geprüft`, `angenommen`, `zurückgestellt`, `abgelehnt`, `umgesetzt`,
`verifiziert`, `ersetzt`). Zurückstellungen brauchen Verantwortlichen und
Wiederprüfungstermin/Auslöser; auch Ablehnungen bleiben wiederauffindbar.

Beispiel **A1-VERSION, abgeleitet, neu**: Die Plattformrelease-Seite bietet zwei
Frameworks; die lokal gelesenen Arduino-Header melden IDF 4.4.6. Vorschlag an
Maintainer/Firmware-Fachreview: Plattform, Framework und eingebettetes SDK im
Register getrennt erfassen. Keine Behauptung eines Produktfehlers, keine
Pinänderung. Verifikation an tatsächlich verwendeter Buildumgebung und beim
nächsten relevanten Build.

## 6. Wirksame Übernahme in die Expertise

1. **Recherche:** Eigener Befund mit Primärquelle und genauem Projektbezug.
2. **Fachreview:** Zuständige Rolle prüft Ableitung, Versionspassung und
   Wechselwirkungen. Bekannte Fälle verlinken, Unklarheiten erhalten.
3. **Entscheidung:** Maintainer nimmt technische Folgearbeit im Mandat an,
   stellt sie zurück oder lehnt sie begründet ab. Produktentscheidungen,
   Änderungen von Abnahmekriterien und Releases bleiben beim Nutzer.
4. **Übernahme:** Bestehende kanonische Fachreferenz ändern; Rollenrouter und
   Checkliste verweisen darauf. Kein paralleler Wissensbestand in globalen Skills.
5. **Anwendung:** Passende Änderung und Prüfung im eigenen Branch durchführen.
   Allgemeine Arbeitsregeln/Hooks erst nach beobachtetem Einsatz, Bewertung
   und Rückweg in [PROMOTION.md](../../PROMOTION.md) dauerhaft übernehmen.
6. **Verifikation:** Review verknüpft Finding, Änderung und tatsächlichen
   Nachweis. PR-Veröffentlichung ist keine Integration; Merge keine
   Geräteabnahme. Fehlende notwendige Messung bleibt offen.

Der bestehende [GitHub-Abgleich](../../handoff/GITHUB-AUTOMATISIERUNG-2026-10-20.md)
übernimmt danach belegten Fortschritt in seinem bisherigen Umfang. Neue Issues,
Zuständigkeiten, Prioritäten und Kriterien liegen außerhalb seines Auftrags.
Neue Auditfolgearbeit wird durch den gesondert beauftragten Maintainerprozess
eingeordnet. Kein direkter Schreibkanal vom Recherche-Agenten in Sync-Marker.

## 7. Automationsbetrieb nach erfolgreichem Pilot – Vorschlag

**Eine zusätzliche chatgebundene Automatisierung** wählt fällige Fachgebiete
und nutzt kurzlebige Recherche-Subagenten. Standardmodell und bestehende
Berechtigungen übernehmen. Kein eigener dauerhafter Zeitplan je Bibliothek.

| Anlass | Vorgeschlagener Umfang |
| --- | --- |
| Wöchentlich, Montag 10:00 Europe/Berlin | Offizielle Releases, Advisories, Herstellerdokumente und fällige Quellen prüfen; maximal 30 Minuten und zwei vertiefte Fragen. Zeit/Zeitzone bei Einrichtung anhand des nächsten geplanten Laufs verifizieren. |
| Rotation | A1–A6 innerhalb sechs Wochen prüfen; unbearbeitete Gebiete sichtbar führen. Stabile Normeditionen quartalsweise auf Revision/Errata prüfen. |
| Projektänderung | Pin, PN532-Patch, SDK-Hash, Besitzer-/Protokollvertrag oder Migration geändert: gezielte Frage beim nächsten Koordinatorlauf per Git-Diff oder auf direkten Auftrag. Kein bereits installierter Webhook. |
| Kritischer Befund | Bei Entdeckung sofort mit Anwendbarkeit melden. Ein Wochenlauf garantiert keine kontinuierliche Überwachung. |
| Vor Migration oder Produktfreigabe | Betroffene Gebiete neu prüfen; alter Wochenbericht ersetzt keine Freigabeprüfung. |

Budgetende erzeugt einen unvollständigen Lauf mit offenem Rest, keine
pauschale Entwarnung. Für lokale Aufgaben müssen Rechner und App laufen.
Manuellen Probelauf und Kontrolle der ersten Ausführungen empfiehlt auch die
[offizielle Automationsdokumentation](https://learn.chatgpt.com/docs/automations?surface=app).
Ein verpasster Lauf bleibt als Lücke sichtbar.

**Berechtigungen des vorgeschlagenen Wochenlaufs:** Quellen, Repository und
GitHub lesen; eigene Befundentwürfe und Laufnachweise im vereinbarten
Projektbereich ablegen. Keine Produktdateien, Pins, Fixtures, Rollenregeln,
Hooks oder GitHub-Planung ändern; keine Gerätehelfer, Uploads, Installationen,
automatischen Commits/Pushes/PRs oder Nachrichten in andere Chats auslösen.
Der separat beauftragte Maintainerprozess behält sein Mandat bis PR.

Jeder Lauf erfasst Start/Ende, Quellenbasis, geprüfte und fehlgeschlagene
Quellen, Diffs, Befund-IDs, Auslassungen und nächsten Prüfpunkt. Je Arbeitsstand
nur ein schreibender Koordinator; parallele Recherche liefert getrennte Beiträge.
Vor Übernahme HEAD und Zieldatei erneut prüfen; bei Änderung neu abgleichen.
Duplikate aktualisieren statt neu anlegen. Zugriffsfehler entwerten die
Aktualitätsaussage; alter Cache ist kein frischer Abruf. Unveränderte Läufe
und bereits gemeldete unveränderte Blocker bleiben still. Benachrichtigt wird
bei relevantem neuem Befund, entscheidender Änderung, Abschluss oder neu
notwendigem Nutzereingriff.

## 8. Dateien, Pilotauftrag und Rückweg

Nach Annahme klein anfangen; folgende Dateien sind **vorgesehen, noch nicht angelegt**:

- `docs/quality/ROLLE.md`: Rollenvertrag und Auditumfang.
- `docs/quality/fachgebiete.yaml`: Komponenten, Versionen, Empfänger, Quellen,
  Gültigkeit und letzte Prüfung; keine Backlogkopie.
- `docs/quality/audits/<lauf-id>/`: Auftrag, Quellenbelege, Findings und Review.
- `docs/quality/entscheidungen/`: Übernahme/Ablehnung mit kanonischem Ziel.
- `.codex/agents/quality_auditor.toml` und optional ein kleiner Projekt-Skill
  nach manueller Erprobung; beide verweisen auf denselben Rollenvertrag.

Laufentwürfe erhalten einzigartige Pfade. Der Maintainer übernimmt relevante
Ergebnisse und Entscheidungen per benannten Dateipfaden in Git. Bis dahin
ist der lokale Bericht sichtbar, aber nicht dauerhaft integriert. Rückweg:
neuen Zeitplan pausieren, Rollenaktivierung zurücknehmen, Berichte/Entscheidungen
erhalten; bestehende CI und GitHub-Sync weiterführen.

**Textentwurf für den ersten manuellen Auditauftrag:**

> Prüfe für Sven401/Larp-Escape ESP32/FreeRTOS und Adafruit-NFC/GPIO gegen
> offizielle Quellen. Ermittle zuerst den tatsächlichen Remote-Head von
> plan/projektuebergabe-kuratierter-stand. Lies AGENTS.md, Maintainer-Einstieg,
> aktuelle Planung in #25 und betroffene Fachverträge. Trenne bestätigtes Soll,
> integrierten Code, offene PRs und Gerätebelege. Stelle Komponente, aufgelöste
> Version, Variante und Aufrufpfad fest. Öffne versionspassende Primärquellen
> sowie relevante aktuelle Änderungshinweise. Prüfe Anwendbarkeit, bekannte
> Findings und Widersprüche. Liefere pro Frage höchstens drei priorisierte
> Befunde oder einen begründeten Ohne-Befund-Bericht: Quelle, Revision, Datum,
> Belegstatus, Empfänger, kanonisches Ziel, konkrete Änderung und Prüfweg.
> Nutze höchstens 30 Minuten und benenne nicht geprüfte Teile. Schreibe nur
> deinen Auditentwurf/Laufnachweis. Ändere keine Produkt-/Rollendateien, Pins,
> Fixtures, Hooks oder GitHub-Inhalte; führe keine Gerätehelfer aus. WLED bleibt
> externe Funktionsreferenz. Fehlende Quellen/Volltexte bleiben offen. Lege
> das Ergebnis dem Fachreview vor; behaupte keine Übernahme oder Produktabnahme.

## 9. Erprobung und Erfolgskriterien

Vor Aktivierung zwei manuelle Läufe an benannten Commits vergleichen. Jeder
relevante Befund braucht Quelle, Anwendbarkeit, Empfänger und überprüfbare
Folge. Der unveränderte zweite Lauf erzeugt keine Dubletten. Mindestens eine
fachliche Entscheidung endet nachvollziehbar in der richtigen Referenz oder
als begründete Ablehnung.

Gegenfälle: falsche SDK-Version, WLED als vermeintliche Dependency, Draft als
Final, unzugängliche Quelle, bekannte B7-Abweichung, inzwischen geänderter HEAD
und fehlende Gerätemessung. Diese Unterschiede müssen erhalten bleiben.

Bewerten: Aufwand pro nützlichem Befund, Fehlalarme, überfällige Gebiete,
Zeit bis Fachentscheidung und tatsächlich geschlossene Nachweislücken.
Ein niedriger Finding-Zähler ist kein Erfolgskriterium. Nach vier geplanten
Läufen Nutzen/Takt prüfen; dauerhafte Regeln erst mit Bewertung und Rückweg
in PROMOTION übernehmen.

**Offen:** Annahme des Entwurfs, Fachreviewbesetzung für bisher nicht
formalisierte Gebiete, Pilot, eventuell benötigte Normvolltexte und gesonderte
Zeitplanaktivierung. Dieser Entwurf führt diese Schritte nicht selbst aus.
