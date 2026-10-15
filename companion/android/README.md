# Larp-Escape: verbindliche Android-App

Kotlin, Jetpack Compose und Material 3. Eigenständige App mit Paketname
`de.larpescape.companion`, Mindestversion Android 13/API33. Bestehende
Webprototypen und der .NET-Diagnoseclient bleiben separate Referenzen.
Die Projekt-App bedient den eigenen LED-Produktdienst mit Funkeln,
Lichtimpuls und Ripple sowie Start/Stop. Der LED-Zweig hat zunächst kein
Spielmodul; seine LED-Berechtigung ist keine Legacy-Spielleiteranmeldung.

`app` enthält UI/ViewModel, DeviceRepository und Android-BLE. `core:protocol`
enthält den plattformunabhängigen, begrenzten FrameAssembler. Alle BLE-Rückrufe
werden mit kopierten Bytes auf dem Hauptthread verarbeitet und an die jeweilige
Verbindungsgeneration gebunden. Ein ViewModel besitzt die Sitzung auch bei
Rotation. Verlassen der App beendet die Sitzung; kein Hintergrundservice.

## Tastenmatrix im nächsten gemeinsamen Test

Die ESP-Seite zeigt vor Wake und LEDs ein 9×9-Raster mit elektrischen
Koordinaten A–I/1–9. Ein Kontakt trägt ein sichtbares Symbol und erscheint
zusätzlich in der Liste aktiver Koordinaten. Auf kleinen Displays lässt sich
das Raster seitlich verschieben; die Zellen bleiben mindestens 40 dp groß.
Jeweils eine einzelne Taste mindestens eine Sekunde halten, loslassen und die
Zuordnung zur physischen Taste im gemeinsamen Test protokollieren.
Elektrische Pinbelegung und physische Zuordnung sind vorläufig.

Nur vollständige, authentisierte Matrixmeldungen mit neuer Statussequenz
halten die Anzeige für weniger als eine Sekunde aktuell. Lesefehler,
Verbindungsende und veraltete Werte zeigen unbekannt. LED-Antworten und
einzelne BLE-Fragmente verlängern die Frische nicht. Bei Mehrfachkontakt
zeigt die App die beobachteten Kontaktkoordinaten ausdrücklich als mehrdeutig;
ihre Zuordnung zu tatsächlich gedrückten Tasten bleibt unklar. Die bestehende LED-/Wake-Firmware
bleibt verbindbar und zeigt sichtbar, dass sie noch keine Matrixdaten liefert.
Die neue Matrix-Prüffirmware und der [gemeinsame Testablauf](../../docs/matrix/test-preparation.md)
werden vor der Geräteprüfung zusammen bereitgestellt. Die Appanzeige selbst
bestätigt noch keine physische Taste.

## Wake-Inputs

Die ESP-Seite zeigt vier Wake-Rohpegel in einem 2×2-Raster. Langes Berühren
hebt eine Kachel auf; Ziehen zeigt den Tausch als Vorschau, Loslassen bestätigt.
Die Anordnung bleibt pro Box lokal erhalten. Lesefehler und veraltete Werte
werden unbekannt, nicht inaktiv. [Testablauf, Hardwarevorbehalt und Upgrade-Reihenfolge](../../docs/wake/test-preparation.md).

## Bauen und installieren

Für die NFC-Baugruppenprüfung bietet die Seite **NFC-Tags** den normalen
Tageditor und einen ausdrücklich aktivierten **Handy-Testtag**. Dessen
Emulation endet bei Verlassen der Seite oder nach drei Minuten. Die
getrennte NFC-Prüffirmware und der anschließende Rundlauf mit einem echten
Tag stehen im [NFC-Testablauf](../../docs/nfc/test-preparation.md).

Alle Befehle in diesem Abschnitt laufen aus `companion/android/`.
Voraussetzung: JDK17 oder 21, Android SDK35 und Build Tools35.0.0. Der Gradle
Wrapper lädt Gradle8.9; Versionen stehen in `gradle/libs.versions.toml`.
SDK über `ANDROID_HOME` oder eine lokale, ignorierte `local.properties` angeben.

```powershell
$env:JAVA_HOME='C:/Program Files/Android/openjdk/jdk-21.0.8'
$env:ANDROID_HOME='C:/Program Files (x86)/Android/android-sdk'
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n de.larpescape.companion/.MainActivity
```

