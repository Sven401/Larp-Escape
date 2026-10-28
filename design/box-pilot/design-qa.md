# Design QA · automatische Stationsverbindung

Historischer Ergebnisstand: passed auf c04a540.
Die erste PO-Korrektur steht in [po-review.md](po-review.md); der aktuelle
Figma-Abgleich und PR-Review in [figma-sync-review.md](figma-sync-review.md).
Dieser frühere Figma-/Browservergleich belegt die neue Einrichtungshilfe nicht.

18. Oktober 2026; Änderung auf Basis e03da1e nach konkretem Nutzerfeedback.
Die Spielleitung wählt eine Station. Bei vorbereiteter Veranstaltung erledigt
die App Suche, eindeutige Gerätezuordnung, Zugang und Wiederverbindung selbst.
Lichtaufträge werden dabei niemals erneut gesendet. Technische Erklärtexte aus
dem vorherigen Entwurf sind entfernt. Die initiale Prüfung bleibt in
[evidence/design-qa-initial.md](evidence/design-qa-initial.md) historisch erhalten.

## Visuelle Quelle und Vergleich

Quelle: aktueller Figma-Frame 3:191 (Bereit) und 3:317 (Unterbrechung), Datei
3uQpZfwREhQ9cHk80jMtRw. Beide Exporte 390×844; lokale Quellen
`evidence/figma-automatic-ready.png` und `evidence/figma-automatic-interrupted.png`.
Browser: 1400×1200, DPR 1, Pixel-Runtime 427×952 bei Scale 1; Appinhalt nach
64 px Chrome-/Safe-Area-Crop 427×840. Native Gerätebreite unterscheidet sich
vom nominalen Figma-Raster; kein vorgetäuschter pixelidentischer Deviceframe.

Gemeinsame Bildvergleiche tatsächlich geöffnet:
`evidence/automatic-comparison-ready.png` und
`evidence/automatic-comparison-interrupted.png`.
Verglichene Zustände identisch: Bereit/Licht aus und Verlust nach Start ohne
Antwort. Browseraufnahme mit vollständigem Bild gesichert, Appausschnitt gegen
DOMrechteck und tatsächliche Bildgröße geprüft. Direkte Clip-Aufnahmen lieferten
teilweise falsche Inhalte; sie wurden verworfen und durch geprüfte Ausschnitte
der Vollaufnahme ersetzt. Browserkompression begrenzt feine Antialiasingprüfung.

## Befunde und Nachprüfung

- P2: Stop war in Figma zunächst als zweite primäre Aktion dargestellt, im Browser
  sekundär. In Figma korrigiert, neuer Export und erneuter Vergleich zeigen die
  passende Gewichtung. Keine verbleibende funktionale oder hierarchische Abweichung.
- Deutsche Zeichen nach einer Dateiumwandlung im Browser fehlerhaft. UTF-8
  korrigiert; folgende Browserrunde zeigt Veranstaltungstext und Zustände korrekt.
- Kein verbleibender P0/P1/P2-Befund im geprüften Änderungsumfang.
- P3: kleine Abstandsabweichungen durch 390/427-px-Breite und native Standardicons.
  Keine wesentlichen Inhalte verdeckt. Radioflächen weiterhin mindestens 48 px.

## Fidelity-Flächen

Schrift: unverändert lokales Roboto 400/500/700; klare Station-/Status-/Aktion-
Hierarchie, keine abgeschnittenen Texte. Layout: fester Stationskopf, erhaltener
Lichtbereich; Statusfläche auf 160 px Mindesthöhe verdichtet. Farben: unveränderte
gebundene Palette #6750a4/#fdfcfe/#13151c, Zustand zusätzlich als Text. Assets:
Standardicons aus Radix, Geräteassets und alle 28 Runtime-Dateien unverändert.
Inhalt: nur Bereit, Lichtzustand, Auswahl und Start/Stop im Normalbetrieb;
keine Belehrung über Versand/Antwort/optische Prüfung. Figma und Browser stimmen
im aufgabenbezogenen Inhalt überein.

## Tatsächlich geprüfte Interaktionen

Ein Stationsklick → automatisches Verbinden → Bereit, ohne Suche-/Kandidaten-/
Verbindungsklick. Start → unklarer Ausgang → automatische Wiederverbindung →
frisch gelesener Lichtzustand. Kein weiterer Start. Unklare Stationszuordnung
→ verständliche Ausnahme, Start/Stop gesperrt. Bei 200 % Schrift kein horizontaler
Überlauf (427 px Client-/Scrollbreite). Browserkonsole ohne Warnungen/Fehler.

14 Modelltests: einschließlich eindeutiger/fehlender/mehrdeutiger Zuordnung,
verweigerter Berechtigung, alter Antworten, offener Aufträge und Wiederaufnahme
ohne Befehlsretry. Typprüfung, Build und Runtimecheck bestanden.
Figma: 19 aktuelle Screens, drei historische technische Frames auf Archivseite,
40 geprüfte aktive Verknüpfungen; keine aktive Route führt ins Technikarchiv.

Grenzen: Simulation vorbereiteter Konfiguration, keine echte neue Stations-
zuordnung oder OS-Berechtigung. Native Android-App, Firmware, BLE, TalkBack und
Nutzerabnahme unverändert offen. Figma-Presentation nicht durchgespielt.
