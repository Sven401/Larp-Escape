# Näheglut: Stopklärung und Spielanschluss

**Status: PR #81 integriert · 18.10.2026.** Auf ausdrücklichen Folgeauftrag
nach vollständig grüner CI in `plan/projektuebergabe-kuratierter-stand` gemergt.
Operativer Projektstand: [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25).

## Abgeschlossener Merge von PR #83

**Belegt · 18.10.2026:** [PR #83](https://github.com/Sven401/Larp-Escape/pull/83)
ist nach **9/9 erfolgreichen PR-CI-Jobs** bei
[`9d8bf44aff9da1d4c47f109c451cb251a9d626cd`](https://github.com/Sven401/Larp-Escape/commit/9d8bf44aff9da1d4c47f109c451cb251a9d626cd)
in `plan/projektuebergabe-kuratierter-stand` integriert. Quelle: ausdrücklicher
Mergeauftrag und anschließend beauftragte Fortschreibung von Projektstand/Übergabe.

- Unveränderter geprüfter Head: `0fb6ab096ebf0858121875b1e028222bc60f35d1`.
- Frischer Fetch bestätigt Eltern `8648f573ed7bbd2e76530b15d3f5883945b7c1a4`
  und `0fb6ab096ebf0858121875b1e028222bc60f35d1`.
- Vollständiger Dateibaum von Head und Merge identisch:
  `975e7e043788a98c31b8680f9f83f668ade1122b`; keine Integrationsänderung.
- Abgeschlossene PR-CI: [Quality, 3/3](https://github.com/Sven401/Larp-Escape/actions/runs/39168415055),
  [LED/Android/Kamera, 4/4](https://github.com/Sven401/Larp-Escape/actions/runs/39168415043),
  [NFC, 2/2](https://github.com/Sven401/Larp-Escape/actions/runs/39168415029).
  Vor Merge keine offenen Reviewthreads oder Änderungsforderungen.

PR #83 integriert ausschließlich die Statusfortschreibung in drei
Übergabe-Markdown-Dateien. Die folgenden älteren Laufbelege bleiben ihren
jeweiligen Commits zugeordnet. Die erfolgreiche #83-PR-CI ist kein Nachweis
für eigene spätere Push-Läufe am Merge oder an dieser erneuten Dokumentation.
Physische Dunkelursache/optische Freigabe, Pausenregel, Stations-/Quellenzuordnung
und Produktadapter bleiben offen; keine neue Produkt- oder Geräteabnahme.

## Integration und Prüfstand

[PR #81](https://github.com/Sven401/Larp-Escape/pull/81) ist bei
[`c4870215a155b9abff2e36fccfe93d42e1a03043`](https://github.com/Sven401/Larp-Escape/commit/c4870215a155b9abff2e36fccfe93d42e1a03043)
übernommen. Nach Merge frisch gefetcht und geprüft:

- Eltern: Zielbasis `7e5f4f38e88ed76358e31dc66d267da9cb0fbc1f` und geprüfter
  PR-Head `9114ff89863501809e06f65892ec6948342eecb6`.
- Vollständiger Git-Dateibaum von PR-Head und Merge identisch:
  `94b3e371dc0349a8d822e6213a3b614a57630610`; kein Integrationsdiff.
- **PR-CI am unveränderten Head: 9/9 Jobs erfolgreich**, unmittelbar vor Merge
  frisch bestätigt: [Quality, 3/3](https://github.com/Sven401/Larp-Escape/actions/runs/39166174771),
  [LED/Android/Kamera, 4/4](https://github.com/Sven401/Larp-Escape/actions/runs/39166174805),
  [NFC, 2/2](https://github.com/Sven401/Larp-Escape/actions/runs/39166174773).
  Keine offenen Reviewthreads oder Änderungsforderungen bei dieser Abfrage.

Der Merge integriert Diagnose und technische Spezifikation, keinen ausführbaren
Produktadapter. Physische Dunkelursache, optische Freigabe, Pausenregel und
Quellenzuordnung bleiben offen. Die Software-/Bildnachweise unten behalten
ihre ursprüngliche Quellbasis; bei diesem Merge keine erneute Geräteprüfung.
Eigene Push-Prüfungen des Mergecommits und nachfolgende Dokumentations-CI sind
zusätzliche Nachweise, nicht die obigen PR-Läufe. Sie sind inzwischen wie folgt
abgeschlossen; jeder Status wurde am 18.10.2026 erneut jobweise ausgelesen:

| Prüfstand | Workflow | Ergebnis |
| --- | --- | --- |
| #81-Merge `c487021` | [LED product](https://github.com/Sven401/Larp-Escape/actions/runs/39166813494) | **4/4 erfolgreich** |
| #81-Merge `c487021` | [Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39166813499) | **Abgebrochen**: Hostprüfung erfolgreich, Android/ESP32 abgebrochen; kein grüner Gesamtlauf. Durch folgenden Dokumentationsmerge abgelöst. |
| #82-PR-Head `edde13f` | [Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39167013482) | **3/3 erfolgreich** |
| #82-PR-Head `edde13f` | [LED/Android/Kamera](https://github.com/Sven401/Larp-Escape/actions/runs/39167013562) | **4/4 erfolgreich** |
| #82-PR-Head `edde13f` | [NFC](https://github.com/Sven401/Larp-Escape/actions/runs/39167013658) | **2/2 erfolgreich** |
| #82-Merge `8648f57` | [Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39167029463) | **3/3 erfolgreich** |

Damit sind die zuvor offenen zusätzlichen Prüfungen abgeschlossen: #82-PR-CI
9/9 grün, Quality am tatsächlichen #82-Merge 3/3 grün und LED am #81-Merge 4/4
grün. Der historische Abbruch bleibt ein Abbruch und wird nicht in Erfolg
umbenannt. Kein fehlgeschlagener oder noch laufender Job in diesen sechs Läufen.

Der Dokumentationsabschluss [PR #82](https://github.com/Sven401/Larp-Escape/pull/82)
ist bei `8648f573ed7bbd2e76530b15d3f5883945b7c1a4` integriert; Eltern
`c487021` und `edde13f`, vollständiger Dateibaum identisch zum Dokumentationshead
(`4b8b66035af73d811c90f443f30e63ecd864f7a5`). Die damals neue Statusfortschreibung
ist inzwischen durch #83 integriert; ihre eigene erfolgreiche PR-CI steht oben
und ist kein rückwirkender Bestandteil dieser Tabelle.
Der operative Ablesestand steht ebenfalls in #25. Keine neue Softwareausführung,
Geräteprüfung oder optische Freigabe durch diesen CI-Statusauftrag.

Die folgende ursprüngliche Basis bezeichnet die Diagnose-/Entwurfssitzung.
Aktuelle Mergefortschreibung auf `codex/pr83-merge-dokumentation` im
Worktree `C:/Users/seide/Documents/Codex/2026-10-18/m/work/naeheglut`.
Vor Fortsetzung aktuellen Integrationsbranch, #25 und Arbeitsbaum neu prüfen.

18.10.2026 · Sitzung `01a150c8-48be-7362-acef-143e74396303`.
Nutzerauftrag: zuerst Stop-/Dunkelbefund klären, anschließend Spielanschluss
konkretisieren. Basis `7e5f4f38e88ed76358e31dc66d267da9cb0fbc1f`, eigener Branch
`codex/naeheglut-stop-spielanschluss`. Projektbeitrag, keine neue Produktfreigabe.

## Ergebnis und Belege

- [Stopdiagnose](../docs/led/naeheglut-stop-diagnose-2026-10-18.md): 650 Originalbilder
  neu gehasht/dekodiert, Originalkriterien exakt reproduziert. 386 schwarze
  TX-Ereignisse in drei Stopfenstern. Auch getrennte Papierrandpixel zeigen
  einen ähnlichen Anstieg (räumliche Korrelation 0,979–0,994).
- **Bestätigt:** erhöhte Kamerapegel sind nicht auf die CAL-Kernmasken begrenzt.
  **Vermutet:** flächiger Licht-/Kameraeinfluss. Reales Restlicht nicht
  ausgeschlossen; keine nachträgliche Basiskorrektur, keine optische Freigabe.
- [Reproduzierbarer Auswerter](../docs/led/probes/stop-audit/audit.py) und
  [kompakte Ergebnisse](../docs/led/probes/stop-audit/result.json). Private
  Ursprungsbilder bleiben am ursprünglichen Belegort.
- Bestehender C++-Prüfweg `run_camera_tests.py` frisch erfolgreich: 9 Camera-Fälle,
  7 NearGlow-Gruppen einschließlich 2002 Werte-/Kurvenfällen, 10 RSSI-Fälle.
  Compilerstart zunächst in Sandbox blockiert, anschließend derselbe Prüfweg
  außerhalb erfolgreich. Keine neue Geräte-/Kameraaufnahme oder Installation.
- [Spielanschluss](../docs/led/naeheglut-spielanschluss.md): technische Spezifikation
  für Eingangsfrische, Instanzen, Stopzustellung, Mischung und Profilbarriere;
  konkrete NG1–NG5-Pakete und Prüffälle. Noch keine Produktimplementierung.

## Offene Abhängigkeiten und nächster Schritt

1. Für physische Ursachenklärung den in der Diagnose vorab beschriebenen
   kontrollierten Schwarz–Gold–Stop–Schwarz-Lauf mit unabhängigen Referenzen
   gemeinsam durchführen. Anschließend gegebenenfalls elektrische Prüfung.
   Der aktuelle Software-/Bildbefund ersetzt diesen Ursachenbeleg nicht.
2. **Nutzerbestätigt:** durchlaufender Spieleffekt wie Funkeln; Erkennung einer
   korrespondierenden Spielstation startet ihn. Stations-/Quellenzuordnung
   konkretisieren. Der dokumentierte Pausenvorschlag bleibt offen; der Nutzer
   hat dazu mit dieser Antwort keine Entscheidung getroffen.
3. Danach NG1/NG2 implementieren; Einstieg mit synthetischem Eingang möglich.
   Kein eigener zweiter Effektkern/LED-Treiber. Zwei alte ±1-Übergangsabweichungen,
   Nah–Fern–Nah-Vollständigkeit und Nutzerurteil bleiben getrennt offen.

Der Original-Geräterückweg aus #76 bleibt letzter belegter Stand. Diese Sitzung
hat keine Geräte berührt. Auftragsumfang Diagnose/Spezifikation abgeschlossen;
optische Ursachenabnahme und Produktumsetzung bleiben explizite Folgearbeit.