Die Pfade oben sind Beispiele für diesen Windows-Prüfrechner. JDK-/SDK-Pfade
bei Bedarf anpassen; `adb` muss im PATH sein oder aus
`$env:ANDROID_HOME/platform-tools/adb.exe` aufgerufen werden. Der erste Build
benötigt Netzwerk für Gradle, Plugins und Bibliotheken. Es gibt keine
Node-/Vite-Buildabhängigkeit.

Für den bereits vorhandenen **lokalen Offlineprüfstand** dieses Rechners:

```powershell
$env:JAVA_HOME='C:/Program Files/Android/openjdk/jdk-21.0.8'
$env:ANDROID_HOME='C:/Program Files (x86)/Android/android-sdk'
$env:GRADLE_USER_HOME=(Join-Path (Get-Location) '.local/gradle-home')
./.local/gradle-8.9/bin/gradle.bat :core:protocol:test :app:assembleDebug :app:lintDebug --offline
```

`.local/gradle-8.9` und `.local/gradle-home` sind ignorierte lokale Werkzeuge,
keine eingecheckten Voraussetzungen eines neuen Klons. Offline funktioniert
nur mit vollständig vorhandenen Downloads. Bei Cache- oder Prozesszugriff-
Fehlern keine Produktdateien ändern; Umgebung prüfen. Für einen frischen
Testlauf statt eines UP-TO-DATE-Ergebnisses:

```powershell
./.local/gradle-8.9/bin/gradle.bat :core:protocol:cleanTest :core:protocol:test --offline
```

## Debug-Labor und Release

`BuildConfig.ALLOW_CONTRACT_LAB` ist in Debug `true`, in Release `false`.
Debug darf den markierten Laborservice lesen. Der eigene LED-Produktdienst
hat Vorrang; ein vorhandener kanonischer Dienst hat Vorrang vor dem Labor.
Release lehnt reine
Laborgeräte bei der Dienstprüfung ab und wählt bei gemeinsam angebotenen
Diensten den eigenen LED-Dienst vor dem kanonischen Legacy-Produktservice, niemals das Labor.
Werbenamen und UUIDs bleiben Auswahlhilfen; erst GATT bestätigt den Dienst.

```powershell
./gradlew.bat :app:assembleRelease :app:lintRelease
```

Release ist unsigniert. Der LED-Produktdienst bietet einen eigenen eingerichteten
LED-Zugang; die frühere Legacy-Spielanmeldung bleibt unverbunden. Der Build
ist ein Prüfartefakt, kein Verteilungs- oder Veröffentlichungsauftrag.

## LED-Produktbedienung

Die ESP-Seite enthält die drei festen Varianten **Funkeln**, **Lichtimpuls**
und **Ripple**. Auswahl allein sendet nichts; „Start“ ersetzt den laufenden
Effekt durch die ausgewählte Variante. „Stop · LEDs aus“ sendet einen eigenen
Ausschaltauftrag. Funkeln läuft bis Stop, Lichtimpuls und Ripple enden nach
jeweils einem Durchlauf dunkel. Wiederholter Start ist bewusst möglich.
Die Anzeige unterscheidet Auswahl, offenen Auftrag und bestätigten Boxzustand.
Eine Ausführungsantwort ist kein Nachweis des sichtbaren Lichts.

Die Box wird zuerst über den getrennten lokalen USB-Einrichtungsweg versehen.
Danach in der echten App „Box-Zugang importieren“ wählen und die erzeugte
JSON-Datei importieren: `schemaVersion:1`, `deviceId` mit 32 Hexzeichen,
`secret` mit 64 Hexzeichen, optional `name` (1–64 Zeichen). Die App akzeptiert
höchstens 2048 UTF-8-Bytes und keine zusätzlichen oder doppelten Felder. Der
Import ersetzt bewusst den einen eingerichteten Box-Zugang auf diesem Handy
und beendet eine bestehende Verbindung. Eine andere Box braucht einen eigenen
Import. Werbename und Bluetoothadresse richten keinen Zugang ein.

