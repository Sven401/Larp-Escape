# Vollständiger Prüfweg der kuratierten Produktbasis

Stand: **14. Oktober 2026**, Aufgabe [B0 / #38](https://github.com/Sven401/Larp-Escape/issues/38).
Entwicklungsbasis ist `plan/projektuebergabe-kuratierter-stand`, Ausgangscommit
`42610abfef07c971d9902ea6ca5a350ed68981bf`. Neue Änderungen auf einem eigenen
`codex/`-Branch vorbereiten und den PR gegen diesen Integrationsbranch richten.
`main` ist nicht die Grundlage dieses Prüfwegs.

## Reproduzierbare Eingänge

Der [Quality-Workflow](../.github/workflows/quality.yml) führt drei unabhängige
Jobs auf frischen Windows-2022-Runnern aus. Jeder Job protokolliert seinen
tatsächlich ausgecheckten Commit. Bei PRs wird ausdrücklich deren Head-Commit
geprüft; ein möglicher späterer Merge benötigt seine eigenen Prüfungen.
Es werden keine Projekt-Builds oder Abhängigkeitscaches aus früheren Läufen
wiederhergestellt. Der erste Lauf braucht Netzwerk.

| Eingang | Festlegung und Herkunft |
| --- | --- |
| Python / PlatformIO | CI: Python 3.11, PlatformIO Core 6.2.0; Plattformen und Bibliotheken unverändert in [platformio.ini](../platformio.ini) gepinnt |
| C++-Formatprüfung | clang-format 19.1.7, bestehende Formatbaseline |
| Mbed TLS | offizielles 2.28.4-Quellarchiv, URL und SHA-256 im [bestehenden Kryptografievertrag](companion/peer-proof-v1.md#reproduzierbare-belege); SHA-256 `504bd29af6e7f9f3de1f0f7b7e16c73987a4194338681acd72b82383a49d55d5` |
| ESP32-ABI | espressif32 6.6.0; unveränderte SHA-256-Pins für `libbt.a` und `libmbedcrypto.a` in [pin_gatt_source_sdk.py](../scripts/pin_gatt_source_sdk.py) |
| Android | CI: Temurin 21.0.8+9.0.LTS, SDK 35, Build Tools 35.0.0; Projektwrapper Gradle 8.9, Plugins/Bibliotheken in `companion/android/gradle/libs.versions.toml` |
| Gradle-Download | [offizielle SHA-256](https://services.gradle.org/distributions/gradle-8.9-bin.zip.sha256): `d725d707bfabd4dfdc958c624003b3c80accc03f7037b5122c4b1d0ef15cecab`; im Wrapper hinterlegt |
| GitHub Actions | alle verwendeten Actions im Workflow auf vollständige Commit-SHAs gepinnt; Versionskommentare dienen der Zuordnung |

[prepare_peer_mbedtls.py](../scripts/prepare_peer_mbedtls.py) lädt das festgelegte
Archiv, prüft seinen Hash vor dem Entpacken und stellt ausschließlich die
benötigten `include/`- und `library/`-Quellen bereit. Danach prüft das
unveränderte [Quellaudit](../scripts/native_peer_mbedtls.py) Inhalt und vollständigen
Dateibestand gegen das Archiv. Vorhandene Eingaben werden erneut geprüft.
Abweichende, fehlende oder beschädigte Eingaben sind Fehler; es gibt keinen
Mock-Ersatz und keine automatische Änderung der Pins.

## Lokal ausführen

Aus einem sauberen Checkout unter Windows mit Python und PlatformIO 6.2.0.
Die folgenden Prüfungen laufen ohne angeschlossene Hardware. Alle nativen
Befehle müssen mit Exitcode 0 enden; bei Fehlern nicht einfach den nächsten
Befehl als erfolgreichen Gesamtnachweis werten.

```powershell
python scripts/prepare_peer_mbedtls.py --directory .pio/peer-mbedtls
$env:LARP_PEER_MBEDTLS_DIR = (Resolve-Path .pio/peer-mbedtls/source).Path
$env:LARP_PEER_MBEDTLS_ARCHIVE = (Resolve-Path .pio/peer-mbedtls/mbedtls-2.28.4.tar.gz).Path

python scripts/check_architecture.py --self-test
python scripts/check_architecture.py
python -m platformio test -e host-regression -v
python -m platformio test -e host-configured-control -v
python -m platformio test -e host-peer-proof -v
python -m platformio run -e profile-tools
python -m platformio run -e az-delivery-devkit-v4 -e esp32-runtime-probe -e esp32-profile-runtime-probe -e esp32-gatts-abi-probe -j 2

$pioPackages = (python -c "from platformio.project.config import ProjectConfig; print(ProjectConfig.get_instance().get('platformio', 'packages_dir'))").Trim()
python scripts/check_peer_crypto_sources.py --root $env:LARP_PEER_MBEDTLS_DIR --archive $env:LARP_PEER_MBEDTLS_ARCHIVE --sdk "$pioPackages/framework-arduinoespressif32"

python -m pip install clang-format==19.1.7
python scripts/check_headers.py
python scripts/check_format.py
```

Für Android JDK 17/21 und SDK 35 wie in der
[Android-Anleitung](../companion/android/README.md#bauen-und-installieren)
über `JAVA_HOME` und `ANDROID_HOME` bereitstellen. Aus `companion/android/`:

```powershell
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug --no-daemon --console=plain
```

`--offline` funktioniert nur bei vollständigen lokalen Downloads. CI verwendet
den Projektwrapper mit Netzwerk; weder ein globales Gradle noch lokale
`.local/`-Verzeichnisse sind Voraussetzungen. Für einen frischen lokalen
Wiederholungslauf bei unveränderten Quellen `--rerun-tasks` ergänzen oder
gezielt die Testausgaben bereinigen. Keine `LARP_EXPORT_*`-Schalter setzen:
die vorhandenen Fixtures werden verglichen, nicht neu erzeugt.

## Abnahme und Belege

Eine erfolgreiche Prüfung nennt **Datum, vollständigen Commit, Lauf-URL und
Ergebnisse je Bereich**. Die Ergebnistabelle im zugehörigen PR und in #38
verweist auf den konkreten vollständigen CI-Lauf. Die folgenden Belege
werden von diesem Lauf erzeugt; diese Anleitung behauptet selbst keinen
bereits bestandenen Lauf.

| Bereich | Erforderlicher Beleg |
| --- | --- |
| Hostregression / konfigurierte Steuerung | separate bestandene Läufe und JUnit-Dateien für `host-regression` und `host-configured-control` |
| Echte Kryptografie | `host-peer-proof` mit erfolgreichem Quellaudit, echten Mbed-TLS-Fällen und eigenem JUnit-Bericht |
| Herkunftsprüfungen | beide SDK-Binärpins im ABI-Build; acht native und zwei SDK-Kryptografie-Negativfälle aus `check_peer_crypto_sources.py` |
| Firmware | Produkt, beide Runtime-Proben und GATTS-ABI erfolgreich; Produkt-Flash/RAM aus dem Buildlog |
| Android | Protokoll-/App-JUnit-Berichte, erfolgreicher Debug-Build, APK-SHA-256 im Log und Debug-Lint mit Fehler-/Warnungszahl |
| Bestehende Qualitätsprüfungen | Architektur samt Fixtures, Profilwerkzeug, unabhängige C++11-Header und Formatbaseline erfolgreich |

Die drei CI-Artefakte `host-tests-<run>-<attempt>`,
`firmware-build-<run>-<attempt>` und `android-reports-<run>-<attempt>`
bewahren JUnit, Firmware-Buildlog einschließlich Ressourcenangaben sowie
Android-JUnit/Lint. Joblogs enthalten Aufrufe und Eingangsprüfungen.
Aufbewahrung folgt den GitHub-Einstellungen; dauerhafte Zahlen und
Aussagegrenzen gehören deshalb zusätzlich in den commitbezogenen PR-/Issue-Bericht.

Downloadfehler, ausgelassene Prüfungen oder fehlende Eingaben lassen den
betroffenen Nachweis offen. Vorhandene historische Ergebnisse im
[Übergabeprüfstand](../handoff/PRUEFSTAND.md) bleiben davon getrennt.
Ein Hosttest oder Build belegt keine Bluetooth-, Handy-, LED- oder
Originalboxabnahme. Dieser Prüfweg enthält keinen Upload oder Gerätebetrieb.
