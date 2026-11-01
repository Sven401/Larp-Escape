# Nähe Glut kompakt: Vorbereitung für zwei Exemplare

Stand31.10.2026, Live-Warenkorb ca23:43UhrMEZ, Basis483c3320 plus Kompaktauftrag.
**Zur Nutzerautorisierung vorbereitet:** Mouser zeigt nach der vom Nutzer
eingegebenen Adresse **99,02EUR Bestellsumme**, kostenlosen FedEx-Expressversand
und alle Bestellmengen als sofort versandbereit. Keine Bestellung ausgelöst.
Diese Liste ersetzt die Größen-/Teileauswahl aus R1, nicht dessen historische
Prüfbelege. Verbindliche Anforderungen stehen in [PLANUNG.md](PLANUNG.md).

## Ausgewählter Modulaufbau zur Ausarbeitung

| Menge | Teil | Abmessung/Bestellbedingung |
| --- | --- | --- |
| 2 | Adafruit5700, QT Py ESP32-S3 mit2MB PSRAM/4MB Flash | Komplettes ESP-Board21,7×17,8×5,7mm, innerhalb25×20mm. Integrierte Antenne; dokumentierter diodegeschützter BAT-Versorgungseingang. Keine Headerhöhe stillschweigend als null rechnen. |
| 2 | Adafruit5646,5×5 NeoPixel Grid BFF | Fertig bestückte und getestete Lichtplatine20,9×17,9×2,8mm mit25 physischen SK6805-Pixeln. Vorgesehen24 aktive Pixel, übriges Pixel schwarz; immer alle25 beim Start löschen. Alternative alle25 aktiv nur ausdrücklich in Firmware/Abnahme dokumentieren. |
| 2 | Adafruit2030, PowerBoost1000Basic | Separate fertig bestückte Versorgungsplatine29×23mm, reine PCB/Bauteilhülle ca2mm. Mit eingestecktem JST-Anschluss ca7,5mm oder mehr Kabelraum; lose optionale Steckverbinder nicht als abWerk verlötet behaupten. Tatsächliche Variante und Anschlussführung gegen Herstellerseite prüfen. |
| 2 | KeepPower P1835C3, geschützte18350,1400mAh nominal | Referenz Akkuteile12078; mindestens1300mAh/2,8A nach dortigem Datenstand. Ø18,60±0,15×38,70±0,3mm, nicht ungeschützte35-mm-Zelle. Hardware-Kurzschluss-/Überlade-/Tiefentladeschutz bleibt unverändert. Keine direkte Lötung an die Zelle. |
| 1 | NitecoreUM2, EU-Händler, zwei Ladeschächte | Externes Laden,4,2-V-Li-Ion-Programm, manuell200mA je Schacht als konservatives Ladeprofil. Lieferrevision und USB-Anschluss vor Kabel-/Netzteilauswahl prüfen. Kein Betriebslader in der Gerätehülle. |

Platinenbestückung ist bei diesen Serienmodulen bereits erledigt. Endmontage,
Verbindungskabel beziehungsweise deren Lötanschlüsse, Schalter, Zellkontakte,
Gehäuse und Firmware gehören weiterhin zum Aufbau. Der Angebotspreis für eine
frühere unbestückte R1-Platine ist nicht auf diesen Lieferumfang übertragbar.

## Tatsächlich geprüfte Bezugswege

### Opencircuit: wegen Termin verworfen

Am31.10. wurden im Browser je2 Stück5700/5646/2030 im unverbindlichen Warenkorb
zusammengestellt und als Lieferland Deutschland gewählt. Die Produktseiten
nannten10–12 Tage; erst der Warenkorb zeigte:

- 5700:20,00EUR/Stück, nicht auf Lager.
- 5646:16,50EUR/Stück,10–30 Arbeitstage.
- 2030:23,25EUR/Stück,10–30 Arbeitstage.
- Gesamt119,50EUR einschließlich angezeigter MwSt. und kostenlosem VersandDE.
- **Lieferprognose3.Dezember2026; gesamte Bestellung als Nachlieferung.**

