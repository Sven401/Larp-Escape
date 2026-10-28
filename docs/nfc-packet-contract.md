# NFC-Paketvertrag und gepinnte Korrektur

Stand: 6. Oktober 2026. Scope: der aktive UID-Lesepfad mit Adafruit PN532 **1.3.4**, keine Freigabe aller Bibliotheksmethoden und kein Hardwaretest. Die Produktion verwendet Software-SPI; das Profilmodell beschreibt ebenfalls NFC/SPI. Reale profilabhängige NFC-Besitzer fehlen noch.

**Ergänzung 24.10.2026:** Der [A1/A2-Pilot](quality/audits/2026-10-24-pilot/README.md)
ergänzt ausführbare Parser- und Patchschutztests sowie die Grenze des SPI-Bools.
Historische Prüfaussagen vom 6. Oktober behalten ihren Datum-/Commitbezug.

## Bestätigter ursprünglicher Befund

Die unveränderte Bibliothek liest in `readDetectedPassiveTargetID` zunächst 20 Byte in den globalen 64-Byte-Puffer und kopiert danach `packetbuffer[12]` UID-Bytes ab Index 13. Gemeldete Längen über 7 lesen alte, nicht neu gelesene Bytes; über 51 lesen außerhalb des Puffers. Header, deklarierte Framelänge und Prüfsumme werden an dieser Stelle nicht geprüft. Der 256-Byte-Zielpuffer im NFCReader schützt dessen Zielbereich, behebt diesen Quellbereichsbefund jedoch nicht.

Der ACK-/Ready-Mechanismus vor dieser Funktion ist kein Nachweis einer vollständigen gültigen Antwort. Bei einer Sieben-Byte-UID liegt der UID-Inhalt in Byte 13..19; Prüfsumme und Paketende befinden sich dahinter. Die ursprünglichen 20 Bytes reichen für deren Prüfung nicht.

