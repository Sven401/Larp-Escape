# Fachverantwortung unter dem Maintainer

Stand: **24.10.2026; nach dem manuellen A1/A2-Pilot vom Nutzer bestätigt
und verbindlich übernommen.** Geltung: beauftragte Projektarbeit.
Quelle: ausdrücklicher Folgeauftrag zur Übernahme der Mandate und Prüfpflichten
in Sitzung `01a16ab1-34d7-73b0-924a-fc2d245b9027`; Dokumentationsbasis
`dc0e3f12b571c534ef258300a47eab8a917387df` in PR #95.
Keine neue Automatisierung, kein dauerhaft laufender Agent und keine zusätzliche
Freigabestufe für jede kleine Änderung.
[Pilot und Nachweise](audits/2026-10-24-pilot/README.md),
[Nutzungsbewertung und Rückweg](../../PROMOTION.md).

## Warum die bisherige Zuordnung nicht genügt

LED und UI/UX haben ausführliche Rollen. Die Geräteausführung und die
Hardware-/NFC-Verträge beschreiben dagegen überwiegend technische Besitzer
und Anforderungen. Ein Task als Ressourcenbesitzer ersetzt keinen Menschen
oder Agenten, der die SDK-Annahme oder den Fehlervertrag fachlich verantwortet.
Der Maintainer musste diese Fragen bisher jeweils neu zuordnen.

Der Pilot zeigte zwei verschiedene Lücken: Queuezugriffe verletzen eine
Portregel, obwohl das Hostmodell die Nullwartezeit bestätigt; ein Bibliotheks-
Fehlerwert erreicht die Spiellogik als gültiges Sensorsignal. Ein eigener
Experte je Bibliothek würde diese Grenzen erneut aufteilen. Daher zwei
zusammenhängende Fachmandate und eine getrennte Nachweisprüfung.

## Zuständigkeiten und Ergebnis

| Funktion | Verantwortet | Liefert und entscheidet im Auftrag |
| --- | --- | --- |
| Maintainer | Gesamtintegration, Priorität, Konflikte, Auftrag und Besetzung | Benennt je Arbeitspaket fachlich Verantwortlichen und Gegenprüfer; ordnet Folgearbeit ein und führt Änderungen bis PR. Merges, Releases und grundlegende Produktentscheidungen bleiben beim Nutzer. |
| Plattform und Systemintegration | Aufgelöste Plattform/Framework/SDK pro Firmwarevariante; Support, ABI; Task-/ISR-/Callbackkontext, gemeinsam genutzte Ressourcen, Fristen und Speicher | Versions-/Kontextnachweis, tragfähiger Nebenläufigkeitsentwurf, Migrations-/Supportentscheidung und passende Prüfungen. Bindet Security-Fachreview für konkrete Advisories, Zugang oder Kryptografie ein; ist keine automatische Security-Qualifikation. |
| Geräteintegration und Hardwarevertrag | Chip und Revision, Pins, Bus/Adapter, Fehler-/Verfügbarkeitsvertrag, Timeout/Recovery, Patch und physischer Rückweg | Durchgängiger Vertrag vom Transport bis zum bewerteten Eingang; Grenzen von Datenblatt und Bibliothek, Fehlerproben und nötige Gerätemessung. Keine elektrische Freigabe aus einem erfolgreichen Build. |
| LED-Effekt-Architekt | Sichtbare Wirkung, Berechnung und vollständige Umsetzung bis Produktintegration | Behält Rolle 2.0. Holt Plattformreview bei Task/Queue/RMT-/Pufferänderungen und Gerätefachreview bei Pins/Bus/elektrischen Grenzen hinzu. |
| UI/UX Designer | Bedienung, Zustände, Zugänglichkeit und Produktdarstellung | Behält bestehenden Rollenvertrag. Stimmt Gültigkeit, Frische und unklaren Ausgang mit dem zuständigen technischen Mandat ab. |
| Qualität und Nachweise | Passung von Behauptung, Quelle, Variante, Prüffall und tatsächlich ausgeführtem Nachweis | Prüft Befund und Übernahme unabhängig vom Autor, benennt Lücken. Entscheidet weder Produktpriorität noch Hardware-/Releasefreigabe; eigene Implementierung darf nicht als unabhängig geprüft gelten. |

Funktionen sind keine Stellen- oder Agentenzahl. Der Maintainer kann ein
Fachmandat übernehmen; für dessen unabhängigen Review braucht es einen anderen
Prüfdurchgang durch einen anderen Bearbeiter. Ein anderer Subagent im selben
System liefert eine fachliche Gegenprüfung, keine externe Zertifizierung.
Der **Fachaudit-Koordinator** organisiert zeitlich begrenzte Quellenaufträge,
Anwendbarkeit und Befundverfolgung innerhalb der Qualitätsfunktion. Er hat
keine zusätzliche Weisungsbefugnis über die Fachrollen.

## Auftrag, Gegenprüfung und Abschluss

Vor Änderungen hält der Maintainer je Arbeitspaket den fachlich Verantwortlichen
und einen anderen Gegenprüfer sowie Ziel, Codebasis, betroffene Varianten und
erforderliche Nachweise im Auftrag oder Arbeitsbericht fest. Für gemeinsame
Grenzen wird ein führendes Mandat benannt; die andere Fachfunktion wird gezielt
einbezogen. Ein passender unabhängiger Review kann mehrere Prüffragen abdecken;
es entstehen keine zusätzlichen Gremien oder separaten Freigaben je Tabelle.

