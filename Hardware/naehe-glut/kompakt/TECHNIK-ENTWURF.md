# Nähe Glut: kompakte Hardwarearchitektur, Vorentscheidung

Stand: 31.10.2026. Ausgangscommit `483c3320`; Quellenrecherche und
Rechenentwurf, keine CAD-/Bestellfreigabe. Fachentwicklung Geräteintegration:
`/root/compact_hardware`; unabhängige Gegenprüfung: `/root/supply_review`;
Koordination/Entscheidung: `/root`. Bestehende R1-Dateien nicht verändert.

**Fortgeschrieben:** Der konkrete Dezember-Modulweg steht im
[Verdrahtungs-/Abnahmevertrag](TECHNIK/VERDRAHTUNG.md). Die nachfolgenden
Alternativen und frühere 5-V-Rechnung sind die Voruntersuchung; für den
ausgewählten 5,2-V-PowerBoost gilt die aktualisierte Laufzeitrechnung dort.

## Verbindlicher Auftrag

- Zwei funktionierende ESP32-S3-Geräte bis Dezember2026; eigene PCB darf den
  Termin nicht gefährden. Alle benötigten Platinen sollen fertig bestückt sein.
- Kleinstmöglicher praktikabler Gesamtformfaktor, **gesamtes ESP-Board maximal
  25×20mm**. Eine größere Trägerplatine darf diese Grenze nicht verschleiern;
  separate LED-/Powerplatinen und Akku haben eigene offen auszuweisende Maße.
- Vier Stunden Betrieb als Auslegungsziel. Bezug ist der Näheglut-Goldeffekt,
  keine Dauer-Vollweißbeleuchtung. Die sichtbare Lichtwirkung wird am Diffusor
  geprüft; identische RGB-Zahlen bedeuten bei anderen LEDs keine gleiche Helligkeit.
- Keine stille Reduktion auf sieben LEDs. R1 hatte24 ausgewählte Pixel,
  der historische Effekt23. Ein fertiges25-Pixel-Modul muss explizit als solches
  benannt werden; alternativ24 aktiv und ein physischer Pixel dauerhaft schwarz.

## ESP-Boards innerhalb der Maßgrenze

| Kandidat | Tatsächliche Hersteller-/Produktmaße | Nutzen / Grenze |
| --- | --- | --- |
| Adafruit QT Py ESP32-S3 **5426** | **21,7×17,8×5,7mm**, 8MB Flash, kein PSRAM | Integrierte Chipantenne (Johanson2450AT18B100 laut Originalschaltplan), dokumentierte diodegeschützte BAT-Pads3–6V; USB/BAT-Quellenentkopplung ist ein wesentlicher Vorteil. Kein Akku-Lader. |
| Adafruit QT Py S3 **5700** | Gleiche QT-Py-Familie, genaue gelieferte Revision prüfen; 4MB Flash/2MB PSRAM | Technisch gleichwertige kompakte Versorgungs-/Antennenalternative mit anderem Speicherprofil. Firmwarekonfiguration getrennt. |
| Seeed XIAO ESP32S3 **113991114** | **21×17,8mm** Grundfläche | Klein, integrierter Lader und MCU-USB/BAT-Umschaltung, aber externe FPC-Antenne; A-02 allein37,4×17,5mm plus65mm Koax. Zusätzliche LED-Akkulast beim Laden ist nicht automatisch Powerpath-entkoppelt. |
| Waveshare ESP32-S3-Zero-N8R8 **33879**, ohne Header | **23,5×18mm** Grundfläche | 8MB Flash/8MB PSRAM, integrierte Keramikantenne, günstiger bestätigter Beschaffungskandidat. USB-VBUS/5V-Pad sind laut publiziertem Schaltplan unmittelbar verbunden: kein direktes paralleles externes5V/USB. Ohne belegte sichere Quellenumschaltung kein bevorzugter Versorgungsweg. |

