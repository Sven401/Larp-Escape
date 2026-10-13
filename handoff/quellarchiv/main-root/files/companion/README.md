# Larp-Escape Companion

Die verbindliche Projekt-App ist die native **Android-App** unter
[android/](android/README.md): Kotlin, Jetpack Compose und Material 3.
Neue App-Funktionen entstehen dort. Android braucht keinen Vite-Server und
keine Node-Abhängigkeiten.

## Einstieg und normale App-Prüfung

[Android-Build, Installation und Prüfung](android/README.md) beschreibt
Voraussetzungen, SDK-Auswahl, lokalen Offlinecache und Handyprobe.
Vom Produkt-Repositoryroot aus:

```powershell
Set-Location companion/android
./gradlew.bat :core:protocol:test :app:assembleDebug :app:lintDebug
```

JDK und Android-SDK müssen wie in der Android-Anleitung eingerichtet sein.
Der erste Wrapperlauf benötigt Netzwerk; Offlineprüfung setzt bereits
vorhandene Gradle- und Plugin-/Bibliotheksdownloads voraus.
Die APK liegt danach unter `app/build/outputs/apk/debug/app-debug.apk`.

Die erste Version sucht ESP-Geräte, bestätigt ihren GATT-Dienst und zeigt im
Debug-Vertragslabor vollständige v2-Statusnachrichten. Trennen löscht den
angezeigten Status. Der Produktionsdienst wird erkannt; Produktanmeldung,
bestätigte Rechte, Produktstatus und Spielleitungsbefehle sind noch offen.
Der Laborzugang ist auf Debug beschränkt. Release erkennt nur den
Produktservice und ist noch keine freigegebene Bedien-App.

## Verträge und Abhängigkeiten

- [contract/](contract/README.md) enthält die gemeinsame v2-Erwartungsbasis
  und die getrennte historische v0-Mockreferenz.
- [firmware-v2/SNAPSHOT.md](contract/firmware-v2/SNAPSHOT.md) nennt Herkunft
  und Prüfsummen der kopierten Firmwarefixtures. Vor Produktintegration
  diese Basis mit dem aktuellen Firmware-Arbeitsbaum abgleichen.
- [OPEN-DEPENDENCIES.md](OPEN-DEPENDENCIES.md) trennt den gemessenen
  Laborstand von den offenen Produkt-, Android- und Dokumentfunktionen.

Firmwareantworten und aktuelle Gerätebindungen bleiben maßgeblich.
Öffentliche Lease-Werte, lokale Entwürfe und gespeicherte Anzeigen verleihen
keine Geräteberechtigung und bestätigen keine ausgeführte Aktion.

## Vite und Diagnose als Referenzen

[app/](app/README.md) bleibt vorübergehend als Vite-Referenz erhalten:
Mocksteuerung, lokaler Szenarioeditor, Leseprobe und Fachtests. Ein bewusster
Referenzstart ist dort dokumentiert; er gehört nicht zum Android-Build.
Die dortigen v0-Befehle und Mockrechte sind kein v2-Firmwarevertrag.

Vorhandene Browserentwürfe vor einer späteren Stilllegung als JSON exportieren
und in Android geprüft übernehmen. Browser-localStorage liegt außerhalb Git;
die Appinstallation übernimmt diese Daten nicht. Vite wird erst nach
Funktionsersatz, Datenübernahme und gesicherter Herkunft archiviert.
Die getrennten Diagnoseexperimente unter `experiments/` liefern Prüfbelege;
sie sind keine weitere verbindliche Projekt-App.