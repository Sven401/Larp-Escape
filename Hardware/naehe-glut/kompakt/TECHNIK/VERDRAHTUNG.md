# Nähe Glut: bestückte Module, Verdrahtung und Abnahme

Stand: 31.10.2026; Arbeitsbasis `483c3320dd2da64cd74e46b45084359d07337ed7`.
Fachentwicklung Geräteintegration: `compact_hardware`; unabhängige Prüfung:
`supply_review`; Koordination/Beschaffung: Root; Gehäuse: `compact_mechanics`.
Entscheidbarer Beschaffungsentwurf, noch keine Geräteabnahme oder Bestellung.
Dieser Modulvertrag ersetzt die offenen Versorgungsvarianten im
[Technikentwurf](../TECHNIK-ENTWURF.md) für den Dezemberweg.

## Lieferumfang je Gerät

| Teil | Auswahl | Tatsächlicher Platzbedarf / Zweck |
| --- | --- | --- |
| ESP32-S3 | Adafruit QT Py **5700**, 4 MB Flash / 2 MB PSRAM | ca. 21,7 × 17,8 × 5,7 mm; gesamte ESP-Platine innerhalb 25 × 20 mm; USB-C, integrierte Chipantenne |
| Lichtplatine | Adafruit **5646**, 5×5 NeoPixel Grid BFF | ca. 20,9 × 17,9 × 2,8 mm; 25 physische SK6805-1515-Pixel; 24 aktiv, einer dauerhaft schwarz |
| Versorgung | Adafruit **2030 PowerBoost 1000 Basic** | 29 × 23 × 2 mm ohne optionale Buchsen; mit JST-Buchse bis 7,5 mm hoch. Separate größere Powerplatine ausdrücklich ausgewiesen |
| Akku | KeepPower **P1835C3**, geschützte 18350 | Konstruktionshülle Ø18,75 × 39,0 mm; konservativ mindestens 1300 mAh / 2,8 A Dauerstrom |
| Zellkontakte | 2× Mill-Max **0906-2-15-20-75-14-11-0** | Hersteller 2 A kontinuierlich / 3 A kurzzeitig; formschlüssig im gedruckten Halter, keine direkte Zelllötung |
| Ein/Aus | E-Switch **EG1218** | SPDT, 200 mA bei 30 V DC, nur EN-Signal; Körper 11,6 × 4,0 × 5,4 mm, 2 mm Betätiger, 4,7 mm Pins, Gesamthöhe 12,1 mm, 2,5 mm Schaltweg |
| Sicherung | Littelfuse **0251002.MXL**, 2 A, axial | 2,8 × 7,11 mm Körper; isoliert nahe dem Pluskontakt, nicht als präzise Strombegrenzung behandeln |
| Leitungen | flexible isolierte Litze AWG22 für Akku/5 V/GND | kurze getrennte Leitungen mit Zugentlastung; EN/A3 dürfen dünner sein |

Alle drei Platinen werden **fertig bestückt gekauft**. Endmontage umfasst
Kontakt-/Schalter-/Sicherungsanschlüsse und Modul-Verbindungsdrähte einschließlich
weniger Lötstellen. Der Lieferumfang ist kein werkseitig vollständig verdrahtetes
Fertiggerät. Keine unbestückte eigene PCB, keine Reflowarbeit und keine separate
PCBA-Fertigung auf dem Dezemberpfad. Eine reine PCB-Fertigungsgebühr entfällt.
Ein Nitecore UM2 wird gemeinsam für beide herausnehmbaren Akkus verwendet.