Primärquelle: [Adafruit-PN532-Quellcode, Tag 1.3.4](https://github.com/adafruit/Adafruit-PN532/blob/1.3.4/Adafruit_PN532.cpp), zusätzlich die lokal über PlatformIO installierte Fassung mit `library.properties` Version 1.3.4. SHA256 des vollständigen CPP-Texts nach CRLF→LF-Normalisierung: `9c6c93f6b1e4aac0f03394058c46673a0ff178e4c9fea0a80d0f9ceab53192f4`.

## Begrenzte Prüfung vor Kopie

Das [NXP-Handbuch UM0701-02](https://www.nxp.com/docs/en/user-guide/141520.pdf), Abschnitte 6.2.1.1 (Seite 28) und 7.3.5 (Seite 116), beschreibt normale Frames mit Länge, Längenprüfsumme, Richtung, Datenprüfsumme und Postamble sowie die Type-A-Antwort mit Zielanzahl, Zielreferenz, UID und optionaler ATS. Die folgenden Kapazitäts- und Ablehnungsregeln sind bewusste Grenzen dieser Implementierung.

`include/GameRuntime/Pn532PassiveFrame.h` enthält einen Arduino-freien Parser, der einen begrenzten Puffer und die Anzahl tatsächlich verfügbarer Bytes annimmt. Ergebnis: `InvalidFrame`, `NoTarget`, `UnsupportedUid` oder `Card`; UID ist eine Wertkopie von höchstens sieben Byte, Länge anfangs null.

- Normaler Header mit Preamble/Startcode; LEN **3..57**, vollständige Antwort `LEN + 7` innerhalb des verfügbaren Puffers.
- Längen- und Datenprüfsumme, Richtung D5, Antwortcode 4B und Postamble geprüft, bevor UID-Felder gelesen werden.
- Null Ziele nur mit passender minimaler Antwort; ein Ziel mit Referenz 1. Mehrere Ziele, ACK/NACK, Fehler- und Extended-Frames werden abgewiesen.
- Akzeptierte UID-Längen **4 oder 7**. Ein gültiger Zehn-Byte-Ausweis ist hier ausdrücklich nicht unterstützt. Die Spielebene nimmt weiterhin ausschließlich sieben Byte an.
- UID vollständig innerhalb des deklarierten Datenbereichs. Eine vorhandene ATS muss einschließlich ihres Längenbytes genau den verbleibenden Bereich belegen. Keine Protokollinterpretation des ATS-Inhalts.
- Keine dynamische Allokation, keine geliehenen Datenzeiger. Erstdurchlauf höchstens 58 Prüfsummenbytes, danach höchstens sieben Kopierbytes; scheiternde Antworten lassen den Ergebniswert leer.

## Reproduzierbare Anwendung beim Build

`platformio.ini` bindet ausschließlich beim Produktionsziel `post:scripts/patch_pn532.py` ein. PlatformIO ruft das Skript nach der Abhängigkeitsauflösung und vor der Übersetzung auf. Es prüft den Bibliotheksnamen, Version 1.3.4 und den vollständigen normierten CPP-Hash. Die eigenständige, editierbare Ersetzung liegt in `scripts/patches/pn532-1.3.4/read_detected.cpp`.

Nur `readDetectedPassiveTargetID` wird ersetzt; der übrige Adafruit-Code und dessen Lizenz bleiben erhalten. Bereits gepatchte Dateien werden mit erwartetem Hash erneut geprüft. Unbekannte Originale, fremde Änderungen, beschädigte Originalkopien und unerwartete Patchänderungen brechen den Build sichtbar ab. Keine automatischen Downloads oder Änderungen an globalen PlatformIO-Paketen.

Die Ersetzung liest einen lokalen initialisierten 64-Byte-Puffer. SPI übernimmt den Rückgabewert der BusIO-Übertragung; I2C prüft zusätzlich das Ready-Byte. HSU liest zuerst fünf Headerbytes und dann die exakte begrenzte Restlänge; dadurch wird eine kurze gültige Antwort nicht als auf 64 Byte gepolstert vorausgesetzt. Bestehende Stream-Zeitlimits gelten weiter. Erst nach vollständiger Parserfreigabe werden UID und Länge publiziert. Nullzeiger werden abgewiesen und die Länge bei lesbarer Längenadresse zunächst auf null gesetzt.

Für die gepinnte BusIO-Version 1.17.4 liefert
[`write_then_read` auf SPI stets true](https://github.com/adafruit/Adafruit_BusIO/blob/1.17.4/Adafruit_SPIDevice.cpp#L403).
Die Bool-Prüfung ist auf dem tatsächlichen Software-SPI-Pfad keine
Gerätequittung. ACK/Ready und vollständige Framevalidierung liefern andere,
getrennt zu prüfende Aussagen. I2C-/HSU-Fehlerkanäle sind gesondert zu testen.

Die ursprüngliche öffentliche API besitzt weiterhin keinen Parameter für die Zielkapazität und setzt einen Aufruferpuffer von sieben Byte voraus. Der NFCReader behält deshalb zusätzlich seinen bisherigen defensiven 256-Byte-Puffer. Die boolesche Bibliotheks-/Legacy-Portantwort unterscheidet noch nicht zwischen keinem Ausweis und Kommunikationsfehler; ein neuer profilabhängiger Besitzer benötigt diesen ausdrücklichen Verfügbarkeitsvertrag.

Die originale Datei wird vor Erständerung als `_larp_pn532_original_1_3_4.txt` im ignorierten, umgebungseigenen Abhängigkeitsordner abgelegt, nach geprüftem Hash. Gepatchter CPP-Hash: `dff799bcb2237543bcfa875a28489b85da00c9ca8193e93f10fd734ca1a04bc3`. Der Patchhelper berücksichtigt Zeilenenden; ohne passende Originalkopie wird eine vorhandene Änderung nicht als geprüft anerkannt.

Lokaler Rückweg, wenn ausdrücklich gewünscht:

```powershell
python scripts/patch_pn532.py ".pio/libdeps/az-delivery-devkit-v4/Adafruit PN532" --restore
```

Der nächste normale Build wendet die konfigurierte Korrektur wieder an. Ein dauerhafter Rückweg ist das Zurücknehmen dieses Projektschnitts; die dabei wieder geöffnete UID-Lücke bleibt dann sichtbar. Eine Änderung der Bibliotheksversion oder des Ersetzungstexts braucht einen frischen Quellenaudit und aktualisierte geprüfte Hashes.

## Verbleibende Grenzen

- Die Antwort liest über SPI/I2C bis zur festen Grenze, auch wenn der tatsächliche Frame kürzer ist. Funktion, Zeitbedarf und Verhalten verschiedener PN532-Boards sind am realen Aufbau noch zu prüfen. Längere Antworten werden nicht unterstützt; Abbruch/Resynchronisation sind Teil des zukünftigen echten Besitzers.
- Software-SPI, I2C und HSU-Zweige sind übersetzt, keine davon in diesem Schnitt praktisch am Leser geprüft. HSU-/I2C-Profiltypen sind nicht dadurch eingeführt. Bestehende ACK-/Ready-Wartezeiten sowie SAM-/Target-Moduswechsel sind unverändert und keine 100-ms-End-to-End-Garantie.
- Andere Bibliotheksmethoden, insbesondere Daten-/Target-/Authentifizierungsfunktionen, wurden nicht umfassend gehärtet. Der globale Bibliothekspuffer bleibt für diese Funktionen gemeinsam; alle PN532-Instanzen müssen weiterhin einem eindeutigen Worker gehören.
- Sichere physische Stilllegung, Treiberressourcen/Lebensdauer, RF-Verhalten, Fehlerdiagnose, Sample-Alter, Recovery und Profilwechsel mit NFC sind offen. Bestehendes `AsTarget` ist keine bestätigte Park-Abnahme.
- Die historische Lücke bei Parser-Randfällen und Patchschutz wird durch die
  [Pilotprüfungen](quality/audits/2026-10-24-pilot/pruefungen.md) bearbeitet:
  15 neue Parserfälle und 10 Patchhelpertests. Synthetische Schutztests,
  echte Quellhashprüfung und Bus-/Geräteabnahme bleiben getrennte Nachweise.
  Tatsächliche UID-Karten, gepatchter Transportadapter und Zeitverhalten sind
  damit weiterhin nicht geprüft. Patchtests beweisen keine atomare Änderung
  bei konkurrierenden Schreibern; Bibliotheksverzeichnis nur exklusiv bearbeiten.


## Beobachteter Prüfstand

6. Oktober 2026: `pio run -e az-delivery-devkit-v4 -j 2` erfolgreich; erster Build meldet `applied`, zweiter regulärer inkrementeller Build `verified`. Tatsächlicher normierter Quell- und Originalkopiehash entsprechen den oben genannten Werten. Produktions-RAM **55.148 Byte**, Flash **1.226.877 / 1.310.720 Byte (93,6 %)**. Der Gesamtpfad für Profilübernahme ist weiterhin nicht in den Produktionscontroller eingebunden. Keine neue Firmware auf das Board geladen.

Architektur **64 Dateien / 152 direkte Includes**, **43** isolierte reine C++11-Header, Format **109 Dateien** mit clang-format 19.1.7; Cppcheck 2.1 `warning,performance,portability` für den Parser mit ausdrücklich gewähltem C++ ohne Befund, Python-Syntaxanalyse des Patchhelpers erfolgreich, `git diff --check` ohne Befund. Der erste Cppcheck-Aufruf interpretierte den Header als C und scheiterte; mit `--language=c++` korrigiert. Frameworkwarnungen bleiben sichtbar. Keine Parserausführung, Fehler-/Rollbackprüfung, Unity-Suite, Remote-CI oder Geräteabnahme.
