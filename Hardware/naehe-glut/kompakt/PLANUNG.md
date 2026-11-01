# Nähe Glut kompakt: Auftrag und Arbeitsstand

Stand:31.10.2026. Ausgangscommit483c3320. Quelle: direkte Nutzeraufträge in
Codex-Sitzung01a192e5-6a3d-7ce2-a9e1-bfacd9a97e3b. Aktiver Entwurf,
**keine Bestell-/Fertigungsfreigabe**.

## Verbindliche Ziele

- Zwei funktionierende ESP32-S3-Exemplare bisDezember2026; eine eigene PCB
  darf diesen Termin nicht gefährden.
- Kleinstmöglichen Gesamtformfaktor aus realen Bauteil-, Akku-, Antennen-,
  Fertigungs- und Montagegrenzen entwickeln. Kein mathematisch bewiesenes
  globales Größenminimum ohne entsprechende Untersuchung behaupten.
- **ESP-Board höchstens25×20mm.** Ein Funkmodul auf einer größeren selbst
  entwickelten ESP-Platine erfüllt diese Grenze nicht allein durch seine
  Modulabmessungen. Separate Licht-/Powerplatinen mit Maßen offen ausweisen.
- **Vier Stunden Akkulaufzeit als Auslegungsziel**, vom Nutzer ausdrücklich
  bestätigt. Energie-, Spitzenlast- und Funkprofil definieren und am Gerät
  später messen; keine Laufzeitzusage allein aus Nennkapazität.
- Fertig bestückte Platinen, keine Bauteiltüten oder versteckte manuelle
  Carrier-Bestückung. Komponenten-/Akkuwahl ist an Codex delegiert.
- Bis zur konkreten prüfbaren Bestellung mit Fertiger, Dateirevision, exakter
  Teilezuordnung, Bruttopreis, Versand, Liefertermin und Prüfgrenzen arbeiten;
  tatsächliche Bestellung erst nach Nutzerautorisierung.

## Umfang und Verantwortung

`/root` koordiniert und führt Beschaffung/Planung, `/root/compact_hardware`
verantwortet den neuen Hardwareentwurf, `/root/compact_mechanics` die kompakte
Packung/Gehäuseentwicklung, `/root/supply_review` prüft unabhängig Hardware-
und Nachweisgrenzen. Vor CAD-Umsetzung zuerst Architektur/konkrete Teile mit
Abmessungen und Lastbudget abgleichen. Keine doppelten Dateibesitzer.

Näheglut behält gleichzeitiges warmes Licht mit weicher Überblendung, Start/
Stop und Funkfunktion zwischen zwei Exemplaren. Die bestehende Effektabsicht
steht in `../../../docs/led/naeheglut-design.md`. R1 sah24 Pixel vor; diese
werden zunächst als Entwurfsziel erhalten, nicht ungefragt auf7 reduziert.
R1-Carrier, großes DevKitC, V50 und R1-Gehäuse sind keine aktuellen Kaufziele.

## Ausgewählter Dezemberweg

Fertig bestückte Serienmodule: **Adafruit5700 QT Py ESP32-S3** (21,7×17,8×5,7mm,
4MB Flash/2MB PSRAM, integrierte Chipantenne), **Adafruit5646** als kleine
25-Pixel-Lichtplatine (24 aktiv vorgesehen) und **Adafruit2030 PowerBoost1000
Basic** als separate29×23-mm-Versorgungsplatine. Herausnehmbare geschützte
KeepPower18350 P1835C3, externes Laden. Quellen, reale getrennte Platinenmaße,
Netze und Grenzen stehen im [Verdrahtungsvertrag](TECHNIK/VERDRAHTUNG.md).

Damit wird keine neue PCB-Fertigung auf den kritischen Dezemberpfad gelegt.
Eine eigene kompakte PCB bleibt eine spätere Optimierung und benötigt dann
eigene Schaltung/Layout, ERC/DRC und Fertigungsreview. Die jetzige Beschaffung
betrifft bereits bestückte Module plus Kontakte, Schalter, Sicherung und Kabel;
Endmontage/Verbindungsdrähte bleiben erforderlich. Der XIAO-/FPC-Ansatz und
kleinere Wandler sind als untersuchte Alternativen im Technikentwurf erhalten.

Die unabhängige [Gegenprüfung](REVIEW.md) sieht keinen technischen
Beschaffungsblocker für diesen ausdrücklich begrenzten Prototyp-Teilesatz.
Sie ist keine Geräte-/Laufzeitfreigabe. Konservative Rechnung im gedämpften
Goldprofil ergibt etwa4,1h bei maximal120mA mittlerem ESP-Verbrauch als
unbestätigtem Budget; tatsächlicher ESP-NOW-Funkverbrauch und Optik müssen
gemessen werden. Kein stiller BLE-only-/Wi-Fi-aus-Architekturwechsel.

Flache Akkuoptionen wurden gezielt auf Strom, Schutz, reale Packmaße
und Lieferbarkeit geprüft. Keine kleinere bestellreife Alternative mit
ausreichendem Stromnachweis lag vor. P1835C3 ist die kleinste derzeit unter
diesen geprüften beschaffbaren Optionen, kein bewiesenes globales Minimum.
Der [Mechanikentwurf](MECHANIK-ENTWURF.md) führt die konkrete Packung und
deren digitalen sowie offenen physischen Nachweise.

Nächster Schritt: den [konkreten Händlerwarenkorb](BESTELLUNG.md) mit
bestätigtem99,02-EUR-Bruttopreis, kostenlosem Express/DDP und tatsächlicher
Sofortversandanzeige zur Nutzerautorisierung vorlegen. Der Händler nennt
keinen verbindlichen Ankunftstag; diese Grenze ausdrücklich mitgeben. Nach
Lieferung Revision/Polung/Federhub, USB-/Versorgungsverhalten, Firmware,
Funk-/Optik-/Wärmeprüfung und vier Stunden Betrieb beider Geräte abnehmen.

## Externe Angebotsvorbereitung

Die lokale frühere R1-Bestückungsvorbereitung wird unter
`.tmp/naehe-glut-r1-assembly-superseded/` archiviert, nicht als Kompaktdatei
verwendet und nicht mit dem aktuellen Lieferumfang versioniert.
Der zuvor versuchte AISLER-Upload wurde
von der automatischen Freigabeprüfung blockiert: ausdrückliche Zustimmung
zur Übertragung der PCB-Konstruktionsdaten an den externen Fertiger nötig.
Kein erfolgreicher Upload, Angebot oder Kauf wird behauptet. Vor einem später
eventuell nötigen Upload erst die richtige kompakte Revision fertigstellen
und diese konkrete Datenübertragung freigeben lassen; keine Ausweichübertragung
am Gate vorbei. Für den gewählten Serienmodulweg ist dieser Upload nicht nötig.
