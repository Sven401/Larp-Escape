# Hauptabläufe der Dev-App

Stand: **18. Oktober 2026**. **Abgeleiteter Designentwurf**, keine neue Appfunktion.
Bestätigter Zweck, separate Auslieferung, Quellen und Codebasis: [Projektreferenz](PROJEKTREFERENZ.md).
Die Dev-App dient Entwicklung, Tests und Experimenten. Technische Details sind
gewollt; Ziel, Versuchsstand und Wirkung müssen trotzdem verständlich bleiben.

## Aufgabenstruktur

Vorschlag: **Geräte · Baugruppen · Versuche · Protokolle**. Ein Versuch kann
manuelle Erkundung oder einen geführten Prüfablauf enthalten. Noch kein gewähltes
Layout. Gerät, Verbindung, Zugang und reale/simulierte Datenquelle bleiben sichtbar.
Einrichten ist auf Geräteebene auffindbar, nicht ausschließlich in der LED-Karte.

## D1 – Gerät vorbereiten und verbinden

**Ziel:** richtige Testbox, passende Software und nachgewiesenen Zugang erreichen.

1. Dev-App öffnen; Entwicklungszweck und Appversion erkennen.
2. Gerät suchen. Bluetooth-/Berechtigungsfehler konkret beheben; leere Suche
   von verweigertem Zugriff unterscheiden. Suchlauf bewusst fortsetzen.
3. Kandidat auswählen; Name/Adresse noch als ungeprüft behandeln.
4. Dienst, Identität, Zugang und Fähigkeiten prüfen. Vorhandenen Boxzugang bei
   Bedarf bewusst importieren: ersetzt derzeit den einen Zugang und trennt die
   Sitzung. Mehrere gespeicherte Zugänge sind Folgearbeit.
5. Unterstützte Baugruppen anzeigen. Nicht auslesbare Firmwareversion als unbekannt
   kennzeichnen; Fähigkeiten nicht aus einer geratenen Version ableiten.

**Ausnahmen:** falscher Zugang, ungeeigneter Dienst, fehlende Fähigkeit, belegte
Steuersitzung und unerreichbare Box jeweils mit nächstem Schritt. Kein automatischer
Gerätewechsel oder gewaltsame Sitzungsübernahme. Vor Zielwechsel laufenden Versuch
beenden oder dessen unklaren Abschluss erhalten.

**Ergebnis/Abnahme:** bereit erst bei bestätigter Identität, Berechtigung und
für die Aktion gültigem Zustand. Import oder BLE-Verbindung allein reicht nicht.
Reale Nachweise an das tatsächlich installierte App-/Firmwarepaar binden.

## D2 – Baugruppe gezielt ausprobieren

**Ziel:** eine Eingabe/Ausgabe untersuchen, ohne andere Versuche zu verlieren.

1. Matrix, Wake, Licht, Audio, NFC oder Funk wählen.
2. Unterstützten Umfang, Ausgangszustand und experimentelle Kennzeichnung sehen.
3. Parameter vorbereiten; Grenzen und Einheiten am Feld erklären. Auswahl sendet nichts.
4. Bewusst ausführen. Offenen Auftrag zeigen und unzulässige Folgeaktionen sperren.
5. Korrelierte Antwort lesen; technische Annahme von physischer Wirkung unterscheiden.
6. Sicht-/Hörbeobachtung oder externen Messbeleg ergänzen (neue Erfassung vorgeschlagen).
7. Stop beziehungsweise natürlichen Abschluss prüfen; bewusst neuen Lauf starten.

| Baugruppe | Konkreter Versuch | Erhaltene Grenze |
| --- | --- | --- |
| Licht | Effekt wählen → Start → Antwort → Licht beobachten → Stop → erneut starten | V2-Start ersetzt einen Effekt; parallele Instanzen benötigen neuen Vertrag |
| Audio | Track/Lautstärke → Play/Pause/Fortsetzen/Stop → Antwort und Höreindruck | BUSY-Pegel bestätigt keinen hörbaren Titel |
| Wake | Eingänge beobachten → räumliche Zuordnung prüfen → Anordnung ändern | Unbekannt ist nicht inaktiv; lokale Anordnung je Box |
| Matrix | Sammeln → drücken/loslassen → Treffer prüfen → Problemstellen → Bericht | Mehrfachkontakte und unbestückte Felder nicht als eindeutige Tasten deuten |
| Funk | Empfänger → Einzelpaket/Messlauf → Zustellung/Rückantwort/Alter → Ende | RSSI ist keine Meterangabe; Funk-ACK keine fachliche Bestätigung |

**Ausnahmen/Abnahme:** Veraltung sperrt betroffene Aktionen. Auftragstimeout oder
Trennung bleibt gegebenenfalls unklar. Protokollfristen bleiben gerätespezifisch;
kein neuer einheitlicher UI-Timer. Ein Stop ohne passende Bestätigung wird nicht
als erfolgreich abgeschlossen dargestellt.

## D3 – Experiment aufbauen, verändern und vergleichen

**Ziel:** Varianten nachvollziehbar untersuchen.

1. Versuch anlegen: Ziel, Gerät, App-/Firmwarestand, Baugruppe, Ausgangsparameter
   und erwartete Beobachtung. Manuelle Standangaben als solche kennzeichnen.
