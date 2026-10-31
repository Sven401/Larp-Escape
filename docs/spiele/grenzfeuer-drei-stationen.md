# Das verstummte Grenzfeuer — drei Stationen

Status: beauftragter Game-Design-Entwurf vom 31.10.2026, keine Produkt-, Geräte-
oder Nutzerabnahme. Quelle: Nutzerauftrag für drei Stationen mit freier
Hardwarewahl und Ergänzung der Bestellliste; Folgeauftrag am selben Tag:
„Mach servos und NFC Teil des Entwurfes“. Geprüfte Projektbasis:
`75c988374408086a28bdd80b98286d7e75139384`.
Verantwortlich: Game Designer. Unabhängige Konzeptgegenprüfung durch den
koordinierenden PM am 31.10.2026: kein blockierender Befund; Ausfallregel für S3
nach dessen Hinweis vereinheitlicht. Geräte-/Elektronikprüfung bleibt offen. Technische Umsetzung und elektrische Freigabe bleiben eigene Arbeit.

## Spielidee und Rahmen

Ein altes Grenzfeuer ist erloschen. Die Gruppe rekonstruiert die Nachricht des
letzten Hüters, richtet drei Lichtleiter aus und entzündet gemeinsam das Feuer.
Drei verschiedene Tätigkeiten tragen die Geschichte: deduzieren, kommunizieren,
gemeinsam handeln. Keine Gegnerrolle, kein Ausscheiden, keine körperliche Strafe.

Arbeitsannahmen: 3–6 Personen ab etwa 12 Jahren, 35–45 Minuten einschließlich
Einweisung und Wegen, betreuter Innenraum oder trockener überdachter Aufbau.
Eine Gruppe im Parcours; eine mobile Spielleitung genügt für den Papierpilot.
Stationszeiten: 8–10 / 10–12 / 5–8 Minuten; Wege/Einweisung 10 Minuten.
Kein harter Gesamttimer. Zeit, Alter und Schwierigkeit müssen im Spieltest geprüft
werden. Spieler brauchen kein Handy. Texte und Symbole sind zusätzlich zu Farbe
vorhanden; alle Bedienelemente müssen sitzend erreichbar sein.

## Station 1 — Das Archiv des Hüters

Zu Beginn übergibt die Spielleitung das NFC-Artefakt **Hütersiegel H**.
Die Auflagefläche trägt „Wecke das Archiv: Hütersiegel hier auflegen“. Erst eine
frische gültige Lesung des lokal zugeordneten H-Tags aktiviert die Tasten für
diesen Durchlauf; danach darf das Siegel entfernt werden. Eine fremde Karte
aktiviert nichts. Die Aktivierung allein ist kein Stationssieg.

Vier große Tasten sind mit **Blatt, Welle, Stern, Flamme** beschriftet, von links
nach rechts zusätzlich 1–4. Daneben liegen vier feste Satzkarten:

- „Die Flamme folgt unmittelbar auf das Blatt.“
- „Der Stern steht vor dem Blatt.“
- „Die Welle schließt die Nachricht ab.“
- „Jedes Zeichen erklingt genau einmal.“

Die Spieler ordnen die Karten gemeinsam und drücken die Zeichenfolge.
**Eindeutige Lösung: Stern → Blatt → Flamme → Welle (3, 1, 4, 2).**
Nach vier Eingaben wird geprüft; vorher zeigt die Anzeige nur die Anzahl
angenommener Eingaben, keine Richtig/Falsch-Auskunft je Taste. Falsche Folge:
kurzer Fehlerhinweis, Eingabepuffer leer, unbegrenzt erneut versuchen. Keine
Zeitgrenze zwischen Tasten. Gedrückthalten zählt einmal; erneute Eingabe erst
nach Loslassen. Gleichzeitige Eingaben gelten als ungültige Eingabe und starten
nur diesen Versuch neu. Erfolg bleibt bis zum bewussten Durchlaufreset erhalten.

