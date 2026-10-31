# Näheglut live: App- und Stationsanschluss

Status: **App 0.3.3/code6 und passende Firmware installiert; begrenzter
Zwei-Stationen-Geräteversuch einschließlich Quellenverlust/Rückkehr bestanden**,
31.10.2026.
Ausgangsbasis `b3e3693dc3904877c5e508273247ab495534f5d8`, Arbeitsbranch
`codex/s3-nearglow-android-live`. Auftrag: vorhandenes Larp Dev um echte
Livewerte beider S3 und während des autonomen Betriebs veränderbare
Funknähe-Helligkeitsbereiche erweitern.

## Aktueller geprüfter Kandidat

Gemeinsamer Implementierungsstand `07750d44e5d9053acf1c0d1af9143087cf1bbf79`.
Firmwareimage 1.030.560 Byte, SHA256
`6dadac8c4fcc6092eb1805a3e5909c57b9e45a232b88e0b2b89c487bf3740712`;
installiertes Debug-APK 0.3.3/code6, SHA256
`d4a839f14aed7bb68fbb420d5e7944b137465045398315ea7d8213e12cf2741e`.
Beide S3-Anwendungen und die erhaltenen Zugangs-/Kurvendaten wurden nach Upload
mit `verify-flash` verglichen. Nur die Anwendung an `0x10000` wurde aktualisiert.
Beide Stationen speichern jetzt die ansteigende Defaultkurve als Revision 3.

- Fünf vollständige Neuverbindungen je Station erfolgreich: fünf verschiedene
  AuthOK je Slot, beide Geräte-Epochen5, identischer App-Prozess18327 und fünf
  Endscreens mit zwei aktuellen Karten. Zwei zeitversetzte gefilterte Logausschnitte
  ergänzen sich, weil der begrenzte Android-Ringpuffer ältere Zeilen verdrängt;
  ihre deduplizierte Union enthält ausschließlich tatsächlich vorhandene
  Original-Logzeilen sowie einen gesonderten Provenienzheader.
- Tatsächliche MTU256/Framebudget244 auf beiden, Proof jeweils ein Fragment.
  In Epoch5 sind Proof/GET/SET je drei erfolgreiche Schreibzugriffe; Notify
  A534/534 und B529/529 erfolgreich an SDK übergeben, Busy/Allokationsfehler/
  Reservevertagungen0, abgetastetes Poolminimum11. SDK-Erfolg ist kein eigener
  Funkempfangsnachweis. Beide UI-Endanzeigen sind separat gesichert.
- 180s paralleler ESP-NOW-Gerätelauf: je351/351 Proben frisch, keine Lesefehler,
  Fault0, Altermaxima502/497ms. Innerhalb desselben Handy-Verbindungspaars wurde
  die Kurve bewusst übernommen: beide dauerhaft Revision3, Glut-Ziel200→1000,
  bestätigte RGB-Ausgabe4/1/0→24/8/0. Der Nutzer bestätigt beide LEDs heller.
- Kontrolliertes Verlassen der App: je39/39 Proben rund20s frisch, Fault0,
  weiterhin ARM, Ziel1000 und RGB24/8/0. Anschließendes Verbinden erfolgt ohne
  automatischen SET-Auftrag. Stationen bleiben Eigentümer der Szene.
- Unabhängige Code-, Größen-, HMAC-, Fristen- und sichtbare UI-Prüfung;
  aktuelle Native-/Android-/GM-Builds und passende Tests bestanden. Die folgenden
  Abschnitte erhalten die getrennten Diagnose- und Zwischenstände.

Physischer Quellenverlust ist zusätzlich belegt: COM7 tatsächlich abgezogen,
A meldet bei weiterhin aktiver Handyverbindung fehlenden Partnerfunk,
Glut-Ziel0 und bestätigte RGB0/0/0; B zeigt unbekannte aktuelle Handywerte.
Der Nutzer bestätigt das sichtbare Ausblenden. Nach Wiedereinstecken kehren
Glut und Partnerfunk ohne Startbefehl zurück, vom Nutzer bestätigt.
Die gesonderten A-Protokolle enthalten39 beziehungsweise72 ungültige Proben,
anschließend165 frische Rückkehrproben; alle Fault0. Beide UI-Zustände wurden
separat gespeichert und unabhängig geprüft.

B war bei der nächsten Bedienabfrage bereits wieder live; deshalb wurde kein
weiterer Verbindenklick ausgeführt. Wer diese Handyverbindung initiiert hatte,
ist nicht protokolliert; kein automatischer Reconnect wird daraus abgeleitet.
USB bestätigt nach dem echten B-Neustart weiterhin dauerhaft gespeicherte und
angewandte Revision3 mit den Defaultpunkten. B-Epoch1 hat nur zwei erfolgreiche
Schreibzugriffe (Proof/GET), keinen neuen SET. A bleibt im bestehenden Epoch6.
Abschließend je16/16 frische Proben beider Stationen, Fault0, ARM dauerhaft,
Ziel1000 und RGB24/8/0. Der Nutzer bestätigt nach der Kurvenübernahme beide
LEDs sichtbar heller als beim20-%-Plateau.

