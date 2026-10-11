# DG-Step v1: Androidcodec und begrenzte Endantwortaufnahme

Stand 2026-10-10. Androidbasis `7e5acb5`. Wireursprung `04c145f`, maßgeblicher
korrigierter Firmwarevertrag `b5a8c430fcf5cf37731c18ba3ac261fce3cc13f7`.
[Quellen und Hashes](PROVENANCE.md). Status: JVM-Hostcodec mit angeschlossener
synthetischer Debugsimulation; kein produktiver GATT-, Rollen- oder Geräteanschluss.

## Bytevertrag und Budgets

`DiagnosticStepV1` codiert/parst exakt 80-Byte-DG-v1-Anfragen Art 4: WakeCapture
Kanal 1 oder LedSingle Index 0, RGB `[16,0,0]`, Hold 500 ms. Header, unbenutzte
Nullfelder, UUID-v4-/Variantbits und gesamte Operation werden geprüft. Nachricht
und Schritt unterstützen unsigned 32 Bit, Boot alle nichtleeren unsigned 64 Bit.
IDs bleiben kanonische Hexfolge, Integer sind Little Endian. Die JSON-Anfragebrücke
validiert den Diagnosevertrag und übernimmt keine erfundenen Operationsparameter.

Endantworten umfassen exakt `104 + 10 * Flankenanzahl`, höchstens 744 Bytes und
64 Flanken. Länge wird vor Feldzugriff/Kopie geprüft. Die ersten 80 Bytes müssen
der gesamten ursprünglichen Anfrage entsprechen: Nachricht, IDs, Timeout und
alle Operationsparameter. Unbekannte Outcome-, Reason-, Evidenz- und Flagwerte
scheitern. Gerätezeiten sind höchstens `2^53-1`, monoton innerhalb Start/Ende;
Dauer ist strikt kleiner als Timeout. Completed-LED braucht mindestens 500 ms,
zwei Renderaufrufe und restoredOff. Wake hat keine LED-Belege; LED keine Flanken
oder Overflow. Roh-/Aktivpegel und Polarität müssen zusammenpassen.

Evidenzprofil 1 ist ausschließlich synthetisch; Versorgung bleibt JSON `null`.
Ein synthetisches `restoredOff` belegt kein physisches Licht. Die JSON-Brücke
bewahrt Outcome, Reason, Zeiten, Flanken und Overflow. Nutzerbeobachtung und
Urteil bleiben Aufgaben des bestehenden Berichts-/Repositoryvertrags.

## Aufnahme, Originalbindung und absolute Frist

`DiagnosticStepAssemblyV1` hält genau einen Auftrag in einem festen 744-Byte-Puffer.
Frames bestehen aus 12 Headerbytes und 1..116 Nutzbytes, höchstens 128 Bytes.
Nachricht, Gesamtlänge, Offset und Bereich werden vor Kopie geprüft. Nur lückenlose
aufsteigende Bereiche sind zulässig. Vollständig enthaltene bytegleiche Duplikate
sind wirkungslos; Änderung, Überlappung, Lücke, Padding, Oversize oder Totalwechsel
schließen den Empfang. Erst `take` liefert nach erneuter Prüfung höchstens einmal.

Lokale GATT-Generation, Connectiontoken/-handle, Authorityepoch, Principal und
Spielleitungsrolle bleiben vollständig gebunden. Aufnahmekopie und aktuelle
Metadaten werden getrennt geprüft. Es gibt noch keinen verifizierten Android-
Issuer; im Debugfake sind alle Werte erfunden. Konstruktion/Bytevergleich erteilen
kein Recht. Ein zukünftiger Adapter muss Route, Sendung, Aufnahme und Lifecycle
serialisieren und Callbacklänge bereits vor seiner eigenen Kopie begrenzen.

Die lokale Frist stammt absolut vom ursprünglichen Auftrag, nicht vom ersten
oder nächsten Fragment. Queuezeit und Duplikate verlängern sie nicht. Gleichheit
zur Frist, Rückwärtszeit, Abbruch, Disconnect und Entzug schließen den Empfang und
löschen Restbytes. Späte Ergebnisse ersetzen keinen terminalen Zustand. Geräte-
und Appuhr bleiben getrennt; keine Uhrensynchronisierung wird behauptet.
Kleine Antwortsegmente ersetzen keine Aufnahmefähigkeit: Control braucht weiterhin
88 effektive ATT-Wertbytes, Schrittanfragen bleiben unsegmentierte 80 Bytes.

## Bedienbare synthetische Debugsimulation

