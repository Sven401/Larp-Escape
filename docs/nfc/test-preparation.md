# NFC: Vorbereitung des gemeinsamen Rundlauftests

Stand: 15. Oktober 2026. **Testplan und Sollwerte; kein Geräteergebnis.**
Die abgeschlossene Handy-Vorerkundung und der wiederhergestellte Wake-Zustand
sind im [Gerätebericht vom 16. Oktober](device-preparation-2026-10-16.md)
belegt. Die Nachweismatrix unten bleibt eine Vorlage für weitere Durchführungen.
Der anschließende [echte NTAG213-Rundlauf](physical-tag-test-2026-10-16.md)
ist einmal vollständig bestanden, einschließlich der dokumentierten
Wiederaufnahme nach zunächst unklarem App-Schreibergebnis.
Die [spätere App-Korrektur](first-write-readback-2026-10-16.md) bestätigt die
Erstbefüllung am moto g14 durch automatische neue Erkennung. Für weitere
Versuche die Bildschirmausrichtung während eines vorbereiteten Auftrags
beibehalten; ein Activity-Neustart verwirft den noch nicht gestarteten Auftrag.
Der Nutzer hat separate beschreibbare Tags bestätigt. Zur automatischen
Vorerkundung liegt zunächst das Handy mit seiner NFC-Antenne über dem Reader
der Box. Danach folgt gemeinsam der Rundlauf **App schreibt Tag → Box liest
und ändert Tag → App liest denselben Tag erneut**.

Basis des Arbeitsstands ist `10400350f31883b4e7b36df1d6875388a2d8953f`.
Das zuletzt dokumentiert installierte Wake-Paar ist `ca86b93`; dessen
[Gerätebericht](../wake/device-test-2026-10-15.md) beschreibt Artefakte und
privaten Flash-Rückweg. Vor einer NFC-Installation den tatsächlich verbundenen
Chip und aktuellen Flashstand erneut abgleichen. Ein früherer Bericht ersetzt
diese Prüfung nicht.

## Was die beiden Prüfungen belegen

| Prüfung | Prüfling und Verfahren | Möglicher Nachweis | Bleibt dadurch offen |
|---|---|---|---|
| Automatische Vorerkundung | Box als Reader, Handy als kontrollierter HCE-Testtag; Type-4-NDEF über ISO-DEP/APDUs | PN532-Kommunikation, NFC-Funkpfad zur Handyantenne, Lesen des Testdatensatzes, Schreiben aus der Box und frisches Zurücklesen | Physischer Tag, Type-2-Seitenschreiben, dauerhafte Speicherung ohne Feld, Bedienung des Tag-Rundlaufs |
| Gemeinsamer Rundlauf | Separater, unterstützter NDEF-Tag nacheinander an Handy und Box | App-Schreiben, Box-Lesen, Box-Schreiben und unabhängiges frisches App-Lesen desselben physischen Tags | Andere Tagfamilien, Spielintegration, allgemeine Schreibfreigabe, Dauertest |

