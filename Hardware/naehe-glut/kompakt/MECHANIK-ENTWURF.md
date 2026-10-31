# Nähe Glut kompakt: Packungsstrategie

**Aktueller Modul-Passprototyp:** [Montage- und Prüfblatt](MONTAGE-PROTOTYP.md),
[native FreeCAD-Datei](mechanik/NG-Compact-Fit.FCStd),
[Geometriebericht](mechanik/geometry-report.json). Gehäusekörper jetzt
**66 ×34 ×38,4 mm**, ohne Schraubenköpfe. Alle sechs Druckteile und die
benannten maximalen Modulhüllen wurden digital ohne Kollision geprüft;
physische Kontakt-/Verpolungs-/Funk-/Wärme-/Laufzeitabnahme bleibt offen.
Verwendet werden QT Py5700, BFF5646, PowerBoost2030 ohne lose Zusatzstecker,
geschützte18350-Zelle und externe Ladung. Die folgenden flachen Akku-/XIAO-
Überlegungen sind frühere Untersuchungen, keine parallele Bestellliste.

Stand: 31.10.2026. **Vorläufiger mechanischer Entwurf, keine CAD-, Druck-
oder Bestellfreigabe.** Basis: `483c3320`; der aktuelle Nutzerauftrag ersetzt
R1 als neue Bestellbasis. Gefordert sind kleinster praktikabler Gesamtformfaktor,
vier Stunden Akkulaufzeit als Auslegungsziel und ein ESP-Board von höchstens
25 × 20 mm. Zwei funktionierende Geräte bis Dezember bleiben vorrangig.

Mechanik: `/root/compact_mechanics`; Hardwarearchitektur:
`/root/compact_hardware`; Koordination: `/root`; unabhängige Fachgegenprüfung:
`/root/supply_review`. Dieser Entwurf ändert keine R1-Lieferdateien.

## Was die Mindestgröße tatsächlich bestimmt

Die 21 × 17,8 mm eines XIAO ESP32S3 sind nur die ESP-Platine. Zur vollständigen
Hülle gehören Akku einschließlich Schutzschaltung und Kabelabgang, externe
Antenne einschließlich Stecker/Kabel, LED-/Leistungselektronik, USB-C-Buchse,
Schalter, Isolierung, Befestigungen und optischer Abstand zum Diffusor.

Ein Herstellerbeispiel zeigt die Größenordnung: Adafruit258 mit 1200 mAh ist
62 × 34 × 5 mm; Adafruit2011 mit 2000 mAh ist60 × 36 × 7 mm. Diese Teile sind
hier **Maßbelege, keine ausgewählte Batterie oder bestätigte Vierstundenlösung**.
Eine kleinere Zelle muss mit ihrem konkreten geschützten Packmaß und zulässigen
Strömen nachgewiesen werden; eine Typbezeichnung der nackten Zelle genügt nicht.

Die Seeed FPC-Antenne A-02 für XIAO ESP32S3 (SKU318020968) hat laut aktueller
Herstellerseite einen37,4 × 17,5-mm-Strahler und65 mm Koaxkabel mit I-PEX1.
Sie kann nicht als Nullvolumen zum21-mm-Board gerechnet werden. Das Kabel
braucht eine zugfreie Führung mit real zulässigem Biegeradius; kein scharfer
Knick am Antennen- oder I-PEX-Anschluss.

## Bevorzugte Grundanordnung

Ein abgerundetes flaches Gehäuse aus schwarzer PETG-Unterwanne und separatem
Diffusordeckel. Elektronik und Lichtfläche werden über der flachen Zelle
gestapelt, statt große Trägerflächen und USB-Kabelschleifen nebeneinander
vorzuhalten. USB-C liegt unmittelbar an einer Gehäusekante; der Betriebs-
schalter wird ohne Öffnen bedient. Keine internen USB-Verlängerungen.

Die FPC-Antenne erhält eine eigene metallfreie End- oder Randzone. Akku,
Schrauben, Magnetverschlüsse, Platinenkupfer und Kabelbündel werden dort
nicht darunter gepackt. Die Antenne wird auf einem geeigneten Kunststoff-
bereich befestigt, nicht auf die Zellenfolie geklebt. Eine konkrete kurze
Distanz zum Akku darf erst aus Antennenunterlagen und Funkprobe folgen.

Die Espressif-Leitlinie für Modul-PCB-Antennen empfiehlt im Endprodukt
mindestens15 mm Freiraum in alle Richtungen und fordert Reichweitenprüfung.
Das ist kein belegter pauschaler Mindestabstand jeder Kunststoffwand zur
externen Seeed-FPC-Antenne. Sie ist als RF-Risiko und Prüfauftrag zu behandeln,
nicht ungeprüft auf die externe Antenne zu übertragen oder wegzulassen.

Für die Befestigung zunächst zwei kleine, von außen zugängliche Schrauben
außerhalb von Antenne und Zellfläche vorsehen. Eine konkrete M2/M2,5-Auswahl
folgt der Wand-/Pfostengeometrie; keine zusätzlichen großen Träger, Muttern-
sortimente, Ringhalter und Klettschlaufen aus R1 automatisch übernehmen.
Schraubenspitzen dürfen den Akkuraum auch bei falscher üblicher Schraubenlänge
nicht erreichen. Zelle nicht durch Schrauben, Schnapphaken oder Deckeldruck
komprimieren; Halterung und Entnahme müssen ohne Beschädigung möglich sein.

