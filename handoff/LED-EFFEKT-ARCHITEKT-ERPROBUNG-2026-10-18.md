# LED-Effekt-Architekt: Erprobung und Abschluss

> Historischer Abschluss der Rollenfassung 1.0. Der danach ausdrücklich erweiterte
> Auftrag wird durch [Rolle 2.0 und reale Lichtversuche](../docs/led/naeheglut-live-results-2026-10-18.md)
> erfüllt. Die folgenden Aussagen zu reinem Entwurf und fehlenden Geräteversuchen
> gelten nur für diesen früheren Bearbeitungsstand.

Datum: **18. Oktober 2026**. Sitzung:
`01a14f9a-35fb-72e1-9acb-132b0d16599d`.
Status: **Rollenfassung 1.0 erstellt, angewandt, geprüft und verbessert**.
Erste Nutzungsbewertung, kein Nachweis langfristiger Produktivitätssteigerung.

## Ergebnis und Herkunft

- [Rollenprofil 1.0](../docs/led/rollen/led-effekt-architekt.md): Auftrag,
  Eingaben, Ergebnisse, Quellenrouter, Ablauf und Entscheidungsgrenzen.
- [Fachlicher Auftrags-/Mischvertrag](../docs/led/effect-command-contract.md):
  Spielabsicht → Instanz → Beitrag → Komposition → getrennte Rückmeldungen.
- [Näheglut-Entwurf](../docs/led/naeheglut-design.md): Nutzerantworten,
  sichtbarer Ablauf, Lebenszyklus, Mathematik, Mischung und Prüfplan.
- [Editierbare Rechnerprobe](../docs/led/probes/naeheglut/README.md),
  [Ergebnisse](../docs/led/probes/naeheglut/result.json) und
  [berechnetes Ablaufdiagramm](../docs/led/probes/naeheglut/preview.svg).