Der aktuelle Zugang liegt als `led-access-v1.bin` AES-256-GCM-verschlüsselt in
`noBackupFilesDir`. Der Schlüssel zum Entschlüsseln bleibt unter dem eigenen Alias
`larpescape-led-access-v1` im Android Keystore; `allowBackup=false`
bleibt gesetzt. Der Importinhalt und das Geheimnis werden nicht geloggt.
Verlust des Keystore-Schlüssels macht einen erneuten Import erforderlich.
Die exportierte USB-Einrichtungsdatei selbst bleibt außerhalb dieses Speichers.
Die Oberfläche zeigt eine neutrale LED-Boxkennung, keinen importierten Produktnamen.
Bei einem Update von einer früheren Zugangsvariante wird der bestehende
Geräteschlüssel erneut über USB exportiert und bewusst importiert. Es gibt keine
automatische Übernahme alter privater Zugangsspeicher. Andere Appdaten bleiben
bei einem regulären Update erhalten; ältere private Artefakte werden nicht gelöscht.

Eigener Dienst: `6c790001-2f50-4d5a-a7f4-35a478c4b101`. Er unterscheidet sich
vom Legacy-Spiel- und vom Vertragslaborservice. Zufällige Handy-, Boot- und
Verbindungswerte binden den gegenseitigen HMAC-SHA256-Nachweis an diese Sitzung.
Befehle und Antworten besitzen getrennte HMAC-Domänen unter `LARP-LED/1/`. Die gemeinsamen
[Firmware-Testvektoren](../../docs/led/fixtures/wire-v1.json) werden direkt von
JVM-Tests gelesen; die dortigen öffentlichen Testschlüssel sind keine Einrichtung.

Alle GATT-Werte sind höchstens 20 Byte groß; die App braucht keine MTU-Erhöhung.
Sie sendet nur einen Auftrag gleichzeitig und wartet auf alle Write-ACKs **und**
die passende authentisierte Geräteantwort. Ein Write-ACK allein zeigt keinen
Erfolg. Veraltete Statussequenzen erneuern die lokale Frische nicht. Ein älteres,
aber korrekt gebundenes Ergebnis kann noch seinen offenen Auftrag abschließen,
ohne einen neueren Status zu überschreiben. Nach einer Sekunde ohne vollständigen
neuen Status sind Befehle gesperrt, nach fünf Sekunden ohne Befehlsabschluss wird
die Verbindung beendet. Es gibt keine automatische Wiederholung eines Starts.

Der explizite Modus `LED_ONLY` wird als „Spielintegration folgt getrennt“ gezeigt
und erlaubt den LED-Start. Für einen späteren Spielanschluss erlaubt allein
`STOPPED` die manuelle Bedienung; `RUNNING`, `PAUSED` und `UNKNOWN` sperren
Start und Stop. Die aktuelle
LED-Firmware behauptet keinen geprüften Zustand eines nicht vorhandenen Spiels.
Appwechsel, Rechteentzug, Bluetooth-Aus und bewusste Trennung entwerten den
Appzustand; die Box schaltet bei erkanntem Verbindungsverlust aus.

Hosttests und Builds prüfen Protokoll, Bedienregeln und Kompilierbarkeit. Der
LED-Produktpfad wurde dadurch nicht auf Handy/Box physisch abgenommen; der
frühere isolierte Rot-an/aus-Versuch war ein anderer Firmware- und Appstand.

## Handyprobe

Bluetoothrechte werden erst nach „ESP suchen“ angefragt und vor BLE-Zugriffen
erneut geprüft. Ablehnung bleibt sichtbar; vor einer erneuten Anfrage zeigt
die App bei Bedarf eine Erklärung mit „Abbrechen“. Ein leerer oder unvollständiger
Dialogrückgabewert verleiht keine Rechte: Beide OS-Berechtigungen sind maßgeblich.
Es gibt keine automatische Anfrage beim Start oder nach Rückkehr.

Die BLE-Sitzung gehört dem ViewModel und bleibt bei Rotation erhalten. Wenn die
App nicht mehr sichtbar ist, schließt sie Verbindung und Suche, löscht Ziele,
Status und Teilframes und verwirft alte Rückrufe/Fristen. Rückkehr erfordert eine
neue Suche. Ein Prozessneustart übernimmt keinen alten Gerätestatus. Rechteverlust
und ausgeschaltetes Bluetooth entwerten die Sitzung ebenfalls; der geschützte
Bluetooth-Systembroadcast wird nur während des Vordergrundbetriebs empfangen.

Die zehn Fake-Repository-Fälle prüfen Verbindungs- und LED-Bedienregeln separat:

