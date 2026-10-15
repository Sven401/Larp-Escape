# Technische Projektübergabe – 13. Oktober 2026

## Ergänzung: eigenständige LED-Produktfunktion, 15. Oktober 2026

Der neue Produktzweig ergänzt die echte Android-App und einen geschützten
BLE-Zugang mit eigener Effektberechnung und SDK-RMT-Ausgabe. Umfang, Quellen,
USB-Einrichtung und Geräteabnahme stehen in
[firmware/led/README.md](../firmware/led/README.md).
Die [gemeinsame Geräteprüfung](../docs/led/device-test-2026-10-15.md)
bestätigt Funkeln, Stop sowie Lichtimpuls/Ripple mit selbstständigem Aus am
installierten eigenen Paar; weiterführende Abnahmefälle bleiben offen.
Die frühere Einbindung einer externen Beispielimplementierung war eine
Fehlinterpretation und ist zurückgenommen; die Owner-Klarstellung steht in
[PR #44](https://github.com/Sven401/Larp-Escape/pull/44).
Die Spielintegration bleibt getrennt. Die folgenden Abschnitte beschreiben
den historischen Übergabestand vom 13. Oktober, nicht die neue LED-Abnahme.

## Einstieg und Stand

Dieser Branch `plan/projektuebergabe-kuratierter-stand` beginnt bei
`a40365d715b2055ee23f767fa53b55ac25243437` (Merge von PR 24 auf
`SvensBranch`). Er integriert die Firmware-Grundlagen E1–E4/F0 aus
`e973818edde56be4f4907a024f9c4f347802a3cc`, die Android-Grundlagen
A0–A3 aus `6383c5906b8fb63748cf4b8de6e9182802b83fa4` und das
Hardware-Quellen-/Schnittstellenpaket aus
`2204d5447236601ed339b6068edb2a830f34743b` durch getrennte
Merge-Commits. Die ursprünglichen Branches und Worktrees wurden dabei nicht
umgeschrieben. Die Zusammenführung ist ein Entwicklungsstand, keine
Geräte-, Funk- oder Fertigungsabnahme.

Die frühere Diagnose-App samt Design und paketweiser Fortsetzung ist am
13. Oktober 2026 als gescheitert verworfen worden. Ihr Code bleibt als
historische Quelle im Diagnose-Archiv; sie ist kein Einstieg für eine
Hardwareabnahme. Das fortbestehende Nutzerziel ist eine am Handy auffindbare,
verständliche und bewusst wiederholbare echte Baugruppenbedienung. Erster
Referenzfall: LEDs starten, stoppen und bewusst erneut starten, mit
korrelierter Geräteantwort und getrennter Nutzerbeobachtung. Am ursprünglichen Übergabestand vom 13. Oktober war dieser Ablauf
weder implementiert noch physisch abgenommen. Der aktuelle begrenzte LED-Nachweis
steht in der oben verlinkten Geräteprüfung vom 15. Oktober.

## Wake-Inputs: gemeinsamer Gerätetest

Die [Wake-Testvorbereitung](../docs/wake/test-preparation.md) führt den neuen
Umfang auf Basis des gemeinsam geprüften LED-Paars: vier Live-Kacheln,
2×2-Tauschvorschau, gespeicherte Anordnung sowie getrennte Software- und
Geräteabnahme. Der [Gerätetest vom 15. Oktober](../docs/wake/device-test-2026-10-15.md)
belegt das installierte Paar `ca86b93`, die räumliche Zuordnung **2 / 1 / 4 / 3**,
Vorschau und Tausch, Loslassen außerhalb/in Rasterlücken sowie Speicherung
nach Neustart/Wiederverbindung.
Die funktionale Sensorreaktion ist beobachtet; Chipmarking und reale Leitung
von Wake 1/B7 bleiben physisch offen.

## NFC: Vorerkundung und echter Tag

Aktueller NFC-Gerätenachweis vom 16. Oktober 2026:
[Handy-Vorerkundung](../docs/nfc/device-preparation-2026-10-16.md) mit zwei
HCE-Zyklen und [ein echter NTAG213-Rundlauf](../docs/nfc/physical-tag-test-2026-10-16.md)
App → Tag → Box → Tag → App sind belegt. Der echte Lauf erforderte nach
zunächst unklarem App-Schreibergebnis ein erneutes Lesen; kein zweites
Schreiben. Die begrenzte NFC-Prüffirmware ist temporär, Wake wurde jeweils
verifiziert wiederhergestellt. Andere Tagtypen und Spielintegration bleiben offen.

## Architektur und Start

Die Firmware liegt in `include/`, `src/` und `lib/`. Die Ausführung trennt
Spiel-/Profilmodell, Geräteplanung, Companion-Vertrag und konkrete Adapter;
`src/main.cpp` ist der Produktstart. Verträge und Fixtures stehen unter
`docs/companion/`, Konfigurations- und Laufzeitverträge unter
`docs/configuration/` und `docs/runtime.md`. `platformio.ini` enthält
getrennte Windows-Hosttests, Produktionsbuild und synthetische ESP32-Proben.

Die Android-Entwicklungsbasis liegt unter `companion/android/` (Kotlin,
Compose, `app` und `core:protocol`). Ihr aktueller Produktpfad ist eine
lesende BLE-Verbindungsprüfung; der A3-Peer-Auth-Kern ist hostseitig. Für
Voraussetzungen, Berechtigungen und Buildbefehle siehe
`companion/android/README.md`. Ein Debug-Build ist kein freigegebener
Spielleitungsclient. Der Webprototyp und die .NET-Diagnose sind separate
historische Quellen.

Die editierbaren und exportierten Hardwareeingänge stehen unter `Hardware/`.
`QUELLENINVENTAR.md`, `SCHNITTSTELLENABGLEICH-2026-10-10.md` und
`HARDWAREVERTRAG-V0.1-2026-10-11.md` kennzeichnen Zuordnung und Pinvertrag
ausdrücklich als vorläufig. Originalbox, CAD-Revision und Softwareannahmen
sind nicht gleichzusetzen.

Zum Start ohne Hardware:

```powershell
pio test -e host-regression
pio test -e host-configured-control
pio run -e az-delivery-devkit-v4 -j 2
cd companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug
```

Der Android-Build braucht JDK 17/21, Android SDK 35 und auf einem neuen
Rechner herunterladbare Gradle-Abhängigkeiten. Für weitere Umgebungen und
isolierte ABI-/Kryptografieprüfungen siehe `platformio.ini` und die
jeweiligen README-Dateien. Die tatsächlichen frischen Ergebnisse und
Grenzen stehen in [PRUEFSTAND.md](PRUEFSTAND.md).

## Quellarchiv und Wiederaufnahme

[quelleninventar.json](quelleninventar.json) erfasst die 40 vorgefundenen
Produkt-Worktrees, alle 41 vorherigen lokalen Branchspitzen, offene
Dateien und begründete Ausschlüsse. Von diesem Branch zweigen lokal ab:

| Branch | Inhalt |
| --- | --- |
| `plan/projektuebergabe-firmware-f1-archiv` | F1-Bond-/Wiederverbindungszweige und vier offene Firmware-Worktrees |
| `plan/projektuebergabe-android-entwuerfe-archiv` | frühere Android-Basis und unveröffentlichte Companion-Dateien des Hauptcheckouts |
| `plan/projektuebergabe-hardwarequellen-archiv` | frühere Hardware-/Prüfzweige, zusätzliche CAD-/POC-Quellen und EasyEDA-Brückenpatch |
| `plan/projektuebergabe-diagnose-historie-archiv` | sämtliche gestoppten Diagnosezweige, offene Guided-UI und vorhandener Bericht |
| `plan/projektuebergabe-led-entwurf-archiv` | aktueller, noch unversionierter LED-Steuerungsentwurf |
| `plan/projektuebergabe-experimente-archiv` | übrige Versuche und lokale Konfigurationsänderungen |

Jeder Archiv-Branch behält seine Original-Commitspitzen als Git-Eltern
erreichbar. Die Dateien im Checkout bleiben der kuratierte Stand; offene
Änderungen liegen unter `handoff/quellarchiv/` mit Manifest,
SHA-256-Prüfsummen, ursprünglichem relativem Pfad und Patch. Einen Patch
nur in einer separaten Kopie seines bezeichneten Quell-Commits anwenden.
Archiv-Branches sind Quellenlager, keine lauffähigen Produktvarianten.
Verschachtelte Worktrees werden über ihre eigenen Einträge erfasst;
erzeugbare Caches und Buildprodukte wurden nicht kopiert.

Die Übergabe ist eigenständig. Ein separates früheres KI-Repository wird
für Verständnis, Build und Bewertung nicht benötigt. Alle neuen Branches
sind ausschließlich lokal vorhanden; ihre spätere Übertragung ist ein
eigener Schritt.

## Offene Entscheidungen und Grenzen des ursprünglichen Übergabestands

- F1-SDK-/SMP-Herkunft, Bond-Wiederverbindung und Rechtebindung sind nicht
  integriert oder am Funk abgenommen; dafür die F1-Quellen einzeln prüfen.
- Eine echte, wiederholbare LED-Bedienung in Android samt Firmwareantwort,
  Stop und erneuter Auslösung fehlt. Der verworfene Diagnoseclient ist
  hierfür kein akzeptierter Ersatz.
- Die Hardware-Unterlagen enthalten keine bestätigte CAD-Netzliste, kein
  fertiges PCB und keine ERC-/DRC- oder Fertigungsfreigabe. GPIO15 für die
  reale LED-Datenleitung ist eine Nutzerbeobachtung ohne vollständiges
  Messprotokoll. Die Zuordnung von Wake/Matrix, der MCP23017-B7-Einsatz,
  Audio, NFC und sichtbare LEDs bleiben physisch offen.
- Die Originalbox trägt einen getrennten Test-Firmware-Stand. Vor jedem
  weiteren Upload gilt der chipgebundene Flash-Rückweg in der vorhandenen
  Hardwaredokumentation; diese Übergabe lädt oder restauriert nichts.
