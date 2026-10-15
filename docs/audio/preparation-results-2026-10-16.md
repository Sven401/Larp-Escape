# Audio-Test: geprüfte Vorbereitung vom 16. Oktober 2026

**Status: softwareseitig vorbereitet; nicht installiert und nicht gehört.**
Geprüfter Implementierungscommit: `39422ed63b96cfb7e70e74aa31f10c8bd1244e65`.
Basis: `e00fe437081582727b2308da7f3b0c835c9bda75` (NFC-Teststand mit LED/Wake).
Sitzung: `01a141b2-046b-7d23-887a-26462c647a27`.
Die Ergebnisse stammen aus den finalen Produktdateien dieses Commits.
Nachfolgende Dokumentationscommits ändern diese geprüften Produktdateien nicht.

## Vorbereitet

Die Companion-App besitzt **Audio testen** mit Tracknummer1–255 (Vorgabe100),
Lautstärke1–10 (Vorgabe5) und bewusstem Start/Stop. UART1 RX25/TX26 sendet
Stop, Lautstärke und MP3-Ordner-Play; nach30 Sekunden wird Stop angefordert.
Boot und erkannter Verbindungsverlust fordern ebenfalls Stop an. GPIO13
bleibt unberührt. UART-Ausgabe, BUSY-Rohpegel und Höreindruck bleiben getrennt.
Der Nutzer bestätigt den Audio-Stand der Starwars-Con auf der Karte;
Titel100 ist ein Kandidat ohne bestätigten Dateiinhalt.

## Frische Softwareprüfungen

| Prüfung | Ergebnis |
|---|---|
| Wire/HMAC | 4.124 Assertions; echte HMAC-Implementierung und unabhängige Python-/C++-/Kotlin-Referenzbytes, Grenzen, Manipulation, Replay, Fristen, Auftragsfolge |
| Audio-Ausgabe | 10 Fälle bestanden: Boot, Start/Stop, Stop vor/bei Play, Grenzen, BUSY, 30-s-Stopauftrag, Fehler, Zeitüberlauf |
| UART-SDK-Adapter | 5 Fälle bestanden: Konfiguration, nichtblockierende Ausgabe, Teilübertragung, Fehler |
| LED/Wake | 16 LED-Fälle, 4 RMT-SDK-Fälle und Wake-/I2C-Prüfungen bestanden |
| Android-Protokoll | 63 Tests, 0 Fehler, 0 übersprungen |
| Android-App | 36 Tests, 0 Fehler, 0 übersprungen |
| Android Lint Debug/Release | jeweils 0 Fehler, 3 Warnungen: OldTargetApi, DataExtractionRules, MissingApplicationIcon |
| Android Debug/Release | beide Builds erfolgreich; Release-APK unsigniert |
| ESP32-Build | erfolgreich; RAM42.496/532.480 Byte, Flash1.134.453/3.145.728 Byte |
| Unabhängige Durchsicht | Wire, Android, Geräteschleife geprüft; Audio-Service nach Link-/Abbruchprüfung verschoben und Firmware anschließend neu gebaut |
| Änderungsprüfung | `git diff --check` ohne Befund; ausschließlich auftragsbezogene Dateien versioniert |

Aufrufe: [Testplan](test-preparation.md#softwareprüfung-vor-installation).
Android: JDK21.0.8, SDK35, Gradle8.9, Offline-Cache; JUnit-/Lintberichte
in den üblichen lokalen `build/`-Ordnern. ESP32: Arduino2.0.14/Espressif32 6.6.0.
Der native Compiler scheiterte zunächst in der Sandbox mit Windows-Fehler623;
dieselben Tests bestanden mit freigegebenen lokalen Buildwerkzeugen.
Eine Warnung im unveränderten Arduino-SDK `esp32-hal-uart.c` verhinderte
den erfolgreichen Firmwarebuild nicht.

## Artefakte (SHA-256)

| Datei | SHA-256 |
|---|---|
| `firmware.bin` | `7235bd6854582a5963832d99279de90dfacfd521e0026000dd6c243815f0272d` |
| `partitions.bin` (nur Abgleich) | `22790624ada38b3379c977dd7fd8452c6a3c86c00c64ec118380c888655e5b91` |
| `app-debug.apk` | `1224084661c387c9908deb12dde7779a5305b74577de7c0bd0209345626bd802` |
| `app-release-unsigned.apk` | `30485a51f353dcd2328132c1e32cfe596c0e31dc757049a91d0e707c01111c27` |

Das Testpaket enthält Debug-APK und Anwendungsfirmware. Die Partitionstabelle
dient nur zum Vorab-Abgleich. Editierbare Quellen bleiben im Arbeitsbranch/PR.

## Nächster gemeinsamer Schritt

Aktuellen Geräte-/Appstand feststellen, chipgebundenen Flash-Rückweg frisch
sichern und verifizieren, dann das passende Paar installieren. Parallel
entstehen NFC-/Matrix-Stände; frühere Protokolle beweisen nicht den aktuellen
Gerätestand. Hier wurden weder Handy noch Box verändert.

Danach [A0–A9](test-preparation.md#gemeinsamer-ablauf) einzeln durchgehen:
Ruhe, Auswahl ohne Ausgabe, Start mit Lautstärke5, Hörbestätigung, Stop,
Neustart, Zeitbegrenzung, Trennen und Wiederverbinden. Der
[Ergebnisbogen](test-record-template.md) bleibt bis dahin offen.
Verdrahtung, SD-Inhalt/Titelzuordnung, Audioausgabe und tatsächliche
Abschaltung sind nicht bestätigt. GitHub-CI ist ein gesonderter Nachweis
am veröffentlichten Commit, kein hier behauptetes lokales Ergebnis.
