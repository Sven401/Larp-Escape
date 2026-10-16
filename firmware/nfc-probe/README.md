# Begrenzte NFC-Prüffirmware

Stand: 15. Oktober 2026. Eigenständige Vorbereitung für den Test
App → NFC-Tag → Box → NFC-Tag → App. Keine Spiel-, LED-, Wake- oder BLE-Funktion.
Ein Hosttest oder erfolgreicher Build ist kein RF- oder Tag-Schreibnachweis.
Gemeinsamer Ablauf: [NFC-Testvorbereitung](../../docs/nfc/test-preparation.md).

Die Probe verwendet den dokumentierten PN532-Software-SPI-Kandidaten
SCK16/MISO17/MOSI23/SS4, Modus 0, LSB zuerst. IRQ/Reset werden von dieser Probe
nicht benutzt; ihre reale Beschaltung wird nicht vorausgesetzt. GPIO34 wird
nicht angesteuert. Der eigene Transport prüft ACK, normalen Header,
Länge, Längenchecksumme, D5-Richtung, erwarteten Antwortcode, Datenchecksumme und
Postamble. Extended Frames, Fortsetzungs-/NAD-Antworten und abgeschnittene Daten
werden abgewiesen. Pro Kommando höchstens 1 s ACK- und 2 s Antwortwartezeit;
die komplette Operation hat ein 20-s-Budget, das vor jedem Kommando geprüft wird.
Ein bereits begonnenes Kommando darf dieses Budget um seine begrenzte Laufzeit
überschreiten. Keine automatischen Schreibwiederholungen.

GPIO-Konfiguration setzt den IOMUX über `gpio_config` explizit. Vor jedem
SPI-Schreibrahmen bleibt NSS mindestens 2 ms ohne Takt niedrig und während
des anschließenden Rahmens weiter aktiv. Die Bereitschaft wertet das definierte
RDY-Bit 0 aus. `NFC SPI` meldet den letzten Befehl, Handshakephase, tatsächliches
Statusbyte und Abfragezahl; `NFC HEADER` bewahrt höchstens acht zuletzt gelesene
Headerbytes. Kein Statusbyte allein bestätigt eine Leserantwort.

## USB-Bedienung

UART0, 115200 Baud, ASCII, ein Befehl pro Zeile. Großschreibung ist verbindlich.
Boot startet weder einen NFC-Leseauftrag noch einen Schreibauftrag.

| Befehl | Wirkung |
| --- | --- |
| `INFO` | PN532-Version und begrenzte Readerkonfiguration prüfen; keine Tagdaten schreiben. |
| `READ` | Ein Type-A-Ziel aktivieren, UID/ATQA/SAK sowie unterstützten NDEF-Speicher frisch lesen. |
| `CHANGE` | Vorbereiteten Datensatz zweimal frisch lesen, vergleichen, Flags 1→2 schreiben, vollständig zurücklesen. |
| `OFF` | PN532-RF-Feld ausdrücklich ausschalten, Antwort validieren. Vor Rückkehr zur Wake-Firmware verwenden. |

`OFF` entwertet die Readerinitialisierung; das nächste `INFO`/`READ`/`CHANGE`
konfiguriert erneut und schaltet das RF-Feld ein. Ein ESP-Reset allein belegt
keinen abgeschalteten, separat versorgten PN532. `OFF OK` ist die bestätigte
Controllerantwort; eine unabhängige Feldstärkemessung ist damit nicht erfolgt.

Vor jedem ausdrücklichen `READ`/`CHANGE` schaltet die Probe das Feld einmal
bestätigt aus, wartet 20 ms, schaltet ein und wartet weitere 20 ms. So lässt sich
ein liegenbleibender Tag erneut aktivieren. Innerhalb der beiden Vorherlesungen,
des Schreibens und der Verifikation wird das Feld nicht absichtlich unterbrochen.

Jeder bekannte Befehl beginnt mit `NFC BEGIN <Befehl>` und endet genau einmal mit
`NFC END <Befehl> <Ergebnis>`. Unbekannte/zu lange Befehle ergeben
`NFC END COMMAND ERROR`. Maximal 31 Befehlszeichen; keine binären Kommandos.
`NFC NDEF offset=... hex=...` enthält höchstens 64 Datenbytes je Zeile.

`READ OK` bestätigt einen vollständigen Lesevorgang der unterstützten
NDEF-Abbildung. `NFC DATA test_record=1 flags=... prepared=... crc_checked=1`
bestätigt zusätzlich den engen Testdatensatz mit gültiger CRC. Fremde Inhalte
können gelesen werden, ohne als gültiger Testdatensatz zu gelten.

Ergebnisse: `OK`, `NO_TARGET`, `UNSUPPORTED`, `READ_ONLY`, `NOT_PREPARED`,
`CHANGED`, `ERROR`, `UNKNOWN`, `VERIFIED`. `CHANGED` bedeutet geänderten
Vorherinhalt vor dem ersten Schreibkommando. Ab dem ersten versuchten
Schreibkommando ergibt jeder Fehler `UNKNOWN`; der Auftrag wird niemals still
wiederholt. Nur eine frische bytegleiche vollständige Rücklesung ergibt
`CHANGE VERIFIED`. Das ist noch keine unabhängige Rücklesung durch das Handy.

## Enger Tag- und Datenvertrag

