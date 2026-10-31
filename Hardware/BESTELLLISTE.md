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
hier ergänzen. Keine Kauftransaktion beauftragt. Mengen sind Bedarf für **einen
Parcours mit drei physischen Stationen**. Bestand nicht inventarisiert; daher
„Neubedarf“ zunächst Bruttobedarf, keine Behauptung, dass nichts vorhanden sei.
Nettobestellmenge = max(0, Bedarf minus tatsächlich zugeordneter geeigneter Bestand).
Keine Reserveelektronik eingerechnet. Budget, Lieferant und Preis noch offen;
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
| G08 | Zertifiziertes externes 5-V-USB-Netzteil, Auslegungsziel mindestens 2 A je Box | 1 | 1 | 1 | 3 | Innenraum-Pilot; reale Last und Reserve messen. Je Board genau ein zulässiger Versorgungsweg; keine parallele USB-/Pin-Einspeisung. |
| G09 | Passendes USB-Strom-/Datenkabel für ausgewähltes Controllerboard | 1 | 1 | 1 | 3 | Steckertyp und Zugentlastung abgleichen. Netzteil/LED-Verteilung als gemeinsames Versorgungskonzept planen. |
| G10 | Niederspannungs-Verteil-/Schutzsatz je Box | 1 | 1 | 1 | 3 Sätze | Klemmen, Hauptschalter, Sicherung/Halter, Abblockung, LED-Datenwiderstand; Werte/Leiterquerschnitt erst nach Strombudget festlegen. |
| G11 | Verriegelbare Stecker-, Litzen-, Isolier- und Montageleitungssatz | 1 | 1 | 1 | 3 Sätze | S1 vier Kontakte, S2 drei mal neun Drehschalteranschlüsse plus Prüftaster/I2C, S3 sechs Kontakte; je Licht- und Versorgungszweig. Längen aus realer Pultplanung ableiten. |

G03 benötigt passende Drehknöpfe: drei Stück in G13 enthalten. G04 bezeichnet
fertige Breakouts, nicht die nackten ICs. S2 nutzt lokale Leitungen innerhalb
eines Pults; kein I2C-Kabel zwischen Stationen. GPIO-Pinplan, Bootpins und
elektrische Schutzdetails sind offen, die Tabelle ist keine Anschlussanleitung.

## Aufbau und Spielmaterial

| ID | Artikel | S1 | S2 | S3 | Gesamt | Verwendung |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| G12 | Standfestes Tischgehäuse/Pult mit zugänglicher Serviceabdeckung | 1 | 1 | 1 | 3 | Frontplattenausschnitte nach konkreten Bedienelementen; Innenraumeinsatz. |
| G13 | Drehknopf passend zu G03 | 0 | 3 | 0 | 3 | Zeiger und feste Skala 1–8; mechanische Kompatibilität offen. |
| G14 | Licht-Streuscheibe mit Symbol-/Textlegende | 1 | 1 | 1 | 3 | Keine Bedeutung allein durch Farbe; S3 als stilisiertes Grenzfeuer. |
| G15 | Sichtschutz/Trennwand für Rückseitentabellen | 0 | 1 | 0 | 1 | Kommunikation an S2; für Zugänglichkeit versetzbar. |
| G16 | Druck-/Laminatsatz gemäß Konzept | 1 | 1 | 1 | 3 Stationssätze | S1 vier Hinweise und vier Symbole; S2 drei Tabellen und drei Skalen; S3 vier Schalter-/zwei Handmarkierungen. |
| G17 | Übergabeumschläge mit gedruckten Karten | 1 | 1 | 0 | 2 + 2 Reserve | A Linserolle und B Feuerkarte, je eine Reserve bei Spielleitung. |
| G18 | Spielleiterpaket | 0 | 0 | 0 | 1 zentral | Lösung, dreistufige Hinweise, Durchlaufkarte, Papierersatz aller drei Pulte, Resetcheckliste und Hütersiegel zum Nachdruck. |

## Bestand, Beschaffung und Freigabe

Der [Hardwareprüfstand](HARDWAREPLANUNG.md) belegt frühere Teilversuche mit ESP,
LED, Expander, Audio und NFC, aber keinen heute frei verfügbaren Bestand von
drei kompletten Boxen. Keine dieser historischen Komponenten wird hier schon
vom Bedarf abgezogen. Audio, NFC, Motoren und Schlösser sind für dieses Konzept
nicht erforderlich; die physische Übergabe übernimmt die Spielleitung.

Nächste Beschaffungsschritte: vorhandene Teile pro ID zuordnen; S2-Steck-/Pinplan
und Frontplatte prüfen; Gesamtstrom und Versorgungsschutz dimensionieren;
konkrete Artikel für G05/G07–G18 auswählen und Nettomengen samt Angeboten
nachtragen. G01–G04/G06 sind recherchierte Referenztypen, keine elektrische
Freigabe für ihre gemeinsame Verwendung. Preise, Steuern, Versand und Budget
werden erst anhand tatsächlicher Angebote zusammengerechnet.

Quellenabruf: 31.10.2026, Herstellerseiten in den Zeilen G01–G04/G06.
Die Quellen belegen die benannten Produktklassen: ESP32-Entwicklungsboard,
Schließertaster, achtpoliger Wahlschalter, 16-I/O-I2C-Breakout und Achtpixelmodul.
Die geplante Kombination, Mengen und Auslegung sind Entscheidungen dieses
Konzeptentwurfs. Keine Verfügbarkeits-, Sicherheits- oder Fertigungsfreigabe.
