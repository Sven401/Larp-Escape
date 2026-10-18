# Begrenzter ESP-NOW-Baugruppentest

**Begrenzter Gerätelauf bestanden:** 34 rückbestätigte Transfers und zwei
beabsichtigte negative Sendefälle. Die Box wurde anschließend vollständig
gegen ihre Vorab-Sicherung verifiziert wiederhergestellt.
[Gerätebericht vom 18. Oktober 2026](../../docs/espnow/device-test-2026-10-18.md).

Diese bewusst eigenständige Prüffirmware verwendet den unveränderten
[`EspNowCommunicator`](../../lib/EspNowCommunicator/EspNowCommunicator.cpp).
`SEND` sendet dessen tatsächliches 32-Byte-Paket: `ACTIVATE`, gefolgt von
Nullbytes. Sie startet weder Spiel noch BLE, Matrix, NFC, Audio oder LED-Ausgabe.
Die Handybedienung und Funkkoexistenz bleiben getrennte nächste Aufgaben.

## Build und Speichergrenzen

```powershell
python -m platformio run -d firmware/espnow-probe -j 2
```

SDK-Basis: Espressif32 6.6.0, Arduino ESP32 2.0.14, Paket 3.20014.231204.
Der vorhandene NFC-Probenfilter entfernt den Arduino-Start; der eigene SDK-Start
initialisiert kein NVS. WiFi läuft mit `nvs_enable=0` und `WIFI_STORAGE_RAM`.
Keine NVS-Reparatur oder automatische Formatierung; keine gespeicherten WLANdaten.
Die zwei Geräte nutzen STA, festen Kanal 1 und unverschlüsseltes Unicast.
Dies ist kein Sicherheits- oder Rollenvertrag für den späteren Spielbetrieb.

**Vor Installation:** ausdrückliche Freigabe für die jeweilige Firmwareersetzung,
beide Chip-MACs, Flashgröße und aktuelle Partitionen prüfen, vollständige private
Sicherungen erstellen und jeweils mit `verify_flash` am gleichen Chip bestätigen.
Den historischen chipgebundenen Rückweg der Box zusätzlich abgleichen.
Nur eine ausreichend große Anwendung bei `0x10000` ersetzen; keine pauschalen
PlatformIO-Uploads, die auch Bootloader/Partitionstabelle schreiben würden.
Die im Build erzeugte Tabelle ist kein Auftrag, bestehende Tabellen zu ändern.

Nach dem temporären Versuch die ursprüngliche Box-Anwendung wiederherstellen
und das vollständige Flash gegen die Vorab-Sicherung verifizieren. Private
Flashabbilder, Zugangsdaten und Gerätemitschnitte gehören nicht in Git.

## USB-Steuerung bei 115200 Baud

Beide Geräte erhalten dieselbe Firmware und zunächst den jeweiligen Gegenpeer.
Keine Aktivierung bei Boot, Peerwahl oder Statusabfrage.

| Befehl | Verhalten |
|---|---|
| `PEER aa:bb:cc:dd:ee:ff` | genau einen unverschlüsselten STA-Peer konfigurieren |
| `SEND` | einmal `GameMessage::Activate` über den echten Produktionssender |
| `STATUS` | Empfang, Rückbestätigung, Linkzustellung, verworfene Quellen, Queuedrops |
| `RADIO OFF` / `RADIO ON` | Gegenstelle vorübergehend unerreichbar machen / zurückholen |

Callbacks kopieren nur begrenzte Ereignisse in eine Queue mit 16 Einträgen;
alle Auswertung, Rücksendung und USB-Ausgabe läuft im Anwendungstask.
Ein akzeptiertes 32-Byte-ACTIVATE vom konfigurierten Peer erzeugt einen
`receive`-Datensatz und eine **nur für diese Probe** definierte Rückbestätigung:
32 Byte, `LARP-ACK` an Offset 0, LE32 Bootkennung an 8, LE32 Empfangszähler
an 12, Nullbytes 16–31. Ein ACK löst kein weiteres ACK aus.

`send.accepted` bestätigt die lokale Annahme, `delivery.ok` nur die MAC-Schicht.
Erst `receive` bestätigt die vollständige Prüfung beim Empfänger; `receipt`
belegt die Rückrichtung. Die ursprüngliche Spielnachricht enthält keine
Auftragsnummer. Deshalb korreliert das Hostwerkzeug kontrollierte, einzeln
gesendete Nachrichten über **beide** USB-Protokolle, Bootkennung und Zähler.
Dies ist keine garantierte Duplikatunterdrückung oder Exactly-once-Semantik.
Die Probe führt nach unklarem Ausgang keinen automatischen Sendeversuch aus.

## Wiederholbarer Gerätelauf

Erst nach separater Installation auf beiden bestätigten Geräten ausführen.
Ports und MACs frisch ablesen; COM-Nummern sind keine Geräteidentität.
`--output` muss ein neuer privater Ordner außerhalb versionierter Dateien sein.

```powershell
python firmware/espnow-probe/run_device_test.py --box COM3 --receiver COM4 --box-mac <BOX-MAC> --receiver-mac <TEST-ESP-MAC> --output <PRIVATER-NEUER-ORDNER>
```

Prüfablauf: zwei Sekunden Ruhezustand ohne Aktivierung, je zehn einzelne
Sendungen in beide Richtungen, erfolgreicher Link-ACK bei absichtlich
abgewiesener Absender-MAC ohne Anwendungsbestätigung, Gegenradio aus/an,
separate Neustarts beider ESPs und je drei Sendungen pro Richtung danach.
Gesamt: 34 positiv rückbestätigte Transfers plus zwei negative Sendefälle.
Der verlinkte Gerätebericht hält den tatsächlich ausgeführten Lauf fest;
ein neuer Aufruf benötigt seine eigenen Rohbelege.
Keine Warteschlangenverluste sind zulässig; Fehler und Teilergebnisse bleiben
in `serial.jsonl` und `report.json`. Das Hostwerkzeug führt keinen Upload aus.

API-Grundlage: [Espressif ESP-NOW, IDF 4.4.6](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/network/esp_now.html).
