# Zwei Android-Apps mit Erhalt der Dev-Installation

Stand: **19. Oktober 2026**. Ergänzung: [nativer Stationspilot mit echter Boxprüfung](spielleiter-stationspilot.md).
Historische technische Trennung vom 18.10.2026: Nutzerauftrag zur direkten Umsetzung in Sitzung
`01a150b6-981d-7791-a37d-34fac8dd18a3`; Grundlage [UX-01](../ui-ux/PROJEKTREFERENZ.md).
Quellbasis vor Implementierung: `2c3964a662eff1498cc63eb089fd8f04f41a598e`.

## Umsetzung und Grenze

| Merkmal | Bestehende Dev-App | Neue Spielleiter-App |
| --- | --- | --- |
| Gradle-Modul | `:app` bleibt erhalten | `:gamemaster` |
| applicationId / Namespace | `de.larpescape.companion` unverändert | `de.larpescape.gamemaster` |
| App-Auswahl / sichtbarer Kontext | Larp Dev / Larp Escape Dev | Larp GM / Veranstaltung und Station |
| Symbol | GM-Companion-Motiv mit DEV-Badge | Originalmotiv von GM Companion |
| Version | Code 2 / 0.2, vorher Code 1 / 0.1 | Code 2 / 0.2; leerer Einstieg war Code 1 / 0.1 |
| Funktion | Alle bisherigen Baugruppen-, NFC- und Szenariowege | Lokale Stationsvorbereitung, Auswahl, automatische BLE-Anmeldung und frischer bestätigter Zustand |
| Manifest | Bestehende BLE-/NFC-Berechtigungen und HCE-Testservice bleiben | BLE Scan/Connect für Stationsprüfung; kein NFC/Internet oder HCE-Testservice |
| Daten | Bisheriger Appbereich unverändert | Eigener isolierter Appbereich, keine Übernahme aus Dev |

Die Apps sind technisch unabhängig installierbar. Installation, Dev-Update mit
Datenerhalt und Appwechsel sind inzwischen am Moto G14 geprüft; siehe
[Gerätebericht und Iconquelle](android-app-geraetepruefung-2026-10-18.md).
Der unten datierte Erstprüfstand bleibt als historischer Beleg erhalten.
Der [native Stationspilot](spielleiter-stationspilot.md) ergänzt lokale Vorbereitung,
Auswahl und lesenden BLE-Zustand; Licht-/Spielaktionen bleiben Folgearbeit.
Der [Designpilot](../ui-ux/BOX-PILOT.md) ist die Grundlage des nativen Zustandsablaufs.
Der ursprüngliche Leerzustand wurde durch die echte Stationsvorbereitung ergänzt.

Die bestehende App wird nicht umbenannt im Sinne ihrer Android-Identität: allein
Launchername, Icon, Kopfzeile und Versionsnummer ändern sich. Kein `applicationIdSuffix`
für Dev, kein Verschieben ihrer Daten und kein Umbenennen ihrer Einstiegsklasse.
Debug und Release behalten je App dieselbe Kennung; sie sind keine zusätzlich
parallel installierbaren Produkte. Ein Signaturwechsel ist kein zulässiger Ersatz
für ein Update der bisherigen Installation.

## Erhaltener Datenbestand

| Daten | Unveränderter Speicher / Beleg |
| --- | --- |
| Boxzugang | `noBackupFilesDir/led-access-v1.bin`, AtomicFile; AES-GCM; Keystore-Alias `larpescape-led-access-v1` in [LedAccessStore](../../companion/android/app/src/main/java/de/larpescape/companion/platform/access/LedAccessStore.kt) |
| Szenario | `filesDir/scenario-draft-v1.json` und AtomicFile-Rückweg in [ScenarioDraftRepository](../../companion/android/app/src/main/java/de/larpescape/companion/data/scenario/ScenarioDraftRepository.kt) |
| Wake-Zuordnung | Private SharedPreferences `wake-input-arrangement`, Schlüssel pro Box in [WakeControls](../../companion/android/app/src/main/java/de/larpescape/companion/ui/device/WakeControls.kt) |
| Matrixläufe | `noBackupFilesDir/matrix-collection`, Laufdateien und Zeiger in [AndroidMatrixCollectionStore](../../companion/android/app/src/main/java/de/larpescape/companion/platform/ble/AndroidMatrixCollectionStore.kt) |
| RSSI-Messungen | `filesDir/rssi-runs`, bisherige CSV-Dateien; [Repository](../../companion/android/app/src/main/java/de/larpescape/companion/platform/ble/AndroidBleRepository.kt), [Journal](../../companion/android/app/src/main/java/de/larpescape/companion/data/device/RssiJournal.kt) |
| Offener NFC-Schreibauftrag | Private SharedPreferences `nfc-write-journal`, bestehende Schlüssel/Format in [AndroidNfc](../../companion/android/app/src/main/java/de/larpescape/companion/platform/nfc/AndroidNfc.kt) |

