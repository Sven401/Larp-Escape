# Android-Diagnosevertrag v1: Hostverbraucher und bedienbare Debugsimulation

## Verbindungsgebundene BLE-Transportschale vom 11. Oktober 2026

`DiagnosticGattPreflightV1` prüft als vorbereitende, reine JVM-Grenze eine
explizit aus der Firmware übernommene GATT-Route gegen die auf **einer**
Verbindung beobachteten Eigenschaften: beide Schreibkanäle mit Antwort,
beide als abonniert angenommene Antwortkanäle und mindestens 128 nutzbare ATT-Wertbytes.
Diese reine JVM-Vorprüfung beweist keinen serverseitig gespeicherten CCCD-Zustand
und keinen tatsächlich empfangenen Notification-Rückruf.
Damit passen ein 69-Byte-GRANT/CANCEL-Auftrag, die 88-Byte-Kontrollantwort,
der 80-Byte-Schrittauftrag und ein einzelnes 128-Byte-Ergebnisfragment.
Die effektive Wertkapazität ist getrennt vom ATT-MTU mit Header zu prüfen.
`PendingDiagnosticControlV1` und `DiagnosticStepAssemblyV1` binden Antworten
zusätzlich an den ursprünglichen Verbindungs-/Rollenursprung und Fristen.

Die nun festgelegte Firmware-GATT-v1-Route verwendet Dienst `...0010`,
Control Write/Reply `...0011`/`...0012` und Step Write/Reply
`...0013`/`...0014` innerhalb `407e6a1d-1cc2-43fd-856b-4aff85a200xx`.
`AndroidDiagnosticGattShellV1` prüft diese Merkmale bei echter Discovery,
fordert MTU 131 an und schreibt Control- und Step-CCCD nacheinander. Zwei
erfolgreiche Descriptor-Write-Rückrufe führen nur zu
`CCCD_WRITES_ACKNOWLEDGED` und zur sichtbaren Meldung
`SUBSCRIPTION_UNPROVEN`. Sie öffnen weder `TRANSPORT_READY` noch den
Diagnose-Schreibpfad. Der Export protokolliert die beiden ATT-Bestätigungen
ausdrücklich als `CCCD_WRITE_ACK` mit offenem Abo-Nachweis. Jede
Firmware-Probe ist fünf Bytes lang (`44 47 01 50 01` für ControlReply,
`44 47 01 50 02` für StepReply). Nur wenn beide exakten Werte auf den
richtigen Antwortmerkmalen derselben GATT-Generation eintreffen **und**
beide Descriptor-Rückrufe bestätigt wurden, meldet die App
`TRANSPORT_READY`. Eine Notification kann vor ihrem lokalen
Descriptor-Rückruf eintreffen; sie wird dann vermerkt, aber der Abschluss
wartet auf beide Rückrufe. Der private Bericht protokolliert
`TRANSPORT_PROBE` je Kanal und `TRANSPORT_PROBE_COMPLETE` getrennt von
`CCCD_WRITE_ACK`. Die festen Probe-Bytes sind ein Transportnachweis,
keine Authentisierung und keine Berechtigung für Diagnosebefehle.
Jede
Rückmeldung wird über die ursprüngliche `BluetoothGatt`-Instanz und lokale
Generation gefiltert; bei Trennung, Berechtigungsverlust oder Neustart wird
der Zustand verworfen. Fehler bei Discovery, MTU oder Abonnement bleiben mit
konkretem Blockgrund sichtbar. Die 10-Sekunden-Einrichtungsfrist verhindert
ein dauerhaft schwebendes Tor. Ein ATT-Wert umfasst MTU minus drei Bytes.

Der Quellstand der Firmware enthält die Diagnoseattribute; ob die aktuell
geladene Box sie anbietet, ist separat am Gerät zu prüfen. Ein bestätigter
CCCD-Schreibvorgang belegt derzeit keinen sendenden Firmware-Abozustand.
Außerdem liefert die produktive Schale derzeit absichtlich keinen
`DiagnosticControlOriginV1`: Der vertrauenswürdige F1-Rollen-/Besitzissuer
fehlt. Deshalb erreicht keine App-Aktion `writeCharacteristic`, selbst falls
der GATT-Link später vollständig aufgebaut wird. Die vorbereiteten
Schreib-/Rückrufmethoden serialisieren je einen Auftrag, verlangen den
ursprünglichen Rollenursprung und die bestätigte Schreibquittung, prüfen
korrelierte Controlantworten und setzen Step-Fragmente nur innerhalb der
ursprünglichen Verbindung zusammen. `CANCEL` sperrt den lokalen Grant vor
dem Sendeversuch. Ein angefragter Schreibvorgang und `ACCEPTED` allein
bestätigen keine LED-Abschaltung oder physische Funktion. Es gibt weiterhin
keinen Geräte-GRANT und keine produktive UI-Freigabe. Die Debugseite bleibt
eine gekennzeichnete Simulation. Die Android-GATT-Discovery belegt auch nicht
die serverseitigen ENCRYPTED+MITM-Berechtigungsflags; diese sind im
Firmwarevertrag und später mit echtem Rollenpfad zu prüfen.