Dieser Warenkorb ist **nicht für die beiden Dezember-Exemplare freigegeben**.
Die frühere Such-/Webablesung mit19,75EUR/16,00EUR und10–12 Tagen ist dadurch
überholt. Der aktuelle Warenkorb ist aussagekräftiger als der Produktteaser.
Es wurde kein verbindlicher Bestellschritt ausgeführt.

[5700](https://opencircuit.shop/product/adafruit-qt-py-s3-2mb-psram-wifi-dev-board),
[5646](https://opencircuit.shop/product/adafruit-5x5-neopixel-grid-bff-add-on-qt-py),
[2030](https://opencircuit.shop/product/powerboost-1000-basic-5v-usb-boost-1000ma-from).

### Mouser: aktueller Warenkorb für beide Exemplare

Nach vom Nutzer übernommener CAPTCHA-Prüfung hat Root die zugänglichen
Produktseiten und den tatsächlichen Warenkorb direkt gelesen. Die früher
gecachten55/90 Stück5700 sind überholt: live27 Stück, dazu6 Stück5646 und
75 Stück2030. Alle folgenden Bestellmengen meldet der Warenkorb als
**„Versand sofort“**; dies ist keine Reservierung und kein zugesicherter
Ankunftstag. Mouser weist zugleich allgemein auf erhöhtes Bestellaufkommen
und mögliche Bearbeitungsverzögerungen hin.

| Menge | Mouser-/Herstellerkennung | Verwendungszweck | Zeilenpreis nettoEUR |
| ---: | --- | --- | ---: |
| 2 | 485-5700 / Adafruit5700 | ESP32-S3-Board | 21,50 |
| 2 | 485-5646 / Adafruit5646 | Fertig bestückte Lichtplatine | 17,12 |
| 2 | 485-2030 / Adafruit2030 | Fertig bestückte Versorgung | 25,72 |
| 6 | 575-906215 / Mill-Max0906-2-15-20-75-14-11-0 | Vier Zellkontakte plus zwei Reserve | 4,54 |
| 2 | 612-EG1218 / E-SwitchEG1218 | EN-Steuerschalter | 1,24 |
| 4 | 576-0251002.MXL / Littelfuse0251002.MXL | Zwei2-A-Sicherungen plus zwei Reserve | 3,54 |
| 1 | 46-USBC2USB1MW-C / AmphenolUSBC2USB1MW-C | USB-A→USB-C-Daten-/Servicekabel1m, gemeinsam für beide Geräte | 9,55 |
| | **Ware gesamt** | | **83,21** |
| | FedEx International Express, **DDP** | Im aktuellen Warenkorb kostenlos | **0,00** |

Nach der vom Nutzer selbst vorgenommenen Adresseingabe bestätigt die
Checkout-Seite „Bezahlung“ **99,02EUR Bestellsumme**; dies entspricht83,21EUR
netto plus15,81EUR deutscher MwSt. Der Warenkorb
umfasst sechs bestückte Platinen und notwendiges Anschlusszubehör, nicht Akku,
Lader, Montage-/Druckmaterial, Drucker oder zwei bereits fertig betriebene Geräte.
Keine Produkte wurden allein als Versandfüller hinzugefügt: Kontakte, Schalter,
Sicherung und ein Datenkabel werden für Aufbau/Service benötigt.

Die sechs Platinen allein hätten64,34EUR netto plus25,00EUR netto Versand
gekostet. Die Bündelung der ohnehin benötigten Teile überschreitet die
Versandfreigrenze und spart diesen zweiten Versandaufwand.

Der unverbindliche Checkout steht bei **Bezahlung**, als Gast. Adresse wurde
vom Nutzer selbst eingegeben; Codex hat keine persönlichen Angaben eingegeben
oder in Projektdateien übernommen. Keine Zahlungsweise ausgewählt, keine
verbindliche Erklärung abgesendet. Die nächsten Schritte benötigen die
Bestellautorisierung und Zahlungsabwicklung durch den Nutzer.

**Lieferbedingungen ehrlich eingegrenzt:** Alle sieben Warenkorbpositionen
melden ihre vollständige Menge als „Versand sofort“, Versandart FedEx
International Express/DDP kostenlos. Auf Versand- und Zahlungsseite erscheint
kein verbindlicher Ankunftstag. Der allgemeine Hinweis auf mögliche Verzögerung
bei hohem Bestellaufkommen bleibt bestehen. Die Artikel sind damit wesentlich
kurzfristiger als der verworfene3.-Dezember-Warenkorb, eine Liefergarantie wird
nicht erfunden. Vor tatsächlichem Absenden Bestand und Preis erneut prüfen;
bei Nachlieferung oder spätem Termin nicht bestellen. Interner Eingangskontroll-
punkt13.11.2026, spätestens20.11. für Aufbau/Prüfung vor Dezember einplanen.

**Konkrete Autorisierungsfrage:** den oben bezeichneten Mouser-Warenkorb
für **99,02EUR brutto einschließlich FedEx Express/DDP nach Deutschland**
freigeben, solange alle Mengen sofort lieferbar bleiben? Diese Frage umfasst
keinen anderen Warenkorb, keine Stückzahlerhöhung und keine spätere Nachlieferung.

Prüfbare [Warenkorbseite](https://www.mouser.de/de/Cart/) im aktuellen Browser;
Warenkorbinhalt ist sitzungsgebunden. Artikelkennung/Mengen oben sind die
reproduzierbare Bestellgrundlage, kein öffentlicher Freigabelink.

### Akku und Ladegerät: separat geprüft

[Akkuteile12078/P1835C3](https://www.akkuteile.de/lithium-ionen-akkus/18350/keeppower/keeppower-18350-1400mah-3-6v-3-7v-li-ionen-akku-pcb-geschuetzt_12078_2798):
Live-Browser31.10. nennt7,40EUR/Stück ab zwei,1–3 TageDE; DHL5,90EUR
unter60EUR Warenwert. **Zwei Akkus einschließlich Versand20,70EUR.** Keine
Reservierung. Die kostenlose Schutzbox ab zwei Akkus ist laut Produktseite
enthalten; keine weiteren Zellen allein für Versandfreiheit hinzufügen.

[NitecoreUM2 NC-UM2](https://www.nitecore.de/nitecore-um2-usb-ladegeraet/nc-um2):
Direktseite22,96EUR inklusive MwSt., sofort/1–3 Tage, VersandDE3,95EUR;
**zusammen26,91EUR**. Kabel-/Netzteilumfang ist vor Kauf zu vervollständigen.
Primärhandbuch: https://charger.nitecore.com/Uploads/FLASHLIGHTS/download/um2.pdf .

Noch separat zu beschaffen: isolierte kurze AWG22-Litzen, Einzelisolierung/
Schrumpfschlauch, Gehäusebefestiger und Ladegerätversorgung. Kontakte, EN-Schalter,
Sicherung und Servicekabel sind oben konkret im Mouser-Warenkorb enthalten.
Printer/Filament werden nach dem neuen
Gehäuse-/Druckumfang aktualisiert, nicht aus dem großen R1-Verbrauch übernommen.

## Bestellautorisierung erst mit konkretem Umfang

Für die Freigabefrage sind Lieferant/Artikelnummern, Stückzahlen, Boardmaße,
technische Beschaffungseinschätzung und Grenzen, Bruttopreis/Versand sowie
die tatsächliche Lieferanzeige oben festgehalten. Die Vierstundenrechnung ist
noch kein Dauerlauf. Der [mechanische Passprototyp](MONTAGE-PROTOTYP.md)
misst66×34×38,4mm ohne Schraubenköpfe; das ESP-Board21,7×17,8mm. Ein
globales Formfaktorminimum oder eine Geräteabnahme wird damit nicht behauptet.