**Ausgelesen:** Ausgangscheckout `codex/led-resonanzkreis` auf
`e7ec0f94de8abdc525e7cf5bdf7e049831dc7782`; PR #71 offen und nicht gemergt.
Fremde Änderung `.vscode/extensions.json` unangetastet. Nach frischem Fetch
eigener isolierter Worktree und Branch `codex/led-effekt-architekt` auf
`dead12040c196ff74ab4be17ba71451a9351a676` (kuratierter Integrationsbranch,
einschließlich D0/#72). Keine Produktänderungen aus #71 übernommen.

Die Analysen aus #71 bleiben über vollständige Commitlinks nutzbar. Die
aktuelle Zielbasis enthält im manuellen Lichtpfad nur IDs 0–3; Resonanzkreis
und Wake→LedOutput aus #71 sind dort noch nicht integriert. Die historischen
Analysen beziehen sich auf Originalquellstand `a40365d`, nicht auf ein hier
geprüftes historisches Gerätebinary.

**Bestätigt durch den aktuellen Nutzerauftrag:** Neben Verhalten, Berechnung
und Prüfplanung verantwortet die Rolle den fachlichen Spiel/Effekt-Vertrag,
parallele Beauftragung, gezielten Stop und mischbare Beiträge. Dies erweitert
die frühere Übergabe, ohne App-/BLE-Gesamtverantwortung abzuleiten.

**Bestätigt in zwei Fragerunden:** Nähe fern→nah, gemeinsames ruhiges
warmgoldenes Leuchten aller 23 LEDs, etwa eine Sekunde Übergang, expliziter
Stop und automatische Erholung aus ungültigen Werten nur bei aktiver Instanz.
Normalisierung, exakte RGB-Werte und Mischbudget sind technische Vorschläge.

## Beobachtete Anwendung und Verbesserungen

Der Kandidat 0.1 wurde vor der Modellarbeit als Datei erstellt und vom
Probe-Bearbeiter gelesen. Ein zweiter Bearbeiter prüfte Rollenprofil/Vertrag/
Sollzahlen unabhängig; ein weiterer prüfte Quellanschlüsse und anschließend
das fertige Modell. Die Auswertung führte zu Fassung 1.0.

| Beobachtung während der Anwendung | Konkrete Verbesserung / Beleg |
| --- | --- |
| Drei getrennte Lichtpfade werden leicht als ein fertiger Mixer missverstanden | Anschlussmatrix belegt vorhandene Ports und Lücken. Rolle verlangt Ist/Original/Soll getrennt. |
| Historische Start-/Resetfallen sind schon analysiert | Quellenrouter nutzt Ripple/Burst/Funkeln gezielt, statt Originalberechnungen erneut auszuführen. Ablehnung ohne Teilwirkung und explizite Zustandsübernahme aufgenommen. |
| Vorhandenes RSSI-Veraltungsziel 20 % widerspricht gewünschtem Ausblenden | Messwertnormalisierung/Frische beim Spiel belassen; Lichtreaktion gesondert festgelegt. Keine Meterkalibrierung erfunden. |
| Wiederholte gleiche Werte können einen Übergang endlos neu starten | Zielgleichheit erhält Timer; Tests für fortgesetzte gleiche und ungültige Meldungen. |
| Ein beendetes Kurvensegment könnte nach vollem uint32-Umlauf wiederkehren | Fertige Rampe festsetzen; eigener servierter Langlauf und Überlauftest. |
| Erstes Gesamt-Reset-Modell entfernte Instanzen, ließ aber Fixture-Hintergrund weiterleuchten | Gesamtausgabe separat sperren, tatsächlichen schwarzen Frame prüfen; alte Epoche darf nicht wieder einschalten. Korrektur im endgültigen Modell geprüft. |
| Review fand fehlenden ständig veränderlichen Eingang | N9 ergänzt; keine falsche Zusage einer festen 1-s-Latenz bei dauerhaft neuen Zielen. Zwischenabrundung korrigiert x=348678/674339 zu 348677/674338. |
| Stop während vorhandener Invalid-Ausblendung war im Text mehrdeutig | Erster Stop erhält ausdrücklich neue 1000 ms, weitere Stops verlängern nicht; eigener Test. |
| Ein lang laufender Effekt darf Auftragsticket/Profilwechsel nicht unbemerkt blockieren | Rolle trennt Verarbeitungsticket, Instanzleben und Profilbarriere; Umsetzung als offene Anschlussarbeit. |

Diese Fälle begründen die Ergänzungen in den Rollenschritten 3, 4 und 6.
Die erste Probe konnte konkrete Fragen beantworten und Fehler im Entwurf
aufdecken. Zeitersparnis wurde nicht gemessen; der belegbare Nutzen ist der
gezielte Rückgriff auf vorhandene Quellen und die aufgedeckten Mehrdeutigkeiten.

## Ausgeführte Prüfungen und Aussagegrenzen

**Gemessen:** 23 benannte Python-Modelltests bestanden, null Fehler,
null ausgelassene Tests. Darin 1001 Pegel als Unterfälle eines Tests,
keine 1001 zusätzlich behaupteten unabhängigen Tests. Umfasst Solltabelle,
Grenzen, unabhängige Instanzen, frische Komposition, Stop/Reset, alte Aufträge,
Duplikate, volle Modellkapazität, 17/33/67-ms-Abtastung, ausgelassene Frames,
uint32-Überlauf, langen servierten Betrieb und fortlaufende Zieländerungen.

Modell-SHA-256:
`a19a3b1d3b4f894d1202209ca3f719eb4f3d7ff3d2e8588fa859a15297fce34d`.
Prüfzeit, Python-Version und Einzeltests im Ergebnis-JSON. Der Hash bindet den
Lauf an die Quelle; `dead120` bezeichnet die Ausgangsbasis, nicht einen Commit,
der das neue Modell bereits enthält. Der abschließende PR/Commit versioniert
das gesamte Paket; dessen eigener Prüfstand wird im Abschluss genannt.

**Belegt durch unabhängigen Quellenreview:** mathematische Tabellen,
Mischbeispiele und Modell entsprechen dem deklarierten Entwurf. Keine
substantiellen Findings nach den oben dokumentierten Verbesserungen.
Resultathash und SVG-XML geprüft. Das SVG wurde zusätzlich rasterisiert
und auf lesbare, unbeschnittene Beschriftungen angesehen; das ist nur
Darstellungsprüfung des Diagramms, keine optische LED-Abnahme.

**Offen:** Produkt-Hosttests, C++-Typen/Überläufe, echte Queue/Profilbarriere,
RMT-Übertragung, Firmwarebuild, elektrischer Nachweis, Kameralauf und
menschliche Spielabnahme. Das Modell importiert keinen Produktcode.
Für diesen Dokumentations-/Entwurfsauftrag wurden keine Geräte angesprochen,
keine App-/Firmware installiert und keine Produktdateien geändert.

## Verbleibende Entscheidungen und nächster Schritt

Der Entwurf ist für eine fachliche Weitergabe vollständig; seine konkrete
Produktanbindung bleibt gesondert zu entscheiden:

1. Spieleinsatz, Start-/Stopzustände, Nähemessquelle, Normalisierung und Frische.
2. Zieladapter, Instanz-/Queuekapazität, Stopzustellung, Ressourcenbudget,
   Profilwechsel sowie Helligkeitsbudget paralleler Beiträge.
3. Goldwerte, Quantisierung und sanften Eindruck am realen Aufbau bewerten;
   anschließend ggf. Arbeitswerte anpassen.

Nächster Schritt bei einem neuen Auftrag: diese Anschlussentscheidungen für
den gewählten Spielpfad festlegen, dann implementieren und den vorbereiteten
Prüfplan stufenweise ausführen. Dieser Abschluss enthält keinen Folgeauftrag
zur Implementierung oder zum Upload. Rollenübernahme und einfacher Rückweg:
[PROMOTION](../PROMOTION.md).
