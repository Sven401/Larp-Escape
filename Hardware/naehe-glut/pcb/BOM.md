# R1: Bestückungsliste für zwei Carrier

Stand 31.10.2026. Diese Liste gehört ausschließlich zur R1-Schaltung in
`circuit.json`. Die Mengen enthalten keine bereits vorhandenen Teile.
Bestand und Liefertermin beim Bestellen prüfen; ein Link ist kein reservierter
Lagerbestand. Hauptmodule, Akkus, Kabel und Drucker stehen in der übergeordneten
Beschaffungsliste. Nicht allein anhand ähnlich klingender Ersatztypen bestellen.

| Ref. | Hersteller / exakte Bestellnummer | Je PCB | Für zwei | Empfohlener Einkauf inkl. Reserve | Quelle / Bestellung |
| --- | --- | ---: | ---: | ---: | --- |
| J1, J3 | Samtec SSW-122-01-G-S, Buchsenleiste 1×22, 2.54 mm | 2 | 4 | 6 | [Samtec](https://www.samtec.com/products/ssw-122-01-g-s) |
| J2 | GCT USB4135-GF-A, USB-C 6 Kontakte, power-only, 3 A | 1 | 2 | 3 | [GCT mit Distributorbestand](https://gct.co/connector/usb4135) |
| J4 | Phoenix Contact 1715734, MKDS 1,5/3-5,08 | 1 | 2 | 3 | [Hersteller](https://www.phoenixcontact.com/en-us/products/printed-circuit-board-terminal-mkds-15-3-508-1715734) |
| U1 | Texas Instruments SN74AHCT125N, PDIP-14 | 1 | 2 | 3 | [TI](https://www.ti.com/product/SN74AHCT125/part-details/SN74AHCT125N) |
| Q1 | onsemi 2N3904BU, TO-92, 1=E / 2=B / 3=C | 1 | 2 | 5 | [Herstellerdatenblatt](https://www.onsemi.com/pdf/datasheet/2n3904-d.pdf), [Artikelsuche](https://www.mouser.de/c/?q=2N3904BU) |
| F1 | Littelfuse 37212000411, TR5, 2 A träge, Raster 5.08 mm | 1 | 2 | 5 | [Herstellerdatenblatt](https://www.littelfuse.com/assetdocs/littelfuse_fuse_372_datasheet?assetguid=1a83e408-fdbe-48de-8389-c4f8ebb8232e), [Artikelsuche](https://www.mouser.de/c/?q=37212000411) |
| C1 | Panasonic EEUFR1A102, 1000 µF / 10 V, Ø10 × 16 mm, Raster 5 mm | 1 | 2 | 3 | [Panasonic mit Bestandssuche](https://industrial.panasonic.com/ww/products/pt/aluminum-cap-lead/models/EEUFR1A102) |
| C2 | KEMET C320C104K5R5TA, 100 nF / 50 V, X7R, 10 %, Raster 2.54 mm | 1 | 2 | 5 | [Herstellermaßblatt](https://yageogroup.com/component-documentation/download/specsheet/C320C104K5R5TA?lang=en), [DigiKey](https://www.digikey.de/en/products/detail/kemet/C320C104K5R5TA/818040) |
| R1 | Yageo MFR-25FBF52-1K, 1 kΩ, 1 %, 0.25 W | 1 | 2 | 10 | [Artikelsuche](https://www.mouser.de/c/?q=MFR-25FBF52-1K) |
| R2, R4, R5, R7 | Yageo MFR-25FBF52-10K, 10 kΩ, 1 %, 0.25 W | 4 | 8 | 20 | [Artikelsuche](https://www.mouser.de/c/?q=MFR-25FBF52-10K) |
| R3 | Yageo MFR-25FBF52-330R, 330 Ω, 1 %, 0.25 W | 1 | 2 | 10 | [Artikelsuche](https://www.mouser.de/c/?q=MFR-25FBF52-330R) |
| R6 | Yageo MFR-25FBF52-100K, 100 kΩ, 1 %, 0.25 W | 1 | 2 | 10 | [Artikelsuche](https://www.mouser.de/c/?q=MFR-25FBF52-100K) |
| R8, R9 | Yageo MFR-25FBF52-5K1, 5.1 kΩ, 1 %, 0.25 W | 2 | 4 | 10 | [Artikelsuche](https://www.mouser.de/c/?q=MFR-25FBF52-5K1) |

Die Widerstände haben nach [Yageo MFR-Datenblatt](https://yageogroup.com/content/datasheet/asset/file/YAGEO-MFR_DATASHEET)
6.3 ± 0.5 mm Körperlänge und 2.4 ± 0.2 mm Durchmesser. Auf 10.16 mm Raster
formen, ohne Zug direkt am Bauteilkörper. Q1-Beine für den vorgesehenen
TO-92-Wide-Footprint auf 2.54 mm benachbarten Rasterabstand formen; die E/B/C-
Reihenfolge bleibt unverändert. U1 wird direkt eingelötet; ein zusätzlicher
IC-Sockel ist nicht Teil dieser geprüften Stückliste oder des Höhenmodells.

Für die PCB-Montage sind je Carrier vier **M3×10-Schrauben und vier M3-Muttern
aus PA/Nylon** vorgesehen (für zwei: je acht, Einkauf je zehn). Metallteile
an diesen Positionen sind wegen des Antennenabstands nicht austauschbar.
Die genauen mechanischen Befestigungen und Druckteile führt die Gehäuseanleitung.

J2 hat sechs SMD-Signalkontakte und vier SMD-Schirmanker. Er ist das einzige
feinere SMD-Teil; unter Vergrößerung löten und auf Brücken prüfen oder diese
Position vom Bestückungsdienst montieren lassen. Keine beliebige USB-C-Buchse
mit gleichem Außenmaß einsetzen. Alle übrigen Bauteile sind durchkontaktiert.
