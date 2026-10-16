# NFC-Vorerkundung und vorbereiteter Tagtest

Stand: **16. Oktober 2026**. Auftrag begonnen am 15. Oktober;
die beiden abschließenden Funkzyklen liefen am 16. Oktober gegen 00:01/00:02
Uhr Europe/Berlin. **Vorbereitung abgeschlossen; physischer Tag-Rundlauf offen.**
Quellstand der geprüften Umsetzung:
`d4892bf67c330a7899d1249bc769187072bc23ff`, auf Wake-Stand `1040035`.

## Ergebnis

Die Box liest und verändert den von der echten Companion-App emulierten
Type-4-NDEF-Testtag. Zwei bewusste Durchläufe mit dem abschließenden Build
bestätigen Flags **1 → 2**, gültige CRC und bytegleiche Rücklesung aller
59 NDEF-Bytes. Die Handy-App bestätigt jeweils drei empfangene
UPDATE-Kommandos, einen vollständig geprüften Datensatz und die Ausgabe
der vollständigen Nachherdaten an den Reader. Das zusätzliche USB-Ergebnis
`CHANGE VERIFIED` bestätigt den tatsächlichen Empfang und Vergleich in der Box.

Ein weiterer `CHANGE` auf Flags 2 ergibt `NOT_PREPARED`; der Schreibzähler
der App bleibt bei drei. Erneutes `READ` liefert Flags 2 auch nach einem
neuen Feldkontakt. Für den zweiten Durchlauf wurde die Handy-Fixture ausdrücklich
neu aktiviert und auf Flags 1 zurückgesetzt. Das Handy blieb auf der Box liegen.

Die [gemeinsame Testanleitung](test-preparation.md) führt anschließend durch
**App schreibt echten Tag → Box liest und ändert → App liest denselben Tag neu**.
Der Nutzer hat separate Tags bestätigt; deren Typ, Sperrstatus und reales
Seitenschreiben wurden in dieser Vorbereitung noch nicht geprüft.

## Geräte und Artefakte

Das angeschlossene moto g14 meldet Android 14, NFC und HCE-Unterstützung.
NFC war eingeschaltet, der Bildschirm entsperrt. Nach ausdrücklicher Aktivierung
war der neue Companion-Dienst als bevorzugter HCE-Dienst mit dynamischer
AID `D2760000850101` registriert; ReaderMode war aus. Nach Ablauf von drei
Minuten sowie nach manuellem Deaktivieren waren AID und Vordergrundpräferenz
wieder entfernt. Die App behielt die Beobachtung als vergangenes Ergebnis bei.

Die reale PN532-Antwort meldet Chip `32`, Firmware **1.6**, Unterstützung `07`.
Der Software-SPI-Pfad SCK16/MISO17/MOSI23/SS4 funktioniert im gemessenen Aufbau.
Das ist ein Funktionsnachweis, keine Durchgangs- oder Pegelmessung. IRQ und
Reset wurden nicht genutzt. Das Handy erschien als Type 4 mit ATQA `0004`,
SAK `20`, vier UID-Bytes und 512 Byte NDEF-Kapazität. Die UID wechselte mit
neuen Feldkontakten; sie ist kein dauerhafter HCE-Identitätsnachweis.

| Artefakt | Größe | SHA-256 |
|---|---:|---|
| Neue installierte Debug-App | 23.105.166 Byte | `f3bba60e5bcfaff31d7875c9e1f5034733733af2d4ce94eee44e6355ed66b69a` |
| Abschließend geprüfte NFC-Firmware | 239.728 Byte | `1b1d26962ef7a7d6801425a2607bf7f059e7420c6bbc8a68e48229f39b21c48c` |
| Gemeinsame Partitionstabelle | 3.072 Byte | `22790624ada38b3379c977dd7fd8452c6a3c86c00c64ec118380c888655e5b91` |
| Wiederhergestellte Wake-Firmware | 1.137.664 Byte | `316661b6daa41ff449ab944839dd18863f962f413ea1db0f35be2ba49c395378` |

Die APK wurde mit Datenerhalt aktualisiert, anschließend vom Handy zurückgelesen
und bytegenau per SHA-256 abgeglichen. Ihr Signierzertifikat stimmt mit dem
vorher installierten Wake-APK überein. Die Probe wurde ausschließlich ab
`0x10000` geschrieben und jeweils separat am Chip verifiziert.

## Rückweg und Zustand am Ende

Vor dem Versuch: Chipkennung gegen den bestehenden privaten Wake-Nachweis
abgeglichen, 4-MiB-Vollflash neu gesichert und mit `verify_flash` am Chip bestätigt.
Die darin enthaltene Anwendung entsprach exakt dem geprüften Wake-Artefakt;
Partitionstabelle und Schlüsselpartition entsprachen dem früheren Rückweg.
SHA-256 des neuen privaten Vollabzugs:
`d82af581f2d09d50f3a0645f8b159bfadc77ce3f336897dd906cb40925788aa8`.

