# Bestellliste — Larp-Escape

Stand: 31.10.2026. Status: **geplanter Bedarf, nichts bestellt**.
Führende Projektliste für den neuen Auftrag
[Grenzfeuer: drei Stationen](../docs/spiele/grenzfeuer-drei-stationen.md).
Im Integrationsbaum `75c988374408086a28bdd80b98286d7e75139384` wurde nach
Bestell-/Beschaffungs-/Einkaufslisten gesucht; zusätzlich GitHub-Issues nach
„Bestellliste“. Keine bestehende führende Liste gefunden. Frühere CAD-BOMs bleiben
Quellen ihrer Revision und werden nicht überschrieben. Sollte eine außerhalb
dieser Quellen geführte Liste existieren, diese Einträge dort zusammenführen.

Nutzerfreigabe: Game Designer darf für das Konzept Hardware frei auswählen und
hier ergänzen; Folgeauftrag: Servos und NFC sind fester Bestandteil. Keine Kauftransaktion beauftragt. Mengen sind Bedarf für **einen
Parcours mit drei physischen Stationen**. Bestand nicht inventarisiert; daher
„Neubedarf“ zunächst Bruttobedarf, keine Behauptung, dass nichts vorhanden sei.
Nettobestellmenge = max(0, Bedarf minus tatsächlich zugeordneter geeigneter Bestand).
Keine Reserveelektronik außer drei ausdrücklich ausgewiesenen Ersatz-NFC-Tags eingerechnet. Budget, Lieferant und Preis noch offen;
keine erfundenen Preise oder Lieferzusagen.

## Elektronik und Bedienung

