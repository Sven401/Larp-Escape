# Gemeinsamer NFC-Rundlauf vollständig in der App

**16. Oktober 2026, Europe/Berlin: ein vollständiger Rundlauf bestanden.**
Geprüfter App-/Firmwarecode: `dc746ec0116e660cd01c0f0f65f27babd715d9f5`,
[PR #52](https://github.com/Sven401/Larp-Escape/pull/52).
Der Nutzer bewegte denselben physischen Tag zwischen Box und Handy und
bestätigte die Appanzeigen. Boxlesen und Boxschreiben wurden ausschließlich
in der echten Android-App ausgelöst; keine USB-NFC-Aufträge.

## Installation und Rückweg

Das moto g14 wurde mit Datenerhalt aktualisiert. Vorherige APK und neue APK
haben dasselbe Signierzertifikat. Die installierte APK wurde vom Handy
zurückgelesen und bytegleich mit dem vorbereiteten Artefakt verglichen.

Vor dem Firmwarewechsel wurden Chipkennung und 4-MiB-Flash geprüft,
der aktuelle vollständige Flash neu gesichert und mit `verify_flash` am
selben Chip bestätigt. Die darin enthaltene Anwendung entsprach dem zuletzt
installierten [Audio-Paar aus PR #51](https://github.com/Sven401/Larp-Escape/pull/51).
Die Partitionstabelle entsprach dem neuen Testbuild; die Schlüsselpartition
entsprach dem bekannten chipgebundenen Rückweg. Vorherige APK, vollständige
Apppartition und Vollflash bleiben privat erhalten.

| Artefakt | SHA-256 |
| --- | --- |
| Installierte NFC-App | `312fadb3df5f2371501c28fbee7907081d2765423fa8c18be52adf1de539630e` |
| Installierte Firmwarevariante `larp-nfc-test` | `a247c7d801ada85d47248709b095c1a1fc1ebe855f32530990d7f4f5ab0f37b2` |
| Frischer Vollflash-Rückweg, 4 MiB | `7986c60f85e852dfba9844e6007d5782e46bc80118f9738fa3d39bb2f80b682b` |
| Partitionstabelle | `22790624ada38b3379c977dd7fd8452c6a3c86c00c64ec118380c888655e5b91` |

Nur die Anwendung ab `0x10000` wurde geschrieben und anschließend separat
am Chip verifiziert. Die Schlüsselpartition `0x310000`/`0x6000` blieb beim
erneuten Lesen bytegleich zum frischen Vorab-Abzug. Kein vollständiges
Löschen, keine neue Schlüsseleinrichtung. Im 20-Sekunden-Startmitschnitt
genau eine Bereitmeldung, keine beobachteten Panic-/Abort-/Brownout- oder
eigenen Fehlermarker. Die App bestätigte den gespeicherten Boxzugang ohne
erneuten Import und zeigte die bestehende Wake-Anordnung 2 / 3 / 4 / 1.
Dies ist kein erneuter funktionaler Wake- oder LED-Test.

## Tatsächlicher Rundlauf

Alle Zeiten unten sind Protokollierungs-/Erfassungszeiten in Europe/Berlin;
sie sind keine auf Millisekunden bestimmten physischen Kontaktzeitpunkte.

| Schritt | Frische Beobachtung und Quelle | Ergebnis |
| --- | --- | --- |
| Box erkennt und meldet, 23:01 | Authentifizierter App-Leseauftrag; Box-Tagkennung und Testpayload Flags 1, Kennwert 20261015, Zeitwert 0, Besuch 1/1/0, CRC gültig; Appanzeige direkt erfasst | bestanden |
| Handy liest | Nutzer bestätigt Flags 1 nach dem Wechsel von der Box zum Handy | nutzerbestätigt |
| App schreibt, bis 23:03 | Nutzer meldet „Geschrieben und durch erneutes Lesen bestätigt“. Diese Anzeige wurde vor dem nächsten Boxauftrag zusätzlich direkt erfasst; Handytagkennung stimmt mit der Boxkennung überein, verfügbare NDEF-Kapazität 137 Byte | bestanden |
| Box liest erneut, 23:04 | Neuer App-Leseauftrag; dieselbe UID und vollständige Vorher-Fixture mit Flags 1/CRC gültig direkt erfasst | bestanden |
| Box schreibt, 23:04 | Einmaliger, in der App bestätigter Schreibauftrag. Box meldet „Flags 2 geschrieben und frisch zurückgelesen“; gleiche UID, Flags 2, übrige Sollfelder unverändert, CRC gültig | bestanden |
| Alte Handyanzeige reicht nicht | Nach der Boxänderung zeigt die App ausdrücklich, dass die letzte Handylesung den Rundlauf noch nicht bestätigt | bestanden |
| Handy liest unabhängig, 23:06 | Nach Nutzerwechsel des Tags zum Handy: „Tagdaten gelesen · Prüfsumme bestätigt“, Flags 2, vollständige Nachher-Fixture und „Handy liest dieselbe Tagkennung und Flags 2 … stimmt mit der Boxänderung überein“. Vom Nutzer bestätigt und zusätzlich direkt aus der App erfasst | bestanden |

Die App-Schreibung setzte bewusst die bekannte Fixture Flags 1 erneut;
eine Nutzdatenänderung gegenüber dem bereits vorhandenen Flags-1-Inhalt
wird dafür nicht behauptet. Die Box änderte anschließend tatsächlich Flags
1 → 2. Der Abschlussindikator verlangt eine neue reine Handylesung nach der
Boxantwort, die gleiche UID und die exakte Nachher-Fixture; eine ältere
Anzeige oder bloße Schreibbestätigung genügt nicht. Die Payloadbytes stehen
im [Testplan](test-preparation.md#fester-datensatz-und-erwartete-änderung).

## Endzustand und Grenzen

NFC-App und NFC-Testfirmware bleiben installiert; der Audio-Stand wurde nicht
wiederhergestellt. Der Tag bleibt auf Flags 2. Nach jedem Boxauftrag versucht
der Worker, das RF-Feld abzuschalten; ein bestätigtes Auftragsergebnis setzt
dessen erfolgreiche Controllerantwort voraus. Keine elektrische Feldmessung.
Der anschließende Wechsel zum Handy lieferte die unabhängige Handylesung.

Es wurde ein physischer NDEF-Tag mit sieben UID-Bytes und 137 Byte von Android
gemeldeter NDEF-Kapazität geprüft. Das genaue Chipmodell wurde in diesem Lauf
nicht gesondert ausgelesen; die Kapazität allein beweist keinen NTAG-Typ.
Ein zweiter Rundlauf, ein anderer Tag vor dem Schreiben, Bluetoothabbruch,
weitere Tagfamilien, Langzeitbetrieb und Wiederherstellung des Audio-Paars
nach diesem NFC-Test sind nicht durchgeführt. Keine allgemeine Schreib- oder
Spielintegrationsfreigabe. Der anfangs noch angezeigte Verbindungshinweis
im NFC-Bereich war trotz bestätigtem Zugang missverständlich; die erfolgreiche
Boxbedienung ersetzt keine Korrektur dieses Hinweises.

Für den Gerätebericht wurden keine unveränderten Softwaretests wiederholt.
Die lokalen 112 Androidtests, NFC-/BLE-/LED-/Wake-Prüfungen und Builds am
Codecommit sind in PR #52 separat dokumentiert.

Private Belege: `%LOCALAPPDATA%/LarpEscape/LabRuns/NFC-App-20261016-dc746ec/`.
Das Installationsmanifest hält Chipzuordnung, Rückweg und Beobachtungsquellen.
Relevante UI-Belege: `app-connected.xml`, `step1-read-result.xml`,
`step5-before.xml`, `step5-read-result.xml`, `step6-result-00.xml`,
`step7-final.xml`. Roh-UIDs, Schlüssel, APK-Sicherungen und Flashabzüge werden
nicht veröffentlicht.
