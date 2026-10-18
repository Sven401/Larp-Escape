# Näheglut: Stopklärung und Spielanschluss

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
