# ESP-NOW: begrenzter Zwei-Geräte-Funknachweis

Stand: **18. Oktober 2026**. **Status: ausgeführt und bestanden für den unten
begrenzten Umfang; vorheriger Box-Flash vollständig verifiziert wiederhergestellt.**

Geprüfter Firmware-/Testquellstand:
`134f379d3d46abc7bb9167be4f8b947cf535e919`,
[PR #65](https://github.com/Sven401/Larp-Escape/pull/65).
Auftrag nach vorbereiteter Prüfung: „installiere die Prüffirmware vorübergehend
auf beide ESPs, führe den ESP-NOW-Test durch und stelle danach die vorherige
Box-Firmware wieder her. Prüfe anschließend die vollständige Wiederherstellung.“
Die frühere automatische Uploadsperre ist durch diese ausdrückliche
Nutzerfreigabe aufgehoben. [Historische Vorbereitung](preparation-2026-10-18.md).

## Zuordnung und Installation

Box und Test-ESP wurden nach der USB-Seriennummeränderung gleichzeitig auf
COM3 und COM4 erkannt. Frische Chip-MAC-Abfragen bestätigten beide Rollen.
Beide sind ESP32-D0WD-V3, Revision 3, 4 MiB. Die tatsächlichen Kennungen stehen
in den privaten Belegen und sind keine öffentlichen Produktkonstanten.

Beide vollständigen Vorab-Sicherungen wurden separat am jeweils identifizierten
Chip mit `verify_flash` bestätigt. Box-MAC, Partitionstabelle und Schlüsselregion
wurden zusätzlich mit den erhaltenen chipgebundenen V2-NFC-Belegen abgeglichen.

Auf beiden ESPs wurde ausschließlich die Anwendung bei `0x10000` geschrieben.
Kein vollständiges Löschen, kein neuer Bootloader und keine neue Partitionstabelle.
Upload-Hashprüfung und jeweils separate Anwendungsverifikation erfolgreich.
Beide Starts meldeten die erwartete STA-MAC und Kanal 1.

| Artefakt | SHA-256 |
|---|---|
| Auf beiden Geräten installierte Prüffirmware | `8f9c2f20a0f7f5230d2ca15b88019b2c459b23aba6adbe778a43e4ba81c53a47` |
| Vollständige 4-MiB-Vorab-Sicherung der Box | `47e73b5295f44335eaeb98a84ca0fdcf8df63459f211d5bd483d3bb16678acda` |

Die SDK-Probe initialisiert kein NVS und setzt die WiFi-Speicherung auf RAM.
Bei Funkstart erscheint deshalb der SDK-Hinweis `esp_phy_load_cal_data_from_nvs:
NVS has not been initialized`; anschließend wird die Funkbereitschaft erreicht.
Die nachfolgende vollständige Box-Verifikation bestätigt, dass ihre gespeicherten
Daten diesen Ablauf unverändert überstanden haben. Keine Panic-, Abort- oder
Brownout-Marker im ausgewerteten Testlauf.

## Ausgeführte Prüfungen

Quelle: tatsächlicher Lauf von
[`run_device_test.py`](../../firmware/espnow-probe/run_device_test.py),
private `test-01/serial.jsonl` und `test-01/report.json`; kein simuliertes Ergebnis.
Ein Transfer verwendet den unveränderten Produktionssender für exakt 32 Byte
`ACTIVATE` mit Nullauffüllung. Der Empfänger prüft Quelle, Länge und sämtliche
Bytes. Seine nur für diese Probe definierte Rückantwort trägt Bootkennung und
Empfangszähler; das Hostwerkzeug gleicht diese mit beiden USB-Protokollen ab.

| Fall | Tatsächlicher Nachweis | Ergebnis |
|---|---|---|
| Ruhezustand | Zwei Sekunden nach Peer-Einrichtung keine automatische Aktivierung | bestanden |
| Box → Test-ESP | Zehn einzelne Sendungen, je exakter Empfang und passende Rückbestätigung | 10/10 |
| Test-ESP → Box | Zehn einzelne Sendungen, je exakter Empfang und passende Rückbestätigung | 10/10 |
| Empfänger weist Quell-MAC ab | Link-ACK erfolgreich; Empfänger-Verwerfungszähler +1, kein akzeptiertes ACTIVATE und keine Anwendungsbestätigung | bestanden |
| Nach Wiederfreigabe der Quelle | Ein weiterer vollständiger Transfer Box → Test-ESP | 1/1 |
| Empfängerradio ausgeschaltet | Lokale Sendeannahme, danach fehlgeschlagene Linkzustellung und keine Anwendungsbestätigung | bestanden |
| Empfängerradio wieder eingeschaltet | Ein vollständiger Transfer Box → Test-ESP | 1/1 |
| Test-ESP neu gestartet | Neue Bootkennung; Peer neu eingerichtet; drei Transfers je Richtung | 6/6 |
| Box neu gestartet | Neue Bootkennung; Peer neu eingerichtet; drei Transfers je Richtung | 6/6 |
| Callbackqueues | Statusprüfungen auch vor dem Löschen der Zähler durch Neustarts; keine Drops | bestanden |

**Gesamt: 34/34 positive Transfers mit Empfänger- und Rückantwortnachweis;
zwei zusätzliche beabsichtigte negative Sendefälle wie erwartet.**
Keine automatische Wiederholung nach unklarem Ausgang. Die absichtlichen
Neustarts sind Teile des Tests, keine beobachteten Abstürze.

## Vollständiger Rückweg der Box

Nach dem Test wurde die Box anhand ihrer Chip-MAC erneut bestätigt und die
gesicherte vorherige Anwendung von `0x10000` bis `0x310000` zurückgeschrieben.
Anschließend wurde **der gesamte 4-MiB-Flash**, einschließlich Bootloader,
Partitionstabelle, Anwendung, NVS, `larpkeys` und übrigen Regionen, gegen den
frischen Vorab-Abzug geprüft:

```text
Verifying 0x400000 (4194304) bytes @ 0x00000000 ...
-- verify OK (digest matched)
```

Der anschließende sechssekündige Startmitschnitt enthält die erwartete Meldung
`LARP LED-only ready; USB provisioning on explicit request` und keinen
beobachteten Panic-, Abort- oder Brownout-Marker. Keine erneute Handy- oder
physische LED-/Audio-/Matrix-/NFC-Funktionsabnahme aus dieser Meldung ableiten.

**Endzustand:** Box auf ihrer vorherigen Firmware, vollständiger Flashabgleich
erfolgreich. Der frei verwendbare Test-ESP bleibt auf der Prüffirmware; ohne
USB-Auftrag sendet sie kein ACTIVATE. Die geänderte CP2102-USB-Seriennummer
bleibt erhalten. Die ursprüngliche Test-ESP-Firmware ist zusätzlich gesichert.

Private Rohbelege und editierbare Ausführungsskripte bleiben im Hauptcheckout
unter `private/usb-serial-20261018/espnow-run/`: `box/` und `receiver/` mit
Identitäts-, Upload- und Verifikationslogs; `test-01/` mit den beiden laufenden
USB-Protokollen und strukturiertem Bericht; `cycle-01.json` bestätigt Installation,
bestandenen Test und verifizierte Wiederherstellung. Keine Schlüssel oder
Vollflash-Abbilder im PR.

## Grenzen und nächster Schnitt

Dieser Versuch belegt den echten Produktionssender und beide Funkrichtungen
zwischen den zwei identifizierten ESPs im aktuellen Nahaufbau auf Kanal 1.
Er belegt ausdrücklich den Unterschied zwischen lokaler Sendeannahme,
MAC-Zustellung und tatsächlich geprüfter Empfängeranwendung.

Nicht geprüft: bestehender `EspNowRuntime` im gesamten Spiel, Handybedienung,
BLE-/ESP-NOW-Koexistenz, Reichweite, Dauerlast, mehrere Peers, Verschlüsselung,
Duplikatunterdrückung oder Exactly-once-Verarbeitung. Der nächste Produktschnitt
ist eine bewusst auslösbare ESP-NOW-Baugruppenbedienung im passenden App-/
Firmwarepaar mit sauber getrennten Sendestatus- und Empfangsbeobachtungen.