Herstellergrundlagen: [QT Py](https://learn.adafruit.com/adafruit-qt-py-esp32-s3/pinouts),
[BFF](https://learn.adafruit.com/adafruit-5x5-neopixel-grid-bff/pinouts),
[PowerBoost](https://www.adafruit.com/product/2030),
[Kontaktmaßblatt](https://www.farnell.com/datasheets/3189521.pdf),
[Schalterzeichnung](https://www.mouser.com/datasheet/2/140/EG1218-3452668.pdf),
[Littelfuse 251](https://www.littelfuse.com/assetdocs/littelfuse-fuse-251-253-datasheet?assetguid=f47a0bb7-8ede-4679-9646-7114c3787688).
Die Maße sind Komponentenmaße, keine addierte oder bereits abgenommene Gehäusegröße.

## Verbindliche Netze

Vor Löten Beschriftung und tatsächliche Boardrevision prüfen. Der folgende
Vertrag verwendet **BAT als QT-Py-Eingang**, nicht dessen 5V-Pad als Einspeisung.

| Von | Nach | Bedeutung |
| --- | --- | --- |
| Zell-Pluskontakt | 2-A-Sicherung Eingang | Ungesicherte Pluslitze so kurz wie mechanisch möglich, vollständig isoliert |
| Sicherung Ausgang | PowerBoost **BAT** | 1S-Zellspannung, rote Litze |
| Zell-Minuskontakt | PowerBoost **GND** | Schwarze Litze |
| PowerBoost **5V** | QT Py Unterseitenpad **BAT** | Boostausgang nominal 5,2 V; QT-Py-BAT-Eingang laut Hersteller 3–6 V |
| PowerBoost **GND** | QT Py **GND** | Gemeinsame Masse |
| QT Py **5V** | BFF **+5V** | Gemeinsame Schiene NACH der USB/BAT-Diodenumschaltung |
| QT Py **GND** | BFF **− / GND** | LED-Rückstrom nicht durch Datenleitung |
| QT Py **A3 / GPIO8** | BFF **IN** | Daten; BFF-A3-Auswahljumper unverändert |
| PowerBoost **EN** | EG1218 Mittelkontakt **2 / COM** | Nur Steuersignal |
| EG1218 Außenkontakt **1** | PowerBoost **GND** | Diese Schalterstellung ist AUS |
| EG1218 Außenkontakt **3** | isoliert, unbeschaltet | EIN: EN durch Modul-Pullup hoch |

QT-Py-5V und PowerBoost-5V **nicht zusätzlich direkt verbinden**. Kein Kabel
PowerBoost-5V→BFF-5V neben der oben bezeichneten Verbindung: Es würde die
Quellenentkopplung umgehen. Keine Versorgung über QT-Py-3V3, keine Verbindung
von BAT zu GPIO. PowerBoost-JST, USB-A und Schraubklemme bleiben unmontiert;
stattdessen an bezeichneten Pads löten. Inputleitung vom Halter zum Wandler
möglichst unter 38 mm, Längenführung mit Mechanik abgleichen.

### Warum USB und Akku so zusammen funktionieren

Der offizielle QT-Py-Schaltplan führt USB über D1 und BAT über D2 auf denselben
internen +5V-Knoten; das Seitenpad 5V liegt an diesem Knoten. Die Dioden
verhindern die gegenseitige Einspeisung. Die BFF erhält damit bei USB-Service
weiterhin Versorgung; ihr Dateneingang bleibt nicht bei versorgtem ESP stromlos.
Die Lötbrücke zur Überbrückung der USB-Schutzdiode bleibt **offen**.

[Originalschaltplan](https://raw.githubusercontent.com/adafruit/Adafruit-QT-Py-ESP32-S3-PCB/main/Adafruit%20QT%20Py%20ESP32-S3.sch)
(Netz +5V: D1/D2-Kathoden, LDO-Eingang und JP3/7; VBAT: D2-Anode).
Wareneingangsprüfung bindet diese Topologie an die tatsächlich gelieferte 5700.
Die [5700-Produktseite](https://www.adafruit.com/product/5700) bestätigt das
diodegeschützte BAT-Pad bis 6 V und verlinkt denselben
[Familienguide mit CAD-Download](https://learn.adafruit.com/adafruit-qt-py-esp32-s3/downloads).
Das CAD-Repository nennt im README noch 5426; eine eigene veröffentlichte
5700-Powerrevision wurde nicht gefunden. Gleicher Guide ist daher ein starker
Beleg für den Entwurf, ersetzt aber nicht die Revisions-/Durchgangsprüfung.
Die Dioden sind NSR0320 in SOD323, laut
[onsemi](https://www.onsemi.com/pdf/datasheet/nsr0320mw2t1-d.pdf) 1 A,
aber zusätzlich thermisch begrenzt (200 mW bei definierten Bedingungen).
Darum sind 1 A keine zulässige Dauerlastfreigabe für diesen kleinen Aufbau.

USB ist Service/Programmierung, **keine Akkuladung**. USB versorgt den ESP und
die BFF auch bei Schalter AUS; AUS bezeichnet den Akkubetrieb. Im Serviceprofil
bleiben alle Pixel schwarz oder im begrenzten Goldeffekt. Kein Vollweißlasttest
an einem gewöhnlichen PC-Port. Vor USB-Service vorzugsweise Zelle entnehmen;
die Diodenentkopplung muss dennoch im späteren USB-/Akku-Paralleltest bestehen.

## Akku, Kontakte und Abschalten

Der PowerBoost verwendet TPS61030 mit echter Lasttrennung im Shutdown. EN nach
GND trennt Ausgang vom Eingang; typisch verbleiben laut Modulhersteller etwa
20 µA am Akku. Der kleine Schalter führt **keinen Akkulaststrom**. Für lange
Lagerung, Laden und Arbeiten am Gerät wird die Zelle entnommen. EN ist kein
galvanischer Batteriehauptschalter. Startvorladung und Kurzschlussbegrenzung
des Wandlers sind Funktionen des ICs, keine zuschaltbare externe Vorladeschaltung.
[TI TPS61030](https://www.ti.com/lit/gpn/tps61030),
[Modulbeschreibung](https://learn.adafruit.com/adafruit-powerboost-1000-basic?view=all).

Die Zell-PCM schützt gegen Über-/Unterspannung und Überstrom entsprechend der
gelieferten Akkuversion. Hersteller nennt 2,5 V Entladeende; dies ist **keine
3,0-V-Betriebsabschaltung**. Der Wandler selbst ist kein Li-Ion-Unterspannungsschutz.
Seine rote LBO-Warnung wird ungefähr unter 3,2 V sichtbar. Warnung am Gehäuse
sichtbar machen; Betrieb dann beenden und Zelle extern laden. LBO ist nicht mit
dem ESP verbunden und kann bis Akkuspannung hochgezogen sein: **nicht direkt an
GPIO anschließen**. Firmwareseitige Akkuüberwachung/Abschaltung ist noch kein
implementierter Lieferstand und wird nicht als Schutzfunktion angerechnet.

Akkuquelle: [KeepPower P1835C3](https://www.keeppower.com.cn/products_detail.php?id=675).
Aktuelle Seite und älteres Datenblatt unterscheiden sich bei Mindestkapazität
und Standardladestrom; deshalb hier konservativ 1300 mAh / 2,8 A. UM2 für die
gelieferte Zelle ausdrücklich auf Li-Ion 4,2 V und **200 mA manuell** einstellen;
keine LiFePO4-/4,35-V-Auswahl. Vor Beschaffung Akkurevision mit Lieferant abgleichen.

Mill-Max-Kontakte werden mit ihrem Flansch im Druckteil gehalten; Lötfahnen
und Drähte erhalten Zugentlastung und Einzelisolierung. Einbauhülle mit zwei
Kontakten samt Lötenden ist etwa **51,2 mm lang**, nicht 39 mm. Mechanik berechnet
freie Kontaktflächen-Distanz 37,4 mm einschließlich Kontaktlängentoleranz;
siehe `../MONTAGE-PROTOTYP.md`. Bei tatsächlichen Zellmaßen und Drucktoleranz
Federn innerhalb zulässigen Hubs prüfen. Kein gequetschter Zellmantel, kein Druck
auf die Schutzplatine und keine leitenden Schrauben über der Zelle.

Pluskontakt muss durch einen isolierenden Kragen zurückgesetzt sein: nur der
erhöhte positive Knopf erreicht ihn; der flache negative Zellboden einer falsch
herum eingelegten Zelle darf ihn nicht erreichen. Das ist eine **zu prüfende
mechanische Verpolsperre**, keine behauptete elektrische Verpolschutzschaltung.
Falls dies mit gelieferter Zelle/Druckteil nicht nachweisbar funktioniert,
keine Gerätefreigabe; Halter überarbeiten. Andere flache oder ungeschützte
18350-Zellen sind kein freigegebener Ersatz.

## Lastprofil und Vierstundenziel

25 physische LEDs werden immer angesprochen, davon im Produktprofil 24 aktiv;
ein definierter Pixel bleibt schwarz. Budget konservativ mit allen 25 gerechnet.
Auslegungsbasis bleibt der bestehende **ESP-NOW/RSSI-Funkpfad**, der die
Wi-Fi-PHY benötigt; kein beschlossener Wechsel auf BLE-only. Vor dem Dauerlauf
Sendeintervall, TX-Leistung, Empfangs-/Scanverhalten und gegebenenfalls
BLE-Koexistenz explizit festhalten. Der Produktanschluss von NearGlow ist noch
offen. Keine parallelen Dauerdownloads oder dauerndes USB-Debugging im
Auslegungsprofil; warmer Goldeffekt mit maximal RGB(24,8,0) je aktivem Pixel
als anfängliche Helligkeitsgrenze. Der 120-mA-ESP-Mittelwert ist ein
**unbestätigtes Energiebudget**, kein bereits nachgewiesenes Funkprofil.
Grundlagen: [RSSI-Test](../../../../docs/espnow/rssi-test.md) und
[ESP-NOW-Gerätenachweis](../../../../docs/espnow/distance-camera-result-2026-10-18.md);
Distanz-/Reaktionsprüfung muss erhalten bleiben.
Boot zuerst GPIO8 LOW, dann einen vollständigen Schwarzframe senden, danach
Effekt freigeben. Internen QT-Py-Pixel samt Power-Enable ausgeschaltet halten.
Keine ungeprüfte Übernahme alter GPIO15/16-Belegung.

| Verbraucher | Rechenannahme im Goldeffekt |
| --- | --- |
| ESP-Board einschließlich Funk | 120 mA Mittelwert am Versorgungseingang; Messziel, kein Hersteller-Garantiewert |
| LED-Kanäle, 25× RGB(24,8,0), 5 mA/Kanal | 15,7 mA |
| LED-Grundlast | vorsorglich 12,5 mA (0,5 mA/Pixel), genauer Revisionswert noch zu prüfen |
| Sonstiges / Wandler-Grundlast als Reserve | 5 mA |
| Gesamt, konservativ am 5,2-V-Ausgang gerechnet | 153,2 mA ≈ 0,797 W |

Bei 85 % angenommener Wandlereffizienz sind das etwa 0,937 W aus dem Akku.
1300 mAh × 3,7 V × 80 % Nutzungsreserve = 3,848 Wh: rechnerisch etwa
**4,1 Stunden**, also kleine Reserve. Bereits 150 mA mittlere ESP-Last würde
das Ziel verfehlen. Gleicher RGB-Wert garantiert nicht gleiche sichtbare
Helligkeit wie alte größere Pixel; nach Diffusor-/Lichtabgleich neu bilanzieren.

Vollweiß aller 25 Pixel plus 500-mA-ESP-Spitze ergibt vorsorglich ca. 0,893 A
auf der Boostseite, bei 3,0 V Akku/85 % etwa 1,82 A; bei 2,5 V etwa 2,18 A.
Dies ist ein **kurzzeitiger Dimensionierungsfall**, kein Produkt-Dauerprofil.
PowerBoost 2030 wurde statt kleinerer 800-mA-Wandler gewählt. **2,18 A liegen
über den 2 A Kontakt-Dauerstrom**; für die 3-A-Peakangabe ist keine zulässige
Pulsdauer belegt. Auch die QT-Py-OR-Diode hätte bei 0,9 A × maximal 0,5 V etwa
450 mW gegenüber 200 mW statischer Datenblattgrenze bei 25 °C. Ein zulässiger
thermischer Transientenverlauf wurde nicht nachgewiesen. Daher ist daraus keine
pauschale Freigabe selbst einer „kurzen Vollweißprobe“ abzuleiten. Erst Pulsdauer,
Tastgrad, Temperatur und Strombegrenzung qualifizieren; bis dahin ausschließlich
das begrenzte Produktprofil prüfen. Dauer-Vollweiß ist keine Nutzeranforderung.

**Normalprofil bei 2,5 V:** mit den obigen 153,2 mA am Boostausgang etwa
0,375 A Akkumittelstrom bei 85 % Effizienz. LED-PWM erzeugt jedoch Kanalspitzen:
bei synchron aktivem Rot und Grün vorsorglich 25 × 2 × 5 mA = 250 mA, auch wenn
der LED-Kanalmittelwert nur 15,7 mA beträgt. Zusammen mit 500-mA-ESP-Spitze,
12,5 mA LED-Grundlast und 5 mA Reserve sind das 0,7675 A Boostausgang bzw.
rechnerisch **1,878 A Akku bei 2,5 V und angenommenen 85 % Effizienz**, ohne
Glättung durch Kondensatoren anzurechnen. Dies ist keine garantierte Obergrenze:
geringere Effizienz und andere Stromverläufe verändern den Wert; bei 80 % wären
es bereits etwa 1,996 A. Die Kontakt-/Dioden-/LDO-Thermik und tatsächlichen
Stromspitzen bleiben Messpflicht, kein freigegebener ESP-Dauerzustand. Mit 2-A-F1
liegt der berechnete normale Mittelstrom deutlich unter dem Sicherungsnennwert.
F1 schützt den
Leitungspfad bei geeigneten Fehlerströmen, begrenzt aber nicht präzise auf 2 A
und ersetzt keine Kontakt-/Diodenthermikprüfung. Die 120-mA-ESP-Mittelgrenze muss
am echten Funkprofil erreicht werden; ansonsten ist neu zu bilanzieren.

## Abnahme je Exemplar, noch auszuführen

1. **Wareneingang:** exakte 5700/5646/2030-Revisionen, Dioden-/Netztopologie,
   Zellmaße, Kontakte und Schalter gegen Dokumente prüfen. Foto und Serienzuordnung.
2. **Ohne Akku/USB:** Sichtprüfung, keine losen Litzen, Lötbrücke offen,
   Durchgang/Polarität aller Netze; EN-Schalter korrekt; keine 5V-BAT-Abkürzung.
3. **Erstinbetriebnahme am strombegrenzten Labornetzteil**, Akku ersetzt:
   mit ausgeschaltetem EN starten; 4,2/3,7/3,2/3,0/**2,5 V** testen. Ausgang und 3V3,
   Einschaltspitze/Neustarts, LBO und Ruhestrom erfassen. Kein absichtlicher
   Kurzschluss an echter Li-Ion-Zelle und kein absichtliches Tiefentladen.
4. **USB-Quellenprüfung:** Akku simuliert vorhanden, EN an/aus, USB ein/aus;
   auf USB-VBUS keine Rückspeisung, kein Rückstrom in Akku/Boostausgang.
   Vorher/nachher Netzspannungen und Strom messen. Alle Schwarz-/Resetfälle
   ohne flackernde/phantomgespeiste LEDs. USB-Portstrom im Serviceprofil messen.
5. **Mechanik:** richtige Zelle sicher gehalten; falsche Polung erreicht keinen
   Stromkreis; Federhub/Toleranz/50 Wechsel, Zugentlastung, keine Scheuerstelle,
   geschlossene Zellabdeckung. Belastbarkeit der Kontaktbefestigung prüfen.
6. **Last/Temperatur:** begrenztes Gold-Produktprofil mit aktiver Funkstrecke
   mindestens bis thermisch stabil, auch bei 2,5 V am Akkuersatz-Netzteil.
   Netzteilstrombegrenzung zunächst 1,5 A, keine Vollweißprobe ohne gesonderte
   Puls-/Thermikqualifikation. Reale kurze Funkspitzen oszillografisch erfassen;
   fallen sie aus dem berechneten Budget, Test abbrechen und Auslegung prüfen.
   Die anfängliche 1,5-A-Begrenzung kann bei PWM-/Funkgleichzeitigkeit ansprechen;
   das ist allein kein Gerätefehlerbeweis und kein vollständiger Spitzenlastnachweis.
   Scope auf 3V3 und 5V, keine Resets; Diode, Wandler, Kontakte und Zelle auf Erwärmung prüfen.
   Grenzwerte aus konkreter Bauteilrevision, zugängliche Gehäuseflächen handwarm.
7. **Produktabnahme beider Geräte:** echter Gold-Näheeffekt mit finalem Diffusor,
   Funk/Distanz in Endlage, vier Stunden ab voller externer Ladung, Strom und
   Restspannung protokollieren. Beide Geräte müssen bestehen; Berechnung allein
   ist keine Laufzeitfreigabe. Falls nicht: zuerst Funk-/Lichtmittelwert senken,
   optische Wirkung erneut prüfen oder größeren Akku mit transparentem Maßzuwachs.

Vor Produktupload den 5700-spezifischen Build (4 MB Flash / 2 MB PSRAM,
Partitionierung/OTA, GPIO8/A3 und 25 physische Pixel) und den chipgebundenen
Flash-Rückweg separat qualifizieren. Der Modulvertrag ist kein erfolgreicher
Firmwarebuild und keine Übertragung früherer Geräteergebnisse auf diese Variante.

Die unabhängige Gegenprüfung beurteilt diesen Stand als **Prototypteilesatz zur
Beschaffung**, nicht als Funktions-, Laufzeit- oder Sicherheitsfreigabe des Geräts.
Beschaffung kann anhand dieser Stückliste vorbereitet werden. Die genannten
Gerätenachweise folgen erst nach Lieferung; fehlende Nachweise nicht als bereits
erfolgreiche Hardware-, Software- oder Sicherheitsprüfung ausgeben.