Android-HCE stellt eine ISO-DEP-Karte mit APDUs bereit; es emuliert keinen
NTAG213/215/216 auf Type-2-Seitenebene. Seine UID kann bei jedem Kontakt
wechseln. Für HCE deshalb keinen stabilen UID-Abgleich als Identitäts- oder
Erfolgsbeweis verwenden. Die kontrollierte App und die vollständigen
Fixture-Bytes sind maßgeblich. [Android-HCE-Dokumentation](https://developer.android.com/develop/connectivity/nfc/hce).

Die vorhandene Spiel-Firmwareklasse `lib/NFCReader/NFCReader.cpp` liest am
Basisstand nur UIDs. Der NFC-Versuch verwendet eine eigene, zeitweise
installierte Prüffirmware unter `firmware/nfc-probe`; er erweitert den
LED-/Wake-BLE-Dienst nicht. Während dieser Prüffirmware sind LED- und
Wake-Bedienung nicht verfügbar. Nach der Vorerkundung wird die zuvor
gesicherte Wake-Anwendung wiederhergestellt und geprüft. Für den gemeinsamen
Tagtest muss dieselbe geprüfte NFC-Firmware erneut bereitgestellt werden.

## Fester Datensatz und erwartete Änderung

Das [GM-Testformat](../../companion/contract/nfc-v1/README.md) ist ein
NDEF-MIME-Record mit `application/vnd.yourapp.tagdata`. Es ist weiterhin ein
Testformat; daraus folgt kein freigegebener Spielobjektvertrag.

| Feld | Von der App vorbereitet | Nach Änderung durch die Box |
|---|---|---|
| Version | 1 | 1 |
| Kennwert / `uidHash` | 20261015 | 20261015 |
| Flags | **1** | **2** |
| `timestampBinary` | 0 | 0 |
| Stationsbesuche | eine Zeile: `1, 1, 0` | unverändert: `1, 1, 0` |
| CRC32, gespeicherte Bytes in Little Endian | `F4 61 7B 57` | `06 D5 B3 7E` |
| Payloadlänge / NDEF-Länge des einfachen Testtags | 25 / 59 Byte | 25 / 59 Byte |

`uidHash` ist hier ein öffentlicher Testkennwert, nicht die ausgelesene
Hardware-UID. Die Box verändert nur Flags und die zugehörige Prüfsumme.
Ein weiterer Stationsbesuch wird nicht angehängt. Die unveränderte Länge
begrenzt die Schreiboperation, macht sie aber nicht atomar.

Bytegenaue Payloads:

```text
Vorher: 019728350101000000000000000000010001010000F4617B57
Nachher:01972835010200000000000000000001000101000006D5B37E
```

Der NDEF-Umschlag besteht aus `D2 1F 19`, den 31 ASCII-Bytes des MIME-Typs
und dem jeweiligen Payload. Ein physischer Type-2-Tag enthält zusätzlich
seinen NDEF-TLV-Umschlag; diese Bytes zählen nicht zu den 59 NDEF-Bytes.
Die Firmware muss den eindeutigen GM-Payload einschließlich seiner Prüfsumme
prüfen, bevor sie einen Änderungsversuch zulässt. Der kontrollierte HCE-Tag
nimmt nur den oben beschriebenen einzelnen Record an. Beim physischen Tag
kann die Probe weitere vollständige, nicht gechunkte NDEF-Records erhalten;
dann ist die Gesamtnachricht länger. Sie bindet den gesamten zuvor gelesenen
Inhalt an den Auftrag und erhält diese zusätzlichen Records bytegleich.
Fehlende, mehrdeutige, beschädigte oder bereits geänderte GM-Inhalte erhalten
keinen neuen Schreibauftrag.

## Vorbereitung und Rückweg

1. Quellstand, Buildvarianten und SHA-256 der neuen APK und Prüffirmware
   festhalten. Softwareprüfungen, Installation und RF-Beobachtungen getrennt
   protokollieren. Keine alten grünen Tests als Ergebnis der neuen Dateien
   übernehmen.
2. Aktuelle Box identifizieren; vollständigen privaten Flash-Rückweg und
   Anwendung gegen den verbundenen Chip verifizieren. Die bestehende
   Partitionstabelle und Schlüsselpartition erhalten. Nur die zur
   Partitionierung passende Anwendung installieren; kein Chip-Erase und
   keine neue Schlüsseleinrichtung. Vorherige APK ebenfalls verfügbar halten.
3. Handy: NFC und HCE-Unterstützung, aktiviertes NFC, Android-/App-Version
   und kontrollierten HCE-Dienst feststellen. Bildschirm für den Versuch
   eingeschaltet und entsperrt halten. Eine unterstützte API oder registrierte
   AID allein ist noch kein Funknachweis.
4. Auf der Seite **NFC-Tags** den Modus **Handy als Testtag** wählen und
   **Testtag vorbereiten und aktivieren** drücken; die normale NFC-Tag-Lesefunktion
   dabei beenden. Androids ReaderMode schaltet Card-Emulation ab. Zum späteren
   Lesen des physischen Tags **Testtag deaktivieren** und **Echter Tag**
   wählen. [Android NfcAdapter](https://developer.android.com/reference/android/nfc/NfcAdapter#enableReaderMode(android.app.Activity,%20android.nfc.NfcAdapter.ReaderCallback,%20int,%20android.os.Bundle)).
5. Bei HCE nur Handy und Box koppeln. Beim echten Tagtest Handy und Tag zum
   Schreiben/Lesen vom Boxreader entfernen; danach ausschließlich den Tag
   an die Box legen. So konkurrieren nicht zwei aktive Reader um denselben Tag.

## Automatische Vorerkundung mit dem Handy

Die Test-App bietet Type-4-NDEF mit AID `D2760000850101`, Capability-Container
`E103` und NDEF-Datei `E104`; die vereinbarte NDEF-Kapazität umfasst 512 Byte.
NDEF-Dateilänge und Nutzlänge sind getrennt zu behandeln: die Datei beginnt
mit zwei NLEN-Bytes, der CC beschreibt deshalb 514 Byte Dateikapazität.
Die eigentliche Testnachricht ist 59 Byte lang.
Das sind Parameter des kontrollierten Versuchs, keine Aussage über den
Speicher eines physischen Tags.

USB-Antworten werden bis zur Abschlusszeile `NFC END <CMD> <RESULT>`
zugeordnet. `INFO` mit `OK` bestätigt den Controller; `READ` mit `OK`
bestätigt die vollständige NDEF-Lesung. Für die vorbereitete Testfixture
muss zusätzlich `NFC DATA test_record=1 flags=1 prepared=1 crc_checked=1`
erscheinen; `OK` allein bestätigt den GM-Payload nicht. Nur `CHANGE` mit `VERIFIED` bezeichnet
einen erfolgreich zurückgelesenen Schreibversuch. `NO_TARGET`, `UNSUPPORTED`,
`READ_ONLY`, `NOT_PREPARED`, `CHANGED` und `ERROR` müssen mit der konkreten
Stufe gelesen werden. `UNKNOWN` bedeutet, dass ein Schreibversuch begonnen
haben kann und kein bestätigtes Gesamtergebnis vorliegt. Ein ausbleibendes
Abschlusszeichen ist ebenfalls kein Erfolg.

1. HCE-Testdatensatz mit **Testtag vorbereiten und aktivieren** auf den
   Vorher-Wert setzen und seinen sichtbaren Zustand festhalten. Die Probe
   läuft höchstens drei Minuten im Vordergrund. Apppause, Seitenwechsel und
   Rotation stoppen sie; den HCE-Modus nicht während eines Boxauftrags
   zurücksetzen.
2. Box über USB mit `INFO` abfragen: Kennung der Prüffirmware und erfolgreiche
   PN532-Initialisierung protokollieren. Eine Controllerantwort bestätigt
   noch keine NFC-Übertragung zum Handy.
3. `READ` auslösen. Die Box muss über NFC einen Type-4-NDEF-Datensatz lesen,
   MIME, Payloadlänge und CRC validieren und **Flags 1** melden. Das Erkennen
   einer UID oder eine erfolgreiche AID-Auswahl allein reicht nicht.
4. Nur nach passender vollständiger Fixture bewusst `CHANGE` auslösen.
   Unmittelbar vor dem Schreiben erneut lesen und den erwarteten Vorher-Wert
   vergleichen. Der Auftrag ist einmalig; das Auflegen des Handys darf nicht
   selbstständig Änderungen wiederholen.
5. Für Erfolg müssen vollständige Schreibantwort und ein frischer
   NFC-Lesezugriff der Box den exakten Nachher-Wert belegen. Die HCE-App zeigt
   zusätzlich **Flags 2**. Ihre Zähler können den Empfang der Leseaufträge
   und die vollständige Ausgabe der Nachher-Bytes belegen. Ob diese Antworten
   bei der Box ankamen und dort übereinstimmten, belegt nur deren eigener
   Rücklesevergleich. Eine reine Empfangszählung oder lokale Vorschau
   ersetzt diesen Rücklesebeleg nicht.
6. Ein weiteres `READ` muss weiterhin Flags 2 ergeben. Ein weiteres `CHANGE`
   darf den bereits geänderten Datensatz nicht erneut verändern. Eine neue
   Wiederholung beginnt erst nach bewusstem HCE-Reset auf Flags 1.
7. Bei Timeout nach Schreibbeginn oder abweichendem Rücklesen gilt
   **UNKNOWN / Schreibausgang unklar**. Keinen automatischen Wiederholungs-
   oder Reparaturauftrag senden. Zuerst tatsächlichen Inhalt erneut lesen,
   anschließend eine neue vollständige Testausführung vorbereiten.
8. Mit `OFF` das NFC-Feld abschalten und `NFC END OFF OK` festhalten;
   ein ESP-Neustart allein belegt kein abgeschaltetes PN532-Feld.
   HCE-Testmodus beenden. Wake-Anwendung aus dem geprüften Rückweg
   wiederherstellen, Flash prüfen, normalen Start und authentifizierte
   Appverbindung mit frischen Wake-Werten kontrollieren. Diesen Rückweg als
   eigenen Nachweis festhalten.

Fehlt schon die Handyantwort, zuerst HCE/ReaderMode, AID-Zuordnung, Bildschirm,
Appzustand und Antennenposition unterscheiden. Fehlende HCE-Kommunikation
allein beweist keinen defekten physischen Tagleser. Wenn eine Verschiebung
des Handys erforderlich ist, bleibt der automatische Versuch bis zu dieser
physischen Änderung offen.

## Gemeinsamer Rundlauf mit einem separaten Tag

Für den ersten Lauf einen entbehrlichen, beschreibbaren, bereits
NDEF-formatierten **NTAG213, NTAG215 oder NTAG216** verwenden. Die Box muss
die unterstützte Variante selbst erkennen. Hersteller-/Typaufdruck allein
genügt nicht. Ein nur formatierbarer Tag ist noch kein unterstützter NDEF-Tag.
Keine Formatierung, Passwortauthentifizierung, Schlüsseländerung oder
Schreibschutzsetzung gehört zum Versuch. [NXP NTAG213/215/216](https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf).

Die begrenzte Probe verlangt passende GET_VERSION- und Capability-Container-
Werte; sie erwartet dort 144, 496 beziehungsweise 872 Byte als Type-2-
Datenbereich. Diese Werte sind von der größeren Herstellerangabe zum
gesamten Nutzspeicher und von Androids tatsächlich verfügbarer NDEF-Kapazität
zu unterscheiden. Ein bekannter NTAG213-Lock-Control-TLV kann vor dem NDEF
liegen; unbekannte Control-TLVs werden abgewiesen. So kann auch ein von
Android grundsätzlich beschreibbarer Tag außerhalb des begrenzten
Box-Testpfads liegen.

1. **Tag in der App lesen.** HCE ist aus, NFC-Tags im Modus **Echter Tag**
   geöffnet und kein
   Schreibauftrag vorbereitet. Tag ruhig an die Handyantenne halten.
   Hardware-UID, Schreibbarkeit und Kapazität notieren. Leeren NDEF-Tag oder
   bereits passende Testfixture mit genau einem MIME-Record verwenden.
   Der allgemeine App-Editor kann zusätzliche Records erhalten; für den
   ersten gemeinsamen Lauf bleibt der einzelne 59-Byte-Testrecord die
   eindeutige Vergleichsgrundlage. Fremde Inhalte ohne gültigen GM-Record
   erhalten keinen Schreibversuch.
2. **Vorher-Datensatz vorbereiten.** **NFC-Testdaten einsetzen** füllt das
   Formular mit Kennwert `20261015`, Flags `1`, Timestamp `0` und
   Stationsbesuch `1, 1, 0`; es schreibt noch nichts. Eingaben vor dem
   Schreibauftrag prüfen. Der allgemeine Editor erhält sonst einen
   vorhandenen Timestamp; deshalb für diesen Versuch die Testdatenfunktion
   verwenden. Ein leerer NDEF-Tag ist die einfachste Ausgangslage; fremde
   oder ungültige Datensätze werden nicht durch die Testdatenfunktion
   überschreibbar.
3. **App schreibt bewusst.** **Schreiben vorbereiten**, danach
   **Schreibauftrag vorbereiten** bestätigen. Tag vom Handy entfernen und
   denselben Tag erneut anhalten. Auf **Geschrieben und durch erneutes Lesen
   bestätigt** warten. Dieser Status verlangt frische RF-Lesedaten;
   ein bloßer Abschluss des Schreibaufrufs reicht nicht.
4. **Vorher-Zustand unabhängig wiederlesen.** Tag erneut entfernen und ohne
   neuen Schreibauftrag anhalten. Die App muss Kennwert 20261015, Flags 1,
   einen Besuch `1, 1, 0` und bestätigte Prüfsumme zeigen. Identität und
   RF-Lesebeleg festhalten; danach Tag vom Handy entfernen.
5. **Box liest.** Unterstützte NFC-Prüffirmware bereitstellen und `INFO`
   prüfen. Handy vom Boxreader wegnehmen; nur den Tag auflegen. `READ`
   auslösen. Typ und Hardware-UID müssen zum gewählten Tag passen; der
   vollständige Datensatz muss exakt dem Vorher-Wert entsprechen.
6. **Box ändert.** Bei stabil liegendem Tag einmal `CHANGE` auslösen.
   Erwartet sind Flags 2 mit neuer CRC und unveränderten übrigen Feldern.
   Frisches Zurücklesen durch die Box muss bytegenau übereinstimmen.
   Kein Erfolg bei bloßer Schreibquittung; kein zweiter Schreibversuch
   bei unklarem Ausgang.
7. **Tag aus dem Boxfeld nehmen.** Dadurch wird die passive Versorgung des
   Tags unterbrochen. Mit dem Tag zum Handy wechseln; in der App ist weiterhin
   kein Schreibauftrag vorbereitet. Den Tag nur lesen.
8. **App bestätigt Boxänderung.** Dieselbe Hardware-UID und Kennwert 20261015,
   jetzt **Flags 2**, unveränderter Besuch und bestätigte CRC müssen aus
   einer neuen RF-Lesung stammen. Erst dieser Schritt schließt den
   angeforderten vollständigen Rundlauf. Eine im Editor eingetragene 2 oder
   ein alter Screenshot ist kein Nachweis.
9. **Bewusst wiederholbar.** Für einen zweiten Lauf in der App Flags 1 erneut
   schreiben und frisch bestätigen. Dann Schritte 4–8 wiederholen.
   Am Ende Tag tatsächlich mit Flags 2 gelesen lassen; `OFF` an der Box
   ausführen und dessen Abschluss prüfen, Wake-Firmware wiederherstellen
   und HCE beenden.

## Nachweismatrix für die Durchführung

Alle Einträge beginnen **offen**. In einem separaten Gerätebericht Datum,
Version, Belegpfad und tatsächliches Ergebnis eintragen. Dieser Plan
behauptet keine bestandene Prüfung.

| ID | Nachweis | Pass-Kriterium | Status |
|---|---|---|---|
| P1 | Installation/Rückweg | Chip, Artefakte und Flash verifiziert; Schlüsselpartition erhalten | offen |
| H1 | HCE-Verfügbarkeit | Handy unterstützt HCE; gewünschter Dienst antwortet real per NFC | offen |
| H2 | Box liest Handy | Vollständige Vorher-Fixture, CRC gültig, Flags 1 | offen |
| H3 | Box schreibt Handy | Expliziter Auftrag; frische Box-RF-Lesung exakt Nachher-Fixture, App Flags 2 | offen |
| H4 | HCE-Wiederholung | Erneutes READ Flags 2; CHANGE auf Nachher-Wert schreibt nicht erneut | offen |
| T1 | App schreibt physischen Tag | Unterstützter NDEF-Tag, Vorher-Fixture frisch zurückgelesen | offen |
| T2 | Box liest physischen Tag | Gleiche Tag-UID, unterstützter Typ, exakte Vorher-Fixture | offen |
| T3 | Box schreibt physischen Tag | Exakter Nachher-Wert durch frisches RF-Rücklesen der Box bestätigt | offen |
| T4 | App liest Boxänderung | Nach Feldwechsel dieselbe UID, Flags 2 und gültige CRC frisch gelesen | offen |
| T5 | Erneuter Rundlauf | Bewusstes App-Zurücksetzen und zweiter unabhängiger Durchlauf T1–T4 | offen |
| R1 | Rückkehr zu Wake | Ursprüngliche Anwendung verifiziert, normaler Start, frische authentifizierte Werte | offen |

Für jeden tatsächlichen Schreibversuch Vorher-/Nachherbytes oder deren
Prüfsummen, Zieltyp, Sitzungsbezug, USB-Auftrag und frisches Leseergebnis
zusammen festhalten. Appanzeige, Boxantwort und Nutzerbeobachtung als
getrennte Quellen benennen. Private Flashabzüge und Zugangsdaten bleiben
außerhalb des Repositories.

## Fehlergrenzen und ergänzende Prüfungen

- Kein Tag, unbekannter Typ, unpassende/defekte NDEF-Nachricht, falscher
  Kennwert, andere Besuche, falscher Timestamp, CRC-Fehler oder Flags 2:
  kein neuer Schreibauftrag. Vor einem Schreibversuch muss Ablehnung von
  einem möglicherweise begonnenen, später fehlgeschlagenen Schreiben
  unterscheidbar bleiben.
- Schreibgeschützt oder zu klein: ablehnen. Keine Lock-, OTP-, CC-,
  Passwort- oder Konfigurationsbytes verändern. Die Type-2-Schreibgrenzen
  müssen zum erkannten Chip und vorhandenen NDEF passen; große Gesamt-
  kapazität allein ist keine Schreibfreigabe.
- Verbindung nach Schreibbeginn abgebrochen oder Rücklesen unvollständig:
  **unklar**, zunächst nur lesen. Teilweise geänderte Nutzdaten sind möglich;
  mehrere Seitenschreibvorgänge bilden keine atomare Transaktion.
- Ein negativer Schreibfehler am echten Tag wird zunächst nicht absichtlich
  provoziert. Abbruch-/Grenzfälle gehören vorab in die Hostprüfungen und
  können danach kontrolliert mit der zurücksetzbaren HCE-Fixture untersucht
  werden. Physisches Teilseitenverhalten bleibt damit trotzdem offen.
- Ein erfolgreicher HCE-Rundlauf darf H2/H3 schließen, aber niemals T1–T4.
  Der Test ist erst vollständig, wenn das Handy den von der Box veränderten
  separaten Tag nach dem Feldwechsel frisch gelesen hat.

Die NFC-Schreiboperationen sind keine Authentifizierung. CRC schützt hier
gegen Datenfehler, nicht gegen absichtliche Veränderung. Der begrenzte
USB-Testpfad ist keine Freigabe für beliebige Tags oder Spielaktionen.

## Abschluss des NFC-Versuchs

Vor dem Wiederherstellen der Wake-Anwendung den Prüfmodus mit USB `OFF`
beenden und `NFC END OFF OK` abwarten. Das weist die bestätigte Annahme
des RF-Aus-Befehls durch den PN532 nach; eine elektrische Feldmessung
ersetzt es nicht. Ein ESP32-Neustart allein schaltet den getrennten
PN532 nicht nachweislich aus. Anschließend den Handy-Testtag deaktivieren,
die gesicherte Wake-Anwendung wiederherstellen und am Chip verifizieren.
Die App soll danach wieder frische Wake-Werte anzeigen. Für den nächsten
Tagversuch wird NFC ausdrücklich erneut initialisiert.
