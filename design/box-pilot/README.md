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

## Editierbarer Figma-Ausgangsentwurf

Die PO-Korrektur ergänzt den Browser um Einrichtungshilfe und beide Auftragsausgänge.
Diese Ergänzung ist in Figma noch nicht nachgeführt. Aktuelle Umsetzungsvorgabe:
[Zustandsvertrag](../../docs/ui-ux/BOX-PILOT.md), [PO-Nachprüfung](po-review.md).

[Datei und Startscreen](https://www.figma.com/design/3uQpZfwREhQ9cHk80jMtRw?node-id=3-20)

19 aktuelle Frames mit Normal-, Wiederaufnahme- und Ausnahmezuständen; drei
frühere technische Einzelschritte auf eigener Archivseite. 40 aktive
Zielverknüpfungen strukturell geprüft. Roboto, gebundene
Farbvariablen und drei wiederverwendbare Button-Komponenten; Auto Layout und
editierbare Texte. Quelle ist keine flache Screenshotablage. Figma-Präsentation
wurde nicht vollständig durchgespielt; der browsergeprüfte Ablauf ist maßgeblich.
Funkunterbrechung und andere Fehler werden im Browser über die getrennte
Teststeuerung ausgelöst. Figma bildet diese Zustände als separat erreichbare Frames ab.

Figma-Raster: 390×844; Browser standardmäßig Pixel 10 mit geschützter
427×952-Runtime. Diese Unterschiede sind keine echte Android-App-Ausführung.

## Prüfung und Herkunft

- `npm run test:pilot`: 19 Zustands-/Befehlsregelfälle.
- `npm run check:runtime`: alle 28 geschützten Dateien unverändert.
- `npm run build`: Typprüfung und Vite-Produktionsbuild.
- `npm run test:sites`: vier Verpackungs-/Workerfälle; kein Deployment beauftragt.
- Tatsächlicher Browservergleich und Grenzen: [design-qa.md](design-qa.md).
- Screenshots und gemeinsame Vergleichsbilder: `evidence/`.
- Ausgewähltes Bild und Designbrief: `../../docs/ui-ux/`.

Vorlage: Product Design 0.1.56, `mobile-app`, Bootstrap vom 18.10.2026.
Geräteruntime und Assets unverändert erhalten; Icons aus der mitgelieferten
Radix-Bibliothek, Roboto aus lokal gepinntem Fontsource-Paket. Keine neue
Illustration oder Fotografie erforderlich. Beispielveranstaltung „Nacht der
Siegel“ und Station „Altes Tor“ sind fiktiv. Native Android-/BLE-/TalkBack- und
Nutzerabnahme bleiben eigenständige Schritte.
