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

## Supportbefund aus beauftragtem Audit · 24. Oktober 2026

Der spätere Nutzerauftrag zum [A1/A2-Pilot](../docs/quality/audits/2026-10-24-pilot/README.md)
erlaubte den Quellenabgleich, nicht die Umsetzung der Migration. Basis
`e61d7642`: lokal aufgelöst Arduino 2.0.14 / ESP-IDF **4.4.6**.
[Espressif AR2024-008 vom 02.07.2024](https://www.espressif.com/sites/default/files/advisory_downloads/AR2024-008%20End-of-Life%20Advisory%20for%20ESP-IDF%20v4.4%20Release%20Branch%20EN.pdf)
beendet den regulären Support der 4.4-Reihe seit Juli 2024, einschließlich
weiterer Fehler-/Securitykorrekturen. Hashpins ändern diesen Status nicht.

**Offen, zuständig Plattform und Systemintegration:** Für den nächsten
beauftragten Planungs-/Migrationsabgleich, spätestens vor Produktfreigabe
oder Pinänderung, eine ausdrückliche Weiterbenutzungsentscheidung mit Grund,
Verantwortlichem und Wiedervorlage oder die Einplanung der Migration vorbereiten.
Der Maintainer entscheidet technische Priorisierung im bestehenden Mandat;
grundlegende Produktentscheidungen bleiben beim Nutzer. Noch keine Ausnahme
beschlossen, kein neues Zieldatum und keine unterstützte Zielversion behauptet.
Die bisherige Festlegung, jetzt keine Pakete zu ändern, bleibt bestehen.

Konkrete Advisories mit einem Security-Fachreview auf tatsächliche Konfiguration,
Aufrufpfad und gepinnte Binaryversion prüfen. Eine EOL-Version, die in einer
Advisory-Matrix fehlt, ist nicht automatisch unbetroffen; alleinige BLE-Nutzung
der App schließt einen aktivierten BTDM-Controllerpfad nicht aus. Die offene
Classic-L2CAP-Frage ist im Pilot begrenzt beschrieben, keine bestätigte CVE-
Betroffenheit. Bei der späteren Zielwahl Supportstatus und private GATT-/Crypto-
ABI-Hooks neu auditieren; Hashes nicht ohne diesen Nachweis austauschen.

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