```powershell
./gradlew.bat :app:testDebugUnitTest
```

Diese Hostfälle ersetzen keine echte Dialog-, Rotation- oder Funkprüfung.

„ESP suchen“ drücken, Geräte in der Nähe erlauben und Bluetooth einschalten.
Die Suche endet nach zehn Sekunden. Ein Ziel antippen. Namen und Werbe-UUIDs
sind Auswahlhilfen, kein Identitätsnachweis. Es erscheinen höchstens 16 Ziele;
Adressen werden nur im Arbeitsspeicher gehalten.

Am v2-Vertragslabor (`86ddc5b0-3878-4ff7-9f55-17c62aeb1001`) liest die App den
Labmarker und abonniert Statusframes. Sie zeigt Boot, Sample, Profilphase,
Spielzustand und Gerätezahl aus einer vollständigen v2-Nachricht. Fragmentlimits
und absolute Fristen entsprechen dem [Laborvertrag](../contract/lab-v2/README.md).
Bei fehlerhaften Frames wird die Verbindung geschlossen. Beim Trennen wird
der Status gelöscht. Danach dasselbe Ziel nochmals wählen, um Reconnect zu prüfen.

Der reine Kotlin-Decoder prüft alle v2-Pflichtfelder, Typen, Zahlen-/Textgrenzen,
Phasen und eindeutige Gerätekennungen. Unbekannte oder doppelte JSONfelder sowie
ungültiges UTF-8 werden abgewiesen. Ein Sample muss innerhalb desselben Boots
größer als das letzte angenommene Sample sein; ein neuer Boot beginnt eine neue
Folge. Ein Reconnect löscht die alte Folge und Anzeige.

Nach **31 Sekunden ohne neues vollständiges Sample** zeigt die App „Überholter
Status · letzter Empfang“. Gleiche/ältere Samples und einzelne Fragmente
verlängern diese Frist nicht. Das ist eine vorläufige lokale Beobachtungsgrenze:
30 Sekunden absolutes Statusübertragungsbudget plus 1 Sekunde zulässiges
Quellalter vor Sendebeginn (`StatusFrameLimits` der Firmware). Der Vertrag
garantiert keinen regelmäßigen Empfang. Die Frist läuft ab vollständigem Empfang
auf der monotonen Handyuhr; ESP-Uptime ist keine synchronisierte Handyuhr.
Auch ein kürzlich empfangener Status ist ein Snapshot und bestätigt keine
momentane Geräteverfügbarkeit oder Rechte. Die separate 40-Sekunden-Frist für
den ersten Status bleibt bestehen. Eine echte ESP-Ausfallprobe ist zusätzlich
zur JVM-Fristprüfung erforderlich.

Am kanonischen Produktservice (`407e6a1d-1cc2-43fd-856b-4aff85a20001`) wird nur
der tatsächliche Dienst bestätigt. Produktanmeldung und Statuszugang sind noch
offen. Keine Spielbefehle, keine Rechte aus öffentlichen Lease-Werten, kein
Profiltransfer. Die Laborfirmware hat synthetische Geräte und eingespeiste
Autorität; ihre Statusanzeige ist keine Produktfreigabe.

## Peer-Anmeldung: nur Hostkern

Der reine Kotlin-Kern `PeerAuthV1` verarbeitet den optionalen Firmwarevertrag
für 18-Byte-Issue, 108-Byte-Challenge, 72-Byte-Proof und 32-Byte-Ergebnis.
Er bindet Challenge und Ergebnis an erwartete Geräte-/Peerwerte, Nachrichten-ID
und Sequenz und normalisiert gültige P256-r/s-Signaturen auf Low-S.
Die Firmware-Hostbeobachtung ist bytegleich übernommen, SHA256
`de088914451110b11309c05ee4f421c9742519451de6b370f8ae22d9572d73f7`.
Sie stammt aus `firmware-peer-ingress` auf `7f2a3e4` plus lokalem Auth-Diff,
verglichen am 2026-10-10; vor Integration mit dem endgültigen Firmwarecommit
nochmals abgleichen. Die drei JVM-Fälle prüfen Bytes, Signaturgrenzen und
widersprüchliche/falsche Bindungen.

