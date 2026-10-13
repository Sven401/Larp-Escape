# Larp-Escape: verbindliche Android-App

Kotlin, Jetpack Compose und Material 3. Eigenständige App mit Paketname
`de.larpescape.companion`, Mindestversion Android 13/API33. Bestehende
Webprototypen und der .NET-Diagnoseclient bleiben separate Referenzen.
Dies ist die Entwicklungsbasis der Projekt-App; die erste Version ist ein
lesender Verbindungsprüfstand, noch keine freigegebene Spielleitungs-App.

`app` enthält UI/ViewModel, DeviceRepository und Android-BLE. `core:protocol`
enthält den plattformunabhängigen, begrenzten FrameAssembler. Alle BLE-Rückrufe
werden mit kopierten Bytes auf dem Hauptthread verarbeitet und an die jeweilige
Verbindungsgeneration gebunden. Ein ViewModel besitzt die Sitzung auch bei
Rotation. Verlassen der App beendet die Sitzung; kein Hintergrundservice.

## Bauen und installieren

Alle Befehle in diesem Abschnitt laufen aus `companion/android/`.
Voraussetzung: JDK17 oder 21, Android SDK35 und Build Tools35.0.0. Der Gradle
Wrapper lädt Gradle8.9; Versionen stehen in `gradle/libs.versions.toml`.
SDK über `ANDROID_HOME` oder eine lokale, ignorierte `local.properties` angeben.

```powershell
$env:JAVA_HOME='C:/Program Files/Android/openjdk/jdk-21.0.8'
$env:ANDROID_HOME='C:/Program Files (x86)/Android/android-sdk'
./gradlew.bat :core:protocol:test :app:assembleDebug :app:lintDebug
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
Debug darf den markierten Laborservice lesen. Release lehnt reine
Laborgeräte bei der Dienstprüfung ab und verwendet auch bei gemeinsam
angebotenen Diensten ausschließlich den kanonischen Produktservice.
Werbenamen und UUIDs bleiben Auswahlhilfen; erst GATT bestätigt den Dienst.

```powershell
./gradlew.bat :app:assembleRelease :app:lintRelease
```

Release ist derzeit unsigniert und noch nicht zur Bedienung freigegeben:
Produktanmeldung, Produktstatus und Befehle fehlen. Der Build ist ein
Prüfartefakt, kein Verteilungs- oder Veröffentlichungsauftrag.

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

Die sieben Fake-Repository-Fälle prüfen die ViewModel-Regeln separat:

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
und absolute Fristen entsprechen dem [Laborvertrag](../../experiments/v2-contract-probe/README.md).
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

Der reine Kotlin-Protokollkern `PeerAuthV1` verarbeitet den optionalen
Firmwarevertrag für Challenge-Issue, 108-Byte-Challenge, 72-Byte-Proof und
32-Byte-Ergebnis. Er bindet Challenge und Ergebnis an erwartete Geräte-/Peerwerte,
Nachrichten-ID und Sequenz, erzeugt begrenzte Proof-Fragmente und normalisiert
eine gültige P256-Signatur auf Low-S. Ein Test vergleicht seine Bytes mit der
unveränderten Beobachtung aus dem Firmware-Hostpfad. Der Kern ist noch nicht an
Android-GATT oder eine private Schlüsselverwaltung angeschlossen. Die öffentliche
Gerätekennung ist kein gegenseitiger Servernachweis; auch der Ergebniszustand
„Granted“ allein erteilt der App keine Spielrechte. Firmware F1, vertrauenswürdige
Einrichtung und produktiver Dienststart bleiben Voraussetzungen für den echten
Anmeldeweg.

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
In dieser Sitzung wurde keine neue APK installiert; die installierte Fassung
wurde nicht erneut per Hash zugeordnet. Erneute Build-/Testversuche scheiterten
vor der Ausführung an der lokalen Gradle-Daemon-Verbindung.

JVM-Tests prüfen vorhandene Firmware-Golden-Fixtures sowie Offsetkonflikte,
Fristen, Größen, Typen, Bytebesitz und Reset. USB-Hardwareprüfung erfolgt mit
der installierten APK, echtem Scan, GATT-Dienst und vollständigem Status.
`tools/phone_check.py` liest nur die UI dieser App und ihren Logtag
`LarpEscape`; Ergebnisse landen ignoriert unter `.local/evidence`.
Die Aktionen `permission-ui`, `permission-tap` und `permission-screen` sind
ausschließlich für den Systemdialog „Geräte in der Nähe“ dieser App freigegeben;
andere App- oder Berechtigungsdialoge werden vom Prüfhilfsmittel abgewiesen.
Mit `--output-dir .local/evidence-a0` bleiben ältere Messungen erhalten:

```powershell
python tools/phone_check.py ui --output-dir .local/evidence-a0
python tools/phone_check.py logs --output-dir .local/evidence-a0
```

Ein einzelner Handytest belegt keinen Dauerbetrieb, weitere Androidmodelle,
Produktanmeldung oder physischen Spielgerätebetrieb. Navigation, Hilt,
Spielleitungsseite und Editor folgen weiteren Vertikalen des Architekturplans.
Für diese einzelne Seite genügt Konstruktorinjektion.

Grundlagen, geprüft am 10.10.2026:
[Android-Architektur](https://developer.android.com/topic/architecture/recommendations),
[BLE-Berechtigungen](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions),
[Compose-Barrierefreiheit](https://developer.android.com/develop/ui/compose/accessibility/api-defaults),
[AGP8.7-Kompatibilität](https://developer.android.com/build/releases/agp-8-7-0-release-notes),
[Compose/Kotlin](https://developer.android.com/jetpack/androidx/releases/compose-kotlin),
[Buildvarianten](https://developer.android.com/build/build-variants).
Bekannter stabiler Versionssatz; keine Behauptung, dies seien die neuesten Versionen.
