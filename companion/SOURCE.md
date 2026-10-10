# Companion: Quellenherkunft und Prüfstand

Kuratierte Entwicklungsbasis vom 2026-10-10, Ausgangscommit
`29da937c4309beddfd45c5dd0fbf84fdf853d8e4`. Die App war im ursprünglichen
Arbeitsbaum unversioniert. Der Ausgangscommit allein enthält diese Quellen nicht.
Das [Dateimanifest](source-manifest.json) nennt 57 einzeln übernommene Dateien,
ihre ursprünglichen SHA256-Werte und die Bytes des kuratierten Pakets.
Drei weitere Dateien dokumentieren die Abgrenzung und halten Clonebytes stabil.
Dieses Dokument und das Manifest sind selbst keine Eingangsquellen.

## Umfang und Herkunft

Android-Gradleprojekt einschließlich Wrapper, App, Protokollkern, JVM-Tests und
Fixtures. Die bereits in MainActivity integrierte NFC-Seite bleibt Teil dieser
Basis; ihr GM-Format ist ausdrücklich ein Testvorschlag. Die NFC-Oracle-Herkunft
steht im [Vertrag](contract/nfc-v1/README.md); originale C#-Projekte sind nicht
mitkopiert. Firmware-v2-Snapshot: Commit `c9654c3c1365f4ada2dd5b6ce82bc5ac9373459b`,
zwölf unveränderte Dateien mit eigenem Hashmanifest.

Fünf gezielt geprüfte Vite-Referenzdateien erhalten Szenarioregeln, Leseprobe,
Speicheradapter und Fachtests. Sie enthalten keine Browserdaten. Der Android-
Fixtureexport ist synthetisch. Vollständige Vite-Oberfläche, v0-Mockabläufe,
Experimente, Telefonautomationshilfen, APKs, Caches und lokale Messprotokolle
sind ausgeschlossen. Die unversionierten Originaldateien bleiben erhalten.

Quelldateien und Firmwarefixtures bleiben bytegleich; Änderungen betreffen
Paketdokumentation, Ausschluss des erzeugten Kotlin-Caches, zwei Markdown-Zeilenumbrueche im Snapshotmanifest und LF-Zeilenenden des Unix-Wrappers. Die `.gitattributes`
verhindert automatische Änderungen an den kuratierten Quelldateien und setzt
die vorgesehenen Wrapperzeilenenden. Der Unix-Wrapper wird ausführbar versioniert.

## Frische Prüfungen am 2026-10-10

Im isolierten Produkt-Worktree auf dem Ausgangscommit, JDK21.0.8, SDK35/
Build Tools35.0.0, Gradle8.9 mit vorhandenem Offlinecache:

```powershell
./gradlew.bat :core:protocol:cleanTest :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug :app:assembleRelease :app:lintRelease --offline --no-daemon
```

BUILD SUCCESSFUL: 112 Tasks, 111 ausgeführt, eine UP-TO-DATE. Core: 26 Fälle;
App: 16 Fälle; jeweils null Fehler und Fehlschläge. Debug- und Release-Lint:
je null Fehler und drei Warnungen. SDK-Metadaten meldeten Schreibschutz,
der Kotlin-Daemon startete nach einer Meldung erneut; der Gesamtbuild bestand.
Die lokalen Cachepfade sind keine eingecheckte Voraussetzung. Ein neuer Klon
benötigt die dokumentierten Downloads.

Snapshotintegrität und beide Szenario-Referenztests bestanden. Der erste
Referenztestlauf zeigte einen fehlenden Speicheradapter; dieser wurde gezielt
mit Herkunft ergänzt, anschließend bestand der Test. Elf JSONfixtures/Schemata
wurden inhaltlich mit `firmware-peer-ingress` auf `7f2a3e4` plus lokalem Diff
verglichen: alle identisch; Zeilenenden der Firmwarequelle können abweichen.
Kein neuer Firmwarebuild oder Funklauf wird dadurch behauptet.

## Grenzen

Keine neue Handy-, NFC-, ESP- oder Vollhardwareabnahme. Historische Handybelege
in der Android-Anleitung sind nicht Teil dieses Pakets. Produktiver Peerpfad,
Schlüsselverwaltung, gegenseitiger Gerätenachweis, Spielsteuerung und
nutzergeführte Vollhardwarediagnose bleiben offen. Release ist unsigniert.
Die drei Lintwarnungen betreffen Ziel-SDK35, fehlende dataExtractionRules
und fehlendes Appicon; sie sind vor einer Verteilung erneut zu bewerten. Dieses Paket autorisiert keinen Upload, Push,
PR, Merge oder eine Veröffentlichung.

Android-Grundlagen erneut gelesen am 2026-10-10:
[Architektur](https://developer.android.com/topic/architecture/recommendations),
[BLE-Berechtigungen](https://developer.android.com/develop/connectivity/bluetooth/bt-permissions)
und [Android-Skills](https://github.com/android/skills). Bei dieser Kuratierung
bleiben Repository/ViewModel/StateFlow und die bestehende Oberfläche erhalten;
kein spezieller Upstream-Skill war erforderlich oder wurde installiert.
