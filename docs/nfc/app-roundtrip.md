# Gemeinsamer NFC-Test vollständig in der App

Stand: 16. Oktober 2026. **Vorbereitete Erweiterung; noch kein Gerätenachweis.**
Auftrag: Tag an der Box erkennen und an die App melden, am Handy lesen und
beschreiben, an der Box Payload lesen und melden, mit der Box schreiben,
anschließend am Handy frisch lesen. Grundlage ist PR #47 bei
`33bf31bcd082ad62c14bd9ffaa2b547e17e919bc`. Dessen
[physischer Rundlauf](physical-tag-test-2026-10-16.md) verwendete USB-Aufträge;
er bestätigt nicht den neuen Bluetoothpfad.

## Bereitstellung vor dem gemeinsamen Lauf

- Neue App zuerst bereitstellen; danach die ausdrücklich gewählte Variante
  `larp-nfc-test` aus `firmware/led`. Die normale `larp-led-product` startet
  weiterhin keinen NFC-Worker. Alte Apps weisen das neue NFC-Fähigkeitsbit ab.
- Vor Installation Chip und vollständigen privaten Flash-Rückweg gemäß
  [bestehendem Ablauf](test-preparation.md#vorbereitung-und-rückweg) erneut
  abgleichen. Diese Vorbereitung hat keinen Upload ausgeführt. Nur die zum
  vorhandenen Partitionslayout passende Anwendung ersetzen; Schlüssel erhalten.
  Die neue Variante nutzt wie die LED-/Wake-Basis Arduino und deren bestehende
  NVS-Initialisierung; die NVS-Freiheit der separaten USB-Probe gilt hier nicht.
- NFC-Modul verwendet den bestehenden geprüften Software-SPI-Pfad mit
  SCK16/MISO17/MOSI23/SS4. Kein Pinwechsel. IRQ/Reset werden nicht verwendet.
- Ein entbehrlicher, beschreibbarer, bereits NDEF-formatierter NTAG213/215/216,
  NFC-Handy und eingerichteter Boxzugang. Für diese Runde kein HCE-Testtag.
  Neue APK-/Firmwarehashes, Codecommit und tatsächlich installierte Versionen
  im Ergebnisbericht festhalten. Frühere Geräteergebnisse bleiben getrennt.
- In **ESP** verbinden und Zugang bestätigen, LEDs stoppen. **NFC-Tags →
  Echter Tag** öffnen. Die Bluetoothverbindung bleibt auf dieser Seite bestehen.
  Handy und Box bleiben beim Tagkontakt räumlich getrennt: immer nur ein Reader
  am Tag. Boxaufträge werden ausschließlich durch die App ausgelöst.

## Sieben Schritte

| Nr. | Gemeinsame Handlung | Erwarteter frischer Nachweis | Ergebnis |
| --- | --- | --- | --- |
| 1 | Tag auf die Box; in der App **Tag an der Box lesen** | App zeigt die von der Box gelesene Hardware-UID. Auch bei leerem/noch nicht unterstütztem Inhalt zählt dies zunächst nur als Erkennung. | offen |
| 2 | Boxmeldung in der App prüfen und UID notieren | Authentifizierte Antwort des aktuellen Boxauftrags; kein Erfolg aus einer Bluetooth-Schreibquittung. | offen |
| 3 | Tag weg von der Box, am Handy lesen | Handy liest dieselbe UID, NDEF-Kapazität und aktuellen Inhalt. | offen |
| 4 | **NFC-Testdaten einsetzen**, **Schreiben vorbereiten**, bestätigen, denselben Tag neu anhalten | Kennwert 20261015, Flags 1, Zeitwert 0, Besuch 1/1/0; vollständige App-Schreibung frisch bestätigt. | offen |
| 5 | Tag zur Box, **Tag an der Box lesen** | Dieselbe UID und vollständige Testpayload mit Flags 1/korrekter CRC kommen in der App an. | offen |
| 6 | **Mit Box Flags 2 schreiben …**, Ziel prüfen, **Einmal schreiben** | Box prüft erwartete UID und Vorherinhalt, schreibt einmal, liest vollständig zurück und meldet Flags 2/CRC. | offen |
| 7 | Tag aus dem Boxfeld nehmen und am Handy ausschließlich lesen | Neue reine Lesung nach der Boxantwort, dieselbe UID, exakte Nachher-Fixture und gültige CRC; erst dann zeigt die App die Rundlaufbestätigung. | offen |

Payloads und unveränderte Felder stehen im
[bestehenden Testplan](test-preparation.md#fester-datensatz-und-erwartete-änderung).
Die Box schreibt weiterhin ausschließlich die feste Änderung Flags 1 → 2.
Beliebige Nutzdaten oder Spielaktionen sind damit nicht freigegeben.
Bluetooth überträgt die 25-Byte-Testpayload und die Tagkennung; der vollständige
NDEF-Speicher wird im NFC-Kern gelesen, beim Schreiben erhalten und verglichen.
Ein fremder/mehrdeutiger Record erzeugt keine passende Testpayload in der App.

## Zusätzliche Abnahmen gegenüber PR #47

1. Nach Schritt 5 Tag A durch Tag B ersetzen: Schreiben ablehnen, B unverändert.
   Danach A erneut lesen, neuen bewussten Auftrag vorbereiten.
2. Bluetooth vor bzw. während eines Auftrags trennen: keine automatische
   Wiederholung. Nach Schreibbeginn bleibt der Ausgang unklar, bis der Tag
   erneut gelesen wurde. Ein bereits gesendetes PN532-Kommando lässt sich nicht
   rückwirkend abbrechen; der Worker prüft Abbruch vor jedem weiteren Kommando.
   Einen physischen Schreibabbruch zunächst nicht absichtlich provozieren;
   synthetische Fehlerfälle und ein entbehrlicher Tag sind Voraussetzung.
3. Alter Appinhalt darf Schritt 7 nicht bestehen lassen. Ebenso wenig genügt
   eine bloße App-Schreibbestätigung oder eine Journalsauflösung.
4. Nach Flags 2 ist die Box-Schreibschaltfläche gesperrt. Einen zweiten Rundlauf
   bewusst mit App-Testdaten Flags 1 beginnen. Kein automatischer Reset.
5. Ohne NFC-Testfirmware bleibt die Box-NFC-Bedienung gesperrt. LED-/Wake-Werte
   bleiben unabhängig; während eines NFC-Auftrags keine LED-Starts.

Für jeden Lauf Datum, App-/Firmwarehash, Boxidentität lokal, Auftrag,
Vorher-/Nachherwerte, getrennte Box-/Handyantworten und Nutzerbeobachtung
festhalten. Status: **bestanden / fehlgeschlagen / unklar / nicht durchgeführt**.
Keine privaten UID-Rohlogs, Flashabzüge oder Zugangsdaten veröffentlichen.
Nach dem Lauf Verbindung beenden; der NFC-Worker schaltet das Feld nach jedem
Auftrag aus. Bei fehlender bestätigter Feld-Aus-Antwort gilt kein Erfolg.
Den zuvor gesicherten Stand bei Bedarf separat wiederherstellen und verifizieren.

## Softwareprüfung

Die Code- und Testergebnisse dieser Erweiterung stehen im zugehörigen PR.
Reproduzierbare Aufrufe:

```powershell
python firmware/nfc-probe/tests/run_tests.py
python test/test_led_ble_wire/run.py
python firmware/led/tests/run_output_tests.py
python -m platformio run -d firmware/led -e larp-led-product -e larp-nfc-test -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug
```

Hosttests vergleichen echte HMACs mit unabhängigen Python-Fixtures, prüfen
Sitzungsbindung, Fristen, laufende und abgeschlossene Duplikate sowie
UID-Bindung vor dem ersten Schreibkommando. Sie ersetzen keine Funkabnahme
des neuen gleichzeitigen Bluetooth-/PN532-Betriebs oder Abbruchmessung am Gerät.

## Begrenzter Bluetoothvertrag

Handshake, Schlüsselspeicher und Fragmentierung bleiben beim
[LED-/Wake-Vertrag](../led/ble-wire-v1.md). Im authentifizierten Statusbyte 37
kennzeichnet Bit `0x20` die NFC-Testfähigkeit; Bit `0x10` bleibt reserviert.
Die LED-Auftragsfolge und die NFC-Auftragsfolge sind unabhängig.

NFC-Befehl: 70 Byte, `01 04`, Sitzungskennung (16), Request-ID (uint32 LE),
Ablaufzeit in Boxmillisekunden (uint32 LE), Operation (1 lesen/2 ändern),
UID-Länge (0/7), UID-Feld (10, Rest null), HMAC-SHA256 (32).
HMAC-Domäne: ASCII `LARP-NFC/1/command` vor den ersten 38 Bytes.
Zulassung verlangt höchstens 3000 ms Restfrist und die nächste Request-ID.
Die unveränderte Fixture muss im Schreibfall frisch vorliegen; die UID
muss zum bestätigten Ziel gehören. Es gibt keine automatische Auftragswiederholung.

NFC-Antwort: 105 Byte, `01 85`, Sitzung (16), Request-ID, nächste Request-ID,
Antwortsequenz, Boxmillisekunden (je uint32 LE), Ergebnis (1), Tagtyp (1),
UID-Länge (1), UID-Feld (10), Payloadlänge (0/25), Payloadfeld (25), HMAC (32).
HMAC-Domäne: ASCII `LARP-NFC/1/result` vor den ersten 73 Bytes.
Tagtypen und Ergebnisse 0–8 entsprechen den expliziten Enums im NFC-Kern;
9 nicht verfügbar, 10 beschäftigt, 11 abgelaufen, 12 Konflikt, 13 falsche Folge.
Nullgefüllte Restfelder sind keine Payload. Nur Ergebnis 8 mit passender neuer
Payload ist eine bestätigte Boxschreibung; keine Antwort/Timeout ist kein Erfolg.
UID und CRC sind weiterhin kein Authentizitätsnachweis des Tags.
