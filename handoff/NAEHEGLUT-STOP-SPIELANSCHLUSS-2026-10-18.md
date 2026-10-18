# Näheglut: Stopklärung und Spielanschluss

**Status: PR #81 integriert · 18.10.2026.** Auf ausdrücklichen Folgeauftrag
nach vollständig grüner CI in `plan/projektuebergabe-kuratierter-stand` gemergt.
Operativer Projektstand: [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25).

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
zusätzliche Nachweise, nicht die obigen PR-Läufe. Letzter Ablesestand und
Dokumentationsabschluss stehen in #25.

Die folgende ursprüngliche Basis bezeichnet die Diagnose-/Entwurfssitzung.
Dokumentationsfortschreibung auf `codex/naeheglut-stop-integrationsstand` im
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
