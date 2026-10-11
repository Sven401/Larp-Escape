# Larp-Escape Companion

Die native [Android-App](android/README.md) ist die Entwicklungsbasis: Kotlin,
Jetpack Compose und Material 3. Sie benötigt keinen Vite-Server.

## Einstieg

JDK17 oder 21, Android SDK35 und Build Tools35.0.0 einrichten. Aus
`companion/android/` ausführen:

```powershell
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug
```

Der erste Wrapperlauf benötigt Netzwerk. Offlineprüfung setzt vorhandene
Gradle-, Plugin- und Bibliotheksdownloads voraus. Weitere Hinweise und
Debug-/Releasegrenzen stehen in der Android-Anleitung.

Die App enthält BLE-Suche, GATT-Diensterkennung und lesenden v2-Laborstatus,
sowie lokale Szenarien mit Leseprobe, atomarer Speicherung und JSON-Dateiauswahl.
Die bereits integrierte NFC-Seite verwendet ein ausdrücklich vorläufiges
[Testformat](contract/nfc-v1/README.md). Release erlaubt keinen Laborzugang.
Produktanmeldung, bestätigte Spielrechte und Spielbefehle sind noch offen.

## Verträge und Referenzen

[Verträge](contract/README.md) und [offene Abhängigkeiten](OPEN-DEPENDENCIES.md)
beschreiben den begrenzten Stand. [Quellenherkunft](SOURCE.md) und das
[Dateimanifest](source-manifest.json) halten die kuratierte Übernahme fest.

Unter [app/](app/README.md) sind gezielt die Vite-Szenarioregeln und ihre
Fachtests als Referenz enthalten. Die vollständige historische Weboberfläche,
Experimente, Caches, APKs, Browserdaten und lokale Handybelege gehören nicht zu
diesem Paket. Vor einer Vite-Stilllegung müssen tatsächliche Browserentwürfe
exportiert und geprüft in Android übernommen werden.

Lokale Dokumente und öffentliche Lease-Werte verleihen keine Geräteberechtigung.
Firmwareantworten und gültige Gerätebindungen bleiben für Aktionen maßgeblich.
