# Stationspilot · interaktiver Entwurf

Freigegebene Richtung: Veranstaltung und Station bleiben Kontext; kompakte
Unterbrechungsanzeige vor räumlich erhaltener Lichtauswahl. Nutzerfreigabe am
18. Oktober 2026 in Sitzung 01a1505d-dfd4-7ad2-89cf-8d7be1b02240.

## Öffnen und ausprobieren

Lokale Vorschau der laufenden Sitzung: http://127.0.0.1:4174/

Für einen frischen Klon mit Node.js 24 und npm: `npm ci`, anschließend `npm run dev`.
Die Laufzeit prüft sich vor Start/Build. Befehle aus diesem Ordner ausführen.
Der Entwurf ist ein isoliertes Web-Prüfartefakt, keine zusätzliche Produktplattform.

Im vorbereiteten Veranstaltungskontext genügt die Stationsauswahl: Suche,
Auswahl des eindeutig zugeordneten Geräts, Zugangsprüfung und frischer Status
laufen automatisch. Fehlende oder mehrdeutige Zuordnung wird nicht geraten.
Auch die Wiederverbindung nach Unterbrechung oder Rückkehr erfolgt automatisch.
Ein erneuter Verbindungsversuch ist kein erneuter Lichtauftrag.

In der Spielleitungsansicht stehen Zustand und Aktionen. Technische Hinweise
über Versand, Geräteantwort oder Sichtprüfung gehören in Diagnose und Abnahme.
Ausnahmen wie fehlender Zugang oder nicht erreichbare Station bleiben sichtbar.
Die vorbereitete Beispielkonfiguration hat bereits Gerätezugriff; nur der
entsprechende Testfall simuliert einen fehlenden OS-Zugriff.

Testfälle außerhalb des Telefons: Abbruch vor Antwort, keine Ergebnisse,
Zugangsfehler, Bluetooth aus, fehlender Gerätezugriff und unklare Zuordnung.
Antwortverlust lässt sich mit ausgeführtem oder nicht angekommenem Auftrag prüfen,
auch für Stop. Einrichtungshilfe nennt Kontext, zuständige Person und Prüfschritte.
„Erneut prüfen“ korrigiert keine Konfiguration; dafür gibt es ausschließlich außerhalb
des Telefons „Einrichtung korrigiert simulieren“. Danach bewusst erneut prüfen.
Bei schmaler Vorschau „Testfälle“ öffnen. Verbindungsverlust wird nach einem
kurzen sichtbaren Unterbrechungszustand automatisch erneut geprüft. Ein erfolgloser
Versuch endet in einem sichtbaren Fehler statt einer endlosen Suchschleife.

## Editierbarer Figma-Entwurf

Einrichtungshilfe und beide Auftragsausgänge sind auf Prototypstand `1dc9227`
nachgeführt. [Startscreen](https://www.figma.com/design/3uQpZfwREhQ9cHk80jMtRw?node-id=3-20),
[Testfälle](https://www.figma.com/design/3uQpZfwREhQ9cHk80jMtRw?node-id=16-1057).
56 aktuelle Stationsscreens, separater Simulationsbereich und drei frühere
technische Screens auf der Archivseite. 148 aktive Navigationsverbindungen mit
gültigen Zielen. Roboto, gebundene Farbvariablen, bestehende Button-Komponenten,
Auto Layout und editierbare Texte; keine flachen Screenshot-Screens.

Fehlgeschlagene Wiederholung, Hilfe/Rückkehr und separat simulierte externe
Korrektur sind getrennt. Start und Stop haben je eigene Pfade für ausgeführt /
nicht angekommen. Abbrechen führt zu einer pausierten Stationsverbindung.
Figma-Presentation: Hilfe → Rückkehr → erfolglose Wiederholung sowie automatische
Wiederaufnahme eines nicht angekommenen Stops im Browser bedient. Weitere
Verknüpfungen strukturell geprüft; nicht alle Screens einzeln durchgespielt.
[Vergleichsbilder und PR-Nachprüfung](figma-sync-review.md).

Figma-Raster: 390×844; Browser standardmäßig Pixel 10 mit geschützter
427×952-Runtime. Diese Unterschiede sind keine echte Android-App-Ausführung.

## Prüfung und Herkunft

- `npm run test:pilot`: 19 Zustands-/Befehlsregelfälle.
- `npm run check:runtime`: alle 28 geschützten Dateien unverändert.
- `npm run build`: Typprüfung und Vite-Produktionsbuild.
- `npm run test:sites`: vier Verpackungs-/Workerfälle; kein Deployment beauftragt.
- Aktueller Vergleich und Grenzen: [figma-sync-review.md](figma-sync-review.md).
- Historische Prüfungen: [PO](po-review.md), [Design-QA](design-qa.md).
- Screenshots und gemeinsame Vergleichsbilder: `evidence/`.
- Ausgewähltes Bild und Designbrief: `../../docs/ui-ux/`.

Vorlage: Product Design 0.1.56, `mobile-app`, Bootstrap vom 18.10.2026.
Geräteruntime und Assets unverändert erhalten; Icons aus der mitgelieferten
Radix-Bibliothek, Roboto aus lokal gepinntem Fontsource-Paket. Keine neue
Illustration oder Fotografie erforderlich. Beispielveranstaltung „Nacht der
Siegel“ und Station „Altes Tor“ sind fiktiv. Native Android-/BLE-/TalkBack- und
Nutzerabnahme bleiben eigenständige Schritte.
