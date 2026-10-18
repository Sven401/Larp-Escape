# Nativer Stationspilot: auswählen, verbinden, bestätigten Zustand lesen

**Status: implementiert und an der lokalen Box begrenzt geprüft.**
Stand: 19. Oktober 2026; Umsetzung und Geräteprüfung in der Nacht 18./19.10.
Quelle: Nutzerauftrag in Sitzung `01a150b6-981d-7791-a37d-34fac8dd18a3`, einschließlich
bestätigter lokaler Vorbereitung und Test an der lokalen Box.
Quellbasis vor Umsetzung: `683fc2b0226705cbc128e753ebfda357d0a24469`.
Implementierungsstand: `f657a94796c894fb2c1ba5a7ad7aa82c35f4cb46`;
geprüfte APKs aus inhaltlich identischem Buildbaum vor diesem Commit.
Designgrundlage: [Stationspilot](../ui-ux/BOX-PILOT.md),
[Spielleitungsabläufe](../ui-ux/SPIELLEITER-APP-ABLAEUFE.md).

## Bedienung und Umfang

1. **Station vorbereiten:** Veranstaltung, Station und Bluetoothadresse der konkret
   zugeordneten Box angeben. Die vorhandene vertrauenswürdige Box-Zugangsdatei
   bewusst im Android-Dateidialog auswählen. Speichern bereitet nur dieses Handy vor.
2. **Station antippen:** Die App sucht ausschließlich dieses vorbereitete Funkziel,
   verbindet, prüft die vollständige Boxidentität gegenseitig und liest den signierten
   Zustand. Es gibt keine Suche-/Kandidatenwahl im normalen Stationsablauf.
3. **Bestätigten Zustand lesen:** etwa „Bereit“, „Licht aus“, „Lichtbetrieb“.
   Die Betriebsart LED_ONLY wird nicht als „Spiel gestoppt“ umgedeutet.
4. **Unterbrechen und zurückkehren:** Alte Werte verschwinden sofort. Nach Rückkehr
   oder Behebung fehlender Systemvoraussetzungen wird neu verbunden und geprüft.
   Eine bewusste Pause bleibt pausiert, bis „Erneut verbinden“ gewählt wird.

Dieser beauftragte Schnitt liest Zustände. Licht-Start/Stop aus dem umfassenderen
Designpilot, Spielsteuerung, Rollenverwaltung und Veranstaltungsverteilung sind
Folgearbeit. Es gibt dafür keine funktionslosen Bedienelemente und keine simulierte
Bereitschaft. Der bestehende Baugruppen-HMAC-Zugang verleiht keine zusätzlichen
allgemeinen Spielleitungsrechte nach VA-01/F1.

Die App-Auswahl behält **Larp GM** mit GM-Companion-Originalmotiv und **Larp Dev**
mit DEV-Badge. GM-Version jetzt Code 2 / 0.2. Dev bleibt unverändert Code 2 / 0.2.

## Zustands- und Verbindungsregeln

| Fall | Verhalten |
| --- | --- |
| Neue Installation / Prozessneustart | Gespeicherte Stationsliste; keine alte Sitzung und kein alter Zustand wiederhergestellt |
| Bluetoothzugriff fehlt | Systemfreigabe bzw. App-Einstellungen anbieten; nach Freigabe automatisch fortsetzen |
| Bluetooth ausgeschaltet | Systemaktion anbieten; nach Einschalten automatisch prüfen |
| Verbinden | Zehn Sekunden gezielte Suche, höchstens zwanzig Sekunden Verbindung; Anmeldung zehn Sekunden, Fragmentierung zwei Sekunden |
| Bestätigter Status | Gegenseitiger V2-HMAC-Nachweis, passende Geräte-/Client-/Sitzungsbindung; nur steigende Statussequenz erneuert die Beobachtung |
| Status mindestens 1000 ms alt | Anzeige entwerten und Sitzung schließen; „Status nicht aktuell“, keine Behauptung „Licht aus“ |
| Verbindungsabbruch / Statusablauf | Ein automatischer Wiederverbindungsversuch nach 700 ms pro Stationsöffnung/Vordergrundphase; danach bewusster Retry, keine endlose Suchschleife |
| Fehlende/falsche Einrichtung | Gesonderte Hilfe und gezielte Korrektur; „Erneut prüfen“ verändert weder Zuordnung noch Rechte |
| Abbrechen / Liste / Hintergrund | Scan, Verbindung und Timer schließen; verspätete Rückrufe über Generationen verwerfen |
| Stationswechsel | Alte Bindung schließen; Zustand und Antworten nicht auf neue Station übertragen |

