# UI/UX-Pilot: Box auswählen, verbinden und wieder aufnehmen

**Status: Pilot gestartet; Designbrief und Zustandsentwurf, keine implementierte neue UI.**
Datum: 18. Oktober 2026. Auftrag und Sitzung: siehe [Rollenvertrag](ROLLE.md).
Geprüfte Quellbasis: `7e72411bb1c416b5864036c79a1e17d9cf46b5ac`.

## Brief

Nutzer: Spielleitung mit Android-Handy an einer lokalen Box. Ziel: eine gefundene
Box bewusst auswählen, ihren Zugang prüfen, eine einfache Lichtaktion ausführen
und nach Funkverlust verständlich weiterarbeiten. Android/Compose und Material 3
sind die vorhandene Plattform. Der Pilot benötigt weder Internet noch Zentrale.
Demodaten bleiben ausdrücklich als Entwurf gekennzeichnet.

Umfang: Suche → Auswahl → Verbindung/Zugangsprüfung → LED-Bedienung → Abbruch →
bewusste erneute Suche/Verbindung → frischen Stand prüfen. Allgemeine
Veranstaltungsverwaltung, neue Rechtevergabe, NFC-Schreiben, Boxwechsel mit
Schlüsselverwaltung und echte Spielsteuerung gehören nicht in diesen Pilot.
Als vorhandene Bedienaktion dient **Funkeln**, mit bewusstem Start und Stop.
Die bestehende App bezeichnet diesen Pfad als LED-Bedienung mit offener
Spielintegration. Keine neue Spielstart-Funktion im Entwurf.

Gestalterische Ausgangslage aus Quellcode: `MaterialTheme(lightColorScheme())`,
Material-3-Typografie, Hauptabstände 24 dp, Gruppenabstand 16 dp, volle
Buttonbreite. Diese Werte sind Bestandsbeobachtungen, kein vollständig
abgenommenes Designsystem. Für die erste Exploration helle Material-Oberfläche,
ruhiger violetter Akzent, klare deutsche Texte und Status zusätzlich zur Farbe.
Violett ist eine Gestaltungsvorgabe für den Pilot, kein ausgelesener Farbwert.
Neue visuelle Optionen variieren vor allem Hierarchie und Interaktion.

## Geprüfter Bestand und Grenzen

| Aussage | Status und Quelle |
| --- | --- |
| ESP/NFC/Szenario sowie Verbindung und Baugruppen stehen in einer scrollbaren Hauptspalte | Im Code bestätigt: `MainActivity.kt`, `onCreate` |
| Aktueller Scan dauert zehn Sekunden und sammelt höchstens 16 Geräte | Im Code bestätigt: `AndroidBleRepository.scan` |
| Werbename und Bluetoothadresse sind kein geprüfter Identitätsnachweis | Bestehender Vertrag und `AndroidBleRepository.onServicesDiscovered` |
| Ein importierter Box-Zugang ist gespeichert; der Import trennt die bisherige Sitzung | README und `AndroidBleRepository.importLedAccess` |
| Authentisierung, frischer Status, Bereitschaft und passender Betriebsmodus sperren/erlauben LED-Befehle | `LedUiState.canStart/canStop` |
| Reconnect und Rückkehr benötigen frische Zustandsprüfung; kein automatischer Start | `LedBleSession`, `DeviceViewModel.foregroundEnded`, README |
| Box bleibt nach VA-01 lokale Spielautorität; BLE-Verlust bedeutet keinen behaupteten Spielstopp | Verbindliches Architektur-Soll; kein Nachweis allgemeiner Spielimplementierung |

Quellpfade relativ zum Repository:
`companion/android/app/src/main/java/de/larpescape/companion/` mit
`MainActivity.kt`, `data/device/{DeviceRepository,LedUiState}.kt`,
`ui/device/{DeviceViewModel,LedControls}.kt`,
`platform/ble/{AndroidBleRepository,LedBleSession}.kt`.
Weitere Verträge: [App-README](../../companion/android/README.md),
[BLE v2](../led/ble-wire-v2.md), [VA-01](../veranstaltungsarchitektur.md),
[Zugang](../companion/veranstaltungszugang.md).

Am 18.10.2026 ergab `adb devices` nach erfolgreichem Serverzugriff **kein Gerät**.
Keine aktuelle Appansicht aufgenommen, keine Bildschirmbedienung, Installation
oder Funkprobe. Die folgende Bewertung ist eine Quellcodeanalyse mit
Gestaltungshypothesen. Vorhandene Hardwarediagramme sind keine UI-Referenzen.
Product-Design-Kontextprüfung: keine gespeicherte Kontextdatei vorhanden.
Projektkontext bleibt in diesem Repository.

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

