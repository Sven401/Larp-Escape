# Rollenarchitektur und Entscheidungswege

Status: vom Nutzer am 30.10.2026 zur Umsetzung bestätigt. Basis: Integrationscommit
`7585e1573240ac92da6e925f484a93429c219673`. Quelle: Auftrag zur Rollenarchitektur
und Zustimmung „Ja, Mach das so.“ in der zugehörigen Sitzung.
Am 30.10.2026 hat der Nutzer die eingerichtete Projektkonfiguration ausdrücklich
abgenommen und den Konfigurationsauftrag abgeschlossen. Praxisdurchlauf A ist
bestanden; B und die Prüfung des ersten tatsächlichen täglichen Laufs bleiben
getrennte nachgelagerte Betriebserprobung, keine offenen Konfigurationskriterien.
Ihre Ergebnisse werden nicht vorweggenommen. [Abnahme und Nachweise](../../handoff/ROLLENARCHITEKTUR-2026-10-30.md).

## Auftrag und Befugnisse

| Rolle | Ergebnis und eigene Entscheidung | Konsultation und Grenze |
| --- | --- | --- |
| Projektmanager | Issues, Reihenfolge innerhalb bestätigter Prioritäten, Meilensteine, Abhängigkeiten, genau ein führender Bearbeiter und ein anderer Reviewer; Abschluss verfolgen | Keine Routineimplementierung, Routine-Codefreigabe, Merges oder technische Grundsatzentscheidung. Strategische Prioritätsänderung zum Product Owner. |
| Maintainer | Architektur, Modulgrenzen, gemeinsame Schnittstellen, Standards, Wartbarkeit und technische Schulden; technische Konflikte entscheiden und Entscheidungen am Vertrag dokumentieren | Nur bei betroffener Grundsatzgrenze konsultieren. Grundlegende Architekturänderung mit Nutzerfreigabe. |
| Reviewer | Aktuellen Diff, Gegenfälle, Tests und Nachweise unabhängig prüfen; Probleme an Entwickler; Routine-Merge und Integrationsnachkontrolle verantworten | Eigene Implementierung ist keine unabhängige Prüfung. Merge nur gemäß [Policy](merge-policy.md); bei eigener Korrektur anderen Reviewer einsetzen. |
| UI/UX Designer | Zustände, Navigation, Sprache, Komponenten, Accessibility, nachvollziehbare Designvorlage und sichtbarer Vergleich | [Bestehende Rolle](../ui-ux/ROLLE.md), Figma/Webpilot erhalten; Android verantwortet native Umsetzung. |
| Game Designer | Spielleiterabläufe auf Nutzen, Spielbarkeit, Vorbereitungsaufwand und Fehlerfolgen bewerten; innerhalb bestätigter Mechanik konkrete Abläufe und Varianten ausarbeiten | Berät früh bei Spielmechanik, Bedienabläufen und Spielerlebnis. Neue Siegbedingungen, Informationsrechte, Spielerrollen oder Produktfunktionen entscheidet Product Owner. Kein technischer Zusatzreview. |
| Android-Entwickler | Native Implementierung, Lifecycle, Appzustände, Protokollanbindung und Tests | UI/UX bei sichtbaren Abläufen; Firmware bei gemeinsamem Vertrag. Keine stillen Produktänderungen. |
| Firmware-Entwickler | ESP32-Implementierung, Schnittstellen und Tests; Plattform-, Geräte- und Infrastrukturmandate aufgabenbezogen besetzen | [Fachprüfpflichten](verantwortlichkeiten.md) bleiben vollständig verbindlich. Keine Gerätefreigabe aus einem Build. |

LED-Effekt-Architekt bleibt gemäß [Rolle 2.0](../led/rollen/led-effekt-architekt.md)
für den Effekt bis zur Produktintegration zuständig. Kein paralleler zweiter
Effektentwickler. Plattform/Systemintegration, Geräteintegration/Hardwarevertrag
und Qualität/Nachweise sind Fachmandate, keine zusätzlichen Daueragenten.
Fachaudit bleibt Quellen- und Befundarbeit innerhalb seines eigenen Mandats.

Organisatorische Konflikte entscheidet der Projektmanager im bestätigten Auftrag,
technische der Maintainer, Review-/Mergefähigkeit der Reviewer. Spielbedeutung,
strategische Richtung und neue Produktfunktionen entscheidet der Nutzer als
Product Owner. Niemand darf fehlende Abnahmekriterien durch Umdeuten schließen.