Prüfung: gezielte JVM-Tests zu Dienst, Eigenschaften, Reihenfolge,
Verbindungsgeneration, MTU- und Blockgrenzen; danach Debugbuild. Diese
Prüfung enthält kein Handy und keine
physische Originalbox. Offizielle Grundlagen, geprüft am 11.10.2026:
[BLE-Überblick](https://developer.android.com/develop/connectivity/bluetooth/ble/ble-overview),
[GATT-Verbindung und Rückrufe](https://developer.android.com/develop/connectivity/bluetooth/ble/connect-gatt-server),
[BluetoothGatt-API](https://developer.android.com/reference/android/bluetooth/BluetoothGatt),
[BluetoothGattCallback-API](https://developer.android.com/reference/android/bluetooth/BluetoothGattCallback),
[Bluetooth-Berechtigungen](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions).
Das aktuelle [android/skills-Angebot](https://github.com/android/skills)
enthält keinen auf diesen BLE-Transport passenden Spezialskill; es wurde
keiner installiert.

## Synthetischer Kontrollpfad vom 11. Oktober 2026

Der geführte Debuglauf führt vor dem ersten Journal und Schritt jetzt einen
`GRANT`-Rahmen durch `DiagnosticControlV1` und dessen korrelierte Antwort. Ohne
passende Antwort entsteht kein neuer Lauf und kein Schrittauftrag. Beim Stoppen
oder Export wird lokal sofort gesperrt und ein `CANCEL`-Rahmen versucht; eine
fehlende Cancel-Antwort öffnet den Lauf nicht wieder. Der vorhandene
`DiagnosticStepV1`-Pfad bleibt für die explizite Wake-/LED-Simulation zuständig.
Der Fake erzeugt Kontrollantwort, Ursprung und Grant-ID selbst. Diese
Byteprüfung ist ausdrücklich **keine** BLE-Verbindung, Rollenprüfung oder
Gerätefreigabe. Ein App-/Firmware-Ende-zu-Ende-Pfad und die physische Abnahme
stehen weiterhin aus.

## Geführte isolierte Vorschau

Branch `plan/nutzergefuehrte-hardwarediagnose-android-guided` baut auf dem
Android-Schrittrahmen `94a9a726` auf. Ausschließlich die drei Preview-Dateien
`README.md`, `app/build.gradle.kts` und `app/src/main/AndroidManifest.xml`
wurden zuerst aus `5305c501` übernommen; anschließend wurden Nutzerführung
und Bedienbeschreibung gezielt weiterentwickelt. Debug bleibt die separate
App `de.larpescape.companion.diagnosticpreview`; kein Gerätebefehl wurde ergänzt.

Die feste Zweischrittfolge, sichtbare Hauptaktion, bestätigtes Überspringen,
Rückblick ohne Wiederholung und gesperrte Gesamtplan-Vorschau sind in
[README.md](README.md#geführter-pilotplan) beschrieben. Fortschritt wird separat
atomar gespeichert. Ein Speicherfehler behauptet keinen Fortschritt.
Ergebnisse, Beobachtung und Urteil stammen weiterhin aus dem bestehenden
geprüften Journal-/Wirepfad. Ungespeicherter Text bleibt ein markierter Entwurf.

Ergänzung 2026-10-11: Ein abgeschlossener Simulationsbericht trägt im privaten
Journal einen atomar zusammen mit dem Bericht geschriebenen Abschlussmarker.
Nach App-Neustart bleiben Beobachtungen eines solchen Laufs unveränderlich;
erneuter Export liefert dieselben Bytes und verschiebt die Endzeit nicht.
Neu begonnene Läufe speichern auch den offenen Zustand ausdrücklich. Bei
älteren Rohjournalen ohne Marker kann ein beendeter Schritt einen früheren
Export nicht belegen: Er wird als Altjournal gesperrt, der vorhandene Befund
bleibt erhalten und die Sperrherkunft auch nach weiterem Neustart sichtbar.
Ein alter noch offener Auftrag wird als `app_restarted` unklar beendet und
niemals automatisch wiederholt. Falls die Endzeit eines unfertigen Altjournals
vor dem letzten Schritt liegt, wird sie beim Sperren auf den Wiederöffnungszeitpunkt
gesetzt; eine zuvor valide Endzeit bleibt erhalten. Die JVM-Tests prüfen diese
Fälle, Abschluss, Wiederöffnung und bytegleichen Wiederexport;
eine Bedienprüfung auf einem Android-Gerät steht noch aus.

Prüfstand am 2026-10-10: 100 JVM-Fälle ohne Fehler oder übersprungene Fälle
(94 bestehende plus 6 neue Fortschritt-/Frist-/Speicherprüfungen); Debugbuild
und Lint ohne Fehler. Die drei bestehenden Warnungen betreffen Target-API,
Backupregeln und Appicon. Die neuen Fälle prüfen Start/Next/Skip, separate
Beobachtung, Nichtprüfung im Export, Rückblick ohne Replay, Entwurfsblockade,
gespeicherten Fortschritt, Positionsspeicherfehler und die sichtbare absolute
10-Sekunden-Frist. Diese Prüfungen belegen keine physische Hardwarefunktion.

Quellenprüfung 2026-10-10: Android Architecture Recommendations (UI-Zustand
im ViewModel), Android UI Design und Compose API Defaults (beschriftete
Material-3-Komponenten mit mindestens 48dp Bedienhöhe). Das aktuelle
`android/skills`-Angebot wurde auf Bedarf geprüft; für diesen grundlegenden
Compose-Ablauf wird kein Spezialskill installiert. Material-3-Buttons bleiben
die primären Aktionen; lange Rohdaten und spätere gesperrte Bereiche werden
nur auf ausdrückliche Auswahl sichtbar.

Weiterführung: Der [DG-Step-/Reassembly-Schnitt](../contract/diagnostic-step-v1/README.md)
ergänzt auf Basis `7e5acb5` den korrigierten Wirevertrag `b5a8c430`. Die Debugseite
führt ihre weiterhin synthetischen Antworten nun durch diesen Bytepfad.
Die folgenden ursprünglichen Paketprüfstände beziehen sich auf `45d3e79`.

Stand: 2026-10-10. Androidbasis `6383c59` (A3-Vertragskern), isolierter Branch
`plan/nutzergefuehrte-hardwarediagnose-android-fake`.
Vertragsquelle: Produktcommit `3ef678167c21d34f88aadebdcb1c2d1d94cc7b30`
auf `plan/nutzergefuehrte-hardwarediagnose-vertrag`, Firmwarebasis F0 `e973818`.
Kein ganzer Firmwarebranch wurde in die separate Android-Abstammung übernommen.
Die folgenden Dateien stammen bytegleich aus den genannten Git-Blobs; die
Hashes bezeichnen die kopierten und versionierten LF-Bytes.

| Quellpfad | Androidpfad | SHA-256 |
| --- | --- | --- |
| `docs/companion/diagnostic-v1.schema.json` | `companion/android/core/protocol/src/main/resources/diagnostic-v1.schema.json` | `a79584cbe71fd1b7d17fb06e9eb012efca752775ac84b1fcda97036f8f4ec5be` |
| `docs/companion/fixtures/diagnostic-v1/completed.json` | `companion/android/core/protocol/src/test/resources/diagnostic-v1/completed.json` | `306c43fc3ad94384cc1eac53bdbeda34348c90a088c31fa23e4602c3ca7b3827` |
| `docs/companion/fixtures/diagnostic-v1/failed.json` | `companion/android/core/protocol/src/test/resources/diagnostic-v1/failed.json` | `ba5ef77af556bfcdd86ca5b0b24debac99ed111db4ca5cb0e09491bd182ad5d0` |
| `docs/companion/fixtures/diagnostic-v1/not-tested.json` | `companion/android/core/protocol/src/test/resources/diagnostic-v1/not-tested.json` | `4cfc256ad9c4034e1f09c6797f021f46c391979e8b430bb76d6102dc6a8a3bed` |
| `docs/companion/fixtures/diagnostic-v1/timeout.json` | `companion/android/core/protocol/src/test/resources/diagnostic-v1/timeout.json` | `fce266c2894d424faf21684b3dcdf6ebaf4d42075104adbc4420e213dd09daaf` |
| `docs/companion/fixtures/diagnostic-v1/cancelled.json` | `companion/android/core/protocol/src/test/resources/diagnostic-v1/cancelled.json` | `512db683e1801857e5e35333974dc1bd9c8a86b9e54c9ae8516bc0325b3d6253` |
| `docs/companion/fixtures/diagnostic-v1/disconnected.json` | `companion/android/core/protocol/src/test/resources/diagnostic-v1/disconnected.json` | `89ca22108f38caddaaa8504bcd14608f7ff2beea786bc9b8bf45189b15666749` |
| `docs/companion/fixtures/diagnostic-v1/app_restarted.json` | `companion/android/core/protocol/src/test/resources/diagnostic-v1/app_restarted.json` | `bb466a01f3fcd5285a3fd8a1c2308fd660c80bbc4cae3a8d4e6210341fa59e98` |
| `docs/companion/fixtures/diagnostic-v1/device_restarted.json` | `companion/android/core/protocol/src/test/resources/diagnostic-v1/device_restarted.json` | `bf26d63fea5e31d244cd1aee7293c606a003167da7eb499b505e5854d24a573d` |

## Verhalten

`DiagnosticReportCodecV1` prüft den gepinnten Schemaumfang, Korrelation, Zeiten,
Quellen und aus den Belegen neu berechnete Urteile. Strict JSON verwirft doppelte
Schlüssel, Float-/Exponentzahlen, ungültiges UTF-8, Surrogate und überschrittene
Budgets (1 MiB, Tiefe 32, 50.000 Werte). Export verwendet das gemeinsame
ASCII-/Integerprofil; acht Goldenberichte müssen bytegleich zurückkehren.
Der kleine Schema-Prüfer unterstützt ausschließlich den Vokabularumfang des
gepinnten Vertrags; neue Schlüsselwörter werden abgewiesen. Das ist kein
allgemeiner JSON-Schema-Validator. Medienmanifeste werden bis zur Implementierung
einer tatsächlichen Byteprüfung explizit abgewiesen.

`FakeDiagnosticRepositoryV1` schreibt den offenen Auftrag vor dem Senden in
`DiagnosticJournalV1`. Das Journalinterface verlangt atomare Ersetzung oder
unveränderten alten Stand bei Fehler. Der Hostfake speichert Kopien im Speicher;
Android-Prozesspersistenz wird dadurch nicht behauptet. Wiederöffnung beendet
einen offenen Auftrag als `app_restarted` und übernimmt keine Sendemöglichkeit.
Jeder Ablauf bleibt `synthetic`; Transportbindung wird als separates Argument
übergeben. Korrelation umfasst Lauf, Schritt, Gerät, Boot, Freigabe und Operation.
Eine Sendequittung bleibt unklar. Nur korrelierte Endantwort und passende
unabhängige Beobachtung ergeben ein simuliertes bestanden. Timeout, Abbruch,
Verlust und Bootwechsel beenden den Lauf. Ein neuer Versuch braucht einen neuen
expliziten Lauf. Überspringen sendet nichts. Doppelte, fremde und späte Antworten
ändern kein Urteil; nach 16 ignorierten Antworten wird der Lauf geschlossen und
ein expliziter Fehler verlangt gesonderte vollständige Transportdiagnostik.

## Prüfen

Aus `companion/android` mit JDK17/21, SDK35 und dem vorhandenen Gradle8.9-Cache:

```
gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug
```

Hosttests prüfen beide Schritte, Beobachtungen, Korrelation/Bindung, Frist inklusive
Warteschlangenzeit, Journalfehler vor Senden, Wiederöffnung ohne Replay, begrenzte
Rohbelege, Nichtprüfung, korrupte Exporte und alle acht Golden-Rundläufe.

## Quellen und offene Anschlüsse

Offizielle Android-Architekturempfehlungen am 2026-10-10 gelesen:
https://developer.android.com/topic/architecture/recommendations
Repositoryzugriff und bevorzugte Fakes passen zum plattformunabhängigen Kern.
https://github.com/android/skills wurde auf konkrete Anwendbarkeit geprüft;
kein spezieller Skill wurde für diese Protokoll-/Debugaufgabe installiert.
Für die ergänzte Nutzerführung wurden außerdem Android UI Design und die
Compose-Barrierefreiheitsgrundlagen gelesen; Material3-Textbuttons und
beschriftete Eingaben verwenden mindestens 48dp Bedienhöhe:
https://developer.android.com/design/ui/mobile
https://developer.android.com/develop/ui/compose/accessibility/api-defaults
Atomare Dateiersetzung folgt dem Android-API-Vertrag:
https://developer.android.com/reference/android/util/AtomicFile

## Bedienbarer Debugpfad

Nur Debug zeigt die Seite „Diagnose · Simulation“. Ein ausdrücklich gestarteter
Fake-Lauf besitzt neue Lauf-/Freigabe-UUIDs und höchstens 60 Sekunden Laufzeit.
Die Seite beendet die BLE-/NFC-Sitzung und benötigt keine neuen Berechtigungen.
Der Nutzer wählt einen Wake- oder LED-Auftrag, löst eine passende oder fehlerhafte
Fake-Antwort ausdrücklich aus und hält seine unabhängige Beobachtung fest.
Auftrag, Telemetrie und Beobachtung stehen getrennt mit sichtbarer Einstufung;
ohne Nutzerbeobachtung bleibt auch eine passende Fake-Antwort unklar. Die Seite
beweist weder Geräteberechtigung noch physisches Licht oder Tastendrücke.

Einzelaufträge haben eine lokale absolute Frist von 10 Sekunden; die Simulation
verlangt genügend verbleibende Freigabe. Abbruch, Verbindungsverlust und
Geräteneustart sind eigenständige bedienbare Fälle. Hintergrund oder Seitenwechsel
beenden den Lauf; Rotation behält den ViewModelbesitz. Ein Prozessneustart liest
das private AtomicFile-Journal, beendet offene Aufträge als `app_restarted` und
sendet nichts. Je explizitem Lauf bleibt eine eigene Datei erhalten, ein atomarer
Zeiger wählt den jüngsten Lauf. Frühere Journaldateien werden nicht gelöscht.
Gespeicherte Beobachtungen bleiben erhalten; unbestätigter Eingabetext ist ein
UI-Entwurf. Der Export schließt einen offenen Schritt mit Abbruch/unklar ab und
verwendet Androids Speichern-Dialog. Ein abgebrochener Dialog löscht den lokalen
Bericht nicht. Der Codec prüft den Import/Export-Rundlauf; eine App-Importseite
und Medienaufnahme sind in diesem Schnitt offen.

Fünf zusätzliche ViewModelfälle prüfen den Zwei-Schritt-Rundlauf, Wiederöffnung,
Hintergrund, getrennte alte Journale, Nichtprüfung und Gerätefehler. Die konkrete
Android-AtomicFile-/Dateidialog-/Touchbedienung wurde nicht auf einem Handy
ausgeführt. Der Paketprüfstand am 2026-10-10: 44 Protokoll-JVM-Fälle und 21 App-JVM-Fälle,
darunter 15 neue Protokoll- und fünf neue ViewModelfälle, ohne Fehler oder
übersprungene Fälle. Debugbuild und Lint erfolgreich. Drei bestehende
Lint-Warnungen betreffen Target-API35, Backupregeln und fehlendes Appicon.
Die neue Gson-Abhängigkeit verwendet den Versionskatalog. Die ersten
Sandbox-Prüfversuche scheiterten an der Gradle-Daemon-Verbindung; die belegten
Prüfungen liefen mit bestehendem Offlinecache außerhalb dieser Einschränkung.

Offen bleiben produktive GATT-Rahmen und Freigabe, vertrauenswürdige Authentisierung,
serialisierter Firmwareowner mit Besitzübergabe und garantierter LED-Abschaltung,
Atomare Android-Dateipersistenz und Dateiexport sind für die Debugsimulation
implementiert und müssen im bedienbaren Handyablauf geprüft werden. Der Kern
ist mit einer Debugseite verbunden, der produktive `DeviceRepository`-Anschluss
ist offen. Neue Berechtigungen, Produktionspersistenz, Medien und
Lebenszyklusverhalten müssen beim echten Anschluss geprüft werden. Es gibt
keinen Upload, keine Installation, keinen Geräteaufruf und keine
physische Abnahme. Die unterbrochene Originalbox-Abnahme bleibt offen.
