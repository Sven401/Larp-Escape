# Audio: Vorbereitung des gemeinsamen Baugruppentests

Stand: **16. Oktober 2026, Europe/Berlin. Testplan und Sollwerte; kein
Geräteergebnis.** Geprüft werden die bewusste Bedienung in der echten
Companion-App, UART-Aufträge an den DFPlayer, der getrennte BUSY-Eingang
und die hörbare Ausgabe der vorhandenen Box. Eine Spielintegration ist
nicht Teil dieses Tests.

Das Verfahren folgt den getrennten Geräte- und Nutzerbelegen der
[LED-Prüfung](../led/device-test-2026-10-15.md), den frischen Zuständen der
[Wake-Prüfung](../wake/device-test-2026-10-15.md) und dem verifizierten
[NFC-Rückweg](../nfc/physical-tag-test-2026-10-16.md). Der
[Ergebnisbogen](test-record-template.md) bleibt bis zur Durchführung offen.
Quellbasis dieses Arbeitsstands ist `e00fe437` des dokumentierten
NFC-Rundlaufs. Die Audio-Erweiterung muss mit ihrem eigenen endgültigen
Commit und Artefakthashes im Installationsbericht bezeichnet werden.
Der [Prüfbericht der Vorbereitung](preparation-results-2026-10-16.md)
führt den gebauten Quellstand, frische Softwaretests und Artefakthashes.

## Vorbereiteter Bedienumfang

- Die Audio-Bedienung läuft über die echte App und ihren authentifizierten
  BLE-Zugang. Trackauswahl und Lautstärkeeinstellung allein starten nichts.
- Ein bewusster Start wählt den angegebenen Titel im MP3-Ordner. Derselbe
  Titel kann durch einen weiteren bewussten Start neu begonnen werden.
- Zulässig sind Titelnummern **1–255** und Lautstärken **1–10**, Vorgabe
  **5**. Nach 30 Sekunden ab abgeschlossener UART-Ausgabe des Play-Befehls
  fordert die Firmware automatisch Stop an. UART-/Loopverzögerung und die
  Reaktion des Moduls kommen hinzu; das ist keine gemessene elektrische
  Abschaltfrist und keine Garantie hörbarer Stille.
- Der erste Hörversuch verwendet Lautstärke **5**; der Prüfmodus begrenzt
  sie auf **10** der DFPlayer-Skala 0–30. Das ist eine Stellwertgrenze,
  keine Messung des Schalldrucks. Bei unangenehmer Lautstärke sofort stoppen.
- Stop sendet einen eigenen Stopauftrag. Boxstart, App-Neustart und
  Wiederverbindung lösen keine automatische Wiedergabe aus. Firmwarestart
  und erkannter Verbindungsverlust fordern Stop an.
- Gerätestatus unterscheidet den ausgegebenen Auftrag, den beobachteten
  BUSY-Rohpegel und fehlende/veraltete Messwerte. Eine UART-Ausgabe oder
  ein BUSY-Wechsel bestätigt noch keinen hörbaren Titel.

Der [Audio-BLE-Vertrag](ble-wire.md) beschreibt Nachrichten, Grenzen und
genaue Bedeutung der Ausgabebestätigung. Vor Start überträgt der Besitzer
Stop, Lautstärke und Play in dieser Reihenfolge; der Play-Befehl `0x12`
nutzt denselben MP3-Ordnerpfad wie das bisherige `playFromMP3Folder`.

## Ergänzung für Play, Pause und Lautstärke

Am 16. Oktober 2026 zusätzlich vorbereitet: Pause und Fortsetzen sowie
**Lautstärke übernehmen** während Play, Pause oder Stopp. Die Eingabe allein
sendet weiter nichts. Play startet den ausgewählten Titel neu; Fortsetzen
setzt ausschließlich den pausierten Titel fort. Die Prüfgrenzen 1–255 und
1–10 bleiben bestehen. Die 30-Sekunden-Grenze ab dem ursprünglichen Play wird
durch Pause, Fortsetzen oder Lautstärke nicht verlängert.

Zusätzliche gemeinsame Schritte nach A2: markante Hörstelle benennen,
Pause drücken und drei Sekunden Stille bestätigen; Fortsetzen drücken und
hörbare Fortsetzung an derselben Stelle bestätigen. Lautstärke 5 → 8 → 3 → 5
jeweils übernehmen und hörbaren Unterschied prüfen. Danach Stop und Play:
der Titel muss neu beginnen. Bei Pause einen anderen Titel auswählen und
Play drücken: der neu ausgewählte Titel muss beginnen. UART-/BUSY-Belege
und Höreindruck für jeden dieser Schritte getrennt im Ergebnisbogen notieren.