Die Spielleitung übergibt beim Erfolg das NFC-Artefakt **Linse L** zusammen mit
der „Linserolle“ (Umschlag A). L wird nicht von S1 beschrieben; die Ausgabe durch
die Spielleitung stellt die physische Reihenfolge her. Inhalt:
„Richte links das Blatt, in der Mitte den Stern und rechts die Welle aus.“
Ein sichtbares Erfolgssymbol unterstützt die Übergabe; kein elektrisches Schloss.
Hinweise in drei Stufen: „Sucht zuerst das Ende“ → „Blatt und Flamme bilden ein
Paar“ → vollständige Folge. Weitere Eingaben nach Erfolg verändern nichts.

## Station 2 — Die Werkstatt der Lichtleiter

Auflegen des NFC-Artefakts **Linse L** weckt die Werkstatt und aktiviert deren
Drehschalter und Prüftaste einmal pro Durchlauf. H oder Feuer F reichen nicht.

Drei rastende Drehschalter mit je acht Positionen stehen links/Mitte/rechts auf
einer Bedienseite. Ihre Stellungen sind als 1–8 markiert. Auf der Rückseite des
Pults befinden sich die drei Symboltabellen. Ein Sichtschutz verhindert bequemes
Ablesen und Drehen zugleich; die Gruppe darf frei reden, wechseln und die Tabelle
abschreiben. Kooperation ist eine Einladung, keine körperliche Zwangshaltung.

Drei Servos bewegen je einen leichten Lichtleiter-Zeiger über eine eigene
Symbolskala auf der Rückseite. Jede gültige Raststellung führt zu der zugehörigen
Tabellenposition: Der Beobachter meldet das tatsächlich angezeigte Symbol zurück,
der Bediener korrigiert das Rad. So wird die Übersetzung als sichtbares mechanisches
Feedback erlebt. Vor Aktivierung stehen die Zeiger auf dem gesonderten Feld
„ruhend“. Acht Spielpositionen und eine Parkposition werden innerhalb eines
mechanisch erprobten Teilwegs kalibriert; keine Gradwerte oder garantierte
Servo-Präzision vorausgesetzt. Ungültige Kontaktzustände bewegen nichts weiter.
Die Tabellen bleiben als lesbare Referenz und barrierearmer Ersatz verfügbar.

| Position | Linker Leiter | Mittlerer Leiter | Rechter Leiter |
| --- | --- | --- | --- |
| 1 | Flamme | Welle | Stern |
| 2 | Blatt | Flamme | Blatt |
| 3 | Mond | Blatt | Sonne |
| 4 | Sonne | Mond | Berg |
| 5 | Berg | Stern | Flamme |
| 6 | Stern | Sonne | Mond |
| 7 | Welle | Berg | Welle |
| 8 | Auge | Auge | Auge |

Eine Person liest die Linserolle, andere übersetzen mithilfe der Rückseite und
stellen ein. **Lösung links/Mitte/rechts = 2 / 5 / 7.** Erst die Taste „Prüfen“
wertet alle drei stabilen Stellungen gemeinsam aus. Fehler: allgemeiner Hinweis,
keine Auflösung einzelner Räder; Einstellungen bleiben bestehen. Übergangszustände
zwischen Rastungen sind ungültig, niemals Erfolg. Erfolg bleibt verriegelt.
Die Logik prüft die Kontakte, nicht eine angenommene Servo-Istposition: SG92R
liefert hier keine externe Positionsrückmeldung. Die Spielleitung prüft vor dem
Lauf alle Anzeigen; bei Klemmen gilt der unten beschriebene Ersatzbetrieb.

Nach Erfolg übergibt die Spielleitung das NFC-Artefakt **Feuer F** und Umschlag B.
Die Feuerkarte darin lautet: „Öffnet Stern und Flamme. Schließt Blatt und
Welle. Zwei Hüter tragen das Feuer, solange drei Atemzüge vergehen.“
Hinweise: „Jedes Rad hat seine eigene Tabelle“ → „Links sucht ihr das Blatt“ →
„Stellt 2, 5, 7 ein und prüft“. Bei Seh-/Mobilitätshürden darf die Tabelle auf
die Bedienseite gelegt werden; das Rätsel bleibt erhalten.

## Station 3 — Das Grenzfeuer

Auflegen von **Feuer F** aktiviert das Finale; erst danach werden Schalter und
Haltefolge gewertet. Linse oder Hütersiegel aktivieren es nicht.

