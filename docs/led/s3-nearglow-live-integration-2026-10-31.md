# Näheglut live: App- und Stationsanschluss

Status: **beauftragte Umsetzung, Prüfungen laufen**, 31.10.2026.
Ausgangsbasis `b3e3693dc3904877c5e508273247ab495534f5d8`, Arbeitsbranch
`codex/s3-nearglow-android-live`. Auftrag: vorhandenes Larp Dev um echte
Livewerte beider S3 und während des autonomen Betriebs veränderbare
Funknähe-Helligkeitsbereiche erweitern.

## Umfang und Zuständigkeit

- Firmware/Plattform/Geräteadapter: `s3_firmware`.
- Android und Bedienbrief: `proximity_concept`, zunächst UI/UX-, danach Androidmandat.
- Unabhängige Architektur-/Security-/Qualitätsprüfung: `s3_review`.
- Koordination, Build-/Geräteausführung und Nachweisabgleich: Hauptinstanz.

Betroffene Varianten: zwei Waveshare ESP32-S3-Zero mit GPIO21-RGB-LED,
S3-Revisionen 0.1/0.2, und Larp Dev auf Moto G14/Android14.
Larp GM, Produkt-TLS-Bearer und D0/P0-Spielpakete sind kein Teil dieses Anschlusses.
Erforderliche Nachweise: reine Parser-/HMAC-/Kurven-/Lifecycle-Gegenfälle,
native S3-Builds, Android-Tests/Build/Lint, unabhängiger Code-/UI-Review,
beide echte BLE-Verbindungen zusammen mit autonomem ESP-NOW und sichtbarer Glut.

## Technischer Vertrag

Kanonisch: [PROX-BLE/1](prox-ble-v1.md),
[Bedienbrief](../ui-ux/s3-nearglow-live.md),
[vorheriger autonomer Geräteversuch](s3-nearglow-pair-2026-10-31.md).

Der begrenzte Devdienst nutzt neue UUIDs und individuelle per USB eingerichtete
Appzugänge. Das ESP-NOW-Paargeheimnis wird nicht in die App importiert.
Verbindungs-/Sitzungskontext, HMAC, Auftragsfristen, Revision und Readback
schützen die Übernahme; ein ATT-Schreiberfolg ist kein fachlicher Erfolg.
Das ist keine Fertigstellung des gesonderten Produkt-TLS-/Enrollmentpfads.

Die Station bleibt alleiniger Eigentümer von Funkmessung, Quellenfrische,
Glutberechnung und LED-Ausgabe. Die App empfängt passive Telemetrie und
sendet nur bewusste Kennlinienänderungen. Abbruch/Beenden der App löst weder
Stop noch Start aus. Kurvenpunkte bleiben auf den Geräten erhalten.
Die aktuelle bestätigte RGB-Ausgabe und das Helligkeitsziel sind getrennte Werte.

Vier monotone RSSI-Stützpunkte erlauben lineare Abschnitte und Plateaus.
Aus dBm wird keine Meterentfernung behauptet. Eine Zwei-Stationen-Änderung
hat zwei getrennte Ergebnisse; eine Teilübernahme bleibt sichtbar.
Ohne frische Partnerantwort blendet die Ausgabe weiterhin nach null aus,
unabhängig von den Kurvenendpunkten.

Die offizielle [ESP-IDF-6.1-Koexistenzbeschreibung für S3](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/coexist.html)
führt ESP-NOW-RX zusammen mit BLE-Verbindung als unterstützten stabilen
STA-Modus auf. WiFi und BLE teilen jedoch denselben Funkteil zeitlich;
connectionless Powersavefenster bleiben auf ihren Standardwerten.
Dies begründet die gewählte Variante, ersetzt keinen Last-/Gerätelauf.
API-/Callback- und mbuf-Besitzregeln werden zusätzlich an den tatsächlich
verwendeten Headern des qualifizierten SDK-Pins geprüft.

## Ausgangszustand und Rückweg