**Type 4:** ISO-DEP Type A, NDEF-AID `D2760000850101`, CC-Datei `E103`,
genau 15 Byte CC Version 2.0, eine NDEF-Datei `E104`. Maximal 1026 Byte
Dateikapazität inklusive NLEN, höchstens 1024 Byte NDEF. CC muss freies Lesen
und für CHANGE freies Schreiben erlauben. MLe/MLc werden berücksichtigt;
APDU-Blöcke bleiben höchstens 64 Byte. UPDATE setzt zuerst NLEN auf null,
schreibt den neuen Inhalt und veröffentlicht NLEN zuletzt.

**Type 2:** Nur NXP-GET_VERSION-Signaturen für NTAG213/215/216, SAK 0,
7-Byte-UID beginnend mit 04. GET_VERSION läuft über InCommunicateThru, da
InDataExchange `0x60` als MIFARE-Authentifizierung interpretiert. UID im Speicher
und Modell-Capability-Container müssen passen: 144/496/872 Byte Datenfläche.
READ verarbeitet eine kurze NDEF-TLV bis 254 Byte sowie NULL/Terminator und
beim NTAG213 die unveränderte Werks-Lock-TLV `01 03 A0 0C 34`.
Andere Lock-/Memory-Control-TLVs und Extended-Längen werden nicht unterstützt.

Schreiben setzt offene CC-Zugriffsrechte, alle statischen/dynamischen Lockbits
null, AUTH0=FF und deaktiviertes UID-/Counter-Mirroring voraus. Gesperrte oder
komplexe Konfigurationen werden nicht freigeschaltet. Alle Schreibadressen
bleiben innerhalb der CC-Nutzdatenfläche; UID, CC, Lock-, Konfigurations- und
Passwortseiten werden nie geschrieben. Erst wird die kurze TLV-Länge auf null
gesetzt, dann ausschließlich geänderte Nutzdatenseiten geschrieben, zuletzt
die ursprüngliche Länge wiederhergestellt. Schreibende Seiten übernehmen alle
übrigen Bytes unverändert aus der frischen Vorherlesung. Die anschließende
Verifikation liest die komplette CC-Nutzdatenfläche neu.

Das MIME-Record `application/vnd.yourapp.tagdata` muss genau einmal vorkommen.
Erforderlicher Payload: Version 1, `uidHash=20261015`, `flags=1`,
`timestampBinary=0`, genau ein Besuch `(nodeId=1, result=1, deltaMinutes=0)`
und gültige CRC32. Nur Flags und CRC ändern sich. Ungechunkte zusätzliche
NDEF-Records bleiben bytegleich erhalten; mehrdeutige/defekte Datensätze werden
abgewiesen. Record-IDs und lange NDEF-Record-Längen bleiben erhalten.

## Build und Prüfungen

```powershell
python firmware/nfc-probe/tests/run_tests.py
python -m platformio run -d firmware/nfc-probe -e nfc-probe -j 2
```

Der Codekern ist C++11 ohne Arduino-Abhängigkeit. Hosttests verwenden unabhängige
Payload-Goldenbytes und simulierte RF-Speicher für Type4 und alle drei NTAGs:
Lese-/Schreibfolge, fremde Records, Schutz-/Kapazitätsgrenzen, geänderte Vorherdaten,
Wiederholung, Schreibfehler/Teilabbruch und falsche Rücklesung. Zusätzlich werden
Framefehler/Abschneidung, UID/ATS, CRC und NDEF-Randfälle geprüft.

Am 15. Oktober 2026 am vorbereiteten Arbeitsbaum ausgeführt: 78 Transaktionsfälle
plus Frame-/Target-/CRC-/NDEF-Prüfungen bestanden; ESP32-Build erfolgreich,
25.172 Byte statisches RAM und 239.361 Byte Anwendungscode laut Buildbericht.
Die ELF-Symbolprüfung enthält den eigenen `app_main` und keine ausführbaren
`initArduino`-, `nvs_flash_init`-, `nvs_flash_erase`-, `nvs_set`- oder
`nvs_commit`-Funktionen. Das ist eine Softwareprüfung; Gerätebeobachtungen werden
im gemeinsamen Testbericht getrennt geführt.

Die SDK-Basis bleibt Espressif32 6.6.0 und Arduino ESP32 2.0.14. Ein lokales
Build-Middleware entfernt nur `cores/esp32/main.cpp` aus diesem Build; Dateien
im installierten SDK werden nicht geändert. Der eigene SDK-`app_main` verwendet
kein `initArduino` und keinen NVS-Initialisierungs-, Schreib- oder Löschaufruf.
Die Partitionstabelle wird direkt aus `firmware/led/partitions.csv` übernommen.
Vor einem beauftragten Upload müssen Chipidentität und vollständiger gesicherter
Flash-Rückweg abgeglichen werden; nur die Anwendung ab `0x10000` ersetzen.

Primärquellen: [PN532 UM0701-02](https://www.nxp.com/docs/en/user-guide/141520.pdf)
(Hostframes/SPI, InListPassiveTarget, InDataExchange/InCommunicateThru) und
[NTAG213/215/216 Datenblatt Rev. 3.2](https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf)
(Speicherkarte, CC, Lockbits, Konfiguration, GET_VERSION). Bestehender
[Payloadvertrag](../../companion/contract/nfc-v1/README.md) und Android-Codec
bleiben die Projektquelle für Nutzdaten und CRC.
