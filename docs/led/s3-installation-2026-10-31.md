# Erster ESP32-S3: Sicherung und Testinstallation

Datum: **31. Oktober 2026**. Status: **ein S3 gesichert und installiert;
Funkeln-Start, Stop und erneuter Start über die Handy-App mit sichtbarer
Nutzerbestätigung belegt. Weitere Effekte und Zwei-Stationen-Test offen.**
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
gelesen und **vor dem ersten Schreibzugriff am selben Chip mit `verify_flash`
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
Der Zugang wurde in der unten beschriebenen Fortsetzung importiert. Die bisherige
Box behält ihre eigene Firmware und ihre eigenen Zugangsdaten.

## Fortsetzung: erster Handy-/LED-Gerätelauf

Auf Nutzerauftrag „weiter“ am selben Tag und nach ausdrücklicher USB-Freigabe
wurde das angeschlossene **moto g14 / Android 14** verwendet. Vorhandene App:
`de.larpescape.companion`, Version **0.2**, versionCode **2**. Kein APK-Update,
keine Neuinstallation und kein Löschen von Appdaten. Der vorhandene verschlüsselte
Zugangsblob wurde zuvor privat gesichert; sein Inhalt wurde nicht angezeigt.

Die S3-Datei wurde per USB in Downloads übertragen und über „Box-Zugang
importieren“ eingelesen. App bestätigt **LED-Box BB71E155**; die Kennung stimmt
mit der Geräteidentität des privaten S3-Exports überein. Der Import ersetzt den
einen aktiven Boxzugang dieses Handys. Die nicht mehr benötigte unverschlüsselte
Transportkopie wurde nach dem Import wieder aus Downloads entfernt, die lokale
private Originaldatei bleibt erhalten. Bluetooth wurde für den Test eingeschaltet.

Die App fand zwei LED-Stationen. Die zuerst gewählte andere Station wurde mit
„Die Box passt nicht zum eingerichteten Zugang“ abgewiesen; kein LED-Befehl wurde
an sie gesendet. Die zweite Station bestätigte den S3-Zugang und meldete
„LEDs bereit“ sowie „LEDs aus“.

| Prüfschritt über die App | Geräte-/Appantwort | Direkte Nutzerbeobachtung |
| --- | --- | --- |
| Funkeln starten | „Box bestätigt: Funkeln gestartet“, „Box meldet: Funkeln“ | „Sie leuchtet und verändert ihre Helligkeit“ |
| Stop | „Box bestätigt: LEDs aus“, „Box meldet: LEDs aus“ | „Ja, sie ist aus“ |
| Bewusst erneut starten | Erneut „Funkeln gestartet“ und „Box meldet: Funkeln“ | „Ja, Funkeln läuft wieder“ |

**Belegt:** Der begrenzte Bedienablauf Start → Stop → erneuter Start funktioniert
mit dieser App und der eingebauten LED dieses S3. Die sichtbare Wirkung ist
nutzerbestätigt; keine Kameramessung, Farb-/Helligkeitskalibrierung oder
zeitliche Effektmessung wurde ausgeführt. Endzustand: Funkeln läuft wieder,
Handy-App im LED-Bereich verbunden. Der Stop-Knopf bleibt verfügbar.

Private Belege: `private/s3-stations-20261031/phone/` mit öffentlichem UI-XML
für Import, abgewiesene andere Box, authentifizierten Ausgangszustand und
Start-/Stop-/Wiederstartbestätigungen, Appversionsmetadaten und datierten
Nutzerbeobachtungen. Zugangsdatei und vorheriger verschlüsselter Zugangsblob
werden nicht versioniert. Die unabhängige Qualitätsprüfung liest nur die
öffentlichen UI-/Exportmetadaten; sie ersetzt keine eigene Lichtbeobachtung.

**Weiter offen:** Lichtimpuls und Ripple, BLE-Abbruch-/Neustartfälle,
Zwei-S3-/RSSI-Lauf, Reichweiten-/Lastprüfung und vollständiger Nähe-Glut-Effekt.
Es wurde nur dieses eine S3-Gerät installiert. Weitere Testschritte stehen im
[S3-Prüfplan](esp32-s3-stations.md).