Vier rastende Schalter sind von links nach rechts **Stern, Blatt, Flamme, Welle**
beschriftet; oben heißt „offen“, unten „geschlossen“. Zwei große Taster tragen
je ein Handsymbol. Das Feuer ist eine hinter einer Streuscheibe montierte
Lichtanzeige mit einem vierten Servo: Sein leichter Zeiger wandert während
des Haltens über die Felder 1–2–3 und bei Erfolg zum Hütersymbol. So zeigt er
den gemeinsam getragenen Aufbau des Grenzfeuers. Keine Flamme, kein Schloss.
Die LED-Anzeige wiederholt den Fortschritt redundant. Bei Servodefekt gilt
ausschließlich der unten beschriebene manuelle Papierbetrieb; ein automatischer
LED-Ersatzmodus ist nicht vorgesehen.

**Lösung: oben / unten / oben / unten und beide Taster ununterbrochen drei
Sekunden halten.** Eine deutlich beschriftete Fortschrittsanzeige zeigt 1–2–3.
Drei Sekunden sind ein lokales Komfortfenster, kein Synchronisationsrennen;
Beginn ist die zweite gedrückte Taste. Wird losgelassen oder die Schalterstellung
falsch, fällt ausschließlich der Haltefortschritt auf null und der Zeiger
kehrt kontrolliert zur Ruheposition zurück. Kein Fortschritt
wird durch vorab gedrückte Tasten beim Start übernommen: zunächst beide loslassen.
Nach Erfolg bleibt das Feuer an; weitere Betätigung erzeugt keinen zweiten Sieg.

Die Spielleitung liest: „Das Grenzfeuer brennt. Eure Nachricht erreicht die
verlorenen Wanderer.“ Die Gruppe erhält ein gemeinsames Hütersiegel aus Papier.
Hinweise: „Offen bedeutet oben“ → „Stern und Flamme nach oben“ → vollständige
Schalterstellung und gemeinsames Halten. Bei Bedarf halten Spielleitung oder
Mitspieler mit; die Taster liegen ohne große Kraft erreichbar auseinander.

## Betrieb, Übergaben und Reset

NFC überträgt hier **nur eine zuvor lokal eingerichtete Artefaktidentität**.
Je ein NTAG213 für H, L und F wird samt eigener Reserve-UID in der betreffenden
Station zugeordnet: H→S1, L→S2, F→S3. Im Lauf werden weder NDEF noch Flags oder
Erfolge auf Tags geschrieben. Erfolgsübertragung erfolgt durch kontrollierte
Ausgabe von L/F und Umschlag durch die Spielleitung, nicht durch einen behaupteten
Fernfakt. UID-Kopien oder vorzeitig ausgegebene Karten könnten aktivieren; das ist
kein Zugangsschutz und kein D0-Nachweis. GM-Rechte, Run-Identität und Recovery
kommen niemals aus einem Spieltag. Einrichtung dieses lokalen Kartenmappings
und Übernahme in Spielpakete sind noch zu implementieren und zu prüfen.

Ein gültiges frisches Einzelkartenereignis setzt nur das lokale Aktivierungslatch.
Wiederholtes Lesen, Entfernen oder erneutes Auflegen ändern den laufenden Zustand
nicht. Falsche Karte: verständlicher Hinweis, kein Reset. Mehrere Tags, ungültiger
Frame oder Lesefehler: keine Aktivierung, keine Erfolgsmeldung; Karte einzeln neu
auflegen. Nach Reset erst leeres Lesefeld feststellen, danach neue Präsentation
verlangen, damit eine liegengebliebene Karte keinen neuen Durchlauf startet.

Der [NFC-Paketvertrag](../nfc-packet-contract.md) beschreibt Software-SPI,
Frameprüfung und die sieben Byte der Spielebene; dessen Grenzen bleiben bestehen.
Der [NTAG213-Gerätelauf](../nfc/physical-tag-test-2026-10-16.md) belegt nur den
historischen Einzeltagversuch. Er belegt weder diesen Mehrstationslauf noch die
neuen Aktivierungsregeln. NFC-Testpayload/Flags werden nicht zu Spielrechten
umgedeutet. Jeder Leser hat einen lokalen Besitzer; keine wartende Funkzentrale.

