# Reproduzierbare Prüfgrundlage B0

Stand: **14. Oktober 2026**, aufgebaut auf dem kuratierten Basiscommit
`42610abfef07c971d9902ea6ca5a350ed68981bf`. Diese Anleitung beschreibt den
Prüfweg für [Issue #38](https://github.com/Sven401/Larp-Escape/issues/38) innerhalb
der [verbindlichen Planung #25](https://github.com/Sven401/Larp-Escape/issues/25).
Maßgeblich sind der [Quality-Workflow](../.github/workflows/quality.yml),
[PlatformIO-Konfiguration](../platformio.ini) und
[Android-Buildbasis](../companion/android/README.md). Beschriebene Aufrufe sind
keine Erfolgsmeldung; Ergebnisse gehören immer zu einem konkreten Commit und Lauf.

## Umgebungen und CI-Umfang

Alle vier Jobs laufen getrennt auf `windows-2022`. Der Runner liefert die
Windows-Werkzeuge; Projektversionen werden ausdrücklich gewählt:

| Teil | Version / Quelle |
| --- | --- |
| Python / PlatformIO Core | 3.11 / 6.2.0 |
| C++ / Formatter | C++11 / clang-format 19.1.7 |
| PlatformIO-Plattformen | `windows_x86@1.2.0`, `espressif32@6.6.0`; Bibliotheken gemäß `platformio.ini` |
| Native Kryptografie | Unveränderte offizielle Mbed TLS 2.28.4-Quellen mit Archivprüfung |
| Android Java / SDK | JDK 17, SDK 35, Build Tools 35.0.0 |
| Android Build | Gradle-Wrapper 8.9, AGP 8.7.3, Kotlin 2.0.21; weitere Pins im [Versionskatalog](../companion/android/gradle/libs.versions.toml) |

| Job | Nachweis |
| --- | --- |
| `hardware-independent` | CI-Helfertests, Architektur samt Fixtures, `host-regression`, `host-configured-control`, Profilwerkzeug, isolierte C++11-Header und Formatbaseline |
| `native-crypto` | Geprüfter Mbed-TLS-Download und `host-peer-proof` mit echtem Software-Kryptografiebackend |
| `esp32-build` | Produktfirmware, Runtime-Probe, Profil-Runtime-Probe und isolierte GATTS-ABI-Probe |
| `android-checks` | Protokolltests, App-JVM-Tests, Debug-APK und Debug-Lint |

Die CI verwendet den eingecheckten Gradle-Wrapper und wählt JDK 17 ausdrücklich.
`JAVA_HOME_17_X64` und `ANDROID_HOME` kommen aus dem Windows-Runner;
SDK 35 und Build Tools 35.0.0 müssen vorhanden sein. Auf einem lokalen Rechner
entsprechende JDK-/SDK-Pfade setzen. Ein frischer Klon benötigt Netzwerk für
PlatformIO, Gradle, Plugins, Bibliotheken und das Mbed-TLS-Archiv. Offlineprüfungen
setzen vollständige lokale Downloads voraus und sind kein Ersatz für diesen CI-Pfad.

## Lokale Aufrufe

Aus dem Repository-Stamm unter Windows mit Python 3.11; ausschließlich der
Android-Aufruf läuft im angegebenen Unterverzeichnis. Diese Schritte laden
keine Firmware auf ein Gerät und installieren keine APK.

```powershell
python -m pip install --disable-pip-version-check platformio==6.2.0 clang-format==19.1.7
python -m unittest discover -s scripts/tests -v
python scripts/check_architecture.py --self-test
python scripts/check_architecture.py
python -m platformio test -e host-regression -v
python -m platformio test -e host-configured-control -v
python -m platformio run -e profile-tools
python scripts/check_headers.py
python scripts/check_format.py
```

Für die echte native Kryptografie bereitet der Helper einen externen
temporären Quellenordner vor. Erst nach erfolgreichem Download und Audit die
beiden Umgebungsvariablen setzen und die Suite starten:

```powershell
$peerCryptoRoot = Join-Path ([IO.Path]::GetTempPath()) 'larp-escape-peer-mbedtls'
python scripts/prepare_peer_mbedtls.py --destination $peerCryptoRoot
if ($LASTEXITCODE -ne 0) { throw 'Mbed-TLS-Vorbereitung fehlgeschlagen' }
$env:LARP_PEER_MBEDTLS_DIR = Join-Path $peerCryptoRoot 'mbedtls-2.28.4'
$env:LARP_PEER_MBEDTLS_ARCHIVE = Join-Path $peerCryptoRoot 'mbedtls-2.28.4.tar.gz'
python -m platformio test -e host-peer-proof -v
```

In GitHub Actions schreibt der zusätzliche Parameter `--github-env $env:GITHUB_ENV`
die beiden Pfade für folgende Schritte. Der Helper verwendet das offizielle
[Mbed-TLS-2.28.4-Archiv](https://codeload.github.com/Mbed-TLS/mbedtls/tar.gz/refs/tags/mbedtls-2.28.4)
und den bereits vorhandenen SHA-256-Pin
`504bd29af6e7f9f3de1f0f7b7e16c73987a4194338681acd72b82383a49d55d5`.
Der [bestehende Audit](../scripts/native_peer_mbedtls.py) prüft die
Include-/Librarydateien gegen das Originalarchiv. Abweichungen und fehlende
Quellen führen zum Fehler; ein Ersatzbackend wird nicht verwendet. Herkunft und
Grenzen des Native-/SDK-Vergleichs stehen im [Kryptografievertrag](companion/peer-proof-v1.md).

```powershell
python -m platformio run -e az-delivery-devkit-v4 -e esp32-runtime-probe -e esp32-profile-runtime-probe -e esp32-gatts-abi-probe -j 2
```

Mit lokal gesetztem `JAVA_HOME` für JDK 17 und `ANDROID_HOME` für das SDK:

```powershell
Push-Location companion/android
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug --no-daemon --console=plain --stacktrace --continue
Pop-Location
```

`--continue` führt nach einem Fehler noch unabhängige Gradle-Aufgaben aus;
ein fehlgeschlagener Test oder Build lässt den Gesamtaufruf trotzdem scheitern.

## Ergebnisse lesen und Grenzen beachten

Unter [GitHub Actions → Quality](https://github.com/Sven401/Larp-Escape/actions/workflows/quality.yml)
den Lauf für den gewünschten Commit öffnen. Jeder Job sichert ein eigenes
Artefakt `quality-<jobid>-<run_attempt>` mit `provenance.json`, `summary.md` und
je Prüfaufruf einer JSON-Datei sowie einem Log. Der
[Beleg-Wrapper](../scripts/quality_evidence.py) erfasst die Aufrufe; der lokale
CI-Ausgabeordner ist `RUNNER_TEMP/larp-quality-GITHUB_JOB`. Ausgecheckter Commit,
PR-Head und Jobstatus werden getrennt ausgewiesen. Dort Lauf, Werkzeugstand
und die tatsächlich ausgeführten Prüfungen abgleichen. Ein übersprungener oder
an einer Voraussetzung gescheiterter Schritt ist kein bestandener Test. Bei Fehlern bleiben die bis
dahin erzeugten Belege erreichbar; fehlt bereits die Vorbereitung, können
auch Berichte fehlen.

Das zusätzliche Artefakt `android-reports-<run_attempt>` enthält die erzeugten
JUnit-XML-Dateien und HTML-Test-/Lintberichte. Lokal entstehen sie unter
`companion/android/core/protocol/build/{test-results,reports}/` sowie
`companion/android/app/build/{test-results,reports}/`; die Debug-APK liegt unter
`companion/android/app/build/outputs/apk/debug/app-debug.apk`.
Die APK wird nicht als CI-Artefakt hochgeladen. Buildausgaben werden nicht
versioniert. Eine APK ist ein Buildartefakt und keine Geräte- oder
Veröffentlichungsfreigabe.

Hosttests prüfen Softwarepfade, die native Kryptografie das offizielle
Softwarebackend, ESP32-Builds Übersetzung und Linken sowie die GATTS-Probe
zusätzlich die isolierte SDK-Anbindung. Android-JVM-Tests und Lint benötigen
kein Handy. Keine dieser Prüfungen bestätigt BLE/SMP am Funk, NFC, reale
Gerätebesitzer, Flashverhalten, LEDs oder einen vollständigen Produktablauf.
Historische Ergebnisse in [handoff/PRUEFSTAND.md](../handoff/PRUEFSTAND.md)
bleiben datierte Belege und ersetzen keinen neuen Lauf.