Bluetoothadresse und Namen sind lediglich vorbereitete Zuordnung und Orientierung.
Erst der authentisierte V2-Nachweis bestätigt die Box. Der Transport besitzt
keine Licht-, NFC- oder Spielaktion: Ausgehend sind ausschließlich Hello und Proof
zugelassen. Die unveränderten [V2-Bytes und Fristen](../led/ble-wire-v2.md) gelten.
Eine aktive Steuerverbindung je Box bleibt maßgeblich; parallele Apps begründen
keine zweite gleichzeitige Boxsitzung.

## Eigene GM-Daten und Modulgrenzen

- `:gamemaster` verwendet ausschließlich `:core:protocol` als gemeinsame Bibliothek.
  Keine Abhängigkeit auf `:app`, keine Einbindung seiner Quellen und keine Dev-Klassen
  im GM-DEX. Dev- und Protokollquellen sind gegenüber `683fc2b` vollständig unverändert.
- Eigene atomare Datei `noBackupFilesDir/stations-v1.bin`, eigener nicht exportierbarer
  Keystore-Alias `larpescape-gamemaster-stations-v1`, AES-256-GCM mit zufälliger IV.
  Metadaten und importierter Zugang werden gemeinsam verschlüsselt gespeichert.
- Strikter versionierter Binärkatalog, höchstens 32 lokale Stationen und 128 KiB
  verschlüsselter Datei; Zugangsdatei höchstens 2048 Byte. Die Stationskapazität ist
  keine Beschränkung der Zahl berechtigter Handys im fachlichen Zugangskonzept.
- Doppelte Boxidentitäten, Bluetoothadressen und Stationsnamen je Veranstaltung
  werden abgelehnt. Bearbeitung erfolgt aus der Stationsliste unter „Vorbereitung“.
- Beschädigte oder nicht entschlüsselbare Daten werden weder überschrieben noch
  durch neue Schlüssel ersetzt. Kein automatischer Import privater Dev-Daten.
- GM verlangt ausschließlich BLUETOOTH_SCAN (neverForLocation) und BLUETOOTH_CONNECT;
  kein NFC, HCE, Bluetooth-Advertising oder Internet. Backup bleibt ausgeschaltet.

Quellen: `StationController`, `StationProof`, `AndroidStationLink`, `StationVault`
und `StationCatalogCodec` unter
`companion/android/gamemaster/src/main/java/de/larpescape/gamemaster/`.

## Tatsächliche Prüfungen

**Rechner:** Projektwrapper Gradle 8.9, JDK 21.0.8, SDK/Build Tools 35.
Beide Apps als Debug und unsigniertes Release gebaut, vier Lintläufe ohne Fehler.
Je Lauf drei bekannte Warnungen (OldTargetApi, DataExtractionRules, ObsoleteSdkInt),
bei Dev zusätzlich ein Informationshinweis zur Compose-State-Erzeugung.

- 88 Protokolltests und 76 Dev-Tests weiterhin erfolgreich (unveränderte Quellen;
  Gradle konnte diese Ergebnisse im abschließenden Lauf als aktuell wiederverwenden).
- 26 neue GM-Tests erfolgreich: 16 Zustands-/Lebenszyklusfälle, fünf echte HMAC-Fälle
  mit vorhandenen unabhängigen V2-Goldenbytes und fünf Katalog-/Grenzfälle.
- Zehn Python-Tests der Appgrenzen und der Prüfer für alle vier APKs erfolgreich.
  Beide Android-CI-Jobs enthalten jetzt auch die GM-Unit-Tests.
- Keine fremden Änderungen im Hauptcheckout übernommen; keine Zusammenführung
  mit dem neueren Integrationsstand ausgeführt.