## Ehrliche Maßabschätzung vor der Teilefestlegung

Zunächst werden Hüllen und ein vollständiger Z-Stapel gerechnet. Für einen
rechteckigen Akku gilt als reine Packungsuntergrenze ohne RF-Endzone:

`L außen >= max(Akkulänge + Kabelabgang, Elektroniklänge) + Montagezugabe + 2 Wände`

`B außen >= max(Akkubreite, Elektronikbreite, Antennenbreite) + Montagezugabe + 2 Wände`

`H außen >= Akkudicke + Isolierung/Freiraum + tatsächlicher Elektronikstapel + Lichtmischabstand + Boden/Deckel`

**Vorläufiges Untersuchungskorridor, keine zugesagten Außenmaße:**

| Annahme | Mechanische Untersuchung |
| --- | --- |
| Geschützter Akku höchstens ca.50 × 34 × 6 mm und Elektronikstapel höchstens7 mm | Hauptkörper ungefähr60 × 42 × 20–22 mm untersuchen; RF-Endzone kann Länge/Breite vergrößern. Das Akkuformat ist noch nicht beschafft/nachgewiesen. |
| Adafruit258 als reale Maßreferenz62 × 34 × 5 mm | Schon ohne gesonderte RF-Endzone ungefähr68–72 × 42 mm Grundfläche untersuchen;20–22 mm Höhe nur bei tatsächlich passendem Elektronik-/Optikstapel. |
| FPC-A02 in separater Endzone statt über dem Akku | Längeres schmales Gehäuse gegen breiteres kürzeres Gehäuse vergleichen; keine fertige Endmaßzusage ohne Akku-/Antennenlage. |

Diese Zahlen sind eine grobe Packungsrechnung aus nominellen Hüllen und
üblichen vorläufigen PETG-Wänden, keine nachgewiesene globale Minimalgröße.
Insbesondere60 × 42 × 14 mm wird hier nicht als belastbare Lösung angegeben:
Akku, Steckverbinder, USB-C und Diffusor können den verfügbaren Z-Raum übersteigen.

## Licht und Wärme

Kleinere SMD-Pixel auf einer eigenen Licht-/Leistungsplatine vermeiden den
65,5-mm-Durchmesser des bisherigen24er-Rings. Die24 Pixel werden nicht aus
Platzgründen stillschweigend auf sieben reduziert. Anordnung als kompaktes
4×6-Feld oder enger Ring ist mit dem Hardwareentwickler zu prüfen.

Ein kleiner Diffusor kann einzelne Lichtpunkte sichtbar lassen. Der notwendige
Abstand wird deshalb über einen kleinen Coupon mit mehreren Abständen und
Wandstärken am realen Goldprofil ermittelt. Kein großer kompletter Erstgehäuse-
druck vor dieser Probe. Mehr Diffusionsabstand erhöht die Gehäusehöhe; dies
gehört in den Maßvergleich und darf nicht im Rendering kaschiert werden.

Lader, Spannungswandler und maximale LED-Last werden nicht direkt auf die
Zelle thermisch gestapelt, ohne deren Verlustleistung und Zelltemperatur zu
prüfen. Vier Stunden Laufzeit müssen bei definiertem Funk-/Lichtprofil gemessen
werden. Ein kleiner Akku, dunkle Standbilder oder Deep-Sleep-Werte allein
belegen diese Laufzeit nicht.

## Vor dem ersten kompakten Fertigungspaket

1. Konkrete Akku-, Antennen-, ESP-, USB-, Schalter- und Elektronikmaße festlegen.
2. Alle getrennten Platinen nennen; ein25 × 20-mm-ESP-Board ersetzt keine
   Gesamtgrößenangabe. Eigenständige LED-/Leistungsplatinen separat bemaßen.
3. Vollständige Hüllen in CAD einschließlich Stecker- und Kabelräumen abgleichen;
   keine Kollisionen, Zellquetschung oder Metalle in der vorgesehenen RF-Zone.
4. Diffusorcoupon sowie Gehäuse-/Steckerpassprobe drucken; anschließend Wärme,
   Funk im geschlossenen und tatsächlich getragenen/positionierten Gehäuse
   und vierstündigen Lauf messen.
5. Erste Prototypenteile nach passendem digitalem Review bestellen; die reale
   Passung, Optik, Funk-, Wärme- und Vierstundenprüfung anschließend durchführen.
   Eine spätere Betriebsfreigabe folgt diesen Nachweisen. Physische Abnahme ist
   keine zirkuläre Voraussetzung für die erste Prototypen-Teilebestellung.

## Quellen

- [Seeed XIAO ESP32S3](https://files.seeedstudio.com/Bazaar/product_pdf/113991114.pdf):21 × 17,8 mm.
- [Seeed FPC A-02](https://www.seeedstudio.com/2-4GHz-FPC-Antenna-1-16dBi-for-XIAO-ESP32S3-p-6440.html):37,4 × 17,5 mm,65-mm-Kabel; abgerufen31.10.2026.
- [Adafruit258](https://www.adafruit.com/product/258):1200 mAh,62 × 34 × 5 mm; abgerufen31.10.2026.
- [Adafruit2011](https://www.adafruit.com/product/2011):2000 mAh,60 × 36 × 7 mm; abgerufen31.10.2026.
- [Espressif PCB Layout Design](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html):Module Placement, Endgehäuse-/Antennenfreiraum und RF-Prüfung; abgerufen31.10.2026.