Dieser A3-Vorbereitungskern hat keinen Android-GATT-Anschluss, keinen privaten
Schlüssel und keine Signaturerzeugung. Ein dekodiertes `Granted` gibt der App
keine Spielrechte. Produktiver Dienststart, Firmware F1, gegenseitiger
Gerätenachweis und vertrauenswürdige Peer-Einrichtung bleiben Abhängigkeiten.
Kein Funk- oder Geräteergebnis dieser Funktion wird behauptet.

## Lokale Szenarien

Die Seite „Szenario“ bearbeitet lokale JSON-v1-Entwürfe mit bis zu 24 Szenen.
Titel, Ziel und Hinweis können beim Bearbeiten leer bleiben; Speichern und
Export verlangen ausgefüllte Titel. „Leseprobe“ zeigt auch ungespeicherte
Änderungen und redaktionelle Lücken. Das ist keine Ausführung auf dem ESP.

„Speichern“ schreibt die vollständige Datei atomar in den privaten Appspeicher.
Ein Fehler bleibt sichtbar und der Entwurf gilt nicht als gespeichert.
„JSON importieren“ liest höchstens 128 KiB über Androids Dateiauswahl;
der importierte Entwurf ersetzt zunächst nur die Arbeitskopie. Erst ein
ausdrückliches Speichern ersetzt den lokalen Speicherstand. „JSON exportieren“
verwendet Androids Speichern-Dialog. Die Datei ist mit dem Vite-Entwurf v1
kompatibel (ID, Reihenfolge, Titel, Ziel, Hinweis).

Vor einer späteren Stilllegung von Vite dort vorhandene Browserentwürfe als
JSON exportieren und auf dem Handy importieren. Der Browser-Speicher wird
nicht automatisch ausgelesen. Die Android-App hat keinen Profiltransfer und
keine Berechtigung aus dem lokalen Dokument.

Handyprüfung vom 10.10.2026 (Moto G14): Der gespeicherte Testentwurf wurde
über Androids Speichern-Dialog als JSON exportiert und über den Öffnen-Dialog
wieder importiert. Der Import erschien als ungespeicherte Arbeitskopie.
Beschädigtes JSON und eine Datei über 128 KiB wurden über denselben Dialog
mit sichtbaren Fehlermeldungen abgewiesen; nach App-Neustart blieb der zuvor
gespeicherte Entwurf erhalten. Die Prüfung verwendete den lokalen Testentwurf,
keine exportierten Vite-Browserdaten. Deren tatsächliche Übernahme ist offen.
Lokale Messdateien: `.local/evidence-a2-continuation/`.

Ein zusätzlicher Vite-zu-Android-Rundlauf am 10.10.2026 verwendete einen mit
`companion/app/src/editorDraft.mjs` erzeugten und exportierten Entwurf mit zwei
Szenen. Die Android-App las die Datei über den Öffnen-Dialog, zeigte Titel und
Szenenreihenfolge, speicherte den Entwurf ausdrücklich und stellte ihn nach
Prozessneustart wieder her. Das ist ein synthetischer Vite-Entwurf; eine
Übernahme zuvor gespeicherter Browserdaten ist damit nicht belegt. Datei und
Messprotokoll liegen lokal unter `.local/evidence-a2-vite/` in beiden App-Teilen.

## NFC-Tags

Die Seite „NFC-Tags“ verwendet Android ReaderMode im Vordergrund, unabhängig
von Bluetoothrechten. Beim Wechsel zur NFC-Seite endet die BLE-Sitzung.
Hintergrund, Seitenwechsel und Rotation verwerfen einen vorbereiteten
Schreibauftrag. NFC muss in den Handyeinstellungen eingeschaltet sein.

Unterstützt werden bereits NDEF-formatierte Tags mit dem GM-Companion-v1-
Datensatz oder leerem NDEF-Inhalt. Der [Datenvertrag](../contract/nfc-v1/README.md)
beschreibt die Referenzbytes. Dieses Format ist ein Vorschlag und ein sichtbarer
Testpfad, noch kein ESP-Spielobjekt-Vertrag. Fremde, beschädigte und mehrdeutige Inhalte werden
nicht überschrieben. Unformatierte Tags und dauerhafte Schreibsperren werden
nicht eingerichtet. Andere vorhandene NDEF-Datensätze bleiben erhalten.

