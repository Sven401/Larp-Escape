# Nähe Glut R1: Beschaffung für zwei Stationen

Stand: 31.10.2026. **Beschaffungsliste für Revision R1; keine ausgelöste Bestellung.**
Der Nutzer hat Komponentenwahl, Akkukonzept und Auswahl eines 3D-Druckers
ausdrücklich delegiert (Sitzung `01a192e5-6a3d-7ce2-a9e1-bfacd9a97e3b`).
Elektronik-Stückliste und mechanische Mengen sind unten verknüpft. Der jeweilige
digitale Freigabestand steht in `REVIEW.md`; reale Abnahme bleibt getrennt.

## Festgelegte Baugruppen

| Menge für zwei Stationen | Teil | Beschaffungs- und Ausführungsbedingung |
| --- | --- | --- |
| 2 | Espressif **ESP32-S3-DevKitC-1-N8R8**, Originalboard | 8 MB Flash/8 MB PSRAM; mechanische Basis v1.1, 25,4 × 62,74 mm. Keine namenlosen oder breiteren kompatiblen Boards einsetzen. [Hersteller](https://www.espressif.com/en/products/devkits/esp32-s3-series/esp32-s3-devkitc-1). |
| 2 | Adafruit **1586**, NeoPixel-Ring mit 24 RGB-LEDs | Außen-Ø65,5 mm, Innen-Ø52,3 mm, Dicke 3,2 mm. [Hersteller](https://www.adafruit.com/product/1586), [EU-Bestellquelle Kiwi Electronics](https://www.kiwi-electronics.com/en/neopixel-ring-24-x-ws2812-5050-rgb-led-with-integrated-drivers-1840). Am 31.10. durch Gegenprüfer 9 Stück/16,49 EUR exkl. VAT ausgelesen; keine Reservierung. BerryBase-Direktabruf meldete nicht lieferbar. |
| 2 | Voltaic **V50**, aktuelle Aluminiumversion, 48 Wh | [Hersteller](https://voltaicsystems.com/v50/), [SeeSense EU, V-BATT-V50](https://seesense.eu/products/power/batteries/v50-47wh-lithium-battery-with-always-on-capability/). Direktabruf 31.10.: IN STOCK, 80 EUR netto/98,40 EUR mit dortiger VAT. Deutsche Endsteuer und Versand erst im Warenkorb verbindlich. |
| 1 | **Bambu Lab A1 mini**, Einzelgerät ohne AMS lite | [Hersteller-Bestellseite](https://eu.store.bambulab.com/products/a1-mini). 180 × 180 × 180 mm Bauraum, PETG geeignet laut [Hersteller-Kurzanleitung](https://cdn1.bambulab.com/documentation/quick-start-f507128172bdf/Quick%20start%20guide%20-%20A1%20mini-EN.pdf). Preis/Lieferbarkeit im Direktabruf nicht lesbar; kein behaupteter aktueller Preis. |
| 1 Rolle | **Bambu PETG HF Black**, 1,75 mm, 1 kg, mit Spule | Gehäuse und Halter; Slicerprofil `Bambu PETG HF @BBL A1M`. [Bestellseite](https://eu.store.bambulab.com/products/petg-hf); Lieferbarkeit nicht bestätigt. Bei Refill zusätzlich passende wiederverwendbare Spule kaufen. |
| 1 Rolle | **Bambu PETG Translucent Clear (32101)**, 1,75 mm, 1 kg, mit Spule | Separat gedruckter Diffusor, kein Mehrfarbenzubehör nötig. [Bestellseite](https://eu.store.bambulab.com/products/petg-translucent). Slicerprofil `Bambu PETG Translucent @BBL A1M`. Optische Wirkung erst am realen Licht prüfen. |
| 2 | **Delock 83326**, USB-A auf USB-C, 0,5 m | Carrier-Versorgung; 24-AWG-Stromadern, Hersteller nennt bis 3 A. [Hersteller/Bestellkennung](https://www.delock.de/produkt/83326/merkmale.html). Kein USB-C-PD-Ausgang des Akkus verwenden. |
| 2 | **Delock 84900**, USB-A auf Micro-B, 0,5 m | ESP-Versorgung und bei getauschtem Anschluss USB-Service; 24-AWG-Stromadern. [Hersteller/Bestellkennung](https://www.delock.de/produkt/84900/merkmale.html). |
| 2 | **Raspberry Pi 15W USB-C Power Supply**, EU, 5,1 V/3 A, schwarz; BerryBase **RPI4NT3AB** | Je ein Netzteil zum Laden der entnommenen V50 am vorgesehenen USB-C-Ladeeingang. Fest angeschlossenes USB-C-Kabel; kein zusätzlicher Ladeadapter nötig. [Hersteller](https://www.raspberrypi.com/products/type-c-power-supply/). 3 A ist die Lieferfähigkeit, kein erzwungener Ladestrom. |

Lagerangaben sind datierte Webablesungen, keine Reservierungen.

## Platinen, Bestückung und Montagebedarf

Die vollständige [Elektronik-Stückliste](pcb/BOM.md) ist Teil dieser Bestellung:
alle dort genannten Einkaufspositionen einmal in der Spalte „Empfohlener Einkauf
inkl. Reserve“ bestellen. Die Mengen decken zwei Stationen ab. Fertigung und
Verdrahtung richten sich nach [FERTIGUNG.md](pcb/FERTIGUNG.md).

| Bestellmenge für beide Geräte inklusive Reserve | Position |
| --- | --- |
| 5 unbestückte PCBs | Revision NG-R1 aus dem Gerber-/Bohrdatenpaket; zwei zum Einbau, drei Reserve. Fertigungsparameter aus `pcb/FERTIGUNG.md` unverändert übernehmen. |
| 1 Satz | Alle Bauteilpositionen aus `pcb/BOM.md`, mit den dort genannten Reservemengen; bei externer Bestückung zwei PCBs vollständig bestücken lassen. |
| je 2 m | Flexible isolierte Kupferlitze 0,5 mm², rot und schwarz, für LED-Plus/Masse. |
| 2 m | Flexible isolierte Kupferlitze 0,25 mm², gelb, für LED-Daten; Leitungen je Station kurz halten und gemeinsam führen. |
| 1 Sortiment | Schrumpfschlauch 2–6 mm für isolierte Lötanschlüsse und Zugentlastung. |
| je 10 | M3×12, M3×10 und M3×16 Zylinderschrauben ISO 4762, Edelstahl A2. |
| 30 | M3-Sechskantmuttern ISO 4032/DIN 934, A2, SW5,5 mm, Höhe2,4 mm. |
| je 10 | M3×10 **PA-/Nylonschrauben** mit Kopf-Ø≤5,5 mm und M3-PA-/Nylonmuttern SW5,5 mm, Höhe≤2,4 mm; diese ersetzen die im PCB-BOM ebenfalls aufgeführten Befestiger, nicht doppelt bestellen. |
| 4 | Wiederverwendbare Klettbänder, 20×ca.300 mm, zur Akkubefestigung. |
| 20 | Kabelbinder 2,5×ca.100 mm, für Ring und Kabel. |

Die [Montageanleitung](mechanik/MONTAGE.md) ordnet Schraubenlängen und Materialien
den Einbaustellen zu. Platinenbefestiger müssen wegen der Antenne aus Nylon sein.
Der Drucker braucht kein AMS und kein zusätzliches Druckbett für diese Dateien.
Die zwei Filamentrollen aus der Hauptliste reichen nach Slicerrechnung für beide
Gehäuse mit Reserve: zusammen etwa 0,78 kg strukturelles PETG und 0,017 kg klares
PETG einschließlich zweier Passproben. Fehldrucke sind darin nicht enthalten.

**Werkzeuge, falls nicht vorhanden:** temperaturgeregelte Elektronik-Lötstation
mit feiner Spitze, bleifreies Elektroniklot und Flussmittel, Entlötlitze,
Pinzette/Lupe für USB-C, Seitenschneider, Abisolierer, Multimeter,
2,5-mm-Innensechskant und passender kleiner Schlitzschraubendreher für die
LED-Klemme. Für eine vollständig bestückte PCB entfällt deren eigene Lötarbeit;
der LED-Ring benötigt weiterhin angelötete Leitungen.

## Reihenfolge zum Schutz des Dezemberziels

Zuerst verfügbare Original-ESP-Boards, Ringe und Akkus sichern. Die eigene PCB
läuft parallel zur Inbetriebnahme dieser Baugruppen. Falls PCB-Liefertermin
oder Erstprüfung die Fertigstellung gefährdet, denselben elektrischen Vertrag
mit vorhandenen Modulen bzw. gelötetem Prototypenaufbau umsetzen; dessen
Verdrahtungs- und Befestigungsprüfung bleibt erforderlich. Kein Warten auf eine
zweite PCB-Revision als Voraussetzung für die zwei Dezemberexemplare.

Vor dem großen Gehäusedruck zunächst den Coupon drucken und Schrauben/Muttern
prüfen. Danach ein Exemplar montieren, elektrisch prüfen und erst anschließend
das zweite fertigen. Der Slicer schätzt für alle sechs Teile etwa 10,4 Stunden
pro Station; das ist reine Maschinenzeit ohne Montage und ohne Fehldrucke.

**Beschaffungsrisiko ESP:** N8R8 war am 31.10. im direkten BerryBase-DE-Abruf
nicht lieferbar, während internationale Seiten positive Bestände nannten.
[Exakte DE-Bestellseite](https://www.berrybase.de/espressif-esp32-s3-devkitc-1-n8r8-development-kit-8mb-psram-8mb-flash).
Die Stückliste behauptet deshalb keine gesicherte Sofortlieferung. Ein anderes
Original-DevKitC-1-Speichermodell darf erst nach dokumentierter Firmware- und
Pinprüfung als Bestückungsvariante eingetragen werden; kein stiller Klonersatz.

## Akkukonzept und Betriebsgrenzen

Der V50 übernimmt Zellen, Schutz und Ladefunktion als unveränderter fertiger
Akkupack. Die eigene PCB entwickelt weder einen Lithium-Lader noch ein BMS.
Das Gehäuse hält den entnehmbaren Akku mit Klett; Laden erfolgt außerhalb
des Stationsgehäuses. Revision 1 ist für trockenen, vor Regen geschützten
Betrieb ausgelegt und besitzt keine geprüfte IP-Schutzklasse.

Zwei USB-A-Ausgänge versorgen getrennte Pluspfade: einer das ESP-Board über
dessen USB-Anschluss, der andere die Carrierplatine und den LED-Ring.
Gemeinsame Masse ist gewollt; die Zweige sind **nicht galvanisch isoliert**.
Die Carrierplatine speist weder den 5-V- noch den 3,3-V-Pin des ESP-Boards.
Das vermeidet eine parallele externe Pinversorgung beim USB-Service.

V50-Herstellergrenzen: 5 V/2 A je USB-A-Ausgang, 3 A zusammen. Always-On
gilt für USB-A ohne Mindestlastabschaltung. Der obere USB-C-PD-Anschluss
darf im Always-On-Betrieb nicht benutzt werden. Zum Abschalten beide
USB-Ausgänge am Akku deaktivieren oder beide Betriebskabel entfernen.
Ein stromloser ESP bei weiter versorgtem Ring garantiert kein dunkles Licht:
der Ring kann seinen zuletzt empfangenen Farbwert behalten.

Auslegungsansatz: 24 × 60 mA = 1,44 A LED-Volllast; der ESP hat seinen
eigenen Port. Das ist eine konservative Rechengröße, keine Lastmessung.
Leiter, Stecker und Schutz werden für den Strompfad ausgelegt; eine
Firmware-Helligkeitsgrenze ersetzt diese Auslegung nicht.

Laufzeit wird in Wh gerechnet, nicht aus mAh bei 5 V: bei beispielhaft
80 % nutzbarer Zellenergie stehen rechnerisch 38,4 Wh zur Verfügung.
Das ergäbe bei 3 W etwa 12,8 h, bei 10 W etwa 3,8 h. Wirkungsgrad und
Verbrauch sind ausdrücklich Annahmen; eine Laufzeitzusage benötigt den
realen Last-/Dauerlauf beider Stationen.

## Firmware-Schnittstelle der neuen Hardwarevariante

R1 verwendet **24 Pixel**, GPIO15 für Daten und GPIO16 für eine über
Transistor freigegebene Datenpufferstufe. Der alte 23-Pixel-Aufbau und seine
Nachweise gelten nicht automatisch für R1. Eine eigene S3-/R1-Konfiguration,
definierter dunkler Start und reale Funktionsprüfung bleiben nötig. Der
Hardwarelieferumfang behauptet keine bereits erfolgte Firmwareportierung.
