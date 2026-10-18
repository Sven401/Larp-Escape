# Stationspilot · interaktiver Entwurf

Freigegebene Richtung: Veranstaltung und Station bleiben Kontext; kompakte
Unterbrechungsanzeige vor räumlich erhaltener Lichtauswahl. Nutzerfreigabe am
18. Oktober 2026 in Sitzung 01a1505d-dfd4-7ad2-89cf-8d7be1b02240.

## Öffnen und ausprobieren

Lokale Vorschau der laufenden Sitzung: http://127.0.0.1:4173/

Für einen frischen Klon mit Node.js 24 und npm: `npm ci`, anschließend `npm run dev`.
Die Laufzeit prüft sich vor Start/Build. Befehle aus diesem Ordner ausführen.
Der Entwurf ist ein isoliertes Web-Prüfartefakt, keine zusätzliche Produktplattform.

Die Stationsauswahl führt über Suche, simulierte Zugriffserlaubnis, Kandidat und
Zugangsprüfung zur Lichtbedienung. Auswahl allein sendet nichts; Start/Stop zeigen
einen offenen Auftrag und anschließend eine simulierte Antwort. Testfälle lassen
sich außerhalb des Telefons wählen; bei schmaler Vorschau „Testfälle“ öffnen.
„Abbruch vor Befehlsantwort“ wählen und Start drücken: Ausgang unklar, neue Suche,
frische Prüfung, kein erneutes Senden. Die simulierte Station kann den ersten
Auftrag bereits ausgeführt haben; Wiederverbinden liest diesen Stand neu.

Zusätzlich: keine Suchergebnisse, falscher Zugang, Bluetooth aus, verweigerter
Zugriff, veralteter Status, Hintergrund/Rückkehr und Schriftvergrößerung.
Die Schalter bedienen keine echten Betriebssystemrechte oder Bluetoothgeräte.
Keine Schlüsseldatei nötig; keine Erlaubnis oder Firmwareänderung wird ausgeführt.

## Editierbarer Figma-Entwurf

[Datei und Startscreen](https://www.figma.com/design/3uQpZfwREhQ9cHk80jMtRw?node-id=3-20)

21 native Frames: B0–B12, vier Fehlerzustände, drei Wiederaufnahmescreens und
Stop-Auftrag. 45 Zielverknüpfungen strukturell geprüft. Roboto, gebundene
Farbvariablen und drei wiederverwendbare Button-Komponenten; Auto Layout und
editierbare Texte. Quelle ist keine flache Screenshotablage. Figma-Präsentation
wurde nicht vollständig durchgespielt; der browsergeprüfte Ablauf ist maßgeblich.
Funkunterbrechung und andere Fehler werden im Browser über die getrennte
Teststeuerung ausgelöst. Figma bildet diese Zustände als separat erreichbare Frames ab.

Figma-Raster: 390×844; Browser standardmäßig Pixel 10 mit geschützter
427×952-Runtime. Diese Unterschiede sind keine echte Android-App-Ausführung.

## Prüfung und Herkunft

- `npm run test:pilot`: neun Zustands-/Befehlsregelfälle.
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