Die Umschläge sind physische Rätselinformationen. Jede Gruppe erhält eigene
Artefakte, Rollen und Durchlaufkarte. Die Spielleitung behält L und F bis zum
jeweiligen beobachteten Erfolg; vorhandenes Vorwissen allein aktiviert S2/S3 nicht.

Vorbereitung (Zielwert 15 Minuten, noch ungemessen): Pulte stellen, Tafeln und
Umschläge und sechs zugeordnete Original-/Reservetags prüfen, jeden Leser mit
richtiger und falscher Karte prüfen, Servoparkpositionen und alle Spielpositionen
bei geschlossenen Schutzhauben kontrollieren, alle Tasten/Schalter testen, richtige und falsche Lösung jeder
Station einmal durchspielen, danach bewusst neuen Durchlauf starten. S1 leer,
S2 alle Räder auf 1, S3 alle Schalter unten und Taster frei; alle
Aktivierungslatches aus, Lesefelder leer, vier Servos geparkt. H ausgeben, L/F
bei der Spielleitung behalten. Spielleitung erklärt:
kein Gewaltaufwand, Hinweise jederzeit, Licht allein ist kein Rätseltext.

Nach einer Gruppe (Zielwert 5 Minuten, noch ungemessen): Umschläge und H/L/F einsammeln,
Zustände dokumentieren, autorisierten Durchlaufreset an jeder Station bestätigen,
Ausgangsstellungen herstellen, Aktivierungslatches löschen, Lesefelder leeren
und Servos kontrolliert parken. Erst dann Bereitschaft prüfen; Tags bleiben
unverändert, es gibt keine zurückzusetzenden Tag-Erfolgsflags. Reset aller drei Boxen
ist drei lokale bestätigte Vorgänge; keine angenommene atomare Funkaktion.
Ein erfolgreicher Teil bleibt beim Wiederholen eines anderen Teils bestehen.

- Funk/Handy/Zentrale weg: lokale Rätsel laufen weiter; Hinweise werden getragen.
- Strom weg: Gruppe pausiert dort. Ohne belegte Wiederherstellung entscheidet
  die Spielleitung anhand ihrer Durchlaufkarte, nicht anhand einer geratenen LED.
  Ein Reset oder Stromausfall ist niemals automatisch ein Sieg.
- Taster klemmt, Schalter ungültig, Anzeige defekt: Station aus dem Elektroniklauf
  nehmen. Spielleitung prüft dieselbe Lösung am Papiermodell und dokumentiert
  „manuell gelöst“; nicht als technisch verifizierten Box-Erfolg ausgeben.
- NFC/Tag defekt: zunächst einzeln neu auflegen; danach zugeordnete Reservekarte
  desselben Typs ausgeben und Original einziehen. Bei Leserdefekt Papierbetrieb:
  Spielleitung prüft Kartenaufschrift und Rätsel; keine automatische Umgehung.
- Servo klemmt/zittert: betroffenen Servozweig abschalten, kein wiederholtes
  Anfahren gegen Widerstand. S2: gedruckte Tabelle/Handzeiger nutzen; S3:
  Spielleitung zählt drei Sekunden am Papiermodell. Als manuell protokollieren.
- Umschlag verloren: beschriftete Reserve aus dem Spielleiterpaket verwenden.
- Pause während S3-Halten: Haltefortschritt verwerfen; nach Fortsetzen beide
  Taster loslassen und erneut beginnen. Bereits abgeschlossener Sieg bleibt.

## Umsetzung und Abnahme

Dieser Entwurf ist **kein installierbares Spielpaket** und nicht der nach einem
Softwarefreeze geforderte unbekannte Drittspieltest. Die vorhandenen
[P0-Referenzen](../spielpakete/p0-referenzspiele-v1.md) bleiben unverändert.
[VA-01](../veranstaltungsarchitektur.md) bleibt die Betriebsgrundlage.
Keine neue App-Navigation und keine neue Wire-Nachricht werden hier festgelegt.