Nach den NFC-Versuchen `OFF OK` bestätigt. Das belegt die Annahme des RF-Aus-
Befehls, keine elektrische Feldmessung. Der Handy-Testtag wurde deaktiviert.
Die Schlüsselpartition bei `0x310000`, Länge `0x6000`, stimmte weiter bytegleich
mit dem Vorab-Abzug überein. Danach Wake-Anwendung wiederhergestellt und
Anwendung sowie Schlüsselpartition nochmals am Chip verifiziert.

Die neue App meldet anschließend **„LED-Box verbunden · Zugang bestätigt“**
und frische Wake-Werte. Es war kein Schlüsselimport nötig. Die vorhandene
Anordnung wurde nicht geändert; die zugehörige Einstellungsdatei trägt einen
Änderungszeitpunkt vor dieser Sitzung. Die App bleibt aktualisiert und verbunden,
die Box läuft wieder mit `ca86b93`. Für den nächsten echten Tagtest wird die
bereitliegende NFC-Prüffirmware erneut benötigt.

## Prüfung der Software

- 87 Android-JVM-Fälle, null Fehler; darunter 14 neue APDU-Fälle mit
  gültigen und unvollständigen Transaktionen, CRC-/Format-/Grenzfehlern,
  Unterbrechung, Fristablauf, Neustart und 10.000 deterministischen
  fehlerhaften APDUs. Debug-Build und Lint erfolgreich. Lint: null Fehler,
  drei bestehende Warnungen und eine Autoboxing-Information.
- 78 Firmware-Transaktionsfälle plus Frame-, Target-, CRC- und NDEF-Prüfungen
  erfolgreich. ESP32-Build erfolgreich: 25.172 Byte statisches RAM,
  239.361 Byte Anwendungscode; Binärgröße siehe Tabelle.
- Unabhängige Reviews für Android-Lifecycle, APDUs, Tagtransaktionen und
  Startpfad. Im ELF eigener `app_main`, keine ausführbaren `initArduino`-,
  NVS-Initialisierungs-, NVS-Lösch- oder NVS-Schreibfunktionen. Das SDK
  enthält weiterhin Coredump-Funktionen; keine pauschale Flash-Schreibfreiheit.
- `git diff --check` erfolgreich. Lokale Sandbox-Prozess-/Compilerfehler
  waren Umgebungsfehler; erfolgreich wiederholte Prüfungen verwendeten
  dieselben Quellen mit freigegebenem Toolchainzugriff.

## Beobachtete und behobene Grenzen

Ein Zwischenbuild meldete beim ersten `INFO` einen ungültigen SPI-Status,
funktionierte später aber ohne Pinänderung. Die genaue Ursache bleibt offen.
Der endgültige Build konfiguriert den GPIO-Multiplexer ausdrücklich, hält NSS
beim Aufwecken bis zum Kommando aktiv und protokolliert Status, Phase und
begrenzte Headerdaten. Sein erster Geräte-`INFO` war erfolgreich.

Der Zwischenbuild konnte nach einem erfolgreichen Lesen das weiter aufliegende
Handy nicht erneut aktivieren (`NO_TARGET`). Ein ausdrückliches RF-Aus/Ein
beseitigte dies. Der endgültige Build erzeugt deshalb genau einmal vor jedem
äußeren READ-/CHANGE-Auftrag einen neuen Feldkontakt, mit 20 ms Auszeit und
20 ms nach Einschalten. Während Vorherlesen, Schreiben und Rücklesen bleibt
das Feld durchgehend aktiv. Die beiden vollständigen abschließenden Zyklen
und der abgelehnte Wiederholauftrag prüfen diesen korrigierten Ablauf.

Die Quellenprüfung fand außerdem, dass PN532 `InDataExchange` das NTAG-
Kommando `60` als MIFARE-Authentifizierung interpretiert. GET_VERSION verwendet
nun `InCommunicateThru`. Dieser Type-2-Pfad ist hostseitig geprüft; HCE bestätigt
ihn nicht. Echte NTAG213/215/216, Schreibschutz, EEPROM-Dauerhaftigkeit nach
Feldwechsel und der gemeinsame physische Rundlauf bleiben offen.

## Belegorte

Der kleine, bereinigte [Maschinenbericht](evidence/hce-2026-10-16.json) enthält
die entscheidenden finalen USB-Ergebnisse und Sollbyte-Abgleiche. Private
Rohbelege liegen im Chat-Arbeitsordner unter `work/evidence/`: `final-info-01`,
`final-read-01/02`, `final-change-01/02`, `final-readback-01/02`,
`final-repeat-refused`, `final-cycle-01/02`, `hce-expired`, `final-off`,
Installations- und Rückwegverifikation sowie `wake-restored-connected`.
Vollflash und APK-Sicherungen liegen getrennt unter `work/private/`, außerhalb
dieses Git-Arbeitsbaums. Vollflash und Schlüsseldateien werden nicht veröffentlicht.