## Ablauf und Abschluss

| Phase / Verantwortlicher | Eingabe → Ergebnis / Abschluss | Konsultation oder Eskalation |
| --- | --- | --- |
| Anforderung / PM | Nutzerziel → Issue mit Umfang und beobachtbaren Abnahmefällen | Offene Produktabsicht zum Nutzer |
| Fachbewertung / passender Spezialist | Anforderung → Nutzen, Risiken und Entscheidungsbedarf | Game Design bei Spiel-/Spielleiterbezug |
| Technische Planung / führender Entwickler | Verträge → Lösung, Varianten, Prüffälle und betroffene Grenzen | Maintainer bei Architektur-/Schnittstellenkonflikt |
| Design falls nötig / UI/UX | Ablauf → Zustände, Referenz, Texte und Akzeptanzfälle | Android prüft Machbarkeit, Game Design Produktfolgen |
| Umsetzung / Entwickler | Auftrag → Diff und passende Selbstprüfung | Plattform/Geräte/Security nach bestehendem Auslöser |
| Tests / Entwickler | Prüfplan → commitbezogene Ergebnisse und offene Grenzen | Fehlender Pflichtnachweis bleibt blockierend |
| Review / anderer Reviewer | Diff und Belege → Befunde oder aktuelle Freigabe | Befunde zum Entwickler; neue Änderungen erneut prüfen |
| Merge / Reviewer | Alle Policybedingungen → bestätigter Integrationscommit und Nachkontrolle | Fehlende Rechte/Identität gezielt eskalieren |
| Abschluss / PM | Integrationsbeleg → gesamten Auftrag/Issue abgleichen | Teilabnahme nicht als vollständigen Abschluss ausgeben |

Kompakter Auftrag: Ziel, verbindliche Quelle, Umfang/Varianten, führender Bearbeiter,
anderer Reviewer, Abnahmefälle, benötigte Nachweise und offene Entscheidungen.
Diese Angaben im bestehenden Issue/PR/Arbeitsbericht führen, kein zweites Backlog.

## Autonomie, Blockaden und Aufwand

Routineentscheidungen trifft die zuständige Fachrolle innerhalb bestehender
Verträge. Gemeinsame Grenzen werden gezielt zwischen Spezialisten abgestimmt;
kein Mehrheitsvotum und keine Freigabekette für jede Kleinigkeit. PM darf im
beauftragten Umfang gezielt Subagenten einsetzen, soweit die aktuelle Sitzung
es zulässt. Übergeordnete Sitzungsregeln bleiben maßgeblich.

Ein Paket hat genau einen Besitzer. Andere Agenten erhalten konkrete Prüffragen
oder getrennte Teilpakete; bei Überschneidung stoppt die Doppelbearbeitung.
Übergabe: Ziel, exakter Commit, betroffene Dateien/Verträge, Abnahmefälle, Ergebnis
und offene Punkte. Kein vollständiger Gesprächsverlauf nötig, sofern das
Werkzeug gezielte Übergabe erlaubt. Vorhandene Belege gezielt wiederverwenden.

Blockade im bestehenden Auftrag: Grund, zuständige Rolle, nächster konkreter
Schritt und Wiedervorlagepunkt. PM prüft bei Übergaben, nach CI-/Reviewereignissen
und vor Sitzungsabschluss erneut; fehlende menschliche Entscheidung einmal
gezielt eskalieren. Unabhängige Arbeit weiterführen. Nach Sitzungsende braucht
Fortsetzung einen tatsächlich eingerichteten Mechanismus, kein bloßes Versprechen.

Typisch Koordinator + Entwickler + Reviewer, Fachrolle nur bei Anlass; die
aktuell vier gleichzeitigen Plätze sind eine Sitzungsgrenze, keine Sollbesetzung.
Messung je Paket: Agentenaufrufe, Laufzeit, soweit verfügbar Tokenverbrauch,
Review-Rückläufe, Wartezeit ab technischer Bereitschaft bis Merge, Nutzereingriffe,
Wiedereröffnungen und ungelöste Nachweise. Unbekannte Werte offen lassen;
keine behauptete Zeit-/Kostenersparnis ohne Vergleich. Nach zwei Routinepiloten
Nutzen prüfen, unnötige Übergaben entfernen. [Übernahme/Rückweg](../../PROMOTION.md).
