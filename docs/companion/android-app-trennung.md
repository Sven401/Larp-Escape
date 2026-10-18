# Zwei Android-Apps mit Erhalt der Dev-Installation

Stand: **18. Oktober 2026**. Nutzerauftrag zur direkten Umsetzung in Sitzung
`01a150b6-981d-7791-a37d-34fac8dd18a3`; Grundlage [UX-01](../ui-ux/PROJEKTREFERENZ.md).
Quellbasis vor Implementierung: `2c3964a662eff1498cc63eb089fd8f04f41a598e`.

## Umsetzung und Grenze

| Merkmal | Bestehende Dev-App | Neue Spielleiter-App |
| --- | --- | --- |
| Gradle-Modul | `:app` bleibt erhalten | `:gamemaster` |
| applicationId / Namespace | `de.larpescape.companion` unverändert | `de.larpescape.gamemaster` |
| Anzeigename | Larp Escape Dev | Larp Escape Spielleitung |
| Symbol | Werkzeug auf grauem Grund | Fahne auf grünem Grund |
| Version | Code 2 / 0.2, vorher Code 1 / 0.1 | Code 1 / 0.1 |
| Funktion | Alle bisherigen Baugruppen-, NFC- und Szenariowege | Eigenständiger Start mit ehrlichem Leerzustand ohne eingerichtete Veranstaltung |
| Manifest | Bestehende BLE-/NFC-Berechtigungen und HCE-Testservice bleiben | Kein BLE-/NFC-/Internetzugriff, kein HCE-Testservice |
| Daten | Bisheriger Appbereich unverändert | Eigener isolierter Appbereich, keine Übernahme aus Dev |

Die Apps sind technisch unabhängig installierbar. Android-Geräteinstallation,
Datenerhalt am echten Handy und native Sichtprüfung sind getrennte, noch offene
Abnahmen. Die Spielleiter-App besitzt in diesem Schnitt keine Stationsverwaltung,
keine BLE-Steuerung und keine simulierten Teststationen. Der [Stationspilot](../ui-ux/BOX-PILOT.md)
ist die Grundlage des nächsten Funktionsausbaus, nicht bereits nativ umgesetzt.
Der Leerzustand enthält keine funktionslosen Einrichtungsbuttons.

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
Der funktionsarme Spielleitungsstart benötigt ihn noch nicht. Spätere Wiederverwendung
von Protokoll-/Domänencode erfolgt über gezielte Bibliotheksabhängigkeiten; echte
BLE-/Zugangsadapter erst mit eigener Zustandsbindung übernehmen. Kein Duplizieren
der kompletten Dev-App als vermeintliches Endnutzerprodukt.

Zwei installierte Apps bedeuten nicht zwei gleichzeitige Steuersitzungen derselben
Box. Die Sitzungshoheit und Rechte nach VA-01 bleiben unverändert. Die neue App
öffnet im jetzigen Stand keinerlei Geräteverbindung.

## Bauen und Artefakte

Aus `companion/android`, mit JDK 17/21, SDK 35 und Build Tools 35.0.0:

```powershell
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:assembleRelease :app:lintDebug :app:lintRelease :gamemaster:assembleDebug :gamemaster:assembleRelease :gamemaster:lintDebug :gamemaster:lintRelease --no-daemon --console=plain
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

## Geräteabnahme vor einem beauftragten Update

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

Diese Gerätefälle sind offen; in der Umsetzung wurde keine App installiert oder aktualisiert,
keine Box angesprochen und keine Firmware hochgeladen. Fehlender Gerätezugriff
wird nicht durch Rechner-/Buildtests als erledigt gewertet.