Benötigt werden lokale entprellte Kontakte, eine vierstellige Eingabefolge,
Raststellungsprüfung, ein Drei-Sekunden-Haltefenster, ein Erfolgs-Latch und
Lichtrückmeldung sowie NFC-Aktivierung und vier Servoanzeigen. Entwickler müssen die Abbildbarkeit im eingefrorenen Modul-
und Profilvertrag getrennt belegen; insbesondere NFC-Mapping, Servoausgabe, Drehschalteradapter und Reset
sind nicht als vorhanden behauptet. Pultverdrahtung/Pinplan, Strombudget und
reale Geräteabnahme folgen unter dem Hardwaremandat. Der aktuelle
[Hardwareprüfstand](../../Hardware/HARDWAREPLANUNG.md) ist keine Freigabe dieser
neuen Zusammenstellung. Einkauf: [führende Bestellliste](../../Hardware/BESTELLLISTE.md).

| Abnahmefall | Erwartung / Nachweis |
| --- | --- |
| Papierpilot ohne Elektronik | Alle drei Lösungen aus ausgegebenem Material herleitbar; Verstehen, Dauer und Hinweise mit mindestens einer Testgruppe protokollieren. Noch offen. |
| NFC-Aktivierung | H/L/F an allen drei Lesern (neun Kombinationen): nur H→S1, L→S2, F→S3; Reservezuordnung, mehrere Tags, Dauerauflage, Fehler, Reset mit liegendem Tag prüfen. |
| Servoanzeigen | S2 alle acht Positionen je Zeiger plus Parken; S3 Ruhe/1/2/3/Sieg; Klemmen und Versorgungsausfall ohne ungewollten Sieg. Kein Istpositionsnachweis aus PWM. |
| S1 richtige/falsche Folge, Mehrfachhalten | Nach H-Aktivierung löst nur 3-1-4-2; vier Eingaben pro Versuch; keine Wiederholung durch Halten. |
| S2 alle 512 Stellungen | Nach L-Aktivierung genau 2-5-7 erfolgreich; ungültiger Kontaktzustand nie erfolgreich. |
| S3 Schalter und Haltefenster | Nach F-Aktivierung alle 16 Masken; nur 1010 plus beide Taster für 3 s erfolgreich; Loslassen/Umstellen bricht ab. |
| Erfolg zweimal / Neustart / Reset | Einmaliger Erfolg; Recovery ausdrücklich geprüft, bewusster neuer Durchlauf ohne Altfortschritt. |
| Vollständig ohne Funk | Alle Rätsel und physische Übergaben spielbar; kein behaupteter verifizierter Fernfakt. |
| Ausfall / Zugänglichkeit | Papierersatz und Hinweisleiter funktionieren; Symbole lesbar, Bedienung sitzend getestet. |

Prüfstand dieser Dokumentation: logische Lösungen und Mengen werden rechnerisch
geprüft; relative Links und Diff geprüft. Kein Probandenlauf, Firmwarebuild,
Geräteupload, Dauertest oder elektrischer Nachweis fand für dieses Konzept statt.

Mechanik-/Versorgungskonzept: Vier SG92R bewegen ausschließlich leichte Zeiger
hinter transparenten Hauben. Keine Spieler greifen in den Bewegungsraum; keine
Personenverriegelung. Servohebel, Freigang, Begrenzung und sichere Parkfahrt sind
am Aufbau zu prüfen. Je S2/S3 ein PCA9685; Logik 3,3 V, Servospannung separat über
V+, gemeinsame Signalmasse, getrennte Plus-Schienen. Niemals Servoenergie aus
ESP-GPIO oder dessen 3,3-V-Regler. Externe geregelte 5-V-Servoversorgung pro Pult,
Sicherung, Pufferung und Abschaltung nach gemessenem Anlauf-/Blockierstrom;
OE/Startzustand verweigernd auslegen, keine unkontrollierte Bewegung beim Boot.
Pulsgrenzen/Last/Strombudget sind offene Hardwareabnahmen, keine fertige Schaltung.

Dokumentprüfung 31.10.2026 nach NFC-/Servo-Erweiterung: 9 relative Linkziele
vorhanden; genau drei Stationen; S1 eine Lösung unter 24 Permutationen, S2
eine unter 512 Stellungen; 24 numerische Bestellzeilen korrekt summiert.
G17 (zwei Umschläge plus zwei Reserve) und G18 (ein zentrales Paket) separat
geprüft. H/L/F-Mapping und neun mögliche Tag-/Stationspaare fachlich abgeglichen:
nur H→S1, L→S2 und F→S3 aktivieren. Dies ist Dokumentlogik, kein NFC-Gerätetest.