Der Verantwortliche begründet die technische Lösung und führt die passenden
Prüfungen aus. Der Gegenprüfer bewertet die geänderten Pfade, Gegenfälle und
Nachweise gegen den Vertrag; er darf seine eigene Implementierung nicht als
unabhängig geprüft bestätigen. Bei Zugang, Kryptografie, konkreten Advisories
oder geänderten Vertrauensgrenzen benennt der Maintainer zusätzlich die nötige
Security-Expertise; ein Plattformtitel allein genügt dafür nicht.

Der Abschluss nennt geänderte Quelle/Commit, tatsächlich ausgeführte Prüfschritte,
Ergebnisse, Reviewbefunde und verbleibende Grenzen. Für erforderliche, aber nicht
ausgeführte Geräte- oder Lastmessungen bleibt die entsprechende Abnahme offen.
Ein positiver Hosttest ersetzt weder SDK-/ABI-Build noch echte Nebenläufigkeits-
oder Hardwarebelege. Technische Integration führt der Maintainer bis PR;
Merge, Release und grundlegende Produktentscheidungen bleiben beim Nutzer.

Die Rollenübernahme allein schließt keinen technischen Befund ab. Die danach
beauftragten A1-/A2-Korrekturen sind mit [eigenem Prüfstand](runtime-fixes-2026-10-24.md)
umgesetzt; historische Fehlerbelege bleiben im Pilotbericht erhalten.
Die dort getrennt benannten Geräte-/Supportgrenzen bleiben offen.

## Verbindliche Prüfpflichten aus dem Pilot

| Auslöser | Verantwortlicher und nötige Gegenprüfung | Mindestnachweis |
| --- | --- | --- |
| Task, Queue, Spinlock, Callback oder ISR geändert | Plattform; Qualität prüft Kontextabdeckung | Für den tatsächlich eingebetteten Espressif-Port Regeln belegen. Keine FreeRTOS-API in Spinlock-Kritischen Sektionen; Nullwartezeit allein genügt nicht. Autorität/Generation beim Umbau weiter atomar behandeln; volle/leere Queue, konkurrierender Producer/Consumer, Entzug/Generationswechsel und verspätete Antwort gezielt prüfen. Hostmodell, betroffene SDK-Builds und Gerätelauf getrennt nachweisen. Herkunft: A1-01. |
| Bibliotheks-/Versionspin, Framework, SDK-Binärpin oder Migration geändert; Support endet | Plattform, bei Security zusätzlich gezielter Fachreview; Maintainer entscheidet Folge | Plattform, Arduino-Version und eingebettetes IDF getrennt; Nutzungs-/Buildvarianten und Supportstatus erfassen. Bei EOL Ausnahme mit Grund, Verantwortlichem und Wiedervorlage oder Migrationsauftrag. Fehlende Advisory-Versionsangabe ist keine Entwarnung. A1-02. |
| Sensorwert oder Gerätefehler über Adaptergrenze geführt | Geräteintegration; Verbraucher und Qualität prüfen Wirkung | Wert, gültig/unbekannt, Alter und Fehler getrennt. NACK, Kurzantwort und Ausfall nach gesundem Start bis zur Spiellogik prüfen. Fehler darf keine positive Spielaktion erzeugen; Wiederkehr nach dem Fehler mit frischer Beobachtung prüfen. Adafruit- und eigene Adapter sowie betroffene Firmwarevarianten getrennt nachweisen. Herkunft: A2-01. |
| Externe Bytes geparst oder Quellpatch geändert | Geräteintegration; Qualität prüft unabhängige Gegenbeispiele | Feste externe Framebeispiele, Verkürzungen, Längen-/Prüfsummenfehler; Patch-Erhalt, Ablehnung fremder Änderungen und Rückweg. Synthetische Testpins und reale Quellenhashprüfung getrennt ausweisen. A2-02. |
| Bool-Erfolg oder Timeout als Zusage weitergegeben | Geräteintegration führt; Plattform prüft Frist/Taskkontext, Qualität den Nachweis | Pro Bus Bedeutung von Erfolg festhalten; vollständigen Aufrufpfad und Deadline messen. SPI-Bool ist hier keine Gerätequittung, `readCard(100)` kein End-to-End-Budget. A2-03. |

Der Reviewumfang wächst mit der berührten Grenze. Eine reine Effektfarbe
braucht keinen neuen Hardwareaudit; eine RMT-/Taskänderung schon. Ein Befund
braucht einen Verantwortlichen und ein überprüfbares Ziel, keine neue Rolle
mit dem Namen der jeweiligen Bibliothek.

## Kanonische Einstiege und Pflege

- Plattform: [Geräteausführung](../configuration/device-runtime.md),
  [Laufzeit](../runtime.md), [Migration](../../handoff/ARDUINO-ESP32-3-MIGRATION.md).
- Geräteintegration: [NFC-Paketvertrag](../nfc-packet-contract.md),
  [Hardwareplanung](../../Hardware/HARDWAREPLANUNG.md).
- Bestehende Rollen: [LED](../led/rollen/led-effekt-architekt.md),
  [UI/UX](../ui-ux/ROLLE.md); [Prüfwege](../quality-checks.md).

Die jeweiligen Verträge bleiben fachlich kanonisch. Das Rollenpapier routet
Arbeit und wiederholt keine vollständigen Spezifikationen. Jeder angenommene
Befund benennt dort eine konkrete Änderung und einen passenden Prüfweg.
Offene Folgearbeit steht im Pilotbericht bis zur Einordnung durch den
Maintainer; der bestehende GitHub-Abgleich erhält dadurch kein neues Mandat,
Issues oder Prioritäten zu erfinden.
