# Design QA · Stationspilot

final result: passed

Datum: 18. Oktober 2026. Ausgangsbasis `17acf2c`; geprüftes Arbeitspaket im
UI/UX-Pilotbranch, zugehöriger Commit/PR in Git. Dies ist eine Prototypprüfung,
keine native Android-/BLE-, optische Geräte- oder Nutzertestabnahme.

## Vergleich und Evidenz

- Bestätigte visuelle Quelle: `../../docs/ui-ux/concepts/veranstaltung-station-v2.png`.
- Quelle: 853 × 1844 Rasterpixel, für die Betrachtung auf die nominalen
  390 × 844 Designpixel normalisiert. Kein vorgetäuschter identischer Deviceframe.
- Browser: 1400 × 1200, devicePixelRatio 1; unveränderter Pixel-10-Runtime-Screen
  427 × 952 CSS px bei Scale 1. Appviewport 427 × 904; Capture schneidet die
  überlagerten 64 px Status-/Safe-Area oben ab: 427 × 840 Appinhalt.
- Zustand: Verlust nach Lichtauftrag vor Antwort, Auswahl Funkeln, kein bestätigter
  aktueller Gerätestand. Quelle und Umsetzung sind im gleichen fachlichen Zustand.
- Vollvergleich: `evidence/comparison.png`; Fokus Status/Aktion:
  `evidence/comparison-status.png`. Beide tatsächlich geöffnet und beurteilt.
- Umsetzung: `evidence/interrupted.png`, Quelle und Vergleich nebeneinander.
  Browseraufnahmen sind vom Browser komprimierte JPEG-Inhalte; ihre ursprünglichen
  .png-Dateinamen werden bei der abschließenden Evidenznormalisierung in echte PNGs
  überführt. Kompression begrenzt die Beurteilung feiner Antialiasingdetails.
- Figma: `evidence/figma-interrupted.png`, nativer Export 390 × 844;
  Screen `3:317` in Datei `3uQpZfwREhQ9cHk80jMtRw`.

## Befunde und Korrekturverlauf

- **P2, behoben:** MobileScroll ist in der Vorlage absolut positioniert. In der
  ersten Appanbindung überlagerte es den Stationskopf. Die App-eigene Scrollhülle
  ist nun relativ im Flexlayout; geschützte Runtime unverändert. Nach frischem
  Start normaler Kopf und getrennt scrollbarer Inhalt sichtbar. Folgeaufnahmen
  ready/interrupted/reconnected zeigen den erhaltenen Kontext.
- **Prüfwerkzeug-Artefakt:** Locator-Klick auf Start scrollte den versteckten
  Telefonvorfahren, obwohl die Aktion sichtbar war. Prüfung mit nativer
  AX-Browserbedienung wiederholt: kein Versatz, Screenshot-Clip jeweils
  x=487/y=188/427×840. Kein Runtime-Lock umgangen und keine geschützte Datei geändert.
- **P2, behoben:** Teststeuerung überdeckte auf schmaler Vorschau den Telefoninhalt.
  Sie ist dort jetzt einklappbar und standardmäßig geschlossen.
- **P2, behoben:** Radiozeilen zunächst 43 px; auf mindestens 48 px vergrößert.
  Figma-Auswahlzeilen ebenfalls 48 px; alle 21 Screens bleiben innerhalb 844 px.
- Keine verbleibenden P0/P1/P2-Befunde im geprüften Prototypumfang.

## Fünf Fidelity-Flächen

| Fläche | Ergebnis |
| --- | --- |
| Schrift | Roboto 400/500/700 lokal; 16 px Basis, 30 px Station, 21.6 px Status, 24 px Licht. Hierarchie, Texte und Umbrüche visuell/DOM geprüft; feinste Kantenglättung wegen Screenshotkompression nicht abgenommen. |
| Abstand/Layout | 24 px Seitenrand, gleichbleibender Stationskopf, Unterbrechungsabschnitt vor erhaltener Lichtauswahl. Pixel-10-Breite weicht bewusst vom nominalen 390-px-Entwurf ab. Kein wesentlicher Inhalt abgeschnitten; große Schrift scrollt. |
| Farben | Oberfläche #fdfcfe, Akzent #6750a4, Text #13151c, sekundär #606573; gesperrte Aktionen #e9e8ed. Keine alleinige Farbcodierung. Keine formale Kontrast-/Accessibility-Freigabe. |
| Assets/Icons | Keine Rasterillustrationen erforderlich. Standardicons aus vorhandener Radix-Bibliothek, native Radiobuttons. Kleines Warndreieck ersetzt das generierte Kreissymbol bewusst; keine erfundene Illustration. Geschützte Geräteassets unverändert. |
| Inhalt | Veranstaltung/Station durchgehend; keine App-/Hardwaremarke als Kopf. Keine erfundenen Lichtmodi. Unklarer Ausgang, bewusste Suche, erneute Zugangs-/Frischeprüfung und gesperrte Befehle korrekt getrennt. |

## Im Browser bedient

Stationsauswahl → Suche → Zugriff erlauben → Kandidat → Zugang prüfen → bereit →
Start → Antwort; Stop → Antwort; Start → Verlust vor Antwort → gesperrt/unklar →
Erneut suchen → frische Prüfung → neuer Gerätestand. Dazu leere Suche,
Zugangsfehler samt Korrekturweg, Bluetooth aus/einschalten, verweigerter Zugriff,
Hintergrund/Rückkehr, Statusveraltung und 200 % Schrift. Simulation ist außerhalb
von Telefon/App als solche beschriftet. Bewusst keine echte Rechtevergabe oder
Systemeinstellung; „Zugriff erlauben“/„Bluetooth einschalten“ sind Simulationsschritte.

Neun unabhängige Modelltests belegen insbesondere: keine Befehle vor Freigabe,
kein zweiter offener Auftrag, keine Wiederholung beim Reconnect und keine späten
Antworten nach Verlust/Abbruch. Typprüfung, Build und Runtimeintegrität erfolgreich.
Browserkonsole bei der geprüften Bedienrunde: keine Warnungen/Fehler.

## Offene Grenzen / P3

- Native Android-Systemdialoge, TalkBack, tatsächliche Textskalierung des OS,
  Funk/BLE, reale Stations-/Boxzuordnung und Nutzerverständlichkeit bleiben offen.
- Statische Figma-Screens bilden den Zustandsvertrag ab; die vollständige
  Fehlersimulation liegt im Browserprototyp. Figma-Verknüpfungen sind strukturell
  geprüft, nicht als vollständige Figma-Presentation-Session durchgespielt.
- Ein kurzer Hinweis unter „Licht“ kann später entfallen; weitere Verdichtung
  ist eine optionale Gestaltungsiteration.
