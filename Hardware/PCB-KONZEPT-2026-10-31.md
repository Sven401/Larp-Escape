# PCB-K0: Empfehlung für reproduzierbar bestellbare Platinen

Stand: **31.10.2026 · Konzeptvorschlag zur Entscheidung, keine Fertigungsfreigabe.**
Auftrag: [#112](https://github.com/Sven401/Larp-Escape/issues/112), Produktplan
[#25](https://github.com/Sven401/Larp-Escape/issues/25). Fachrolle: beauftragter
Hardware-Architekt (Codex); Ausarbeitung und PM-Review sind getrennte Schritte.
Geprüfte Ausgangsbasis nach frischem Fetch:
`0de42307044c62fc29805b5431050a791c9e72ad` auf
`origin/plan/projektuebergabe-kuratierter-stand`.

## 1. Konkrete Empfehlung

**R1 als einen Controller-/I/O-Träger in KiCad neu zeichnen:** steckbares,
eindeutig beschaffbares ESP32-Entwicklungsboard und DFPlayer; zwei direkt
bestückte MCP23017, Anschluss- und Schutzbeschaltung auf dem Träger. PN532,
LED-Streifen, Lautsprecher und Sensor-/Matrixkabel bleiben externe Baugruppen.
Erst fünf vollständige Platinensätze fertigen lassen: zwei zur Inbetriebnahme,
zwei zum Vergleich/Mehrboxversuch und einer als Reserve. Das ist eine vorgeschlagene
Pilotmenge, keine bestätigte Nutzerstückzahl und keine Bestellung.

Ein gemeinsamer Träger vermeidet zunächst einen zweiten Fertigungsauftrag,
zusätzliche Steckverbindungen und eine ungeprüfte I²C-Verlängerung. Eine räumlich
abgesetzte I/O-Platine ist die Alternative, wenn die Vermessung einen wirklichen
Montage-/Kabelvorteil zeigt. Heute ist der räumliche Aufbau nicht ausreichend
belegt, um die Teilung verbindlich zu empfehlen. NFC-Antenne und LEDs bleiben
wegen ihrer nutzungsabhängigen Position ohnehin außerhalb des Trägers.

Für den ersten Pilot **unbestückte PCBs mit dokumentierter Eigenbestückung**
vorsehen; SOIC-Expander und gut zugängliche passive Teile mit dafür befähigter
Montagekraft. Parallel ein Angebot für einseitige SMT-Teilbestückung einholen.
Diese wird bei gleichem geprüften BOM-Stand gewählt, wenn ihr Mehrpreis unter
den eingesparten Montage-/Prüfstunden zum vereinbarten Stundenansatz liegt.
Steckmodule, Kabel und ggf. THT-Stecker bleiben ausdrücklich Restmontage.
Kein eigenes ESP32-HF-Design, kein eigener NFC-Antennenentwurf und keine neue
Audioverstärkerentwicklung für R1.

**Warum jetzt:** Der Träger kann gegen einen begrenzten Hardwarevertrag entwickelt
werden, während Spielsoftware und native ESP-IDF-Anbindung weiterlaufen.
Ein vollständiger Produkt-Freeze ist keine Voraussetzung für R1. Änderungen
an elektrischen Schnittstellen brauchen dagegen vor Layoutabschluss eine
Entscheidung. R1 allein ist noch keine freigegebene Spielbox.

## 2. Bestand und tatsächliche Prüfung

Quellen: [Hardwareplanung](HARDWAREPLANUNG.md),
[Hardwarevertrag](HARDWAREVERTRAG-V0.1-2026-10-11.md),
[Quelleninventar](QUELLENINVENTAR.md),
[Nachweisregister](../handoff/BAUGRUPPEN-NACHWEISE.md) und aktuelle Adapterquellen.
Die historischen Ergebnisse LED-01, WAKE-01, MATRIX-01, AUDIO-01 sowie NFC-01
bis NFC-04 behalten ihren dokumentierten Umfang. Sie belegen keine elektrische
Gesamtfreigabe und keinen heutigen installierten Stand.

Am Ausgangscommit tatsächlich ausgeführt:

- Drei EasyEDA-JSON-Dateien mit Python gelesen: jeweils ein Schaltplan enthalten.
  Zwei Expanderdateien sind laut Inventar Duplikate; Originale bleiben erhalten.
- Beide SCHDOC-Dateien strukturell gelesen: Header nennt Protel Schematic Capture
  **ASCII Version 5.0**. Dateiendung allein beweist keine Importfähigkeit.
- Vorhandenen [geometrischen Quellprüfer](pruefung/extract_source_nets.py)
  erfolgreich ausgeführt. Er analysiert Verbindungen geometrisch und SCHDOC nur
  strukturell; sein historisches `source_commit` ist kein heutiger Prüfnachweis.
- Keine `.kicad_pcb`-/`.PcbDoc`-Layoutquelle im Hardwareordner gefunden.
- `kicad-cli` ist in der geprüften Umgebung nicht verfügbar. **Kein nativer
  CAD-Import, keine Netzlistenfreigabe und kein ERC/DRC ausgeführt.**
- Aktuellen Baugruppenpfad geprüft: Wake1 benutzt weiterhin `0x20 B7`;
  Matrixzeile1 `0x21 B7`. Die unten vorgeschlagenen Ersatzpins sind dort frei.

KiCad dokumentiert EasyEDA-Standard-JSON-Import. Die vorhandenen ASCII-SCHDOCs
werden deshalb nicht als bereits importierbare Altium-Quelle vorausgesetzt.
Ein Importversuch an einer Kopie ist K1-Aufgabe, kein Anlass zur Übernahme alter
Netze. Empfehlung bleibt Neuzeichnung aus dem geprüften Schnittstellenvertrag;
Import dient als Vergleichsbild/Quellhilfe. Quellen nie überschreiben.
[KiCad-Importdokumentation](https://docs.kicad.org/10.0/en/eeschema/eeschema.html).

## 3. Varianten und erste Revisionsgrenze

| Variante | Vorteil | Nachteil/Risiko | Empfehlung |
| --- | --- | --- | --- |
| A: ein Träger mit steckbarem ESP/Audio, eigenen Expandern | Ein Bestelllos; wenige interne Verbinder; Module austauschbar; überschaubares Layout | Größere Fläche; Modulhöhe; Gehäuse muss passen | **R1-Standard**, bis Vermessung dagegen spricht |
| B: Controller-/Versorgungsträger plus I/O-Platine | I/O nahe Matrix/Sensoren; mechanisch getrennt tauschbar | Zwei Designs/Bestelllose; weitere Verbinder; Buskapazität/Störfestigkeit prüfen | Nur bei belegtem Montagevorteil; Verkabelung und Mehrkosten in K1 vergleichen |
| C: weitgehend integrierte Platine mit ESP-Modul und Audiofunktion | Weniger Handmontage und Modulabhängigkeiten bei späteren Serien | Mehr Entwicklungs-, HF-/Versorgungs- und Abnahmeaufwand; größere Fehlerfolgen | Nach Pilot und belastbarer Stückzahl erneut bewerten |

R1 umfasst 4 Wake-Eingänge, 9×9-Matrix, 23 vorhandene LEDs als Referenzlast,
DFPlayer-UART/BUSY, externes PN532-SPI, USB-Service über das ESP-Board und
BLE/ESP-NOW/WLAN des ausgewählten ESP-Moduls. Andere LED-Zahlen und Audioausgänge
werden nur nach eigenem Last-/Schnittstellenvertrag freigegeben.

Die alte Box bleibt eine eigene Legacy-Revision. R1 hat lesbaren Revisionsdruck,
eine eindeutige Hardwarekennung in der Konfiguration und einen passenden
Firmware-Build. Weder Dateiname noch Funkerkennung sollen eine Pinrevision erraten.

## 4. Elektrischer Zielvertrag und konkrete B7-Lösung

Empfehlung: zwei MCP23017 in SOIC, MPN-Kandidat `MCP23017-E/SO`, Adressen
`0x20` und `0x21`, Logikziel 3,3 V; endgültige MPN, Versorgung und Pull-ups
nach Datenblatt-/Modulprüfung in K1. Keine anonymen Expander-Steckmodule als
serienbestimmendes Teil. [Microchip-Produkt und Datenblatt](https://www.microchip.com/en-us/product/mcp23017).
Das im [bestehenden Vertrag](HARDWAREVERTRAG-V0.1-2026-10-11.md) benannte
Datenblatt DS20001952D und die jeweilige aktuelle Herstellerfassung gelten
für die neue Revision; GPA7/GPB7 nicht als Eingänge einplanen.

**Vorgeschlagene R1-Neubelegung, noch nicht umgesetzt:**

| Funktion | Historischer Baugruppenpfad | R1-Vorschlag |
| --- | --- | --- |
| Wake1 | `0x20 B7` | `0x20 B2` |
| Wake2/3/4 | `0x20 B6/B4/B3` | unverändert |
| Matrixzeile1 | `0x21 B7` | `0x21 A0` |
| Matrixzeilen2–8, Zeile9 | `0x21 B6…B0`, `0x20 B0` | unverändert |
| Matrixspalten1–9 | `0x20 B1/A0…A6/B5` | unverändert; inaktiv hochohmig |
| LED | ESP GPIO15 | erhalten, Pegelstufe nach identifiziertem LED-Typ |
| DFPlayer | ESP TX26/RX25, BUSY14 | erhalten, Signalrichtung beschriften |
| PN532 | SCK16/MISO17/MOSI23/SS4 | erhalten; IRQ/Reset nicht neu voraussetzen |
| I²C | SDA21/SCL22 | Ziel erhalten, reale Module/Pull-ups prüfen |

R1-Firmware muss Wake-Maske, Registerkonfiguration und Matrix-Port-A-Lesung
ändern; ein einzelner Konstantentausch genügt nicht. Auch bisher pauschal als
Eingang konfigurierte unbenutzte A7/B7 müssen gemäß Datenblatt behandelt werden.
Hardwarekennung wählt die gesamte Pin-/Registerbeschreibung. Alte Box und
Prüffirmware bleiben unangetastet. Vor Layoutabschluss Hostprüfung der
Zuordnung, Masken und Fehlerfreigabe; vor R1-Abnahme echte Eingangs-/Loslass-
und Matrixproben. Diese Empfehlung stellt die historische Chipbestückung
nicht nachträglich fest und setzt die bestehenden Funktionstests nicht zurück.

Versorgungsziel: eine definierte externe Kleinspannungsquelle mit getrennt
bemessenen LED-, Audio- und Logikzweigen, gemeinsamer kontrollierter Masse,
Verpol-/Überstromschutz und zugänglichen Mess-/Trennpunkten. Noch keine konkrete
Eingangsspannung oder Sicherung auswählen: LED-/Audiomodul und Last sind offen.
USB darf keine externe Versorgung rückspeisen; die genaue Isolation hängt vom
gewählten ESP-Board ab. LED-/Lautsprecherstrom nicht durch dessen Regler führen.
Leiterbreiten, Sicherung, Stecker und Kabel nach Worst-Case-Strom und Temperatur
bemessen; Softwarehelligkeitsgrenze ersetzt das nicht.

Bevorzugt verriegelte, polarisierte Signalstecker und getrennte, stromgeeignete
Versorgungsanschlüsse. Raster, Herstellerfamilie und Gegenstecker erst nach
Kabelquerschnitt und Last festlegen. BOM erfasst beide Steckerseiten, Crimpkontakte
und Werkzeug. Kein universeller Stecker nur aufgrund gleichen Rasters.
ESP-Antenne am Rand und frei nach Richtlinie des ausgewählten Moduls;
PN532 außerhalb störender Metall-/Audio-/Strombereiche positionieren.
[Espressif-Hardwareleitfaden](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/).

## 5. Offene Nachweise und Freigabetore

| Lücke | Nächster konkreter Nachweis / Rolle | Blockiert |
| --- | --- | --- |
| Modulidentität und Geometrie | Hardware: ESP-/DFPlayer-/PN532-/LED-MPN, Fotos, Pinraster, Außenmaße, USB-/SD-Zugang und Gehäuse/Befestigung vermessen | Footprints, R1-Layoutabschluss |
| B7 / tatsächliche alte Bestückung | Hardware #39 identifiziert Chips; Firmware prüft R1-Mapping samt allen Portmasken und Legacy-Trennung | Neuer Pinvertrag und Schaltplanfreigabe |
| Wake-/Matrixpegel und Leitungen | Hardware: Sensorart/Polarität, Kabelplan, Pull-ups; #54 gezielt C3/G3/H3/A8 untersuchen | Eingangs-/Steckerfreigabe, nicht gesamtes Konzept |
| Last/Audio/Versorgung | Hardware: reale Rails, Leerlauf und kombinierte Spitzen, Rückleiter, Verstärkerweg und USB-Rückspeisung messen | Schutz-/Leiter-/Netzteilbemessung |
| Pegel/Startpins | Hardware + Firmware: LED-DIN und PN532/UART/BUSY gegen identifizierte Datenblätter, Boot/Reset mit angeschlossener Peripherie | Schaltung und Inbetriebnahme |
| Mechanische Teilung | Hardware: ein Träger im Gehäuse skizzieren; B nur bei nachweisbarer Verbesserung, Busleitung/-kapazität vermessen | Entscheidung A/B vor Layout |
| Teile lieferbar | Beschaffung: MPN, Lieferant, Datum, Menge, genehmigte Alternativen und Footprintvergleich | BOM-Freeze und Angebot |
| Native Geräteadapter | Firmware: unterstützte Revision, Buildhash und Anschlussprüfpfad benennen | Gemeinsame Funktionsabnahme; nicht Start von CAD |

K1 darf mit Fotos, Messungen und Vertrag unabhängig vom Gesamtspiel arbeiten.
Ein offener Messwert erhält Eigentümer und Termin-/Zugangsbedarf; er wird nicht
mit „funktioniert historisch“ geschlossen. Historische neun unbestückte Matrixfelder
bleiben im alten Bericht erhalten. Für die neue Revision müssen bestückte Felder
und räumliche Zuordnung ausdrücklich festgelegt werden.

## 6. CAD, Bibliotheken und reproduzierbare Ausgabe

Empfehlung **KiCad 10.0**, Patchversion vor K1 festhalten und auf Exportrechner/CI
identisch verwenden. Keine automatische Projektkonvertierung durch wechselnde
Versionen. Repositorypfade für Symbole/Footprints/3D-Modelle, Bibliothekstabellen
mit Projektbezug, Herstellerzeichnung und Lizenz/Herkunft je Sonderfootprint.
Zuerst ESP-/Audio-Sockel und Stecker 1:1 ausdrucken und mit echten Teilen prüfen.
K1 liefert insbesondere die konkrete ESP-Board-MPN samt Lieferquelle und
Abgleich von Flash/RAM, herausgeführten Pins, Boot-/Strappingverhalten, USB und
Regler gegen das aktuelle Firmwareziel. Die ESP32-Baureihe wird nicht still
ausgetauscht; fehlende Kompatibilität verlangt eine benannte Firmware-/Hardware-
Entscheidung vor dem Footprint-Freeze.
Jede Padnummer gegen Datenblatt und Schaltplansymbol prüfen; 3D-Passung allein
reicht nicht. Unbestückte Varianten ausdrücklich DNP kennzeichnen.

Geplante Struktur: `Hardware/pcb/r1/{controller,lib,mechanik,pruefung}`;
freigegebene Ausgabe unter `Hardware/releases/<revision>/`. Das Konzept erzeugt
diese CAD-Quellen noch nicht. R1 erhält eigene native Netzliste und lesbaren
Schaltplan; alte Quellen bleiben Herkunft.

Vorgesehener Export aus frischem Checkout: exakten Commit und Toolversion prüfen,
ERC sowie DRC ausführen, Kupferzonen aktualisieren, Gerber und Excellon erzeugen,
Schaltplan/BOM/Positionsdaten exportieren, Manifest und SHA-256 erstellen.
Die offiziellen [CLI-Befehle](https://docs.kicad.org/10.0/en/cli/cli.html)
für `sch erc`, `pcb drc`, `pcb export gerbers`, `pcb export drill` und
`pcb export pos` werden in K2 als gepinntes Exportrezept konkretisiert und
wirklich ausgeführt. Keine heute behauptete reproduzierte CAD-Ausgabe.

Tor: keine unbegründeten ERC-/DRC-Fehler; Ausnahmen einzeln mit Ursache und
Freigabe. Unabhängiger Betrachter prüft ausgegebene Gerber/Bohrdaten:
Kontur, geschlossene Umrisse, NPTH/PTH, Lagen, Masken, Pin1, Beschriftung,
Polung, Montagebohrungen, Antennenfreiraum und Außenmaße. Dann Fertiger-DFM
und Bauteilpositionsvorschau mit derselben Revision abgleichen.

## 7. Fertigervergleich und Beschaffungsentscheidung

Recherche: **31.10.2026**, nur Herstellerquellen. Vergleichsannahme für beide:
ein Design, zunächst 2 Lagen FR-4, 1,6 mm, 1 oz Kupfer, grüne Maske,
bleifreies HASL, vorläufig 100×100 mm, Standardfertigung ohne Eiloption;
5/10/25 vollständige Sätze. Abmessung, Kupfer und Lagen sind Kalkulationsparameter,
kein Layoutzwang: Last, Routing und Gehäuse können sie ändern.

| Punkt | JLCPCB | PCBWay |
| --- | --- | --- |
| Unbestückt | Standard-Gerberangebot nach tatsächlichen Daten einholen | Dasselbe Gerberpaket und dieselbe Spezifikation anbieten lassen |
| Teilbestückt | Economic-PCBA als Vergleichskandidat; Bibliotheksabgleich und Basic/Extended-Einstufung beachten | Mixed-/SMT-/THT-Auftrag mit identischer DNP-/Restmontageliste vergleichen |
| Komplettbestückt | Nur bei bestätigter Verfügbarkeit aller MPN und Montagefähigkeit, keine automatische Gleichsetzung mit fertiger Box | Ebenfalls Modulmontage/Restarbeit einzeln im Angebot bestätigen |
| Preisbeleg | Aktuelle Preisliste veröffentlicht Setup, Schablone, Lötstellen und Feeder; Komponenten separat | Rechner warnt ausdrücklich, PCB und Bauteile sind im dortigen Montagepreis nicht enthalten; final nach Dateiprüfung |
| Angebotseingang | Gerber, BOM, CPL und Teileauswahl | Gerber, BOM, Centroid/Positionsdaten und Montagevorgaben |
| Termin | Nach BOM-/Dateiprüfung zum Angebot; aktuell kein verbindlicher Projekttermin | FAQ: Montagezeit beginnt erst bei vollständigen Teilen, PCBs und Unterlagen; kein Lieferdatum zusagen |

Quellen: [JLC-Preisliste](https://jlcpcb.com/help/article/pcb-assembly-price),
[JLC-FAQ](https://jlcpcb.com/help/article/pcb-assembly-faqs),
[PCBWay-Angebotsseite](https://www.pcbway.com/quotesmt.aspx),
[PCBWay-FAQ](https://www.pcbway.com/assembly-faq.html).
JLC-FAQ und jüngere Preisliste nennen unterschiedliche Gebührensätze; deshalb
unten ausdrücklich die Preisliste mit Aktualisierung **22.10.2026** verwenden.
Keine Angebotsanfrage oder Dateiübermittlung an Hersteller wurde ausgeführt.

**Empfehlung:** JLCPCB als ersten Kalkulationskandidaten für SMT-Teilbestückung,
PCBWay als Gegenangebot für identischen Umfang. Das ist eine Beschaffungsroute,
kein belegter Preisgewinner. Erste unbestückte Pilotplatinen beim günstigeren
vergleichbaren Gesamtangebot nach DFM bestellen, sobald K3 freigegeben ist.
Nicht die Architektur zugunsten eines einzelnen Fertigerkatalogs verbiegen.

## 8. Kostenrechnung: belegte Gebühren und offene Gesamtkosten

Alle Herstellergebühren hier in USD, Abruf 31.10.2026, ohne Umrechnung.
Für die **hypothetische** einseitige Economic-Teilbestückung: Setup 8,18,
Schablone 1,53, SMT 0,0016 je Lötstelle, Extended-Feeder 3,07 je Typ laut
[JLC-Preisliste](https://jlcpcb.com/help/article/pcb-assembly-price).
Rechenannahme: 100 SMT-Lötstellen pro Träger, vier Extended-Typen,
keine Zusatz-/Handlöt-/Sondergebühren. Diese Zahlen beschreiben noch keine R1-BOM.

Teilgebühr `A(N) = 8,18 + 1,53 + 4×3,07 + N×100×0,0016`.

| Menge N | Berechnete Teilgebühr A(N) | Anteil pro Träger |
| --- | --- | --- |
| 5 | 22,79 USD | 4,558 USD |
| 10 | 23,59 USD | 2,359 USD |
| 25 | 25,99 USD | 1,0396 USD |

**Das ist kein PCB-/Boxpreis und kein Angebot.** Gebühreneinstufung und
zusätzliche Positionen müssen am echten Auftrag bestätigt werden. PCBWay wird
ohne Dateien nicht mit einem erfundenen Konkurrenzgesamtpreis eingetragen.

Vollständige Vergleichsrechnung je Anbieter v und bestellter Menge N:

`Gesamt_v(N) = PCB_v(N) + Montage_v(N) + Bauteile_v(N) + Module(N)`
`              + Stecker/Kabel(N) + Versand_v + Abgaben_v + Restarbeit(N)`

| Position | Heutiger Status / Ermittlung für K3 |
| --- | --- |
| CAD/Prüfaufbau einmalig | Aufwand unten; Kosten = freigegebene Stunden × vereinbarter Satz + benötigte Prüfmittel |
| PCB | Offen bis Gehäuse, Maße, Lagen, Kupfer und Angebote feststehen |
| SMT-Montage | Obige Teilrechnung als Orientierung; endgültiges Angebot inklusive aller Gebühren |
| Elektronische Teile | Offen: freigegebene BOM mit MPN, Mengenpreis, Überschuss-/Mindestmengen und Verfügbarkeit |
| Module | Je Träger ein exakt bezeichnetes ESP-Board und DFPlayer; PN532 extern; Lagerbestand nicht als gratis Serienmaterial rechnen |
| Stecker/Kabel | Beide Seiten, Kontakte, Crimpen, Zugentlastung, Befestigung und Montage erfassen |
| Versand/Abgaben | Lieferadresse/-art, Zahlungswährung und tatsächliche Abgaben im Checkout/Angebot separat ausweisen |
| Eigenarbeit | Planannahme 1–2 h je Träger bei Eigenbestückung; 0,5–1 h bei Teilbestückung, jeweils einschließlich Sicht-/Basistest; nach Pilot messen |
| Reserve | Für Serie zunächst 10 % zusätzliche komplette Sätze, aufrunden; beim 5er-Pilot bereits ein Reservesatz enthalten |

Bei 5/10/25 gewünschten nutzbaren Seriensätzen also Angebote für 6/11/28 Sätze
anfragen, alternativ tatsächlich verfügbare Fertigerlosgröße transparent führen.
Reserveteile nicht doppelt als pauschalen Zuschlag berechnen. Kosten pro nutzbarem
Satz = Gesamtkosten geteilt durch Zielmenge; Gehäuse, Netzteil, Sensoren und
LEDs/Lautsprecher separat als komplette Boxkosten erfassen. Keine bestätigte
Preisobergrenze möglich, solange die offenen Positionen fehlen.

Entscheidungsregel Teilbestückung:
`Mehrpreis_fertig <= eingesparte_Stunden × vereinbarter_Stundensatz`.
Montagequalität und fehlende eigene Lötfähigkeit sind zusätzliche Auswahlgründe;
ein nominell günstigerer PCBpreis allein entscheidet nicht. Kostenvergleich in K3
als datiertes Kalkulationsblatt mit gespeicherten Angeboten ausfüllen.

## 9. Vollständiges Paket für Bestellung und Nachbestellung

Jede Freigabe bindet Quelle, Varianten und Ausgabe an dieselbe Revision:

1. Native KiCad-Dateien einschließlich Bibliotheken, Mechanikdaten und Werkzeugpin.
2. Schaltplan-PDF, native Netzliste, Pin-/Stecker-/Kabelvertrag und Firmwarematrix.
3. Gerber, Excellon mit PTH/NPTH und Kontur, Fertigungsspezifikation und DFM-Befund.
4. BOM mit RefDes, Menge, Wert, Package, Hersteller-MPN, Beschaffungsquelle,
   DNP/Variante und ausdrücklich freigegebenen Alternativen; Anbieter-SKU ergänzend.
5. Positionsdatei mit Einheit, Ursprung, Seite und Rotation; Montagezeichnungen
   mit Pin1/Polung sowie klare Liste der externen Module und Restmontage.
6. ERC-/DRC-Berichte, Ausnahmeentscheidungen, unabhängige Exportprüfung,
   1:1-Footprintabgleich und mechanischer Passnachweis.
7. Prüf-/Inbetriebnahmeanleitung, Messpunkte, Sollgrenzen mit Datenblattbezug,
   Prüfsoftwarehash, Firmware-Revisionszuordnung und Pilotmessberichte.
8. Manifest mit Commit, Revision, Toolversion, Varianten, UTC-Erzeugungsdatum und
   SHA-256 jeder Ausgabedatei; archivierte Angebote/Bestellparameter ohne Zugangsdaten.

Nachbestellung: freigegebene Ausgabe wählen, Hashes prüfen, Verfügbarkeit/Änderungen
und Konditionen neu prüfen, gleiche Fertigungsspezifikation benutzen. Keine
stillschweigenden MPN-/Modulersatzteile oder neu generierten Exporte aus anderem
HEAD. Bei Änderungen neue Revision samt betroffenen Nachweisen. Ein zweiter
Bearbeiter muss den Bestellumfang allein aus diesem Paket bestimmen können.

## 10. Pilotprüfung und Abnahme

Vor Einschalten: Sichtprüfung, Polung, Footprints, Montage, Widerstands-/Kurzschluss-
prüfung der stromlosen Rails; Prüfgerät und Platinenseriennummer protokollieren.
Strombegrenztes Netzgerät, Multimeter, Oszilloskop und Temperaturmessung vorsehen.
Testpunkte für Eingang, Logik, Audio, LED, GND sowie Bus/Reset/LED-DIN vorsehen.

| Stufe | Messung / Beobachtung | Bestehen / Stopp |
| --- | --- | --- |
| P0 unbestückt/teilbestückt | Kontur, Padnummern, offene/kurze Netze, mechanische 1:1-Passung | Fehler vor Modulen korrigieren; keine Testfirmware als Ersatz |
| P1 Versorgung ohne Lastmodule | Rails, Ruhestrom, Isolation USB/externe Quelle, Schutzfunktion mit definiertem Prüfaufbau | Innerhalb der in K1 festgelegten Hersteller- und Bauteilgrenzen; ungeklärte Rückspeisung = Stopp |
| P2 Module einzeln | Boot/Reset, UART/BUSY, NFC-SPI, LED-DIN, beide Expander, je Wake-/Matrixeingang | Gemessene Pegel und Timing passend zum identifizierten Teil; Überstrom/Einbruch/Fehlpolung = Stopp |
| P3 kombiniert | Gleichzeitiges Audio/LED/Funk und Matrix/Wake; Spannung am Verbraucher, Stromspitzen, Temperatur | Grenzen und Reserve aus K1 einhalten; Brownout/Übertemperatur/Busfehler Ursache klären |
| P4 Anwendung | Start/Stop/Wiederstart, Eingangsfreigabe, Abbruch, Autonomie, Neustart; mindestens zwei R1-Boards | Zugeordnete Firmware/App und tatsächliche physische Wirkung getrennt belegen |
| P5 Dauer/Funk/Mechanik | Planannahme 8 h Pilotdauerlauf, Funkparallelität/Verlust/Wiederkehr im Zielgehäuse, Steckerzug und Servicezugang | Definierte Last und Fehlerfälle ohne unerklärten Reset; 8 h sind Pilotumfang, kein Lebensdauernachweis |

Absolute Sollwerte werden aus den wirklich ausgewählten Teilen vor dem Test
festgelegt, nicht während eines Fehlversuchs erfunden. Ergebnis nennt Seriennummer,
Revision, Prüfmittel, Lastprofil, Softwarehash, Rohwerte und Abweichungen.
Fertiger-E-Test/AOI sind eigene Belege; sie ersetzen keinen kombinierten Betrieb.
Bei Uploads gilt zusätzlich der gesonderte Geräteauftrag und chipgebundene
Flash-Rückweg der Hardwaredokumentation. Dieser Konzeptauftrag führt keinen aus.

## 11. Umsetzbare Folgepakete und Entscheidung

Aufwände sind **Architektenschätzung**, keine Lieferantenangebote/Terminzusagen;
ein Personentag = 8 h. Teilelieferung, Nutzerzugang zur Box und Fehlerschleifen
kommen als Kalenderwartezeit hinzu. Schätzung gilt für Variante A.

| Paket | Liefergegenstand / Rolle | Aufwand | Eintritt und Abschluss |
| --- | --- | --- | --- |
| K1 Vertrag/Mechanik/Beschaffung | Vermessung, MPN-/Last-/Pegelbelege, R1-Pinplan, A/B-Entscheidung; Hardware + Firmware | 3–5 PT | Boxzugang; offene layoutkritische Punkte geschlossen, unabhängiger Vertragsreview |
| K2 Schaltung/Layout | KiCad-Quellen, geprüfte Footprints, BOM, Hardwarekennung/Firmware-Migrationsplan; CAD + Firmware | 5–8 PT | K1; ERC/DRC und Mechanik-/Netzreview bestanden |
| K3 Fertigungspaket | Reproduzierte Exporte, Angebotsvergleich, Gesamtkosten, Prüfplan; CAD + PM/Beschaffung | 1–2 PT | K2; unabhängige Exportprüfung, konkrete Bestellvorlage zur Nutzerfreigabe |
| K4 Pilot | Fünf Sätze, Montage, P0–P5 und Befund; Hardware + Firmware | 3–5 PT | Freigegebene Bestellung, Teile da; dokumentierte Pilotfreigabe oder Revisionsliste |
| K5 Wiederbestellfreigabe | Paket, Änderungs-/Kompatibilitätsmatrix, zweiter Bearbeiter prüft Nachbestellung; Maintainer + PM | 1–2 PT | Pilot bestanden; vollständiger unveränderter Bestellsatz nachvollziehbar |

Grundpfad 13–22 PT; für eine mögliche Layoutkorrektur zusätzlich 2–4 PT sowie
neue Fertigungs-/Lieferzeit einplanen. Firmware-Mapping/Hostnachweis ist in K1/K2
enthalten; vollständige native Geräteadapter-/Spielintegration aus #31 ist
zusätzliche Produktarbeit. Variante B erhöht die Schätzung vorläufig um 2–4 PT
für zweite Platine, Verbinder und Bus-/Mechaniknachweise. Vor Beauftragung mit
tatsächlicher CAD-/Montagekapazität nachschätzen.

Zur Entscheidung vorgeschlagen:

- R1-Architektur A, KiCad-Neuzeichnung, austauschbare ESP-/Audiomodule und zwei
  direkt bestückte Expander mit neuer R1-Pinbelegung als Arbeitsrichtung annehmen.
- Pilot mit fünf Sätzen planen; endgültige Zielmenge und Beschaffungsbudget nach
  K3-Kalkulation festlegen. Für K1 werden Boxzugang und Hardwaremesskompetenz benötigt.
- K1 separat zur Umsetzung beauftragen. K2 beginnt nach dessen fachlicher Freigabe;
  Fertigung erst nach konkreter Kosten-/Bestellfreigabe in K3.

**K0 ist mit diesem prüfbaren Konzept geliefert, noch nicht vom Nutzer angenommen.**
#112 bleibt bis zur Konzeptannahme offen. Es gibt noch keine freigegebene CAD-
Revision, keinen Gesamtpreis und keine bestellfähigen Fertigungsdaten. Das Konzept
benennt den begrenzten nächsten Auftrag, mit dem diese Lücken geschlossen werden.