Die angeschlossenen S3 wurden über USB erneut ihren MACs zugeordnet:
COM4 `34:B7:DA:59:77:E8`, COM7 `E4:B3:23:F8:0F:0C`; COM3 bleibt unberührt.
Private Vorupdate-Abzüge, Manifeste und Logs liegen unter
`private/android-live-20261031/`. Beide enthalten 4.194.304 Byte und wurden
vollständig am jeweiligen Chip verglichen. Die eingebettete Anwendung ist
bytegleich zum bisher geprüften autonomen Abbild `daec0ad6…`.
Hashgleiche private Zweitkopien einschließlich bisheriger APK und Manifest:
`%LOCALAPPDATA%/LarpEscape/ESP32-Backups/s3-nearglow-android-live-20261031/`.
Kein Abbild oder Zugangsgeheimnis wird versioniert.

| Chip | Aktuelles Vorupdate-Vollbackup SHA-256 |
| --- | --- |
| COM4 / `34:B7:DA:59:77:E8` | `8f006782467ca58fcbaadf76527909ecf1b068aa3773b8beacf8a38bc35ed318` |
| COM7 / `E4:B3:23:F8:0F:0C` | `161ed54abe0dac13059dd223ca3e94f2213ef03f72ba70a24d6a6c2fccb5e7e7` |

Die erste COM4-Leseprobe war nach einem dazwischenliegenden Anwendungsstart
nicht mehr vollbytegleich; zwei Standard-NVS-Sektoren `0xb000/0xc000` hatten
sich geändert. Kein Inhalt wurde daraus gedeutet. Der wiederholte Vollabzug
wurde deshalb ohne Anwendungsstart zwischen Lesen und Vergleich verifiziert;
nur diese erfolgreiche Probe ist der maßgebliche COM4-Vorupdate-Rückweg.

Die tatsächlich installierte App wurde frisch vom Handy gesichert:
`de.larpescape.companion`, Version0.2/Code2, APK SHA-256
`2f4fa0aaa1e766e42a000a1173352f5942b2d52828563ec938cdd0db7946de2d`.
Sie ist nicht identisch mit dem früher nur statisch geprüften Kandidaten
`c814…`; ein gleicher Versionsname allein belegt keine Binäridentität.
Die neue App erhält eine höhere Versionskennung. Signaturen der vorhandenen
APK und des ersten neuen Debugbuilds sind bereits gleich:
SHA-256 `6b755048e55c8be19ef3f73b1a015f96735f6e79148ac8960bd244e6f9ba4837`.
Die Signatur wird vor tatsächlichem Update erneut geprüft; kein Deinstallieren
oder pauschales Löschen bestehender Appdaten.

## Tatsächlich ausgeführte Prüfungen

- Erster Android-Stand mit neuen Zustands-/Kennlinien-/Zugangsmodellen und
  Composekomponenten: `:core:protocol:test`, `:app:testDebugUnitTest`,
  `:app:assembleDebug` erfolgreich (Offlineprüfstand, JDK21/SDK35/Gradle8.9).
  Noch kein Nachweis des anschließend entwickelten BLE-Transports.
- Vorherige App auf echtem Handy geöffnet und Referenzbild
  `private/android-live-20261031/before.png` aufgenommen und angesehen.
  Große Schrift und knappe bisherige Navigation sind in den neuen
  responsiven Entwurf eingeflossen. Noch keine Sichtprüfung des fertigen Anschlusses.
- Anschließend vollständiger Android-Lauf mit Protokoll-/App-JVM-Tests,
  Debug-/Release-Build, Debug-/Release-Lint sowie GM-Tests und GM-Debugbuild
  bestanden. Nach den gezielten Frische-/UI-Korrekturen erneut Protokoll- und
  App-Tests, beide Builds und beide Lints bestanden. Zwölf neue Tests an
  Credential-, Wire- und Zustandspfaden sind in den JUnit-Ergebnissen erfolgreich.
- Signaturgleiches Update 0.3/code3 am Handy installiert, zunächst ohne
  Stationszugänge. Erste lange Ansicht wurde nach tatsächlicher Sichtprüfung
  gezielt zu kompakten A/B-Karten mit aufklappbaren Details geändert.
  `nearglow-compact-empty.png` zeigt beide Karten im oberen Viewport;
  diese Leerdarstellung ist unabhängig angesehen, aber noch kein BLE-Gerätelauf.

Weitere konkrete Software-/Geräteergebnisse, geprüfte Commits und Einschränkungen
werden nach der Implementierung ergänzt. Keine erforderliche Abnahme wird aus
diesen ersten Builds oder dem Vertragsentwurf abgeleitet.