Größere systematische räumliche Annäherung, elektrische Last-/Fehlerinjektion,
Stromausfall während NVS-Commit
und vollständige RF-/Security-/Produktqualifikation sind getrennte Nachweise.
Der Entwurf [PR119](https://github.com/Sven401/Larp-Escape/pull/119) bleibt wegen
der neuen Vertrauensgrenze offen; keine Integration nach main und kein Release.

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

## Konkreter Geräteversuch: Zwischenstand

Firmware-/Featurecommit `deb9ab986c6b74255a1755a2e5d9f3fce0db5b2a` ist nach
unabhängiger Source-/Securityprüfung und erneutem SDK-Build auf beiden S3
installiert. Anwendung `s3-proximity-live-v1`, 1.028.576 Byte, SHA-256
`986514bffb7604a69e0f8290bce43294d5d49bafe494c4b02224d95a6bd76379`.
Die unveränderten vier Flashadressen und DIO/80MHz/4MiB sind am Buildmanifest
geprüft, alle Schreibhashes bestätigt; Bootloader, Partitionstabelle und
Anwendung wurden pro Chip zusätzlich unabhängig verglichen. Keine NVS-Löschung.
Paarung und ARM blieben erhalten. Neue individuelle Handyzugänge und die
fehlende Defaultkurve wurden nur durch den ausdrücklichen USB-Export eingerichtet.
Klartext-Importdateien wurden nach dem Import vom Handy entfernt; die App hält
getrennte Keystore-geschützte Slots.

Die erste reale beidseitige Anzeige zeigte unterschiedliche aktuelle Werte
(z.B. A -51dBm/78%, B -49dBm/82%) und passende LED-RGB-Ausgaben. Ein paralleler
12-Sekunden-USB-Lauf lieferte je 24/24 frische, fehlerfreie Proben. Der Editor
zeigte für doppelte dBm-Punkte eine gesperrte Übernahme; reine Entwurfsänderungen
änderten die von USB gelesene Revision 1 nicht.

Ein bewusst übernommenes 20-%-Plateau wurde auf beiden Stationen als Revision2
dauerhaft bestätigt, einschließlich unabhängiger USB-Abfrage. Der Nutzer
bestätigte beide LEDs sichtbar dunkler. Nach vollständigem Beenden der App
blieben beide Stationen je 16/16 Proben frisch, Fehler0, Ziel200 und RGB4/1/0.
Nach neuem Appverbinden blieb Revision2 erhalten, ohne automatische SET-Wiederholung.

**Geräteabnahme noch offen:** Der Nutzer meldete danach Abstürze beim Verbinden
von B. Der gezielt gefilterte Crashbericht und unabhängiges `javap` bestätigten
einen P1: Unqualifiziertes `run` in fünf GATT-Callbacks band an Kotlin-stdlib
statt an den vorgesehenen Session-Guard. Die bisherigen Zustands-/Protokolltests
hatten die tatsächliche Callbackbindung nicht ausgeführt. Die erste positive
Quellbewertung wird deshalb nicht als Callback-Sicherheitsnachweis fortgeführt.

Fixcommit `77065849d1564698d6bfa57f8bdab484d7365f3a`, App0.3.1/code4, bindet
alle fünf Callbacks explizit an den produktiven geschützten Dispatcher.
Vier neue Tests einschließlich der tatsächlichen kompilierten Callbackklasse
sowie unabhängige Bytecodeprüfung bestätigen die Bindung; vollständiger
Debug-/Release-/Test-/Lint-Lauf erfolgreich. Der erste Testlauf wurde wegen
der falschen Annahme einer Datei- statt AGP-JAR-Klassenressource verworfen und
nach portabler Korrektur erfolgreich wiederholt.

Der korrigierte App-Prozess blieb beim nächsten beobachteten Verbindungsfehler
stabil; beide Links wurden jedoch noch mit einer abgefangenen Fehlermeldung
geschlossen. Die genaue Ursache wird mit ausschließlich öffentlichen
Callback-/Exceptionklassen-/Stackframe-Diagnosen eingegrenzt. Kein Payload,
Schlüssel, Exceptiontext oder fremder Appbericht wird dafür protokolliert.
Der bisher geplante Funkverlust-Appversuch ist **nicht bestanden/noch offen**:
Die zugehörigen 237 USB-Proben waren durchgehend frisch, COM7 blieb angeschlossen.
Er wird nach stabiler Zweier-Verbindung wiederholt.

Diagnosecommit `9a49ffd` ergänzt begrenzte öffentliche Abbruchmetadaten über
`BLE-DIAG` sowie ausschließlich im Debugbuild Handshakephasen der App.
Unabhängig geprüfte Hostläufe: Core400, Adapter165, Protokoll319, Runtime60,
zehn tatsächliche Bearerfälle mit 828 Assertions; sechs Callbacktests erfolgreich.
Der erste Diagnoseentwurf konnte bei Disconnect mit voller Ingressqueue den
ursächlichen Fehler überschreiben. Ein epochbezogener Latch und der konkrete
Gegenfall korrigieren dies vor dem Geräteupload. SDK-Build erfolgreich.
Beide S3 erhielten nur das Anwendungsimage an `0x10000` (1.029.232 Byte,
SHA256 `fa3c0ab5d5e4e557ced9b7c4b30196a997eb770f0fe91cde6540dd929d7b3043`).
Anwendung und zusätzlich gesicherte aktuelle `larpkeys`-Partition wurden danach
jeweils durch `verify-flash` bestätigt; Paarung, Zugänge und Revision2 unverändert.

Der wiederholte Geräteversuch bestätigt bei beiden Stationen Grund4
(absolutes Telemetriealter), Nachrichtentyp132: A Offset84/Alter1000ms,
B Offset144/Alter1006ms. Die App hatte vorher auf beiden echte AuthOK erhalten
(A1976ms/B2827ms nach Verbindungsanforderung). Android meldet danach Status19;
der App-Prozess bleibt erhalten. Die ausgehandelten Verbindungsintervalle waren
45ms. Der Befund belegt das erreichte Sendebudget einschließlich FIFO-Wartezeit,
keine reine Funkdurchsatzmessung. `sdkCode=0` ist für diesen Abbruchgrund der
Default und schließt vorangehende Busy-Retries nicht aus. Nächster gezielter
Vergleich ist eine Android-Anfrage höherer Verbindungspriorität mit identischer
Firmware und unveränderten Fristen. Eine Anfrage allein gilt nicht als Erfolg.

Der HIGH-Vergleich handelt nach Discovery auf beiden Links tatsächlich 15ms
statt 45ms aus. Nach einem abgefangenen WRITE-Fehler bei B und bewusstem erneutem
Verbinden bleiben beide echten Liveanzeigen über drei Minuten verbunden.
Parallel sind je 350/350 USB-Proben frisch und Fehler0; die gespeicherten
Abbruch-Epochen/-Zeitwerte bleiben unverändert. Dies ist ein begrenzter positiver
Dauerlauf, keine erfolgreiche Serie von Neuverbindungen.

App0.3.2/code5 ergänzt begrenzte DEBUG-Metadaten für abgewiesene Schreibcallbacks.
Vollständige Protokoll-/App-Tests, Debug-/Release-Build und beide Lints bestehen.
Der erste anschließende Neuverbindungsversuch zeigt A live, B dagegen erneut
kontrolliert abgewiesen: SDKstatus17, `writing=true`, vier ausstehende Fragmente,
passende Session-/Characteristicidentität, etwa 2,55s nach AuthOK. Die Station
meldet nur das anschließende Abbestellen durch Appcleanup, keinen früheren
lokalen Abbruch. Ressourcendruck im ATT-/NimBLE-Pfad wird untersucht; automatische
Schreibwiederholung und Lockerung der Schutzprüfungen sind nicht implementiert.

Pufferdiagnosecommit `0b62652` (Anwendung SHA256
`dd2c7ddb665821de24d49555a1c3f3f1d13723d8355e244ae39d87a09d581d06`)
ist auf beiden S3 an `0x10000` installiert und mit unveränderter aktueller
`larpkeys`-Sicherung verglichen. Unabhängige Hostprüfung: 935 Assertions in elf
Bearerfällen plus Core400/Adapter165/Protokoll319/Runtime60; nativer SDK-Build
erfolgreich. Ein erster Neuverbindungsdurchgang liefert beide Karten live und
keinen gemessenen Pooldruck. Der zweite Durchgang reproduziert B-Schreibfehler17.
Stats wurden vor weiterem Reconnect im selben Epoch gelesen: Access6/6
(Proof plus erster GET-Frame), 333 Notifyversuche/99 erfolgreich, 234 Busy,
232 Allokationsfehler, Minimum globaler freier MSYS-Blöcke24, im tatsächlich
gewählten Notify-Pool dagegen0. A zeigt ebenfalls 64 Allokationsfehler und
Poolminimum1. Keine AccessCopy-Ablehnung; Appcleanup bestellt danach Notify ab.

Der genaue qualifizierte SDK-Quellpfad reserviert die ATT-Schreibantwort vor
dem Accesscallback. Seine Poolwahl fällt bei leerem kleinem Pool nicht einfach
auf den noch freien größeren Pool zurück. `NOTIFY_TX` meldet im selben
Aufrufpfad nur den Sendeversuch, keinen Funkabschluss; es wird deshalb nicht
als Completion-Credit umgedeutet. Als gezielter Fix wird vor SDK-Übergabe ein
Restbestand im tatsächlichen Pool geschützt. Diese Messung belegt die
beobachtete Ressourcenknappheit, keine vollständige Lastqualifikation.

Großschrift1,3: echte Ansicht mit A-live/B-Fehler unabhängig angesehen; beide
Karten und B-Verbindenknopf vollständig lesbar, Navigation bricht geordnet um.
Handy-Schriftgröße danach wieder1,0. Kein vollständiger Tastatur-/Editor- oder
Screenreader-Nachweis. Der allgemeine Validierungshinweis nennt noch nicht die
konkreten widersprechenden Punktnummern (begrenzter P3-UX-Restpunkt).

Reservefix `0e4f952` schützt vier freie Blöcke nach eigener Notify-Allokation.
Unabhängige 14 Bearerfälle/1232 Assertions und nativer Build bestanden;
Image SHA256 `2a1218a2fb5df255e808e35d203ef54b9a8a89ab26842dedb76bb1f037b8a1b3`
auf beiden installiert und Anwendung/`larpkeys` verglichen. Der nächste Versuch
hat bei B keine Allokationsfehler mehr, aber 277 Schutzvertagungen und den
Abbruchgrund13: Ein aus sechs ATT-Schreibfragmenten bestehender GET überschreitet
die unveränderte 2s-Assemblierungsfrist. Die erste Fünferserie scheitert damit;
der Reservefix allein ist ausdrücklich kein vollständiger Stabilitätsnachweis.

Der daraufhin gebaute gemeinsame MTU-Kandidat verwendet denselben 8-Byte-Header,
dieselben maximal256 Nachrichtenbytes und dieselbe HMAC-/Fristprüfung.
Die App0.3.3/code6 wartet nach der Prioritätsanforderung auf den tatsächlichen
MTU-Callback (mindestens156, höchstens517) und begrenzt ATT-Werte auf244 Byte.
Erst danach folgen Discovery und Anmeldung. 145-Byte-Telemetrie passt dann in
ein Fragment; auch Challenge, Proof, AuthOK, GET und Result benötigen jeweils
nur eines. Firmware unterstützt weiterhin MTU23/20-Byte-Fragmente; Standardpfade
der bisherigen LED-Protokolle behalten128 Nachrichtenbytes und20-Byte-Frames.

Gemeinsame Softwareprüfung: unabhängig Core400/Adapter165/Protokoll1218/Runtime60
und18 tatsächliche Bearerfälle/1701 Assertions erfolgreich. Alle sechs real
gebauten Android-Callbacks einschließlich MTU sind im geschützten Dispatcher
gebunden; 27 NearGlow-JVM-Tests bestanden. Vollständiger Android-Lauf mit
Protokoll-/App-Tests, Debug-/Release-Build und beiden Lints sowie GM-Tests und
GM-Debugbuild erfolgreich (2m34s). Native Build-/Sizeprüfung erfolgreich:
DIRAM146.063 Byte belegt/195.697 frei laut Linkbericht, PreferredMTU256 und
Mainstack8192. Die feste RX-Queue wächst um3584 Byte, lokale Owner-/Hostframes
jeweils um224 Byte; kein gemessener Stack-Highwater wird daraus abgeleitet.
Das Geräteimage hat SHA256
`6dadac8c4fcc6092eb1805a3e5909c57b9e45a232b88e0b2b89c487bf3740712`.
Der passende neue Geräteversuch folgt getrennt; aus Softwareprüfung wird kein
Erfolg der erneut erforderlichen Fünferserie abgeleitet.

Der abgeschlossene neue Geräteversuch steht im aktuellen Ergebnisabschnitt
am Anfang dieses Berichts. Zusätzlicher aktueller CMake-/SDK-PSA-Abgleich:
Beide echten Proximity-PSA-Tests bestanden am MTU-Quellstand. Beim weitergehenden
40-Test-Lauf bestehen39 Tests; ausschließlich der TLS-Peerversuch fehlt zunächst
wegen eines Kryptografiemoduls im falschen Python. Nach Auswahl der bereits
vorhandenen SDK-Pythonumgebung besteht der gezielt wiederholte TLS-Test.
Der erste Hostbuild brauchte zuvor den vollständigen Werkzeugpfad für den
Assembler. Beide Umgebungsfehler wurden getrennt vom Quellcode behandelt;
der fehlgeschlagene Gesamtlauf wird nicht rückwirkend als grüner Lauf bezeichnet.