**Nutzerbestätigt am 16. Oktober 2026 in dieser Sitzung:** DFPlayer-RX an
ESP GPIO26, DFPlayer-TX an GPIO25 und BUSY an GPIO14. Quelle: Antwort
„GPIO26/25/14 bestätigt“. Dies ersetzt die bisher offene As-built-Zuordnung
für diese drei Leitungen; keine neue Durchgangs-, Pegel- oder Versorgungsmessung.
GPIO13 bleibt unberührt. Die historischen Zeichnungen bleiben abweichende Quellen.

## Ausgangslage und offene Hardwarefragen

Der bestehende Spielpfad in
[`DFMinniHandler.cpp`](../../lib/DFMinniHandler/DFMinniHandler.cpp) nutzt
UART1, 9600 Baud, 8N1, ESP-RX **25**, ESP-TX **26**, BUSY **14**
(LOW als busy) sowie einen als WAKE bezeichneten GPIO **13**. Diese Werte
waren zunächst Quellenbefunde; die drei UART-/BUSY-Leitungen sind inzwischen wie oben nutzerbestätigt. Der bisherige
Wake-Pfad schaltet GPIO13 mehrfach und später periodisch; daraus folgt
keine bestätigte Bedeutung des tatsächlich angeschlossenen Signals.
Ein Pinwechsel oder Versuch mit anderen GPIOs gehört nicht zum Hörtest.
Der neue Prüfpfad lässt **GPIO13 vollständig unberührt**. Er übernimmt
keine Wake-Impulse aus dem alten Spielpfad.

Der [Schnittstellenabgleich](../../Hardware/SCHNITTSTELLENABGLEICH-2026-10-10.md)
führt abweichende ältere Zeichnungen auf: DFPlayer-RX an GPIO17, TX an
GPIO16, BUSY ohne gezeichneten Anschluss und keine bestätigte WAKE-Leitung.
GPIO16/17 dienen im zuletzt erfolgreich getesteten NFC-Pfad dem PN532.
Die Zeichnungen sind deshalb keine Grundlage für automatisches Umschalten
der Audiopins. Die drei UART-/BUSY-Leitungen wurden wie oben bestätigt; WAKE und der angeschlossene
Lautsprecher beziehungsweise Verstärker bleiben zuzuordnen.

Derselbe historische Abgleich berichtet einen Versuch mit **Track 100**
bei vorhandener Versorgung, aber ohne hörbaren Ton und ohne beobachteten
BUSY-Verlauf. Die Ursache blieb offen. Das beweist weder einen defekten
Player noch eine fehlende Datei; ein nicht angeschlossener BUSY-Eingang
kann ebenfalls keine Wiedergabe nachweisen.

Der Nutzer hat für diese Vorbereitung bestätigt, dass die vorhandene
microSD den **Audio-Stand der Starwars-Con** enthält. Es wurde keine genaue
Dateiliste, kein Dateiname und kein bestimmter hörbarer Inhalt bestätigt.
Die Karte muss für die Vorbereitung nicht neu beschrieben werden.

Als erster **Kandidat** ist Titel **100** vorgesehen. Die aktuelle
[`GameDefinitions.cpp`](../../src/GameDomain/GameDefinitions.cpp) ordnet
100 dem Kristall „Fuchs“ der Rolle „Abenteurer:in“ zu; 101/102 sind dort
„Grün“/„Seil“, 200 der Rollentitel. Diese Softwarezuordnung bestätigt nicht
die tatsächliche Benennung oder den Inhalt der Con-Karte. Titel 100 wird
im MP3-Ordner über dieselbe numerische Adressierungsart wie im alten
Spielpfad angesprochen. Weder ein konkreter vorhandener Dateiname noch
eine erfolgreiche Wiedergabe wird daraus abgeleitet.

Vor dem gemeinsamen ersten Start festhalten:

1. Vorhandene microSD, gewählter Titel und erwarteter hörbarer Inhalt,
   soweit bekannt. Eine numerische Auswahl allein bestätigt keine Datei
   auf der Karte. Mit Kandidat 100 beginnen oder einen vom Nutzer eindeutig
   benannten anderen Titel wählen; den tatsächlich gehörten Inhalt erfassen.
2. Tatsächlicher Lautsprecher-/Verstärkeranschluss und betriebsbereite
   Versorgung; die Box bleibt im bekannten Aufbau. Keine Umverdrahtung
   während eines laufenden Tests.
