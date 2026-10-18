# Notwendige zukünftige Arbeit: Arduino-ESP32 3.x

**Status: beschlossen und einzuplanen; Umsetzung offen, noch nicht begonnen.**
Datum: 18. Oktober 2026. Quelle: ausdrücklicher Nutzerauftrag in der
ESP-NOW-Sitzung `01a14c5f-f677-7403-a000-bf20e5ba93a3`: Aktualisierung als
zukünftige notwendige Projektarbeit dokumentieren und jetzt keinen weiteren
Aufwand in die Migration investieren. Kein Auftrag zur sofortigen Umsetzung,
zu einem Geräteupload oder zum Austausch der funktionierenden Installation.

## Anlass und Ausgangsbasis

Die ESP-Firmware soll künftig auf **Arduino-ESP32 3.x mit der dazu passenden
ESP-IDF-5.x-Basis** aktualisiert werden. Konkrete Zielversion, kompatible
PlatformIO-Pakete, Termin und Aufwand werden erst bei Wiederaufnahme bestimmt.

Geprüfte Ausgangsbasis: ESP-NOW-Arbeitsbranch bei `9493bf2`,
`firmware/led/platformio.ini` und `firmware/espnow-probe/platformio.ini`:
Platform `espressif32@6.6.0`, Arduino-Paket `3.20014.231204` (Arduino-ESP32
2.0.14, ESP-IDF 4.4). Empfang in `firmware/led/src/EspNowRadio.cpp` verwendet
die ältere Schnittstelle ohne Empfangsmetadaten; der LED-Adapter
`firmware/led/src/RmtLedBus.cpp` verwendet die ältere SDK-RMT-Schnittstelle.
Prüfweg: Konfiguration und diese Quellen am 18. Oktober 2026 gelesen.

Motivation ist eine langfristig passende Frameworkbasis und insbesondere der
direkte Zugriff auf paketbezogene ESP-NOW-Signalstärke (RSSI) für die geplante
Näheanzeige und mögliche näheabhängige Funkelintensität. RSSI bleibt eine
Näherung; die Aktualisierung allein schafft keine genaue Entfernungsmessung.

## Vorgemerkter Umfang und Abschlusskriterien

- Unterstützte, reproduzierbar gepinnte Buildumgebung auswählen; Migration
  zuerst getrennt vom funktionierenden Geräte-/Firmwarestand erproben.
- ESP-NOW-Empfang auf die neue Metadatenschnittstelle anpassen; LED-RMT,
  BLE-Schnittstellen und weitere tatsächlich betroffene Adapter prüfen und
  nötigenfalls migrieren. Keine pauschale Annahme unveränderter Kompatibilität.
- Betroffene Firmwarevarianten bauen und passende Host-/Protokolltests
  ausführen. App-Vertrag, Authentifizierung und vorhandene Schlüssel erhalten.
- Vor später ausdrücklich beauftragten Uploads chipgebundene Rückwege sichern;
  danach BLE-/ESP-NOW-Koexistenz, App-Zugang sowie LED, Audio, NFC und Matrix
  am jeweiligen Gerät erneut prüfen. Ergebnisse je Baugruppe getrennt belegen.

Die eigentliche RSSI-Anzeige, Kalibrierung und Effektsteuerung sind gesonderte
Funktionsarbeit; deren konkrete Ausgestaltung ist mit diesem Plan nicht
beschlossen. Für die Migration jetzt keine Pakete aktualisieren, Prototypen
erstellen oder zusätzliche Untersuchungen beginnen.

## Bereits geprüfte Referenzen

- [Espressif: Migration Arduino-ESP32 2.x → 3.0](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html)
  beschreibt den Wechsel von ESP-IDF 4.4 auf 5.1 und inkompatible Änderungen.
- [Espressif: RSSI bei ESP-NOW](https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html#how-do-i-obtain-the-corresponding-rssi-when-transferring-data-using-esp-now)
  beschreibt direkten Metadatenzugriff in neueren Versionen und den bisherigen
  Mitschnittweg. Referenzen am 18. Oktober 2026 geprüft; Zielversionen bei
  Wiederaufnahme erneut bewerten.
