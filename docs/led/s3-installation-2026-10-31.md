# Erster ESP32-S3: Sicherung und Testinstallation

Datum: **31. Oktober 2026**. Status: **ein S3 gesichert, installiert und per USB
erreichbar; Handy-/Sichtprüfung und Zwei-Stationen-Test offen**.
Quelle: ausdrücklicher Nutzerauftrag in Sitzung
`01a18f48-193f-7763-a3e4-9731b29c0086`: ESP32-S3 auf COM4 sichern und anschließend
vorbereitete Testfirmware für den LED-Test über die Handy-App flashen.

Installierte Quellen: Code `f7d73324551283275463fa155f22d123492f8f9f`,
Dokumentations-/Buildstand `02a823b112b7e3eb5796c36a7751a53c73153ea6`,
Ziel `larp-s3-espnow-test`, [PR #115](https://github.com/Sven401/Larp-Escape/pull/115).
Firmware unverändert; dieser Bericht erweitert ausschließlich den Gerätenachweis.
Verantwortlich für die Ausführung: Geräteintegration im Hauptagenten;
unabhängiger Belegprüfer: `s3_review`, ohne eigenen Portzugriff.
Die abschließende unabhängige Prüfung bestätigt Backuphash, Chipzuordnung,
Schreib-/Verifikationslogs, Testdateihashes und öffentliche USB-Antwort ohne
offene Beanstandung; die private Zugangsschlüsseldatei wurde dabei nicht geöffnet.

## Identität und geprüfter Rückweg

Frische esptool-4.5.1-Abfragen bestätigen **ESP32-S3 Revision v0.1**, MAC
`34:b7:da:59:77:e8`, **4 MiB Flash**, Quad-Flashkonfiguration. Sicherheitsflags
`0x00000000`; keine aktive Flashverschlüsselung oder Secure-Boot-Sperre im
ausgelesenen Status. Kein Forcieren und keine eFuse-/OTP-Änderung.
COM4 bezeichnet den Anschluss dieses Laufs, keine dauerhafte Geräteidentität.

Der vollständige ursprüngliche Flash wurde ab Adresse 0 mit **4.194.304 Byte**
gelesen und **vor jedem Schreibzugriff am selben Chip mit `verify_flash`
erfolgreich verglichen**. SHA-256:
`ef926a939b08a81894b2ad3e6228bd48855f8af8140aac1eb438940effb0cdf2`.

Private Belege im Arbeitsbereich:
`private/s3-stations-20261031/com4-34b7da5977e8/` mit Originalabbild,
Identitäts-/Verifikationslogs, Layoutauswertung, Testdateien, Hashmanifesten
und dokumentiertem Vollrückweg. Zusätzlich dauerhaft kopiert nach
`%LOCALAPPDATA%/LarpEscape/ESP32-Backups/s3-34b7da5977e8-20261031-004633/`;
die Prüfsumme der Kopie wurde nochmals kontrolliert.

Der Rückweg schreibt das unveränderte Originalvollabbild ausschließlich auf
diesen erneut identifizierten Chip ab 0, mit Flashparametern `keep`, und prüft
anschließend wieder den gesamten Flash. Er ist vorbereitet, **nicht ausgeführt**.
Eine Wiederherstellung würde auch den neu erzeugten Testgerätezugang entfernen.

## Tatsächlich geschriebener Umfang

Vor dem Upload wurde das unveränderte S3-Ziel erfolgreich neu gebaut und die
Adressbelegung aus der tatsächlichen PlatformIO-Uploaderkonfiguration gelesen:

| Adresse | Datei | Länge |
| --- | --- | ---: |
| `0x0000` | S3-Bootloader | 14.032 Byte |
| `0x8000` | Partitionstabelle | 3.072 Byte |
| `0xe000` | initiale OTA-Metadaten (`boot_app0.bin`) | 8.192 Byte |
| `0x10000` | Testanwendung | 1.253.936 Byte |

Testanwendung SHA-256:
`4ac7f3c684fb6afedb90a8b8fefe64ef6c1eca4e63ebcb186af9cd797003a3b0`.
Flashparameter: DIO, 80 MHz, 4 MiB. Alle vier Bereiche nach dem Schreiben
zusätzlich unabhängig mit `verify_flash` erfolgreich verglichen.

**Notwendige Initialisierung:** Im ursprünglichen Partitionslayout liegt
SPIFFS ab `0x310000` mit Größe `0xf0000`. Die neue Schlüsselpartition
`larpkeys` belegt davon `0x310000` bis `0x315fff` (24 KiB); darin waren 1.816
Bytes nicht gelöscht. Nach verifizierter Vollsicherung, erneutem MAC- und
Sicherheitsabgleich wurde **ausschließlich dieser Bereich** gezielt gelöscht,
damit die Testfirmware ihr neues Schlüssel-NVS anlegen kann. Dortiger alter
SPIFFS-Inhalt wird ersetzt und bleibt im Vollbackup erhalten. Kein vollständiges
`erase_flash`, keine pauschale Löschung anderer Datenpartitionen. COM3 blieb
unberührt.

Der erste separate Vergleich mit `--before no_reset` scheiterte beim Verbinden
an einem USB-Schreibtimeout. Die anschließende Wiederholung mit normalem Reset
war auf demselben Chip für **alle vier Bereiche erfolgreich**; kein erneuter
Firmwareupload dafür. Der Verbindungsfehler ist damit kein offener Bytevergleich.

## Start und Handy-Einrichtung

Nach dem Neustart wurde folgende öffentliche USB-Antwort empfangen:

```text
LARP S3-ZERO mac=34:B7:DA:59:77:E8 ledPin=21 pixels=1 order=RGB
```

In diesem Lauf blieb die Abfrage mit DTR ausgeschaltet ohne Antwort; mit
DTR eingeschaltet wurde der Status empfangen. Auch der anschließende explizite
USB-Zugangsexport mit eingeschaltetem DTR war erfolgreich und wurde gegen das
bestehende Importschema geprüft. Diese Beobachtung ist kein allgemeiner
Nachweis für alle USB-Treiber oder Boardrevisionen.

Die Zugangsschlüssel wurden nicht ausgegeben oder versioniert. Die private
Importdatei liegt unter
`%LOCALAPPDATA%/LarpEscape/Provisioning/s3-34b7da5977e8-access-20261031.json`.
Sie wurde **noch nicht in die Handy-App importiert**. Der Import ersetzt dort
den bisher eingerichteten einen Boxzugang; die bisherige Box behält ihre eigene
Firmware und ihre eigenen Zugangsdaten.

Nächster Schritt: Datei auf das Handy übertragen, in der vorhandenen Companion-App
„Box-Zugang importieren“ wählen, den S3 verbinden und Funkeln starten, stoppen
und bewusst erneut starten. Danach Lichtimpuls und Ripple prüfen. Dabei
USB-/BLE-Antworten und tatsächliche Sichtbeobachtung getrennt dokumentieren.

**Nicht ausgeführt:** LED-Start oder optische Beobachtung, Handy-/BLE-Abnahme,
Zwei-S3-/RSSI-Lauf, Reichweiten-/Lastprüfung oder vollständiger Nähe-Glut-Effekt.
Es wurde nur dieses eine Gerät installiert. Weitere Testschritte stehen im
[S3-Prüfplan](esp32-s3-stations.md).