3. Die implementierte UART-/BUSY-Konfiguration mit der vorhandenen Box
   abgleichen. Nicht beobachtete Leitungen ausdrücklich offen lassen.
   Die Vorbereitung verlangt keine unbestätigten WAKE-Impulse.

## Installation und Rückweg vor der Durchführung

Die Vorbereitung allein dokumentiert noch keinen Upload. Den später
tatsächlich angeschlossenen Chip und die tatsächlich installierte App
erneut feststellen; parallel vorbereitete NFC-/Matrix-Stände dürfen nicht
stillschweigend als unverändert angenommen werden.

1. Quellcommit, APK-/Firmware-/Partitionstabellenhash sowie Build- und
   Testergebnisse des genauen Paars festhalten. Alte grüne Ergebnisse
   gelten nicht automatisch für spätere Änderungen.
2. Vorhandene APK privat sichern; neue App mit Datenerhalt und kompatibler
   Signatur installieren. Installierte APK zurücklesen und ihren SHA-256
   mit dem vorbereiteten Artefakt vergleichen. Zuerst die zugehörige App,
   danach die Firmware bereitstellen.
3. ESP-Chipkennung und 4-MiB-Flashgröße gegen den privaten Rückweg abgleichen.
   Den aktuellen vollständigen Flash neu sichern und am selben Chip
   verifizieren. Partitionstabelle und Schlüsselpartition getrennt prüfen.
4. Bei passender vorhandener Partitionierung ausschließlich die Anwendung
   ab **0x10000** schreiben und danach separat verifizieren. Kein Chip-Erase,
   keine neue Schlüsseleinrichtung. Die Partition `larpkeys` bei
   **0x310000**, Länge **0x6000**, muss zum Vorab-Abzug bytegleich bleiben.
5. Startprotokoll und Authentifizierung prüfen; Audio bleibt ohne Startauftrag.
   Für die physische Stille ist zusätzlich die Nutzerbeobachtung nötig.

Der zuletzt dokumentiert wiederhergestellte Wake-Stand ist `ca86b93` mit
Firmware-SHA-256
`316661b6daa41ff449ab944839dd18863f962f413ea1db0f35be2ba49c395378`.
Das ist ein dokumentierter Vergleichswert, kein Nachweis des aktuellen
Flashinhalts. Die zuletzt dokumentierte NFC-App kann inzwischen geändert
sein. Immer den tatsächlich gesicherten, chipgebundenen Stand als Rückweg
verwenden und die passende APK bereithalten.

Vor Rückkehr zu Wake zuerst Audio bewusst stoppen und Stille beobachten.
Ein Neustart des ESP allein beweist keinen Stop des separat versorgten
DFPlayers. Anschließend die gesicherte Anwendung wiederherstellen,
Anwendung und Schlüsselpartition verifizieren, normalen Start sowie
authentifizierte Appverbindung mit frischen Wake-Werten prüfen. Flashabzug,
Zugangsdateien und Gerätekennungen bleiben privat außerhalb Git.

## Softwareprüfung vor Installation

Die folgenden Befehle prüfen die genauen Quellen des später zu
installierenden Paars. Ergebnisse, Warnungen und Artefakthashes getrennt
vom Hörtest dokumentieren:

```powershell
python test/test_led_ble_wire/run.py
python firmware/led/tests/run_output_tests.py
python firmware/led/tests/run_audio_tests.py
python -m platformio run -d firmware/led -e larp-led-product -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:lintDebug :app:lintRelease :app:assembleDebug :app:assembleRelease
```

Die Wireprüfung verwendet echte HMAC-Implementierungen und gemeinsame
Referenzbytes. Sie deckt Audio-Domänen, Längen, Revisionen, Requestfolge,
Duplikate, Fristen und getrennte LED-/Audioantworten ab. Die Audio-Suite
prüft Boot-Stop, Stop-/Lautstärke-/Play-Folge, Grenzen, absichtlichen
Neustart, den Stopauftrag nach 30 s und UART-Fehler über einen asynchronen
Bus sowie den tatsächlichen SDK-Adapter gegen Stubs. LED-/Wake-Prüfungen
sichern die vorhandenen Funktionen ab. Android-Prüfungen decken Nachrichten,
Bedienfreigabe und die unabhängige Gültigkeitsfrist der Audioanzeige ab.

Diese Aufrufe installieren nichts. Ein erfolgreicher Firmwarebuild
bestätigt die SDK-Anbindung, keine angeschlossene Audio-Baugruppe.

