# Vollständiger Prüfweg der kuratierten Produktbasis

**Zusätzlicher Abnahmeumfang · 18.10.2026:** [VA-01](veranstaltungsarchitektur.md) und [VA-T01–VA-T12](veranstaltungsarchitektur-umsetzung.md#4-gemeinsame-offene-abnahmekriterien) ergänzen dezentrale Ausfall-/Abgleich-/Funkfälle (#63). Bestehende B0-/Baugruppenprüfungen bleiben erhalten; diese Dokumentation führt keinen neuen Test aus und weist keine verteilte Abnahme nach.

Stand des B0-Verfahrens: **16. Oktober 2026**, Aufgabe [B0 / #38](https://github.com/Sven401/Larp-Escape/issues/38).
Entwicklungsbasis ist `plan/projektuebergabe-kuratierter-stand`, Ausgangscommit
`42610abfef07c971d9902ea6ca5a350ed68981bf`. Neue Änderungen auf einem eigenen
`codex/`-Branch vorbereiten und den PR gegen diesen Integrationsbranch richten.
`main` ist nicht die Grundlage dieses Prüfwegs.

## Integrierter Prüfstand · 18. Oktober 2026

B0 ist durch PR #56 im Zielbranch bei `784984d` integriert. Die CI dieses
Mergecommits ist vollständig erfolgreich; [aktuelle Laufzuordnung und
Workflow-Abgrenzung](../handoff/REPOSITORY-HYGIENE-2026-10-18.md).
Die oben genannte `42610abf` ist die historische Ausgangsbasis.
Der Windows-Androidjob baut inzwischen zusätzlich `:app:assembleRelease`.
Die Baugruppen-CI ergänzt gemeinsame Wire-/Adaptertests und getrennte
LED-/NFC-/Matrix-Firmwarevarianten. Ein Softwarelauf ist keine Geräteabnahme.

## Reproduzierbare Eingänge

Der [Quality-Workflow](../.github/workflows/quality.yml) führt drei unabhängige
Jobs auf frischen Windows-2022-Runnern aus. Bei Pull Requests prüfen alle drei
Jobs den von GitHub erzeugten Zusammenführungsstand mit dem Zielbranch.
Push-Läufe prüfen den jeweiligen Branch-Commit; ein manueller Lauf prüft den
gewählten Ref. Dafür bleibt der `ref`-Parameter von `actions/checkout` ungesetzt,
entsprechend der [GitHub-Ereignisdefinition](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#pull_request).
Der tatsächlich ausgecheckte Commit, Ereignis-SHA sowie PR-Head und PR-Basis
stehen getrennt in den Prüfbelegen. Eine spätere Änderung an Head oder Basis
braucht einen neuen Nachweis des dann zusammengeführten Standes.
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

Die Konsolidierung beruht auf [PR #43](https://github.com/Sven401/Larp-Escape/pull/43)
bei `95212a7db5da7eae682ec9cfa09ee93c8e235dcb`. Aus der Alternative
[PR #42](https://github.com/Sven401/Larp-Escape/pull/42) bei
`e2c39806902888b69641bdbaa32f33612ef202bd` stammen die Prüfung des
Zusammenführungsstands, der [Protokollhelfer](../scripts/quality_evidence.py)
und die Grundlage der [Helfertests](../scripts/tests/test_prepare_peer_mbedtls.py).
Die strengere Quellenextraktion, `--directory`-Schnittstelle, drei Jobs,
Kryptografie-Negativprüfungen und Gradle-Prüfsumme von #43 bleiben erhalten.
Die Alternative wird nicht als zweiter Workflow oder zweite Prüfanleitung gepflegt.

## Lokal ausführen

Das [D0-Vertragsmodell](veranstaltung/d0-vertrag-v1.md) ist hardwarefrei und
benötigt nur die Python-Standardbibliothek: `python scripts/check_d0_contract.py`.
Es prüft die versionierten semantischen Gegenbeispiele, keine Signaturen,
CBOR-Bytes, Flashdurabilität oder installierte Produktfunktionen.

Die [D1-Rechnerreferenz](veranstaltung/d1-persistenz-v1.md) nutzt ebenfalls nur
die Python-Standardbibliothek:
`python -m unittest discover -s scripts/tests -p test_d1_journal.py -v`.
Sie prüft dateibasierte Hosttransaktionen, Wiederaufnahme und echte Prozessabbrüche.
SQLite ist dabei ein Rechnerbackend, keine Firmwareabhängigkeit. Kein Beleg
für ESP-Flash, physische Stromausfälle, D0-Verifizierer oder Gerätewirkungen.

Aus einem sauberen Checkout unter Windows mit Python und PlatformIO 6.2.0.
Die folgenden Prüfungen laufen ohne angeschlossene Hardware. Alle nativen
Befehle müssen mit Exitcode 0 enden; bei Fehlern nicht einfach den nächsten
Befehl als erfolgreichen Gesamtnachweis werten.

```powershell
python -m unittest discover -s scripts/tests -v
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
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:assembleRelease :app:lintDebug --no-daemon --console=plain --stacktrace --continue
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
| CI-Helfer | 20 Offlinefälle: 12 für Archivprüfung und Erhalt vorhandener Eingaben, 8 für echte Kindprozesse, Windows-Batchargumente, Exitcodes und Belegschutz; Windows-Batchfall nur unter Windows |
| Hostregression / konfigurierte Steuerung | separate bestandene Läufe und JUnit-Dateien für `host-regression` und `host-configured-control` |
| Echte Kryptografie | `host-peer-proof` mit erfolgreichem Quellaudit, echten Mbed-TLS-Fällen und eigenem JUnit-Bericht |
| Herkunftsprüfungen | beide SDK-Binärpins im ABI-Build; acht native und zwei SDK-Kryptografie-Negativfälle aus `check_peer_crypto_sources.py` |
| Firmware | Produkt, beide Runtime-Proben und GATTS-ABI erfolgreich; Produkt-Flash/RAM aus dem Buildlog |
| Android | Protokoll-/App-JUnit-Berichte, erfolgreicher Debug-Build, APK-SHA-256 im Log und Debug-Lint mit Fehler-/Warnungszahl |
| Bestehende Qualitätsprüfungen | Architektur samt Fixtures, Profilwerkzeug, unabhängige C++11-Header und Formatbaseline erfolgreich |

Die drei Artefakte `quality-hardware-independent-<run>-<attempt>`,
`quality-esp32-build-<run>-<attempt>` und `quality-android-<run>-<attempt>`
enthalten jeweils `provenance.json`, Aufruf und Exitcode je ausgeführtem
Prüfschritt, vollständige kombinierte Standard-/Fehlerausgabe sowie
`summary.md`. Die Provenienz nennt auch Runner-Image, Python und Lauf-URL.
Werkzeugversionen, Quellaudit, Ressourcenwerte und APK-Prüfsumme stehen in den
jeweiligen Befehlslogs. Die drei Host-JUnit-Dateien liegen im Host-Artefakt;
`android-reports-<run>-<attempt>` bewahrt zusätzlich Android-JUnit, HTML-Testberichte
und Lintberichte.

Zusammenfassung und Upload laufen auch nach einem fehlgeschlagenen Prüfschritt.
Der Protokollhelfer gibt dessen Exitcode unverändert zurück; ein fehlendes
Programm wird ebenfalls als Fehler erfasst. Bereits vorhandene Protokolle
werden nicht überschrieben. Eine fehlende Zeile in der Zusammenfassung ist
keine bestandene Prüfung. Schlägt bereits Checkout oder Python-Einrichtung
fehl, bleibt der GitHub-Joblog maßgeblich; ein noch nicht initialisierter
Protokollhelfer kann dafür keine eigenen Belege erzeugen.

Lokal ist dieselbe Protokollierung optional mit einem frischen Ausgabeordner
möglich. Aus dem Repositorywurzelverzeichnis beispielsweise:

```powershell
python scripts/quality_evidence.py --output .pio/quality-run-01 init
python scripts/quality_evidence.py --output .pio/quality-run-01 run --name ci-helpers -- python -m unittest discover -s scripts/tests -v
python scripts/quality_evidence.py --output .pio/quality-run-01 finish
```

`init` zeichnet den Commit auf, nicht uncommittete Dateiänderungen. Für einen
commitbezogenen Nachweis deshalb einen sauberen Checkout verwenden. `finish`
schreibt nur den Bericht und ersetzt nicht die Prüfung der einzelnen Exitcodes.
Aufbewahrung folgt den GitHub-Einstellungen; dauerhafte Zahlen und
Aussagegrenzen gehören deshalb zusätzlich in den commitbezogenen PR-/Issue-Bericht.

Downloadfehler, ausgelassene Prüfungen oder fehlende Eingaben lassen den
betroffenen Nachweis offen. Vorhandene historische Ergebnisse im
[Übergabeprüfstand](../handoff/PRUEFSTAND.md) bleiben davon getrennt.
Ein Hosttest oder Build belegt keine Bluetooth-, Handy-, LED- oder
Originalboxabnahme. Dieser Prüfweg enthält keinen Upload oder Gerätebetrieb.