**Gerät:** Moto G14, Android 14 / API 34, bestehende lokale V2-Box. Ausschließlich
GM wurde mit gleicher Paketkennung und Signatur aktualisiert. Die Dev-Installation
wurde weder aktualisiert noch deinstalliert, geöffnet oder zurückgesetzt.

| Gerätefall | Beobachteter Nachweis |
| --- | --- |
| Lokale Vorbereitung | „Lokale Vorbereitung“ / „Lokale Box“ über native Form und Dateidialog gespeichert, keine Demo-Station vorgeladen |
| Zugang | Bestehenden Datensatz über den vorhandenen vertrauenswürdigen USB-Lesebefehl ausgelesen; keine Firmware installiert, kein Schlüssel neu erzeugt |
| Normalweg | Stationsklick und erforderliche Systemfreigabe führen automatisch zu „Bereit“, „Licht aus“, „Lichtbetrieb“ |
| Bluetooth aus/ein | Alter Zustand verschwindet; nach Einschalten ohne manuelle Suche wieder authentisierter aktueller Zustand |
| Hintergrund/Rückkehr | Erneute automatische Anmeldung und frischer Zustand |
| Bewusste Pause | Pause bleibt auch nach Hintergrund/Rückkehr erhalten; erst bewusste Wiederverbindung liest neu |
| Prozessneustart | Gespeicherte Station in der Liste, kein behaupteter Altzustand; Stationsklick liest Zugang aus verschlüsseltem Katalog und bestätigt neu |
| Schrift 200 % | Zustand lesbar und Pausenaktion erreichbar; lokale Aufnahme tatsächlich angesehen |
| Rotation | Gemessenes Querformat 2400 × 1080; gewählte Station und frischer Zustand nach Neuerstellung; Aufnahme angesehen |
| Datentrennung | Alle zehn zuvor erfassten Dev-Dateien nach GM-Update und vollständiger Geräteprobe SHA-256-identisch |
| GM-Speicherung | Echter Katalog verschlüsselt; Einrichtungsgeheimnis und Stationsmetadaten nicht als Klartext enthalten; Entschlüsselung nach Prozessneustart erfolgreich |

Die Funkadresse der bekannten lokalen Box wurde zunächst aus der vorhandenen
ESP32-Konfiguration mit vier universellen MAC-Adressen und der dokumentierten
Basisadresse abgeleitet, dann durch die tatsächliche Verbindung samt vollständiger
HMAC-Identitätsprüfung bestätigt. Grundlage der Ableitung:
[Espressif, ESP-IDF 4.4.7: MAC Address](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/system.html#mac-address).
Dies ist ein Prüfweg für diese lokale Box, keine automatische Adressableitung im Produkt.

Installierte GM-Debug-APK: SHA-256
`a718aa579838d36ecc34123f1ff0cd9052ef69553a2b32dbcfdb26756871b517`.
Die zurückgelesene installierte APK stimmt bytegleich mit dem geprüften Build überein.
Keine Licht-/Spielaufträge, kein NFC-Schreibversuch und kein Firmwareupload.
Displayeinstellungen nach Schrift-/Rotationstest wiederhergestellt; testweises
USB-Wachhalten danach ausgeschaltet.

Roharchive, Einrichtungsdatei und Screenshots bleiben ausschließlich im ignorierten
lokalen Verzeichnis `companion/android/gamemaster/build/native-pilot-verification/`.
Die temporäre Zugangsdatei in Downloads des Handys wurde nach bewusstem Import
entfernt. Keine Schlüssel, privaten Gerätedaten oder Screenshots im Pull Request.

## Verbleibende Grenzen

TalkBack, mehrere reale Stationen, Langzeitbetrieb und tatsächlicher Funkverlust
bei weiterhin eingeschaltetem Bluetooth bleiben Gerätefolgeprüfungen. Deren
Zustands-/Generationsregeln sind durch die genannten Modelltests geprüft.
Es gibt keine neue allgemeine Spielleitungsrechte-, Profil- oder Spielabnahme und
keine optische Abnahme allein aus der signierten Lichtmeldung. Start/Stop folgt
als eigener nächster Schnitt auf dieser bestätigten Stationsverbindung.