Zuerst Tag lesen, Kennwert/Flags/Stationsbesuche bearbeiten, „Schreiben
vorbereiten“ wählen und bestätigen. Der vorhandene Zeitstempel bleibt erhalten;
bei leeren Tags ist er der Referenz-Standardwert 0. Tag entfernen und denselben
Tag erneut anhalten. Der einmalige Auftrag vergleicht Tagkennung und vollständigen
frisch gelesenen Inhalt, prüft Schreibschutz und Gesamtkapazität und liest nach
dem Schreiben erneut über Funk. Erst exakte Übereinstimmung bestätigt Erfolg.
Tagverlust nach Schreibbeginn ergibt einen unklaren Ausgang und keine automatische
Wiederholung. Ein vor dem Schreiben synchron gespeichertes Journal überlebt
einen Prozessneustart; zunächst denselben Tag erneut lesen. Das Journal ist keine
Schreibqueue und wird nie selbstständig ausgeführt. Die Tagkennung dient der
Bedienbindung, nicht als Sicherheits- oder Authentizitätsnachweis.

JVM-Tests und erfolgreiche Builds belegen keine Zuverlässigkeit am physischen
Tag. Die Hardwareabnahme umfasst Referenz-Tag lesen, leeren Test-Tag schreiben,
erneut lesen, unveränderte weitere Datensätze, Schreibschutz, anderen Tag und
Tagverlust während des Schreibens. Grundlagen:
[NFC](https://developer.android.com/develop/connectivity/nfc/advanced-nfc),
[Ndef](https://developer.android.com/reference/android/nfc/tech/Ndef),
[ReaderMode](https://developer.android.com/reference/android/nfc/NfcAdapter).

## Prüfung und Grenzen

NFC-Hardwarestand vom 10.10.2026: Auf dem angeschlossenen Testhandy wurde
ein vorbereiteter Schreibauftrag auf einem NDEF-Tag mit 137 Byte Kapazität
ausgeführt. Das App-Protokoll zeigt Schreiben abgeschlossen, frisches
RF-Rücklesen (55 Byte) und `nfc_write_result=VERIFIED`; weitere Lesevorgänge
validierten den Inhalt erneut. Damit ist eine direkte Schreib-/Rücklesefolge
am realen Tag belegt. Eine Änderung der Nutzdaten gegenüber dem Ausgangsinhalt
ist in dieser Messung nicht dokumentiert. Geänderte Daten, Fehlerfälle,
Mehrrecord-Erhalt und Dauerbetrieb bleiben separat zu prüfen; die Ursache
früherer unklarer Schreibausgänge ist weiterhin offen.
Lokale Messdateien: `.local/evidence-live/phone-log.txt` und `phone-ui.txt`.
In der NFC-Sitzung vom 10.10.2026 wurde keine neue APK installiert; die installierte Fassung
wurde nicht erneut per Hash zugeordnet. Erneute Build-/Testversuche scheiterten
vor der Ausführung an der lokalen Gradle-Daemon-Verbindung.

JVM-Tests prüfen vorhandene Firmware-Golden-Fixtures sowie Offsetkonflikte,
Fristen, Größen, Typen, Bytebesitz und Reset. USB-Hardwareprüfung erfolgt mit
der installierten APK, echtem Scan, GATT-Dienst und vollständigem Status.
Lokale historische Handyprotokolle gehören nicht zum kuratierten Paket.
Neue Prüfungen mit Nutzerhandlungen werden vom Nutzer in der Projekt-App
geführt; ein abgeschlossener exportierter Bericht wird anschließend ausgewertet.

Ein einzelner Handytest belegt keinen Dauerbetrieb, weitere Androidmodelle,
Legacy-Spielanmeldung oder physischen Spielgerätebetrieb. Legacy-Spielleitungsfunktionen bleiben ein eigener Anschluss.
Für diese einzelne Seite genügt Konstruktorinjektion.

Grundlagen, geprüft am 10.10.2026:
[Android-Architektur](https://developer.android.com/topic/architecture/recommendations),
[BLE-Berechtigungen](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions),
[Compose-Barrierefreiheit](https://developer.android.com/develop/ui/compose/accessibility/api-defaults),
[AGP8.7-Kompatibilität](https://developer.android.com/build/releases/agp-8-7-0-release-notes),
[Compose/Kotlin](https://developer.android.com/jetpack/androidx/releases/compose-kotlin),
[Buildvarianten](https://developer.android.com/build/build-variants).
Bekannter stabiler Versionssatz; keine Behauptung, dies seien die neuesten Versionen.
