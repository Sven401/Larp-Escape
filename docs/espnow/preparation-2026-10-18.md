# ESP-NOW: Gerätezuordnung und vorbereiteter Funkversuch

Stand: **18. Oktober 2026**. Basis `d61f77fe753c1a837c13a67edb282d6a79610a30`.
**Status: USB-Zuordnung geprüft; Funkversuch vorbereitet, noch nicht installiert.**

## Bestätigte USB-Befunde

Quelle: einzeln vom Nutzer angeschlossene Geräte, frische esptool-MAC-Abfragen,
PySerial-Auflistung und Windows-PnP außerhalb der Sandbox am 18. Oktober 2026.
Beide ESP32-D0WD-V3 hatten zunächst unterschiedliche Chip-MACs, aber dieselbe
CP2102-USB-Seriennummer. Die USB-Kennung unterscheidet sich von der Chip-MAC.
COM3 allein war daher kein belastbarer Identitätsnachweis. Die frühere fehlende
Erkennung ist nicht abschließend allein diesem Konflikt zugeschrieben.

Auf ausdrücklichen Auftrag wurde beim allein angeschlossenen, anhand seiner
Chip-MAC bestätigten Test-ESP ausschließlich das Seriennummerfeld geändert.
Verwendet wurde die signierte Herstellerbibliothek aus dem vom Nutzer
heruntergeladenen AN721SW-Paket. Die Konfiguration wurde vorher als
Hersteller-HEX und als auslesbare Felder gesichert. Unabhängige Rücklesung:
neue Seriennummer gesetzt, übrige auslesbare Felder unverändert.

**Nach USB-Neuverbindung belegt:** Windows erkennt beide gleichzeitig mit
unterschiedlichen PnP-Instanzkennungen, Box auf COM3 und Test-ESP auf COM4.
Erneute Chip-MAC-Abfragen bestätigen beide Rollen. PySerial zeigt die neue
Seriennummer mit Bindestrich leer an; Windows-PnP liefert sie korrekt. Deshalb
weder ein leeres PySerial-Serienfeld noch eine COM-Nummer allein verwenden.
Die Chip-MACs und vollständigen privaten Rohbelege werden nicht veröffentlicht.

## Sicherung und vorbereiteter Softwarestand

Vor jedem vorgesehenen Firmwarewechsel wurden **beide vollständigen 4-MiB-
Flashabbilder frisch gelesen und jeweils am gleichen Chip mit `verify_flash`
bestätigt**. Box-Chipkennung, Partitionstabelle und Schlüsselpartition wurden
zusätzlich gegen die erhaltenen V2-NFC-Belege vom 17. Oktober abgeglichen.
Der vorhandene Box-Start meldet `LARP LED-only ready`; daraus allein folgt keine
vollständige Versions-/Apppaar-Abnahme.

Die neue [Funkprobe](../../firmware/espnow-probe/README.md) baut den echten
Produktionssender direkt aus dem Repository. Keine Kopie seiner Paketlogik.
ESP32-Build erfolgreich: RAM 41.588 Byte, Programm 681.201 Byte.
Ein vorhandener Frameworkhinweis in `esp32-hal-uart.c` bleibt sichtbar;
die eigenen Probenquellen wurden mit `-Werror` übersetzt. Das Python-
Gerätetestwerkzeug wurde auf Syntax geprüft. Noch keine Ausführung der
Testfälle und keine Aussage über tatsächliche Funkzustellung.

Private, editierbare Vorbereitung und Gerätebelege liegen im Hauptcheckout
unter dem ignorierten Ordner `private/usb-serial-20261018/`;
frische Chip-Sicherungen und Verifikationslogs unter dessen `espnow-run/`.
Herstellerpaket, ursprüngliche CP2102-Konfiguration sowie Ausführungsskripte
bleiben erhalten. Keine privaten Binärdateien oder Schlüssel im PR.

## Freigabetor und nächster Schritt

Die automatische Freigabeprüfung hat den Aufruf für den temporären Upload
**vor der Ausführung abgewiesen**: Der ESP-NOW-Testauftrag sei keine ausdrückliche
Zustimmung zur vorübergehenden Ersetzung der wichtigen Box-Firmware.
**Kein Upload auf Box oder Test-ESP erfolgt.**

Konkret vorbereitet ist: nur die Anwendung bei `0x10000` auf beiden eindeutig
identifizierten ESPs ersetzen, separat verifizieren, den in der README
beschriebenen begrenzten Zwei-Geräte-Funkversuch ausführen und anschließend
die vorherige Box-Anwendung zurückschreiben. Danach das vollständige Box-Flash
gegen die frische Vorab-Sicherung prüfen. Die Anwendung auf dem Test-ESP darf
laut Nutzer verworfen werden; dessen vollständige Sicherung bleibt dennoch
erhalten.

**Offen:** ausdrückliche Uploadfreigabe für diesen konkreten Ablauf,
Gerätelauf samt Fehlerfällen und Wiederherstellungsnachweis. Handybedienung,
BLE-/ESP-NOW-Koexistenz, Reichweite, Dauerlast, verschlüsselte Peers und
Spielintegration sind durch diese Probe nicht abgenommen.