Diese Speicherimplementierungen und Formate werden nicht verändert. Kein Migrations-
oder Bereinigungslauf, kein Zurücksetzen von Einstellungen, keine Neuanlage des
bestehenden Keystore-Schlüssels durch die Apptrennung. Bestehende externe Exporte
bleiben an ihrem bisherigen Ort. Beide Apps verwenden `allowBackup=false`, keinen
`sharedUserId` und keinen gemeinsamen Datenprovider. Bibliotheksprovider erhalten
je App eigene Authorities aus ihrer applicationId.

**Updatebedingung:** Paketkennung und zum installierten Stand kompatibles
Signaturzertifikat müssen erhalten bleiben; Version darf nicht zurückgehen.
Die Debug-Signierung nutzt unverändert den vorhandenen lokalen Android-Debugschlüssel.
Ein anderer Rechner/CI kann einen anderen Debugschlüssel besitzen. CI-APKs sind
somit nicht automatisch Updates für das Handy. Bei Signaturkonflikt abbrechen,
keine Deinstallation oder Datenlöschung als Umgehung. Ohne tatsächlichen Abgleich
mit dem installierten APK wird kein datenerhaltendes Geräteupdate behauptet.

Offizielle Grundlage, am 18.10.2026 gelesen:
[Application ID und Namespace](https://developer.android.com/build/configure-app-module),
[Signierung und Updates](https://developer.android.com/studio/publish/app-signing).
Die Release-APKs sind weiterhin unsigniert; Verteilung/Produktionssignierung ist
nicht Teil dieser Umsetzung. Es wird kein privates Signaturmaterial versioniert.

## Modulgrenzen

`:app` bleibt alleiniger Besitzer der Entwicklungsoberfläche, Android-Gerätetreiber,
Datenspeicher und HCE-Testemulation. `:gamemaster` hat ein eigenes Manifest, Ressourcen
und eine eigene Activity; keine Abhängigkeit von `:app`, keine Einbindung seiner
Quellordner und keine versteckte Entwicklermodus-Umschaltung. Beide nutzen die
bereits gepinnten Compose-/Lifecycle-/Coroutine-Versionen aus dem Versionskatalog.

`:core:protocol` bleibt der vorhandene gemeinsame reine Kotlin-Kern der Dev-App.
Der native Stationspilot nutzt ihn jetzt ebenfalls, ohne Dev-Treiber oder -Speicher
zu übernehmen. Sein BLE-/Zugangsadapter besitzt eine eigene Zustandsbindung.
Weitere Wiederverwendung erfolgt über gezielte Bibliotheksabhängigkeiten; die
komplette Dev-App wird nicht als vermeintliches Endnutzerprodukt dupliziert.

Zwei installierte Apps bedeuten nicht zwei gleichzeitige Steuersitzungen derselben
Box. Die Sitzungshoheit und Rechte nach VA-01 bleiben unverändert. Der Stationspilot
öffnet eine eigene authentisierte Verbindung zur ausgewählten Box; eine zweite
gleichzeitige Boxsitzung wird nicht versprochen.

## Bauen und Artefakte

Aus `companion/android`, mit JDK 17/21, SDK 35 und Build Tools 35.0.0:

```powershell
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:assembleRelease :app:lintDebug :app:lintRelease :gamemaster:testDebugUnitTest :gamemaster:assembleDebug :gamemaster:assembleRelease :gamemaster:lintDebug :gamemaster:lintRelease --no-daemon --console=plain
```

| App | Debug | Release (unsigniert) |
| --- | --- | --- |
| Dev | `app/build/outputs/apk/debug/app-debug.apk` | `app/build/outputs/apk/release/app-release-unsigned.apk` |
| Spielleitung | `gamemaster/build/outputs/apk/debug/gamemaster-debug.apk` | `gamemaster/build/outputs/apk/release/gamemaster-release-unsigned.apk` |

Aus dem Repository-Stamm:

```powershell
python -m unittest discover -s scripts/tests -p test_android_apps.py -v
python scripts/check_android_apps.py
# Optionaler Vergleich mit einem tatsächlich passenden, erhaltenen Dev-APK:
python scripts/check_android_apps.py --dev-baseline <Pfad-zum-bisherigen-Dev-APK>
```

Der Prüfer braucht `ANDROID_HOME` (alternativ `ANDROID_SDK_ROOT`) und `JAVA_HOME`.
Er kontrolliert vier APK-Identitäten/Labels/SDK-Stände, unabhängige Einstiege,
Providerkollisionen, Backup-/UID-Grenzen, Erhalt des Dev-Testservices, fehlende
Dev-Klassen im Spielleitungs-DEX und die Gültigkeit beider Debugsignaturen.
Der optionale Vergleich prüft Dev-Paketkennung, Version und Signatur gegen das
angegebene Alt-APK. Das ist ein Artefaktbeleg, kein Nachweis gespeicherter Handydaten.
Beide Android-CI-Jobs bauen/linten beide Apps und führen den Prüfer aus.

## Prüfstand der Umsetzung

**Bestätigt am 18.10.2026**, Implementierungscommit
`3afc09a7e0c5fa097427713d65ca8ecac5e4bb9e` (inhaltlich identischer lokaler Buildbaum),
Windows, JDK 21.0.8, Projektwrapper Gradle 8.9, SDK/Build Tools 35:

- Debug und unsigniertes Release beider Apps erfolgreich gebaut; vier Lintläufe
  ohne Fehler. Je Lauf drei Warnungen (`OldTargetApi`, `DataExtractionRules`,
  `ObsoleteSdkInt`), bei Dev zusätzlich `AutoboxingStateCreation` als Information.
  Diese Hinweise sind keine native UX-/Backup-Abnahme und wurden nicht unterdrückt.
- 88 Protokolltests und 76 Dev-App-Tests erfolgreich, keine übersprungenen Tests;
  acht Tests des Trennungsprüfers erfolgreich.
- `check_android_apps.py` erfolgreich für alle vier APKs. Paket-/Manifestgrenzen
  und gültige Debugsignaturen bestätigt, kein Dev-Code im Spielleitungs-DEX.
- Die sieben oben verlinkten Speicherdateien sind per `git diff --exit-code`
  gegen die Quellbasis `2c3964a` unverändert. Bei bestehendem Dev-Kotlin-Code
  wurde ausschließlich die sichtbare Kopfzeile in `MainActivity` angepasst.
- Separater Code-/Ablaufreview: eigene Einstiege und Speichergrenzen, keine
  Geräteaktion der Spielleitung, kein vorgetäuschter Einrichtungsschritt.
  Die native Sicht-, Touch- und Accessibility-Prüfung bleibt offen.

**Zusätzlicher, rein lesender Gerätebeleg:** Im Verlauf wurde ein Moto G14 erreichbar.
`pm path` und `dumpsys package` bestätigten die installierte Dev-App
`de.larpescape.companion`, Code 1 / Version 0.1. Ihr APK wurde ohne private Appdaten
lokal ausgelesen (SHA-256
`80a24cad9b0d3e7e91b6bd29b00fdbad1cd4ff4c5f8d4a4405438823329f999b`).
Der Prüfer mit diesem APK als `--dev-baseline` bestätigt dieselbe Paketkennung,
denselben Signierer und den Versionsanstieg auf Code 2 / 0.2. Auch der Vergleich
mit dem bereits vorhandenen lokalen Dev-APK war erfolgreich. Damit ist die
Updateidentität abgeglichen; ein tatsächlich ausgeführtes Update samt Datenerhalt
ist weiterhin nicht geprüft. Die installierte App wurde nicht verändert.

SHA-256 der lokal geprüften neuen Artefakte (kein reproduzierbarer Build zugesagt):

| Artefakt | SHA-256 |
| --- | --- |
| Dev Debug | `cb4812f80f193388a3cc2a1056eb215cb2f950fca8d593d7adfe930eeb7c9d8e` |
| Spielleitung Debug | `97ddb5735977ae08bc6b1bd251bde90524a8b4f4c4d7a2f157e40d53622da6b1` |
| Dev Release, unsigniert | `ad316b937dd7681a505526b6671849fac42d166dcea5bf5207a390ff506ad245` |
| Spielleitung Release, unsigniert | `b547d879512807ae84253de11ce90389e81c01be9b3d3581fef341f0a2a68dd1` |

## Geräteprüfweg für weitere Updates

1. Tatsächlich installiertes Dev-Paket, Version und öffentliches Signaturzertifikat
   feststellen; Baseline-APK erhalten. Keine Zugangsdaten in Prüfberichte aufnehmen.
2. Vorhandensein von Szenario, Wake-Zuordnung, Matrixläufen, RSSI-Journal und
   NFC-Auftragszustand dokumentieren; Boxzugang durch reguläre Funktion prüfen,
   keinen privaten Schlüssel exportieren. Das ist keine vollständige Datensicherung.
3. Dev mit gleicher Kennung und kompatiblem Zertifikat aktualisieren, ohne
   Deinstallation oder Löschen der Appdaten. Bei Ablehnung stoppen.
4. Dieselben Daten und Boxzugang nach Update/Neustart erneut prüfen.
5. Spielleiter-APK zusätzlich installieren; beide Launcher und getrennte
   Versions-/Appinformationen prüfen. Spielleitung zeigt leeren Veranstaltungskontext.
6. Beide Apps nacheinander öffnen. Dev-Daten/Bedienung bleiben erhalten;
   Spielleitung enthält weder Dev-Daten noch Testtag-/Funksteuerung.
7. Unabhängiges Update der Spielleiter-App prüfen. Dev bleibt unverändert.

Der ursprüngliche reine Buildprüfstand wurde durch die beauftragte
[Geräteprüfung](android-app-geraetepruefung-2026-10-18.md) ergänzt: beide Apps
installiert, Dev ohne Deinstallation aktualisiert und Datenerhalt geprüft.
Die Abnahmegrenzen für Gerätefunktionen und Accessibility stehen im Bericht.