Die Debugseite behält ihre ständig sichtbare Simulation-/Fake-Kennzeichnung,
Aufträge, getrennte Telemetrie/Beobachtung, Abbruch und Berichtexport. Erst nach
erfolgreichem atomarem Pending-Journal erzeugt der Transportcallback einen
`SyntheticDiagnosticStepV1` mit ursprünglicher Frist. Der ausdrückliche Antwortbutton
erzeugt synthetische DG-Bytes, führt sie durch Reassembly/Originalkorrelation und
übergibt danach JSON an das Fake-Repository. Kein Gerät wird angesprochen.
Eine passende Fakeantwort ohne Beobachtung bleibt unklar. Die Grant-/Control-
Simulation bleibt lokal; ein echter Serviceanschluss wird nicht vorgetäuscht.

Hintergrund, Abbruch, Export oder neue Simulation beenden alte Aufnahme. Verspätete
Callbacks betreten keinen neuen Lauf oder späteren Schritt. Appneustart liest nur
das Journal: offene Schritte werden `app_restarted/unclear`, ohne automatische
Sendung/Wiederholung. Checksummenexport bewahrt synthetische Quelle, Rohtelemetrie
und unabhängige Beobachtungen. Overflow/64 Flanken bleiben im Export fehlgeschlagen.

## Unabhängige Vektoren und frühere Zeitfehler

Der Python-Generator benutzt nur `struct`/`uuid`, keinen Kotlincodec. Zehn Vektoren
enthalten vollständige Anfrage-/Antwortbytes und Segmente bei Kapazitäten 13, 20,
127 und 128. Wake-max64 (744 Bytes) und LED (104 Bytes) sind die kompletten
byteidentischen C++-Goldenantworten. Weitere Fälle: Overflow, Lowpolarität,
Failed/Cancelled/Rejected, UINT32-/UINT64-Grenzen und letzte exakte Gerätezeit.

| Negative Regression | Minimale Änderung/Beleg | Erwartung ab `b5a8c430` und JSON v1 |
| --- | --- | --- |
| `wire-duration-equals-timeout` | Wake Timeout 1000, Start 100, Ende 1100; Offset 88 LE64 `4c04000000000000` | Dauer 1000 abweisen |
| `wire-led-short-hold` | Sonst gültige LED-Golden, Start 100, Ende 600 auf 599; Offset 88 LE64 `5702000000000000` | Completed bei 499 ms abweisen |

`04c145f` akzeptierte beide wireseitig; die JSON-Berichtsprüfung lehnte sie schon
ab. Korrigierte Firmwarebasis, Androidparser/Reassembly und Brücke lehnen beide
nun ausdrücklich ab. Ihre Bytes bleiben als negative Regression erhalten.
Kein Wert wird repariert oder günstig gerundet. Kürzere Failed-/Cancelled-LED-
Ergebnisse bleiben gültig.

```
python companion/contract/diagnostic-step-v1/generate_vectors.py --check
```

Aus `companion/android`:

```
gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug
```

Endgültiger lokaler Prüfstand: 71 Protocol-JVM- und 23 App-JVM-Fälle bestanden
ohne Fehler oder übersprungene Fälle; 16 neue Step-/Bridgefälle und zwei neue
ViewModelfälle. Debugbuild und Lint bestanden. Lint meldet die drei bekannten
Warnungen `OldTargetApi`, `DataExtractionRules`, `MissingApplicationIcon`.
Der unabhängige Generator bestätigt alle zehn Vektoren. Unveränderte C++-Quelle
`04c145f` bestand 756 Assertions; korrigierte saubere Quelle `b5a8c430` bestand
955 Assertions (199 zusätzliche Zeitgrenzenprüfungen) sowie zehn unabhängige
Grenzvektoren und alle 744 Wake-/104 LED-Bytes. Beide Runner kompilierten nur in
temporären Verzeichnissen; fremde Produktdateien wurden nicht verändert.

## Konkrete nächste Abhängigkeiten

1. Versionierter Wire-AdmissionACK: der bisherige synchrone Firmware-Rückgabewert
   ist kein Drahtbeleg. Verlorene/abgewiesene Anfragen bleiben timeout/unklar;
   Sendeerfolg ist kein Endergebnis.
2. Produktservice mit konsistenten Control-/Stepfähigkeiten, echtem GATT und
   vertrauenswürdiger F1/F2-Originalroute. Restaurierter Bondmodus vor SMP-Reset
   sowie Service-/Actor-/Credential-/Registryanschluss bleiben offen.
3. Garantiert bedienter Produktowner, Komponentenbesitz, Spielstartfence,
   Idle-/Timer-/Aus-Nachweis und geprüftes LED-/Wake-Hardwareprofil.
4. Konkrete Android-Persistenz-/Lebenszyklus-/Dateidialogprüfung auf einem Handy;
   danach vom Nutzer selbst bediente physische Prüfung in der Projekt-App.

Dieses Paket führt keine Installation, Funk- oder Geräteaktion aus. Hosttests
ersetzen keine physische Abnahme; die ursprüngliche Boxabnahme bleibt offen.
