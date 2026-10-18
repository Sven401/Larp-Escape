# Hauptabläufe der Spielleiter-/Nutzer-App

Stand: **18. Oktober 2026**. **Abgeleiteter Designentwurf** auf bestätigten
Produktentscheidungen; native Umsetzung und Nutzerabnahme offen.
[Projektreferenz, Apptrennung und Quellen](PROJEKTREFERENZ.md).

## Aufgabenstruktur und Normalweg

Vorschlag: **Veranstaltung/Stationen · Stationsbedienung · Vorbereitung · Zugänge**.
Aufgabenbereiche, noch keine festgelegten Tabs. Vorbereitung/Zugangsverwaltung
erscheinen passend zur tatsächlichen Befugnis. Rohpegel, Messparameter und
Versuchssteuerung gehören in die separate Dev-App.

Bediengrundsatz aus [PR #78, aktueller Stationspilot](https://github.com/Sven401/Larp-Escape/blob/3b2f6b4ffd561681dc5b14d13d2f0225eb640cd4/docs/ui-ux/BOX-PILOT.md):
**Station auswählen → automatisch verbinden und prüfen → bedienen.**
Im vorbereiteten Normalweg keine manuelle BLE-Kandidatenwahl, kein Dateiimport
und keine ständigen technischen Abnahmetexte. Der Pilot demonstriert Licht,
nicht fertige allgemeine Spielsteuerung. Automatische Verbindung braucht einen
nativen Anschluss; bisherige Android-Lebenszyklusregeln nicht still umgehen.

## S1 – Veranstaltung vorbereiten und Zugang herstellen

**Ziel:** vor dem Einsatz Station–Box-Zuordnung und Bedienberechtigung kennen.

1. Veranstaltungskontext auswählen oder im noch zu spezifizierenden Einrichtungsweg
   übernehmen. Ohne vorbereiteten Kontext gezielt zur Einrichtung führen.
2. Station–Box-Zuordnung und lokalen Zugang prüfen. Ähnliche Namen oder Funkadressen
   allein begründen keine Zuordnung.
3. Box-Verantwortlicher ordnet mit Einrichtungsschlüssel die Box der Veranstaltung
   zu und richtet den ersten Spielleitungszugang ein.
4. Weitere Zugänge von einer an dieser Zielbox berechtigten Spielleitung bewusst
   vergeben lassen. Jede Box bestätigt ihre eigene dauerhafte Übernahme.
5. Stationsliste unterscheidet bedienbar, nicht erreichbar und noch einzurichten.
   Unerreichbar bedeutet weder unberechtigt noch ausgeschaltet.

**Offen:** Einladungs-/Übertragungsmedium, Eingabeform und technische Zuordnung.
Kein QR-, Cloudkonto- oder Universalschlüsselverfahren beschlossen. Fehlende oder
mehrdeutige Zuordnung führt zu Hilfe mit Veranstaltung, Station, zuständiger
Person und konkretem Prüfauftrag. Keine beliebige Ersatzbox wählen.

**Abnahme:** „Erneut prüfen“ ändert keine Konfiguration oder Rechte. Rückkehr aus
Hilfe erhält den ungelösten Fehler. Erst echte Korrektur und erfolgreiche neue
Prüfung geben Bedienung frei. Kapazitätsfehler verdrängen keine bestehenden Zugänge.

## S2 – Station öffnen und bedienen

**Ziel:** eine vorbereitete Station ohne Protokollwissen bedienen.

1. Station im Veranstaltungskontext antippen.
2. App sucht die eindeutig zugeordnete Box, prüft Identität, Zugang und frischen
   Zustand automatisch. „Station wird verbunden …“ mit Abbruch anbieten.
3. Bei fehlenden Systemrechten/Bluetooth konkrete Systemhilfe; nach Behebung
   automatisch prüfen. Nach erfolglosem begrenztem Versuch „Nicht erreichbar“
   und „Erneut versuchen“, keine endlose Suchschleife.
4. Bereiten Stationszustand und zulässige Aktionen zeigen. Auswahl bleibt lokal.
5. Aktion bewusst auslösen; offenen Auftrag und passende Sperren zeigen.
6. Erst passende Bestätigung als Ergebnis darstellen. Fehler mit Ursache und
   nächstem Schritt; Absenden allein ändert nicht den angezeigten Gerätestand.

**Erster Schnitt:** Funkeln wählen → Start → bestätigter Zustand → Stop.
Spielstart, Pause, Reset, Hinweise und weitere Aktionen benötigen eigene
Produkt-/Profilverträge; ihre Verfügbarkeit wird nicht vorausgesetzt.
Bei Reset und anderen folgenreichen Aktionen Ziel und tatsächlichen Daten-/
Fortschrittsverlust vor Bestätigung nennen. Harmlose Bedienaktionen benötigen
nicht pauschal einen zusätzlichen Bestätigungsdialog.

## S3 – Verbindung verlieren und wieder aufnehmen

**Ziel:** ohne technischen Suchablauf weiterarbeiten, ohne alte Aktionen zu verdoppeln.

1. Zustand entwerten: „Aktueller Zustand unbekannt“, bei offenem Auftrag auch
   „Ergebnis offen“. Kein Rückschluss auf Spielstopp oder Licht aus.
2. Automatisch begrenzt neu verbinden; Identität, Zugang und frischen Zustand
   prüfen. Verbindung wiederholen, niemals Spiel-/Lichtauftrag wiederholen.
3. Nach Erfolg tatsächlichen Zustand zeigen. Beispiel bei zuvor ausgeschaltetem
   Licht: nicht angekommener Start lässt es aus. Bei zuvor laufendem Effekt kann
   ein nicht angekommener Stop ihn weiterlaufen lassen. Maßgeblich ist die frische
   Beobachtung; autonome Zustandsänderungen nicht aus dem Auftrag erraten.
4. Falls nötig neue Aktion bewusst auslösen. Historischer Auftragsausgang bleibt
   ohne korrelierten Beleg offen, auch wenn der jetzige Zustand bekannt ist.
5. Bei Misserfolg Hilfe und bewussten erneuten Verbindungsversuch anbieten.

**Bewusste Pause:** Abbrechen oder Rückkehr zur Liste beendet automatische Versuche.
Erst „Erneut verbinden“ oder neue Stationsöffnung beginnt sie wieder. Alte Rückrufe
aktivieren weder verlassene Station noch Aktion. Prozessneustart beginnt ohne
behauptete alte Sitzung; gespeicherter Kontext ist kein Recht und kein aktueller
Zustand. Hintergrund/Rückkehr und Rotation nativ prüfen; kein Hintergrundservice
wird hier beschlossen.

## S4 – Station wechseln und Steuerung übergeben

**Ziel:** mehrere Stationen nacheinander bedienen, ohne andere Spielleitungen zu verdrängen.

1. Stationsliste öffnen; offene Ergebnisse der bisherigen Station dort zuordnen.
2. Andere Station wählen; vorherige Steuersitzung geordnet verlassen.
3. Neue Box eigenständig zuordnen, authentisieren und frisch prüfen. Alte
   Statuswerte, Zugänge oder Befehle nicht auf das neue Ziel übertragen.
4. Bei belegter Steuerverbindung „Station wird bereits bedient“ und Weg zur
   bewussten Übergabe anbieten. Keine automatische Übernahme.
5. Nach tatsächlicher Freigabe neu prüfen, dann bedienen.

**Offene Detailentscheidung:** Übergabeprotokoll, Zeitgrenzen und welche Information
zur anderen Spielleitung sichtbar sein darf. UI ersetzt diese Spezifikation nicht.
Eine App muss nicht alle Boxen gleichzeitig verbinden. Dev- und Spielleiter-App
auf demselben Handy erhalten keine Ausnahme von einer aktiven Steuersitzung je Box.

## S5 – Inhalte vorbereiten und je Box aktivieren

**Ziel:** lokalen Entwurf, veröffentlichtes Paket und aktive Konfiguration unterscheiden.

1. Lokalen Szenariotext bearbeiten/importieren und als Leseprobe prüfen.
2. Ausführbares Profil/Veranstaltungspaket separat auswählen und validieren;
   heutiger Textentwurf erzeugt noch kein ausführbares Profil.
3. Zielboxen, Inhalte, Fähigkeiten und offenen Übertragungsstand prüfen.
4. Vollständig vorbereitete Konfiguration je Box unter zulässigen Betriebsbedingungen
   bewusst aktivieren; Folgen für laufenden Durchlauf vorher zeigen.
5. Aktiven Stand je Box zurückprüfen. Unerreichbare, inkompatible oder unbestätigte
   Ziele bleiben ausstehend, abgelehnt oder unklar.

**Offen:** fachlicher Profileditor, Veröffentlichung, Verteilung und Zentralenanschluss.
Keine stille Aktivierung über alle Boxen. Lokal speichern bedeutet „Auf diesem
Handy gespeichert“, nicht „Station aktualisiert“. Produktive NFC-Spielobjekte
benötigen eigenen Vertrag; Rohdaten-/Testeditor und Handy-Testtag werden nicht
in den normalen Spielleitungsablauf übernommen.

## S6 – Zugänge verwalten und wiederherstellen

**Ziel:** Folgen vorab verstehen und pro Box belegten Abschluss erhalten.

1. Zielbox und Vorgang wählen; erforderliche Befugnis prüfen.
2. Umfang und Folgen anzeigen, dann bewusst bestätigen.
3. Je Zielbox dauerhafte Übernahme und frischen Zugang prüfen.
4. Fehlende Antwort bleibt unklar: tatsächlichen Stand prüfen, Verwaltungswechsel
   nicht automatisch wiederholen.

| Vorgang | Befugnis und Nutzerführung |
| --- | --- |
| Zugang vergeben/entziehen | Berechtigte Spielleitung an Zielbox; letzten Zugang schützen; je Box bestätigen |
| Einrichtungsschlüssel wechseln | Bisheriger Schlüssel; neuen separat sichern und Verfügbarkeit nachweisen; erst nach erfolgreicher Prüfung alten aus Aufbewahrung entfernen |
| Alle berechtigten Handys verloren | Einrichtungsschlüssel; mindestens einen Ersatzzugang vorbereiten; alte Zugänge sperren und Ersatz gemeinsam übernehmen; Profile/Spielstand erhalten |
| Veranstaltung wechseln | Einrichtungsschlüssel; neue Veranstaltung und Spielstandreset bestätigen; alte Veranstaltungsrechte sperren, Profile erhalten |
| Einrichtungsschlüssel verloren | Kein datenerhaltender Ersatzweg; Neuinitialisierung mit Datenverlust gesondert bestätigen; technischer Auslöseweg offen |

Maßgeblich: [Zugangsvertrag](../companion/veranstaltungszugang.md), K1–K5/W1–W6
und Unterbrechungspunkte. Keine neue zentrale Administratorrolle und kein
versprochener sofortiger Entzug auf unerreichbaren Boxen.

## S7 – Überblick bei unvollständiger Verbindung

**Ziel:** bekannte Stände und offene Aufgaben sehen, ohne Vollständigkeit vorzutäuschen.

1. Stationen mit bestätigtem oder letztem bekanntem Stand anzeigen.
2. Quelle/Alter zugänglich halten; Cache wird durch Empfang vom Vermittler nicht aktuell.
3. Unklare/ausstehende Ergebnisse öffnen; frischen Stand bei Kontakt prüfen.
4. Spätere Sammelaufträge je Ziel bestätigen; Teilabschluss verständlich darstellen.

**Späterer Ausbau:** Zentrale/Abgleich gemäß D3, keine vorhandene Appfunktion.
Kein globaler Sofortstart/-stop oder Sicherheits-Not-Aus über Funk zugesagt.
Fehlende Zentrale blockiert vorbereiteten lokalen Spielbetrieb nicht.

## Abnahmekatalog

| ID | Fall | Erwartung |
| --- | --- | --- |
| SL-01 | Vorbereitete Veranstaltung | Stationsauswahl genügt; Verbindung automatisch |
| SL-02 | Zugang/Zuordnung fehlt | Konkrete Hilfe; Fehler bleibt ohne echte Korrektur |
| SL-03 | Start/Stop mit/ohne Zustellung, Antwort verloren | Frischer Istzustand korrekt; kein automatischer zweiter Auftrag |
| SL-04 | Abbrechen/Stationswechsel | Kein späterer Zugriff oder Ergebnis am falschen Ziel |
| SL-05 | Andere steuernde Sitzung | Keine Verdrängung; verständlicher Übergabeweg |
| SL-06 | Hintergrund/Rotation/Neustart | Native Sitzungsregeln und sichtbarer Status stimmen überein |
| SL-07 | Entwurf/Paket/aktive Boxkonfiguration | Verschiedene Stände; keine falsche Übernahmebestätigung |
| SL-08 | Verwaltungsabbruch/Offlineziel | Je Box bestätigt, ausstehend oder unklar; keine blinde Wiederholung |
| SL-09 | Zentrale/Funk fehlt | Lokale Autonomie; Cache nicht als aktuelle Erreichbarkeit |
| SL-10 | Große Schrift/TalkBack/kleine Breite | Hauptaktion, Status, Hilfe vollständig bedienbar |
| SL-11 | Beide Apps installiert, Dev aktualisiert | Spielleiter-App/Daten unverändert; keine Testoberfläche im Produktweg |

**Offene Abnahmeziele** des neuen Produktstands. Prototypbelege aus PR #78 gelten
nur für dessen Simulation. Native Umsetzung, passendes App-/Firmwarepaar und
Nutzerprüfung müssen die tatsächliche Bedienung belegen. Technische Abhängigkeiten:
#27/#28/#37 (Zugang), #33/#34 (Status/Sitzung), #30/#31 (Profil/Spiel),
D3/D4 (Veranstaltung/Abnahme).