## Gemeinsamer Ablauf

Nach jeder physischen Beobachtung die Antwort des Nutzers abwarten. Nicht
beobachtete kurze Wiedergabe wird nicht als bestanden gewertet; ein neuer
Start erfolgt nur bewusst als neuer Versuch.

| ID | Schritt | Pass-Kriterium / getrennte Beobachtung |
|---|---|---|
| A0 | App verbinden, keinen Start drücken | Frischer authentifizierter Status; Nutzer bestätigt keine Wiedergabe. BUSY-Ruhepegel notieren. |
| A1 | Kandidat 100 oder benannten Titel auswählen, Lautstärke 5 einstellen | Auswahl löst keine Wiedergabe aus; Nutzer bestätigt Stille. |
| A2 | Genau einmal Start drücken | App-/Boxauftrag und Uhrzeit erfasst; Nutzer hört und beschreibt den Titel. Eindeutige Inhaltszuordnung nur bei erkanntem Inhalt; BUSY-Verlauf separat notiert. |
| A3 | Während hörbarer Wiedergabe Stop drücken | Stopauftrag erfasst; Nutzer bestätigt Ende der Ausgabe. BUSY-Rückkehr separat prüfen. |
| A4 | Denselben Titel bewusst erneut starten | Nutzer erkennt einen neuen Beginn; keine automatische Wiederholung. |
| A5 | Während Wiedergabe nochmals Start drücken | Nutzer erkennt den Neustart desselben Titels. Danach bewusst stoppen. |
| A6 | Einmal ohne manuellen Stop laufen lassen, höchstens bis zum automatischen Stopauftrag nach 30 s | Natürliches Ende vor 30 s oder begrenzten Stop getrennt kennzeichnen; Nutzer bestätigt tatsächliche Stille. BUSY-Verlauf separat erfassen. |
| A7 | Während Wiedergabe Verbindung bewusst trennen | Erkannte Trennung fordert Stop; Nutzer bestätigt Stille. Zeit bis zur Trennung nicht als gemessene Garantie ausgeben. |
| A8 | Wiederverbinden und App neu öffnen | Frischer Status; keine selbsttätige Wiedergabe. Derselbe Titel startet erst durch bewussten neuen Auftrag. |
| A9 | Abschluss mit Stop | Stopauftrag und hörbare Stille getrennt bestätigt. Bei Rückweg anschließend Wiederherstellung nach obigem Ablauf. |

Falls ein zweiter bekannter Titel vorhanden ist, A1–A3 mit ihm wiederholen
und seinen Inhalt eindeutig zuordnen. Lautstärkestufen nur bewusst und
innerhalb der Prüfgrenze wechseln; das ist kein Linearitäts- oder
Schalldrucktest. Fehlende microSD, absichtlich falscher Titel, Unterspannung
und Umbauten werden im ersten gemeinsamen Lauf nicht provoziert.

## Ergebnisregeln

Für jeden Versuch drei Belege nebeneinander erhalten: **App-/Boxauftrag**,
**BUSY-Rohpegel/-Verlauf**, **Nutzer-Hörbeobachtung**. Ein „gesendet“ belegt
das vollständige Verlassen der ESP-UART durch die Befehlsbytes, keine
DFPlayer-Annahme oder korrekte Datei. `busyValid` bedeutet nur eine gültige
GPIO-Abtastung, nicht „Player gefunden“. BUSY LOW wird nur als beobachteter
Pegel mit konfigurierter Bedeutung
geführt, solange die reale Leitung nicht bestätigt ist.

Kein Ton bei ausgegebenem Auftrag bleibt ein fehlgeschlagener Hörversuch
mit offener Ursache. Hörbarer Ton ohne BUSY-Wechsel kann den Hörpfad
bestätigen; der BUSY-Pfad bleibt offen. Unerwarteter Titel, Knacken,
Verzerrung oder nicht endende Wiedergabe werden wortgetreu aufgenommen.
Nicht selbstständig durch andere Pins, Lautstärkeanstieg oder
unbeschränkte Wiederholungen nach einer Ursache suchen.

Softwaretests und Firmwarebuild prüfen die Umsetzung; sie schließen
keine der physischen Hör-/Verdrahtungsfragen. Dieser Plan enthält weder
eine Audioabnahme noch Strom-/Signal-/Dauerlaufmessungen. Der spätere
Gerätebericht nennt genaue Artefakte, Datum, Einzelresultate, Rohbelegorte
und offene Punkte.