Quellen: [QT Py Produkt5426](https://www.adafruit.com/product/5426),
[QT Py Pinouts](https://learn.adafruit.com/adafruit-qt-py-esp32-s3/pinouts),
[XIAO S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/),
[Seeed A-02](https://www.seeedstudio.com/2-4GHz-FPC-Antenna-1-16dBi-for-XIAO-ESP32S3-p-6440.html),
[Waveshare Varianten](https://docs.waveshare.com/ESP32-S3-Zero),
[Waveshare Schaltplan](https://files.waveshare.com/wiki/ESP32-S3-Zero/ESP32-S3-Zero-Sch.pdf).

**Vorentscheidung:** QT Py mit BAT-Entkopplung ist elektrisch der einfachere
USB-Serviceweg; Beschaffbarkeit der exakten Variante muss vor Auswahl gesichert
sein. Waveshare bleibt lieferbarer Kandidat, nicht durch bloßes Aus-/Abstecken
als hardwareseitig USB-sicher erklärt. Kein eigener S3-Modulentwurf ist nötig,
solange ein kleiner Serienbaustein das Gesamtvolumen bereits erfüllt.

Antennen dürfen nicht zwischen Akku und LED-PCB eingeklemmt werden. PCB- und
Keramikantennen brauchen die jeweiligen Herstellerfreiräume; XIAO-FPC darf nicht
in Akku-/Metallgröße verschwinden. Funkabnahme im geschlossenen Gehäuse bleibt
Pflicht. Gehäuseaußenmaße stammen erst aus dem separaten Packungsentwurf.

## Bevorzugter Dezemberweg: fertige, kleine Serienplatinen

Root hat die fertige **Adafruit5646 5×5 NeoPixel Grid BFF** als Kandidat
eingebracht. Hersteller bestätigt25 bestückte/geprüfte SK6805-1515-Pixel,
Kompatibilität mit QT Py/XIAO und externe5V/GND/IN-Lötpads. Der Hersteller
bestätigt für genau dieses Modul3V-Logik an5V-Versorgung. Der SK6805-Hersteller
nennt für EC15 fünf mA pro Farbkanal. Das vermeidet die Neuentwicklung und
Bestückung einer eigenen LED-Platine vor Dezember.

Das Modul kann als25-Pixel-Matrix genutzt oder mit24 aktiven Pixeln und einem
explizit schwarzen Pixel betrieben werden. Alle25 Pixel müssen bei Boot mit
einem vollständigen schwarzen Frame initialisiert werden. Nicht lediglich24
Bitsätze senden und den Zustand des letzten Pixels undefiniert lassen.

[Adafruit5646](https://www.adafruit.com/product/5646),
[Adafruit-Modulvertrag](https://learn.adafruit.com/adafruit-5x5-neopixel-grid-bff/overview),
[SK6805-EC15 Herstellerstrom](https://www.normandled.com/News/view/aid/246.html).

Die frühere Idee separat gespeister BFF-/ESP-Zweige ist verworfen. Verbindlich
ist jetzt der [Modul-Verdrahtungsvertrag](TECHNIK/VERDRAHTUNG.md): PowerBoost
speist QT-Py-BAT; die BFF erhält ihre Versorgung ausschließlich vom QT-Py-5V-Pad
nach den USB/BAT-OR-Dioden. Dadurch bleibt der LED-Dateneingang bei USB-Service
nicht stromlos. Keine zusätzliche direkte Boost→BFF-Verbindung herstellen.
Dies ist Endmontage an bereits fertig bestückten Platinen, keine PCB-Bestückung.

### Kleine fertig bestückte Wandler

| Kandidat | Herstellermaße / Daten | Bewertung |
| --- | --- | --- |
| Pololu **4941 U3V16F5** | 13,1×8,1×3mm; 5V±4%; typisch1,6A **Eingang**, 2A Eingangs-Schaltlimit | Für Gold-Mittelbedarf ausreichend. Bei3V/85%η ergeben1,6A Eingang nur0,816A bei5V. Keine zugesicherte0,88A-Vollweiß+MCU-Spitze. Kein Kurzschluss- oder Verpolschutz; nicht pauschal als1,6A-Ausgang ausgeben. |
| Adafruit **4654 MiniBoost TPS61023** | **17,8×11,3×5,6mm**, tatsächlich **5,2V** Sollwert; Hersteller misst800mA Ausgang bei3V,1100mA bei3,5V; EN mit vollständiger Abschaltung | Kurzschluss-/Thermoschutz und EN sind Vorteile. Auch hier Vollweiß+500mA-MCU-Spitze bei fast leerem Akku oberhalb der gemessenen800mA-Grenze; Näheglutprofil und Spitzenlast abnehmen. |

[Pololu4941](https://www.pololu.com/product/4941),
[Adafruit4654](https://www.adafruit.com/product/4654),
[TI TPS61023](https://www.ti.com/product/TPS61023).

Ein stärkerer kleiner Serienwandler kann die Peakreserve erhöhen. Es ist aber
nicht sinnvoll, für eine frei erfundene Dauer-Vollweißfunktion einen großen
Akku einzubauen. Verbindliches Profil und sichere Überlastreaktion getrennt
festlegen. Unabhängig vom Wandler braucht eine entnehmbare Zelle verpolsichere
Kontaktierung bzw. passenden Verpolschutz und ausreichenden Leitungsschutz.

## Akku: kleiner geschützter Energiespeicher statt48Wh-Powerbank

Konkreter Rundzellenkandidat **KeepPower P1835C3**, geschützt, nominal1400mAh.
Aktuelle Herstellerseite: min1380mAh,3A Dauerentladung,18,5×38,9mm,
Standardladen280mA. Ein älteres Herstellerblatt nennt min1300mAh,2,8A Dauer,
max18,5×39mm. Für die Vorrechnung werden konservativ **1300mAh/2,8A** benutzt;
der gelieferte Revisionsstand muss zum Ladeprofil passen. Nicht aus dem
Standardladestrom ein ungenanntes zulässiges Maximum ableiten.

Diese Zelle erlaubt externe Ladung in einem geeigneten CC/CV-Li-Ion-Lader,
z.B. XTAR VP2 mit manuell0,25A/4,2V oder einem entsprechend einstellbaren
VC4SL. Dadurch entfallen Lader, USB-Ladebuchse und Lade-Powerpath im Gerät.
Der Lader ist ein zusätzlicher Beschaffungsposten, kein Teil der Gehäusegröße.
Eine Rundzelle ergibt einen schmaleren, dickeren Körper; ein flacher geschützter
LiPo kann trotz ähnlichem Zellvolumen für flaches Tragen günstiger sein. Erst
das Packungsmodell entscheidet, nicht das Foto der nackten ESP-Platine.

[P1835C3 aktueller Hersteller](https://www.keeppower.com.cn/products_detail.php?id=675),
[älteres P1835C3 Datenblatt](https://cdn03.plentymarkets.com/i9a0e0hd8l6w/frontend/Datenblaeter/Keeppower/P1835C3/P1835C3_3.7V_1400mAh_datasheet__1_.pdf),
[XTAR VP2](https://www.xtar.cc/product/xtar-vp2-charger-24.html),
[XTAR VC4SL Hersteller](https://www.xtar.cn/product/177.html).

### Vorläufige Vierstundenbilanz

Rechengrößen, keine Lastmessung: MCU-Board im Funkbetrieb im Mittel120mA
an der5V-Schiene, fünf mA Zusatzbudget und bei einer eigenen24-Pixel-
5-mA-LED-Platine höchstens0,5mA Grundstrom je Pixel. Goldmaximum RGB24/8/0:

- LED-Kanalstrom24×5mA×32/255 =15,06mA.
- LED-Grundstrom24×0,5mA =12mA.
- Gesamt5V-Mittelstrom120+15,06+12+5 =152,06mA.
- Mittlere Ausgangsleistung0,760W; bei85% Wandlerwirkungsgrad0,894W am Akku.
- Nutzbare Rechenenergie1,3Ah×3,7V×0,8 =3,848Wh.
- Rechnerische Laufzeit **4,30h**. Für BFF25Pixel steigt der Ruhestrom; bei
 5,2V/gleichen Stromannahmen etwa **4,1h**. SK6805-Revision/Grundstrom noch prüfen.

Die20% Energiereserve deckt hier alle nicht nutzbaren Kapazitätsanteile
gemeinsam ab, ist kein getrennter Nachweis für Kälte, Alterung,3V-Abschaltung
und Fertigungstoleranz. Nach optischem Abgleich höhereRGB-Werte erfordern eine
neue Bilanz. Bei150mA statt120mA MCU-Mittelstrom fällt der gewählte Akku unter
vier Stunden. Der echte Funk-/Effekt-Dauerlauf muss daher vor finaler
Kleinakku-Freigabe Mittelstrom und entnommene Wh messen.

Spitzenlast separat: bei25×15mA LED-Weiß plus25×0,5mA Grundlast,
500mA MCU-Spitze und5mA Sonstiges liegen etwa0,893A an5V an. Bei3V Akku/85%η
sind das **1,75A Eingang**; kleiner Wandler muss diese Spitze vertragen oder
eine ausdrücklich eingeschränkte sichere Lastkonfiguration erhalten. Die
2,8A-Zellgrenze allein bescheinigt keine Wandler-, Stecker- oder Thermikfreigabe.

Wandler mit Startschwellen1,8/2,7V sind keine passende Li-Ion-Betriebsabschaltung.
Für ein angesetztes Ende bei3V braucht es eine definierte Unterspannungs-
Abschaltung/Hysterese oder eine anders konservativ belegte Energie-/Schutz-
Strategie. Die PCM-Notabschaltung bei2,5V ist kein Ersatz für diese Annahme.

## Optionale eigene kompakte LED-/Power-PCB

Technischer Kandidat: separate Zielplatine24×20mm mit24 Worldsemi
**WS2812C-2020-V1** (5mA/Kanal), TPS61023DRLR5V-Boost, einzeln abgeblockten
Pixeln, passendem Datenbuffer mit dunklem Start, Verpol-/Stromschutz und
Unterspannungssupervisor. Keine neue große ESP-Trägerplatine. Diese Größe ist
ein Layoutziel, **keine platzierte/geprüfte CAD-Datei**. Fertiger kann alle
SMD-Teile montieren; Kosten/Liefertermin erst nach BOM-/CAM-/DFM-Prüfung.

Für exakt LCSC C2976072 gilt HerstellerblattV1: VDD3,7–5,3V,
VIH min2,7V, Grundstrom<0,5mA/Pixel. Nicht mit V6 oder einem generischen
WS2812C-Blatt gleichsetzen. Ein5V-AHCT-Buffer schafft sichere GPIO-Marge.
Der12-mA-Kandidat WS2812B-2020-V6 hat andere Strom-/Pegeldaten und ersetzt
die5-mA-Variante nicht ohne neue Bilanz. SK6805 ist wiederum ein anderer Chip.

[V1-Herstellerdatenblatt bei LCSC](https://datasheet.lcsc.com/datasheet/pdf/09f4c697250f44092a00c11d7628f2c6.pdf),
[Worldsemi Familienvergleich](https://www.world-semi.com/ws2812-family/223.html),
[LCSC C2976072](https://www.lcsc.com/de/product-detail/RGB-LEDs_Worldsemi-WS2812C-2020-V1_C2976072.html),
[TI SN74AHCT1G125](https://www.ti.com/product/SN74AHCT1G125).

Die native S3-Neuentwicklung mit ESP32-S3-MINI-1-N8(15,4×20,5×2,4mm)
wird vorerst nicht bevorzugt: Das ganze ESP-Board müsste dennoch≤25×20mm
bleiben; USB, Schutz, Stromversorgung und Antennenfreiraum kommen hinzu.
Diese neue RF-/Versorgungsentwicklung spart bei batteriebegrenztem Gesamtmaß
nicht automatisch Gehäusevolumen und erhöht das Dezemberrisiko.

## Nächste Entscheidung und Nachweise

1. Tatsächlich kurzfristig lieferbares QT-Py-/anderes USB-sicheres ESP-Board,
   fertiges kleines LED-Modul und Wandler auswählen; genaue Maße/Revisionen
   mit dem mechanischen Packungsmodell binden.
2. Verdrahtung/Quellenentkopplung und Last-/Unterspannungsvertrag konkretisieren;
   kein Modul-Header als impliziten Beweis verwenden.
3. Nur wenn ein eigener Entwurf messbar Volumen spart und den Termin nicht
   gefährdet, CAD/BOM/CAM/Bestückungsquote erarbeiten. Keine Bestellung aus R1.
4. Beschaffung, Bauteil-/Polungs-/USB-/Kabelkontrolle, Passprobe, Funk-/Optik-/
   Wärme-/Vierstundenlauf beider Geräte durchführen. Digitale Daten allein
   belegen keinen dieser physischen Punkte.