| ID | Artikel / ausgewählte Referenz oder Beschaffungsspezifikation | S1 | S2 | S3 | Gesamt | Zweck / Anschluss / offener Abgleich |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| G01 | ESP32-DevKitC V4 mit WROOM-32E, Referenz [Espressif](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32/esp32-devkitc/user_guide.html) | 1 | 1 | 1 | 3 | Je autonome Box; GPIO/I2C. Konkrete Boardrevision, Firmwareziel und freie Pins vor Beschaffung abgleichen; vorhandene ESPs nicht automatisch anrechnen. |
| G02 | Große Schließertaster, momentary, potentialfreier Kontakt; Referenz [Adafruit 1192](https://www.adafruit.com/product/1192) | 4 | 1 | 2 | 7 | Vier Symbole, Prüfen, zwei Hände. GPIO gegen Masse mit passendem Pull-up/Entprellung; enthaltene Tasterbeleuchtung bleibt unbeschaltet. |
| G03 | Rastender achtstufiger Drehschalter SP8T, [Adafruit 2925](https://www.adafruit.com/product/2925) | 0 | 3 | 0 | 3 | Je acht getrennte Kontakte zur eindeutigen Stellungserfassung; Mechanik, knob-Kompatibilität und Kontaktübergänge prüfen. |
| G04 | MCP23017-Breakout, [Adafruit 5346](https://www.adafruit.com/product/5346) | 0 | 2 | 0 | 2 | 32 I/O zusammen: 24 Drehkontakte plus Prüftaster; I2C mit zwei verschiedenen Adressen. 3,3-V-Betrieb/Pull-ups und echte Pinbelegung prüfen. Keine bestehende Matrixverdrahtung übernehmen. |
| G05 | Rastender SPST-Ein/Aus-Pultschalter ohne Beleuchtung, beschriftbar | 0 | 0 | 4 | 4 | Vier Feuerwege; potentialfreie Kontakte an Eingänge. Konkrete Bauform nach Frontplattenmaß auswählen. |
| G06 | Acht adressierbare RGB-Pixel auf kleinem Modul, Referenz [NeoPixel Stick 1426](https://www.adafruit.com/product/1426) | 1 | 1 | 1 | 3 | S1 Eingabefortschritt, S2 Status, S3 Haltefortschritt/Feuer. Versorgung und Datenpegel nach ausgewählter Revision; kein bereits abgenommener Effekt. |
| G07 | Logikpegelwandler für adressierbare LED-Daten, 3,3-V-Eingang / zur LED passender Ausgang | 1 | 1 | 1 | 3 | Eigener LED-Datenpfad je Box. Konkretes Treiberteil, Versorgung und Ausgangsverhalten durch Hardwareverantwortlichen dimensionieren. |
| G08 | Zertifiziertes externes 5-V-USB-Netzteil, Auslegungsziel mindestens 2 A je Box | 1 | 1 | 1 | 3 | Nur Logik, NFC und LEDs im Innenraum-Pilot; reale Last und Reserve messen. Servoenergie separat über G23. Je Board genau ein zulässiger Versorgungsweg; keine parallele USB-/Pin-Einspeisung. |
| G09 | Passendes USB-Strom-/Datenkabel für ausgewähltes Controllerboard | 1 | 1 | 1 | 3 | Steckertyp und Zugentlastung abgleichen. Netzteil/LED-Verteilung als gemeinsames Versorgungskonzept planen. |
| G10 | Niederspannungs-Verteil-/Schutzsatz je Box | 1 | 1 | 1 | 3 Sätze | Klemmen, Hauptschalter, Sicherung/Halter, Abblockung, LED-Datenwiderstand; Werte/Leiterquerschnitt erst nach Strombudget festlegen. |
| G11 | Verriegelbare Stecker-, Litzen-, Isolier- und Montageleitungssatz | 1 | 1 | 1 | 3 Sätze | S1 vier Kontakte, S2 drei mal neun Drehschalteranschlüsse plus Prüftaster/I2C, S3 sechs Kontakte; je Licht- und Versorgungszweig plus drei NFC-SPI-Anschlüsse und zwei Servo-I2C-Anschlüsse. Längen aus realer Pultplanung ableiten. |

G03 benötigt passende Drehknöpfe: drei Stück in G13 enthalten. G04 bezeichnet
fertige Breakouts, nicht die nackten ICs. S2 nutzt lokale Leitungen innerhalb
eines Pults; kein I2C-Kabel zwischen Stationen. GPIO-Pinplan, Bootpins und
elektrische Schutzdetails sind offen, die Tabelle ist keine Anschlussanleitung.

## NFC und Servos — verbindlicher Konzeptbedarf

| ID | Artikel / Referenz | S1 | S2 | S3 | Gesamt | Verwendung / offener Abgleich |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| G19 | PN532-Breakout, Adafruit 532, [Herstelleranleitung](https://learn.adafruit.com/adafruit-pn532-rfid-nfc/breakout-wiring) | 1 | 1 | 1 | 3 | Je ein lokaler Leser im SPI-Modus, 3,3-V-System; Boardrevision/Pinplan und Stromaufnahme prüfen. Kein RC522-Ersatz ohne neuen Abgleich. |
| G20 | NTAG213-Karte oder Token mit Antenne, sieben Byte UID, [NXP-Datenblatt](https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf) | 2 | 2 | 2 | 6 | H/L/F je ein Original und eine eigene lokal zugeordnete Reserve; drei im Spiel, drei bei SL. Stationsspalten bezeichnen Ziel-Leser, nicht Ausgabeort. Keine nackten ICs bestellen. |
| G21 | TowerPro SG92R Mikroservo, [Adafruit 169](https://www.adafruit.com/product/169) | 0 | 3 | 1 | 4 | S2 drei Symbolzeiger, S3 Fortschrittszeiger; leichte Last. Quelle nennt 4,8 V nominal und 3–6 V; 5 V geplant. Winkel/Impulse am Exemplar kalibrieren. |
| G22 | PCA9685 PWM/Servo-Breakout, [Adafruit 815](https://www.adafruit.com/product/815) | 0 | 1 | 1 | 2 | I2C, 3,3-V-Logik, getrenntes V+ für Servos; je ein lokales Board. S2 Adressen gegen beide MCP23017 abgleichen; OE beim Start sperren. |
| G23 | Externe geregelte 5-V-Servoversorgung, für Servo-Anlauf-/Blockierlast ausgelegt | 0 | 1 | 1 | 2 | Eigene Versorgung pro Servopult, getrennt von G08; gemeinsame Signalmasse, Plus nicht verbinden. Stromklasse erst nach Messung von drei bzw. einem SG92R festlegen, keine ungeprüfte Amperegarantie. |
| G24 | Servo-Stromverteil-/Schutzsatz mit passend bemessener Sicherung, Abschaltung, Pufferkondensator und Leitungen | 0 | 1 | 1 | 2 Sätze | Ergänzt G10 für getrennten Servozweig; Leitungsquerschnitt/Stecker/Kapazität nach Lastmessung. Niemals Versorgung über ESP-GPIO/3,3-V-Regler. |
| G25 | Mechaniksatz aus Servoaufnahme, passendem Horn/Schrauben, leichtem Zeiger, Skala, transparenter Schutzhaube und Servo-Verlängerung | 0 | 3 | 1 | 4 Sätze | Freier Weg ohne erreichbare Quetschstelle; keine Schlösser. Im Servo-Lieferumfang enthaltene Hörner/Schrauben nicht doppelt beschaffen. |
| G26 | Nichtmetallischer Artefaktträger/Lesefeld-Markierungssatz | 1 | 1 | 1 | 3 Sätze | Je zwei beschriftete Taghüllen (Original/Reserve) und eine Auflagefläche; Metallabstand/RF-Reichweite am Aufbau prüfen. |

G20 wird ausschließlich gelesen, keine NDEF-Erfolgspayload geplant. 144 Byte
Nutzerspeicher des NTAG213 sind hier kein Kapazitätsbedarf für Spielzustand.
Die sechs UIDs werden nicht in öffentliche Dokumentation geschrieben. Ersatzkarten
haben eigene UIDs und müssen vor dem Lauf zugeordnet sein. Der
[NFC-Projektvertrag](../docs/nfc-packet-contract.md) bleibt maßgeblich; kein
neuer Treiber oder geprüfter Profilbesitzer wird durch diese Liste behauptet.

Der [PCA9685-Versorgungsleitfaden](https://learn.adafruit.com/16-channel-pwm-servo-driver/hooking-it-up)
trennt Logikversorgung und Servoenergie. G23/G24 müssen Anlauf, gleichzeitige
Bewegung und Blockierfall tragen; G08 ist dafür nicht mitgerechnet. Gemeinsam mit
Hardwareverantwortlichen sind OE-Bootzustand, 3,3-V-Signalverträglichkeit des realen
Servos, Masseführung, Pufferung und Brownoutverhalten zu prüfen. Bei ungeklärtem
Bauteilstand bleibt Beschaffungsspezifikation offen; kein stiller GPIO-Direktanschluss.

## Aufbau und Spielmaterial

| ID | Artikel | S1 | S2 | S3 | Gesamt | Verwendung |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| G12 | Standfestes Tischgehäuse/Pult mit zugänglicher Serviceabdeckung | 1 | 1 | 1 | 3 | Frontplattenausschnitte nach konkreten Bedienelementen; Innenraumeinsatz. |
| G13 | Drehknopf passend zu G03 | 0 | 3 | 0 | 3 | Zeiger und feste Skala 1–8; mechanische Kompatibilität offen. |
| G14 | Licht-Streuscheibe mit Symbol-/Textlegende | 1 | 1 | 1 | 3 | Keine Bedeutung allein durch Farbe; S3 als stilisiertes Grenzfeuer. |
| G15 | Sichtschutz/Trennwand für Rückseitentabellen | 0 | 1 | 0 | 1 | Kommunikation an S2; für Zugänglichkeit versetzbar. |
| G16 | Druck-/Laminatsatz gemäß Konzept | 1 | 1 | 1 | 3 Stationssätze | S1 vier Hinweise und vier Symbole; S2 drei Tabellen und drei Servo-Symbolskalen; S3 vier Schalter-/zwei Handmarkierungen und Fortschrittsskala; drei NFC-Auflagehinweise. |
| G17 | Übergabeumschläge mit gedruckten Karten | 1 | 1 | 0 | 2 + 2 Reserve | A Linserolle und B Feuerkarte, je eine Reserve bei Spielleitung. |
| G18 | Spielleiterpaket | 0 | 0 | 0 | 1 zentral | Lösung, dreistufige Hinweise, Durchlaufkarte, Papierersatz aller drei Pulte, Resetcheckliste und Hütersiegel zum Nachdruck. |

## Bestand, Beschaffung und Freigabe

Der [Hardwareprüfstand](HARDWAREPLANUNG.md) belegt frühere Teilversuche mit ESP,
LED, Expander, Audio und NFC, aber keinen heute frei verfügbaren Bestand von
drei kompletten Boxen. Keine dieser historischen Komponenten wird hier schon
vom Bedarf abgezogen. NFC und vier Servos gehören verbindlich zum Konzept.
Audio und Schlösser sind nicht erforderlich; Artefaktausgabe übernimmt die
Spielleitung, die lokale NFC-Lesung aktiviert die jeweilige Station.

Nächste Beschaffungsschritte: vorhandene Teile pro ID zuordnen; S2-Steck-/Pinplan
und Frontplatte prüfen; Gesamtstrom und Versorgungsschutz dimensionieren;
konkrete Restartikel einschließlich G23–G26 auswählen und Nettomengen samt Angeboten
nachtragen. G01–G04/G06/G19–G22 sind recherchierte Referenztypen, keine elektrische
Freigabe für ihre gemeinsame Verwendung. Preise, Steuern, Versand und Budget
werden erst anhand tatsächlicher Angebote zusammengerechnet.

Quellenabruf: 31.10.2026, Herstellerseiten in den Zeilen G01–G04/G06 und G19–G22 sowie
PCA9685-Versorgungsleitfaden; neu abgeglichen am selben Tag.
Die Quellen belegen die benannten Produktklassen: ESP32-Entwicklungsboard,
Schließertaster, achtpoliger Wahlschalter, 16-I/O-I2C-Breakout, Achtpixelmodul,
PN532/SPI, NTAG213, SG92R und PCA9685 mit getrennter Servoversorgung.
Die geplante Kombination, Mengen und Auslegung sind Entscheidungen dieses
Konzeptentwurfs. Keine Verfügbarkeits-, Sicherheits- oder Fertigungsfreigabe.