## Vorgeschlagene Designregeln

Ein eindeutiger Hauptschritt je Screen; Boxbezug und Sitzungsstatus im selben
Bereich. Zustände durch Text und Form, zusätzlich Farbe. Keine unbestätigten
Erfolgsanzeigen. Sekundäre Einrichtung getrennt von Start/Stop. Bedienziele
mindestens 48 dp als Entwurfsziel, Körpertext ungefähr 16 sp; 200 % Schriftgröße,
kleine Breite und Screenreader werden im nativen Review geprüft. Platz für
systemseitige Insets, keine schwebenden Elemente über wichtigen Aktionen.
Designziele sind noch keine gemessene Zugänglichkeitsabnahme.

## Akzeptanz- und Prüfplan

| Fall | Erwartung | Geeigneter Nachweis |
| --- | --- | --- |
| Normalweg | Gewählte Box, Prüfung, Bedienbereitschaft und bestätigter Auftrag sind unterscheidbar | Erst Prototypablauf, später Compose/Android |
| Bluetooth aus / Rechte abgelehnt | Verständliche Ursache und kontrollierter Rückweg | Android-Systemdialoge auf Emulator/Gerät |
| Keine Ergebnisse / falscher Zugang | Kein Erfolg, keine automatische Einrichtung; erneute Suche möglich | Prototyp und native Integration |
| Abbruch vor Start | Zustand entwertet, Bedienung gesperrt, explizite Wiederaufnahme | Zustandsprüfung + Android/Funk |
| Abbruch nach Versand, vor Antwort | „Ausgang unklar“; kein erneuter Start beim Reconnect | Fake-/Integrationstest + reale Funkprobe |
| Veralteter Status | Bestehende Sperre sichtbar, keine aktuelle Wirkung behauptet | Frischetests + UI-Zustandsvorschau |
| Wiederaufnahme | Zugang und frischer Status geprüft; bewusster neuer Start nötig | Prototyp + native Integration |
| Hintergrund / Rotation / Neustart | Bestehende Lebenszyklusregeln korrekt sichtbar | Android-Laufzeittest |
| Kleine Breite / große Schrift | Kein abgeschnittener Status, wesentliche Aktionen erreichbar | Native Vorschauen, 320 dp und 200 % Schrift |
| Screenreader / Farbe | Zustand und Aktionen ohne alleinige Farbcodierung verständlich | Semantikprüfung + TalkBack |

## Umsetzungspakete und aktueller Stand

1. **Erledigt:** Rollenvertrag, Quellen-/Zustandsanalyse, Brief und Prüfplan.
2. **Erzeugt, Auswahl offen:** [drei visuelle Material-nahe Alternativen](EXPLORATION.md) für B10
   (Unterbrechung/Wiederaufnahme); gleiche Inhalte, andere Hierarchie/Interaktion.
   Bildgenerierung erhält die obigen Code-/Designregeln, keine angeblich vorhandenen
   Screenshots. Nutzer wählt die Richtung vor dem Bau.
3. **Nach Auswahl:** editierbare Figma-Frames für B0/B3/B4/B6/B7/B10/B11 und
   Kernkomponenten/Zustände; Vorlagenherkunft und Texte im Repository festhalten.
4. **Danach:** bedienbarer Prototyp mit ausdrücklich simuliertem Verbindungsverlust
   und Befehlsausgang; kein BLE-Scheinbetrieb. Review gegen obige Akzeptanzfälle.
5. **Separater Implementierungsschritt:** Compose-Komponenten, Semantik und
   Zustandsbindung; bestehende Protokollregeln erhalten, native Prüfungen durchführen.

Offen: visuelle Auswahl, Figma-Datei, Prototyp, Nutzungsbewertung der Rolle,
Android-Sichtprüfung, tatsächliche Geräte-/Funkabnahme. Der laufende Chat kann
nach Auswahl fortsetzen; ein späterer Einstieg liest diese Datei und den PR.

## Prüfung dieser Rollenfassung

Am 18.10.2026: Skill-Creator-Validator bestanden, Agenten-TOML syntaktisch und
auf Pflichtfelder geprüft, lokale Markdownverweise und alle drei Bildhashes
geprüft; `git diff --check` ohne Whitespacefehler. Der Validator benötigte eine
lokal bereitgestellte YAML-Abhängigkeit; diese wurde anschließend entfernt.
Keine App-/Firmwareänderung, daher kein neuer Build oder Produkttest.
Keine Behauptung automatischer Agentenregistrierung im laufenden Desktop.
