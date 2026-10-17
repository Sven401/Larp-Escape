# Maintainer-Einstieg

Aktuelle fachliche Ergänzung vom **18. Oktober 2026**:
[abgestimmtes veranstaltungsbezogenes Zugangskonzept](../docs/companion/veranstaltungszugang.md)
mit Rechteverwaltung, Schlüsselwechsel, Wiederherstellung, Unterbrechungspunkten
und offenen Abnahmekriterien. **Sollkonzept, keine Implementierung.**
Die folgenden datierten Übernahmeangaben bleiben historische Belege.

Stand: **14. Oktober 2026**. Diese Datei führt den aktuellen Projekteinstieg
und die Nachweise zur Maintainer-Übernahme. Das verbindliche Mandat steht in
[AGENTS.md](../AGENTS.md). Quelle der Vereinbarung ist der vom Nutzer am
14. Oktober 2026 ausdrücklich zur Umsetzung beauftragte Übernahmeplan.

## Arbeitsbasis und Quellen

| Gegenstand | Status und Quelle |
| --- | --- |
| Repository | **belegt:** [Sven401/Larp-Escape](https://github.com/Sven401/Larp-Escape); lokales `origin` und GitHub-Branchliste am 14. Oktober 2026 abgeglichen. |
| Integrationsbasis | **belegt:** `plan/projektuebergabe-kuratierter-stand` bei [`42610abfef07c971d9902ea6ca5a350ed68981bf`](https://github.com/Sven401/Larp-Escape/commit/42610abfef07c971d9902ea6ca5a350ed68981bf); lokaler HEAD und Remote nach Fetch identisch, Arbeitsbaum zu Beginn sauber. |
| Erster Maintainer-PR | **bestätigt:** Arbeitsbranch `codex/maintainer-uebernahme`, Ziel `plan/projektuebergabe-kuratierter-stand`; Umfang: `AGENTS.md`, diese Datei und der Link-/Verfügbarkeitsabgleich in `handoff/README.md`. |
| Historischer Default-Branch | **ausgelesen:** `main` bei [`29da937c4309beddfd45c5dd0fbf84fdf853d8e4`](https://github.com/Sven401/Larp-Escape/commit/29da937c4309beddfd45c5dd0fbf84fdf853d8e4); eine spätere Integration ist gesondert zu planen. |
| Veröffentlichte Übergabe | **belegt:** Der kuratierte Branch und alle sechs in der [Übergabe](README.md#quellarchiv-und-wiederaufnahme) genannten Archivbranches sind am 14. Oktober 2026 auf GitHub vorhanden; über die Branchliste geprüft. |

Die [technische Übergabe](README.md) führt zu Architektur, Android-Einstieg,
Hardwarequellen und Quellarchiven. Der [Prüfstand vom 13. Oktober](PRUEFSTAND.md)
bleibt ein datierter historischer Beleg. Neue fachliche Erkenntnisse an der
passenden bestehenden Stelle ergänzen und von hier aus verlinken, wenn sie den
Projekteinstieg ändern; keinen parallelen Architekturstand anlegen.

Die frühere separate KI-Werkbank kann historische Entscheidungen und lokale
Messungen belegen. Für die neue Maintainer-Arbeit gelten die Nutzervereinbarung
und die Projektunterlagen dieses Repositories. Alte Arbeitsregeln, Hooks oder
Fortsetzungsaufträge werden durch ihre Archivierung nicht erneut beauftragt.

## Offene Arbeit und Abhängigkeiten

**Ausgelesen am 14. Oktober 2026:** [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25)
bündelt die bisherigen Meilensteine; die technische Übergabe vom 13. Oktober
ergänzt jüngere Entscheidungen. Der dortige Verweis auf die frühere Werkbank
beschreibt die damalige Herkunft der Fortschrittsmeldungen. Vor Übernahme eines
Status aktuellen Code, Quelle und Prüfergebnis abgleichen.

- **F1/F2:** [Bond-Wiederzugang](https://github.com/Sven401/Larp-Escape/issues/27),
  [Einrichtung und Gerätenachweis](https://github.com/Sven401/Larp-Escape/issues/28)
  bleiben Voraussetzungen für den Produktzugang. Archivierte Firmwareentwürfe
  sind vor einer Übernahme einzeln zu bewerten.
- **F3/A3:** [Handy-/ESP-Produktablauf](https://github.com/Sven401/Larp-Escape/issues/29)
  und [erste Android-Spielleitungsaktion](https://github.com/Sven401/Larp-Escape/issues/37)
  benötigen den gesicherten Zugang und korrelierte Geräteantworten.
- **F4/F5 und Android:** Reale Speicherung, Gerätebesitzer, Ressourcen- und
  Betriebsabnahme sowie die offenen Status-, Berechtigungs-, NFC- und
  Szenarienprüfungen bleiben im bestehenden Backlog. Ein Build schließt diese
  Aufgaben nicht ab.
- **Nutzerziel aus der Übergabe:** LEDs in der echten Android-App starten,
  stoppen und bewusst erneut starten, mit sichtbarer Geräteantwort und
  getrennter Nutzerbeobachtung. Die frühere Diagnose-App wurde verworfen;
  ihr Archiv ist kein freigegebener Umsetzungspfad. Der Referenzfall ist
  im kuratierten Stand noch nicht implementiert oder physisch abgenommen.

Diese Übernahme schafft den Einstieg und den Prüfstand. Eine fachliche
Folgeänderung bekommt einen eigenen, begrenzten PR mit passenden Abnahmekriterien.

## Prüfwege

Die vorhandene [Quality-CI](../.github/workflows/quality.yml) führt bei Push und
Pull Request zwei Jobs auf Windows aus:

- `Hardware-independent checks`: Architekturprüfung samt Fixtures,
  `host-regression`, Profilwerkzeug, unabhängige C++11-Header und Formatbaseline.
- `ESP32 firmware build`: Produktfirmware sowie Runtime- und Profil-Runtime-Probe.

Versionen und genaue Aufrufe stehen im Workflow; die Basis nutzt Python 3.11,
PlatformIO Core 6.2.0 und clang-format 19.1.7. Weitere Prüfumgebungen stehen in
[platformio.ini](../platformio.ini). Die Quality-CI enthält keinen Android-Lauf
und keinen Hardwaretest.

Die [Android-Anleitung](../companion/android/README.md) beschreibt JDK 17/21,
Android SDK 35, Build Tools 35.0.0 und den Gradle-8.9-Wrapper. Aus
`companion/android/` die folgenden vorhandenen Prüfungen ausführen; lokale
JDK-/SDK-Pfade über die dokumentierten Umgebungsvariablen bereitstellen:

```powershell
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug
```

Ein Offline-Lauf setzt vollständige lokale Downloads voraus. Fehlende
Abhängigkeiten oder Werkzeuge mit konkretem Fehler dokumentieren; sie sind
weder ein bestandener Test noch automatisch ein Produktfehler.

## Prüfstand dieser Übernahme

- **Historisch belegt:** [Quality-Run 38470091132](https://github.com/Sven401/Larp-Escape/actions/runs/38470091132)
  auf Diagnosearchiv-Commit `8e1fc9ab` war erfolgreich. Der Vergleich vollständiger
  Git-Bäume am 13. Oktober 2026 zeigte gegenüber `42610abf` ausschließlich sechs
  zusätzliche Archivdateien unter `handoff/`; Produktdateien und Workflow waren
  identisch. Das ist kein CI-Lauf auf dem neuen Maintainer-Commit.
- **Aktueller CI-Prüfort:** Die Checks des
  [Maintainer-PRs](https://github.com/Sven401/Larp-Escape/pulls?q=is%3Apr+head%3Acodex%2Fmaintainer-uebernahme)
  liefern das Ergebnis am jeweiligen veröffentlichten Commit. Nach einem neuen
  Commit dessen eigenen Quality-Lauf auswerten; einen früheren grünen Lauf
  nicht auf den neuen Stand übertragen.
- **Gemessen am 14. Oktober 2026:** Android-Aufruf oben mit `--offline --no-daemon`
  auf Basiscommit `42610abfef07c971d9902ea6ca5a350ed68981bf`, JDK 21.0.8 und
  Gradle 8.9 erfolgreich; 58 Aufgaben ausgeführt. 45 JVM-Fälle bestanden
  (29 Protokollkern, 16 App), keine Fehler oder ausgelassenen Tests; Debug-APK
  gebaut. Lint meldet null Fehler und drei bestehende Warnungen:
  `OldTargetApi`, `DataExtractionRules`, `MissingApplicationIcon`.
  Belege sind die erzeugten JUnit-XML-Dateien unter
  `companion/android/{core/protocol,app}/build/test-results/` und
  `companion/android/app/build/reports/lint-results-debug.html`.
  Diese Buildausgaben sind lokal regenerierbar und werden nicht versioniert.
  Der vorhandene Gradlecache genügte für den Offline-Lauf; der anfängliche
  Sandbox-Schreibfehler wurde durch genehmigten Cachezugriff behoben.
  Kein Handy-, Bluetooth-, NFC- oder sonstiger Hardwarelauf wurde durchgeführt.
- **Offen laut Übergabe:** Ein erneuter echter Kryptografie-Hostnachweis benötigt
  das gepinnte Mbed-TLS-2.28.4-Archiv und den Quellpfad gemäß
  [native_peer_mbedtls.py](../scripts/native_peer_mbedtls.py).

## Spätere Integration nach main

**Ausgelesen am 14. Oktober 2026:** Das aktive
[Ruleset „Main“](https://github.com/Sven401/Larp-Escape/rules/24680694)
gilt für den Default-Branch. Es enthält eine `update`-Sperre ohne
Bypass-Akteure; die aktuelle Verbindung kann sie nicht umgehen. Außerdem sind
signierte Commits, lineare Historie, Squash sowie Review-Anforderungen gesetzt.
Die Liste konkreter erforderlicher Statuschecks ist leer.

Dies ist ein Integrationsblocker für eine spätere Aktualisierung von `main`.
Vor deren Vorbereitung den dann aktuellen Regelsatz erneut lesen und die nötige
Entscheidung dem Nutzer vorlegen. Der erste Maintainer-PR richtet sich gegen
den kuratierten Übergabebranch.
