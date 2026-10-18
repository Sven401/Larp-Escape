# Historische Pilotanalyse vor automatischer Stationsverbindung

Status: ersetzt; Quelle: PR #78, c04a540, 18.10.2026. Keine Umsetzungsvorgabe.
Aktueller Vertrag: [BOX-PILOT.md](BOX-PILOT.md).

## Zustands- und Übergangsentwurf

| ID | Auslöser/Zustand | Sichtbare Rückmeldung und Aktion | Übergang/Bedingung |
| --- | --- | --- | --- |
| B0 | Einstieg, keine Sitzung | „Box verbinden“; „Boxen suchen“ | Bewusster Suchstart prüft Bluetooth und Rechte |
| B1 | Bluetooth aus oder Rechte fehlen | Konkreter Grund und passende Systemaktion; keine scheinbar leere Ergebnisliste | Nach Behebung bewusst erneut suchen; keine Startanfrage beim Appstart |
| B2 | Suche läuft | Fortschritt „Suche läuft …“; entdeckte Kandidaten | Bestand beendet nach 10 s; früheres Antippen wählt Kandidat |
| B3 | Ergebnisse / keine Ergebnisse | Namen als unbestätigte Kandidaten; „Erneut suchen“ bei leerer Liste | Auswahl beginnt frische Prüfung; keine vertraute Identität aus Werbenamen |
| B4 | Verbinden, Dienste/Zugang prüfen | Gewählter Kandidat; „Box-Zugang wird geprüft …“; „Abbrechen“ als zu implementierende UI-Aktion auf vorhandenes Trennen | Bedienung gesperrt; Authentisierung und aktueller Status erforderlich |
| B5 | Zugang fehlt/falsch oder ungeeigneter Dienst | Grund, „Box-Zugang importieren“ bzw. zurück zur Suche | Import ist keine neue Rechtevergabe; danach erneut verbinden |
| B6 | Authentisiert und bedienbereit | Geprüfter Boxbezug soweit verfügbar; „Lichtsteuerung“; Funkeln auswählen, „Start“, „Stop · LEDs aus“ | Auswahl sendet nichts; Start nur nach bestehenden Freigaberegeln |
| B7 | Auftrag offen | „Start angefragt · warte auf Box“; weitere Aufträge gesperrt | Erst passende authentisierte Antwort bestätigt; Write-ACK allein genügt nicht |
| B8 | Antwort bestätigt | „Box meldet: Funkeln“; sichtbares Licht separat prüfen | Keine Aussage über optische Wirkung ohne Beobachtung |
| B9 | Status veraltet, BLE noch vorhanden | „Status nicht aktuell“ und Grund für gesperrte Bedienung | LED-Befehle nach bestehender 1-s-Frischegrenze gesperrt; keine erfundene neue Frist |
| B10 | Funkverlust / Auftrag ohne Ergebnis | „Verbindung unterbrochen“; „Aktueller Boxzustand unbekannt“; bei offenem Auftrag „Ausgang unklar“; „Erneut suchen“ | Keine aktive/grüne Altanzeige; kein automatischer Retry; LED-Auftragsfrist 5 s im Bestand |
| B11 | Wiederaufnahme | Neu suchen, Kandidat wählen, Zugang und frischen Zustand prüfen | Erst dann B6/B8; alte Auswahl darf als Entwurf erhalten bleiben, führt aber keinen Auftrag aus |
| B12 | App im Hintergrund / Prozessneustart / bewusste Trennung | Sitzung geschlossen, keine Wiederaufnahmebehauptung | Bewusster Neustart des Suchwegs; Rotation erhält bestehendes ViewModel gemäß Bestand |

Wichtige Produktgrenze: Die Formulierung „Boxzustand unbekannt“ darf weder
„Box aus“ noch „Spiel gestoppt“ suggerieren. Das bestehende LED-Testverhalten bei
Verbindungsverlust ist nicht auf autonome Spiele verallgemeinerbar.

## Erste Hypothesen und Vorschläge

- **H1, P2:** Lange Baugruppenseite erschwert den schnellen Wechsel zwischen
  Verbinden und der gewünschten Aktion. Vorschlag: klarer Verbindungsabschnitt
  und fokussierte Lichtbedienung, weitere Baugruppen außerhalb des Pilotpfads.
  Quellbeleg: Reihenfolge der Controls in `MainActivity`; Sicht-/Nutzungsprüfung offen.
- **H2, P1:** `connected` bedeutet noch nicht bedienberechtigt. Gestaltung muss
  Transport, Zugangsprüfung und Bereitschaft unterscheidbar machen. Keine
  Behauptung eines vorhandenen visuellen Fehlers; UI-Vorschlag gegen echte Zustände prüfen.
- **H3, P1:** Abbruchnachricht „Getrennt“ allein erklärt unklaren Befehlsausgang
  und Wiederaufnahme nicht. Vorschlag: Grund, entwerteter Status und expliziter
  Suchweg am selben Ort. Tatsächliche Wahrnehmung noch ungeprüft.
- **H4, P2:** Bisher generische Bezeichnung „LED-Box“ hilft mehreren ähnlichen
  Kandidaten wenig. Keine erfundene Stationszuordnung: verlässliche Kurzkennung
  oder physischer Identifikationsweg benötigt eigenen technischen Vertrag.
  Pilot zeigt deshalb keine zugesagte Mehrbox-Identifizierung.