2. Reale Hardware, Simulation oder aufgezeichnete Daten ausdrücklich unterscheiden.
3. Ausgangslauf ausführen; Ergebnis und Beobachtung sichern.
4. Variante mit sichtbarem Parameterunterschied anlegen; bewusst anwenden.
5. Läufe anhand derselben Kriterien vergleichen; bewertet/unbewertet sowie
   bestanden/abweichend/unklar/abgebrochen unterscheiden.
6. Beenden und Ergebnis sichern/exportieren. Keine Zugangsschlüssel im Bericht.

**Bestand versus Ausbau:** Matrixbericht und RSSI-Journal sind vorhandene Teilwege.
Ein universeller Versuchseditor, Variantenvergleich und gemeinsamer Export sind
Vorschläge. Das [serielle LED-Live-Labor](../led/interactive-development.md) bleibt
ein separates Werkzeug. Kamerabilder künftig als Herkunftsbeleg verknüpfen;
eine integrierte Kamerasteuerung ist nicht zugesagt. Firmwareänderung, Installation
und chipgebundener Flash-Rückweg bleiben getrennte Vorgänge.

**Abnahme:** Wiederholen erzeugt einen bewusst neuen Lauf; vorherige Parameter
bleiben erhalten. Simulation erscheint nie als Hardwarebeleg. Auch bei fehlender
Abschlussantwort bleibt der Versuch auffindbar.

## D4 – NFC-Rundlauf prüfen

**Ziel:** Lesen, kontrolliertes Schreiben und Rücklesen nachvollziehen.

1. Echten Tag oder zeitlich begrenzten Handy-Testtag bewusst wählen.
2. Tag lesen; Identität, Kapazität und vorhandene Daten prüfen.
3. Bearbeiten oder Testdaten nur ins Formular übernehmen.
4. Schreibumfang bestätigen; denselben Tag erneut anhalten.
5. Rücklesen und Zuordnung prüfen. Fehlende Bestätigung bleibt unklar;
   zuerst lesen, nicht blind nochmals schreiben.
6. Für den Box-Rundlauf Tag zur Box bringen, Antwort erfassen und danach erneut
   am Handy lesen; Ergebnisse getrennt vergleichen.

**Ausnahmen/Abnahme:** Tagwechsel, Entfernung, Schreibschutz, Formatfehler und
Vordergrundverlust erklären. Testemulation endet nach bestehendem Vertrag bei
Seiten-/Vordergrundverlust oder nach drei Minuten. Das Testformat wird nicht
zum produktiven Spielobjekt erklärt. Ein anderer Tag bestätigt keinen alten Auftrag.

## D5 – Unterbrechung, Rückkehr und Abschluss

**Ziel:** nach Funkverlust oder Appwechsel erkennen, was gesichert ist.

1. Betroffene Echtzeitwerte entwerten; letzten Stand nur als Vergangenheit zeigen.
2. Offenen Auftrag als „Ausgang unklar“ erhalten, wenn sein Ende nicht belegt ist.
3. Im normalen manuellen Testweg bewusst neu verbinden; Identität, Zugang und
   frischen Zustand prüfen. Kein automatischer Start oder Wiederholen eines Auftrags.
4. Neuen Istzustand und früheres Auftragsergebnis unterscheiden: „Licht aus“
   allein belegt nicht die Ausführung des alten Stops.
5. Versuch fortsetzen, neuen Lauf beginnen oder mit bekanntem Abschlussstatus beenden.

Vor Navigation, die eine Sitzung beendet, die Wirkung auf laufende Versuche
kenntlich machen. Beim bewussten Abschluss Stop anfragen, sofern möglich, dann
Ergebnis sichern und trennen. Ohne Antwort „Beendet, Ausgabe nicht bestätigt“
statt Erfolg. Persistenz unklarer Versuche über Prozessende ist neue Folgearbeit;
der heutige In-Memory-Zustand belegt sie nicht. Manuelle Ausgaberegeln nicht als
pauschalen Stopp des autonomen Spiels interpretieren.

## Prüffälle für den ersten Umsetzungsschnitt

| ID | Fall | Erwartung |
| --- | --- | --- |
| DEV-01 | Zwei installierte Apps | Dev eindeutig erkennbar; Updates/Daten getrennt |
| DEV-02 | Falsches Gerät/fehlender Zugang | Kein Versuch freigeschaltet; konkrete Hilfe |
| DEV-03 | Parameter wählen/zurücknehmen | Kein Auftrag vor bewusster Ausführung |
| DEV-04 | Start/Stop beantwortet oder Antwort verloren | Bestätigt und unklar getrennt; keine automatische Wiederholung |
| DEV-05 | Funkverlust/Appwechsel/Szenario-Wechsel | Werte entwertet; offene Wirkung erkennbar |
| DEV-06 | Neue Variante | Ausgangslauf/Differenzen erhalten; Simulation und Hardware getrennt |
| DEV-07 | NFC-Schreibausgang unklar | Erst Rücklesen; anderer Tag bestätigt nichts |
| DEV-08 | Große Schrift/TalkBack | Ziel, Werte, Sperrgründe und Aktionen erreichbar |

Alle Fälle sind **offene Abnahmeziele** dieses Entwurfs. Software-, native UI-
und Hardwareprüfungen getrennt protokollieren; historische Teilbelege siehe
[Nachweisregister](../../handoff/BAUGRUPPEN-NACHWEISE.md).
