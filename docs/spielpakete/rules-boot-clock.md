# Native Startkennung und Rules-Zeitquelle

Status: **Host und beide SDK-Varianten unabhängig geprüft; Gerätequalifikation
offen**, 01.11.2026. Folgearbeit zu
[#93](https://github.com/Sven401/Larp-Escape/issues/93) und dem
[semantischen Fortschrittsbesitzer](rules-progress-owner-v1.md).
Plattform/Implementierung: developer_p1a3; andere SDK-/Lifecyclegegenprüfung:
module_digest_review; unabhängige Fehler-/ABIprüfung: quality_p1.

## Einmaliger Besitzer

`Esp32RulesBootClock` ist ein Prozesssingleton mit privatem Konstruktor und
gelöschten Copy-/Moveoperationen. Neue RulesOwner- oder Recoveryobjekte erzeugen
keine neue Uhrdomäne. Der erste Initialisierungsversuch bleibt einschließlich
seines Fehlerausgangs gespeichert. Wiederholtes `beginEarlyStartup()` würfelt
weder Kennung noch Zeitanker neu und erwirbt keine weitere Entropie.

Die vorhandene, unveränderte `Esp32StartupIdentityEntropy` aktiviert die
Startupentropie, liest genau 16 Bytes und gibt die Ressource wieder frei.
Null-/unbeschriebene Kennung wird verworfen. Erst nach Freigabe und gültigem
erstem vorzeichenbehaftetem Timerwert wird der Dienst bereit. Es gibt keinen
Chip-ID-, Zeit-, Hash- oder konstanten Ersatz für fehlende Entropie.

Beide nativen `app_main` rufen dies als ersten fachlichen Schritt auf und
kehren bei Fehler zurück, bevor weitere Begin-/Funk-/Peripheriepfade starten.
Die vorab angelegten globalen Objekte initialisieren dort keine solche
Hardware. Das ist ein geprüfter Quellreihenfolgebefund, kein ausgeführter
Startup- oder Hardwarebeleg. Die schmale gemeinsame SDK-Komponente
`larp_boot_clock` bindet nur den neuen Dienst und den vorhandenen Entropieadapter
ein; sie fügt weder NVS noch Schlüsselbereitstellung hinzu.

## Gemeinsame Zeitdomäne

`IClock` und `IManualClock` lesen denselben `esp_timer_get_time()`-Bereich und
liefern volle uint64-Millisekunden. Negative Werte und Rückwärtslauf werden
bereits auf der ursprünglichen Mikrosekundenebene dauerhaft geschlossen, auch
wenn beide Werte in dieselbe gerundete Millisekunde fallen. Beide Ports teilen
diesen Fehlerzustand; gleiche Werte sind erlaubt. Fehlerausgaben werden geleert.
Eine kopierte Bootkennung stammt aus demselben Dienst und ist nur im gültigen
Zustand verfügbar. Spätere tatsächliche ManualOwner-/Orderingbesitzer müssen
diesen gemeinsamen Produzenten verwenden; dieser Schritt instanziiert sie nicht.

Das unterstützte Profil ist exklusiver früher Startup, anschließend ein
serialisierter Worker. Keine konkurrierenden oder ISR-Aufrufe; daraus folgt
keine SMP-/Taskbridgequalifikation. Die Kennung ist ein öffentlicher
Lebensdauerbezug, kein Schlüssel, keine Berechtigung oder dauerhafte Identität.
Der Dienst ist auch kein Produzent administrativer Token oder privater D0-Schlüssel.

Die [RNG-Vorgaben des gepinnten SDK](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/system/random.html)
verlangen das Ende der Startupentropienutzung vor ADC, I2S und Funk.
Der [Timervertrag](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/system/esp_timer.html)
beschreibt Mikrosekunden seit Initialisierung; Deep Sleep startet diese Domäne
neu. Eine neue Prozessdomäne braucht eine neue Startkennung, keine automatische
Spielfortsetzung. Tatsächliche Schlaf-/Reset-/Ressourcenverläufe bleiben zu prüfen.

## Tatsächliche Nachweise

Eigene strenge C++11-Prüfung: sechs frische Prozesse, 192 Assertions.
Unabhängige Prüfung: **15 frische Prozesse, 426 Assertions**, zusätzlich
Konstruktor-/Copy-/Move-/Interfaceprüfungen zur Compilezeit. Es werden die
tatsächlichen Bootclock-, StartupEntropy- und Identityquellen gelinkt; nur die
drei kleinen SDK-Header/-Funktionen sind explizite Stubs. Kein Testfreund,
privater Feldzugriff, Resetknopf oder alternativer Uhrbesitzer.
[Korpus, Reproduktion und genaue Fehlerfälle](../../test/fixtures/esp32-rules-boot-clock-review/README.md).

Geprüft sind Enable→Fill16→Disable→Timer-Reihenfolge, Allzero/unbeschriebene
Entropie, negative Initial-/Laufzeit, permanente Submillisekundenregression,
portübergreifende Regression, Gleichheit, Nullzeit, Werte oberhalb 32-Bit-ms
und bis INT64_MAX-us sowie Reentry ohne zweiten Erwerb. Ein gültiger nichtnull
Testwert beweist ausdrücklich keine Entropiequalität.

Frische gemeinsame Regression: 43 CTests, PIO 263/263, Architektur 190 Dateien/
416 Includes, bestehende 109 eigenständige Header plus neuer Libheader separat;
Formatbaseline 380 und neue Dateien gesondert geprüft. Diese Host-/Stubbelege
ersetzen nicht das Bauen beider tatsächlichen SDK-Images dieses Folgestands.

Source-SHA256:

- Header `c756660a4cb07df48506d033cd2e0a4e01b31a28171facfde9d93579f434c4c2`.
- Body `37d401d5a2c1c89e8967db052abebe6724fc4b96be67651acdfdd5ab0d4bc134`.
- Unveränderter Entropieadapter
  `12e25f8d772cdfb122abc506d439c211154653d3e74ee6277862fbf39b669c3a`.

## Nachgelagerte native Qualifikation

Head `da4d4095424aec927524f8eafd7103a55cb9e787`: zwölf erfolgreiche PR-Jobs
in sechs Workflows, alle tatsächlichen Originalcheckouts auf Prüfmerge
`fd85ec533613c68b0c0f1a7497986791531b724c`. Dessen Eltern sind exakt
Zielbasis `c4f1f824c6274e9a8d7676212a2a96be4d4a77b9` und genannter Head.
pull_request, Attempt 1, Actions-App und abschließend unveränderte Refs bestätigt.

[Native 41266590666](https://github.com/Sven401/Larp-Escape/actions/runs/41266590666),
Originaljob 124730553880: **92/92 CTests ohne Fehler, Disabled oder Skips**.
Beide Clockziele laufen vollständig mit sechs Prozessen/192 Assertions und
15 Prozessen/426 Assertions; dazu Ordering 830 und Owner-PSA 300 mit dessen
vollständigen Artefakt-/Signatur-/Bindingprüfungen. Beide tatsächlichen SDK-
Images kompilieren `larp_boot_clock` mit Clock und unverändertem Entropieadapter
gegen bootloader_support, esp_hw_support und esp_timer. Originaldescriptor und
Buildlogs enthalten keine test/fixtures- oder Stubpfade in den aktiven Quellen/
Includes der Images. Pin `9a97f6c54ec638111ce55cd36581b3c192f15207` mit
rekursiven Submodulen bestätigt.

Originalarchiv 13067760853, SHA256
`d97d25070193f6060417bd114e6ff351d17ac55c105daccf01775611be30ec96`,
lokal `private/pr135-da4d4095-quality`. `quality_p1` prüfte den tiefen nativen
Archivbeleg; `module_digest_review` separat alle zwölf Jobs/Checkouts/Refs.
Quality 41266590637, LED 41266590689, NFC 41266590655, ESP-NOW 41266590663
und Rollen 41266590748 sind ebenfalls vollständig erfolgreich. CI-XML:
GM 35/Worker 9, Protokoll 111/fünf V1-Tests, Dev-App 77 ohne Fehler oder Skips.
Androidarchiv 13068275620, SHA256
`282eb92d2c9e1f113a619c06553175c6d9c467b73f8383ea62679300f15f0713`.
Der frühere offene SDK-Nachweis ist damit für genau da4d4095 ergänzt;
nachfolgende Eingangsänderungen sind davon nicht erfasst. PR bleibt Draft.

Geräteentropie, Ausschluss realer Ressourcenkonflikte, Schlaf-/Resetbeobachtung,
SMP, tatsächliche Eingabe-/Installations-/OriginKeybesitzer, Event-/GM-Rechte
und Running sind nicht durch diese Prüfung belegt. Tatsächliche OriginalInput-
produzenten mit dauerhafter lokaler Generation bleiben offen; die vom P0
festgelegte Korrelationssemantik wird durch die Clockquelle nicht umgesetzt.
Keine Geräteaktion, kein Upload oder Release; #93 bleibt offen.
