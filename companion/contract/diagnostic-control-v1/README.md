# DG-Control v1: Android-Hostcodec

Stand: 2026-10-10. Androidbasis `45d3e79`, Firmware-Vertragsquelle
`3209275136f96d3a6a0013c485e70d692e428cf4`. [Quellen und Hashes](PROVENANCE.md).
Status: plattformunabhängiger Controlcodec mit synthetischen Hostfällen;
kein produktiver Android-GATT-, Rollen- oder Geräteanschluss.

`DiagnosticControlV1` codiert Capability, Grant und Cancel als exakt 69 Byte und
parst die 88-Byte-Endantwort dieser Controlanfrage. `DG`, Version, Art, Reserven,
IDs und alle unbenutzten Nullfelder werden geprüft. Jede unbekannte Version,
Art oder Ergebnisnummer wird abgewiesen. Flags müssen exakt `3` sein:
synthetisch **und** nur Control; logisches Diagnoseschema `1` und maximales
Grant-TTL `60000` werden nicht als beliebig erweiterbare Fähigkeiten gelesen.
Das Paket ergänzt keine Wake-/LED-Step- oder Geräte-Endergebnisrahmen.

Mehrbytewerte sind unsigned Little Endian. Nachricht hat 32 Bit; der Bootwert
unterstützt alle nichtleeren 64-Bit-Werte, auch oberhalb `Long.MAX_VALUE`.
UUID-v4-/Variantbits und kanonische UUID-/Gerätehexreihenfolge werden geprüft.
Gerätezeiten bleiben in `0..2^53-1`; ein erfolgreicher Grant verlangt gültige
Grant-UUID, steigende Zeiten und exakt das angeforderte TTL. Abgewiesene Grants
haben keinen Grant und keine Ausgabe-/Ablaufzeit. Cancel antwortet mit dem
ursprünglichen Grant; auch `Accepted` bedeutet ausschließlich Controlabbruch.
Es gibt im Ergebnis kein `restoredOff`, kein Lichturteil und keine App-Rollenfreigabe.

## Originalbindung und Parserbudget

`PendingDiagnosticControlV1` hält genau eine immutable Anfrage und eine vor dem
Senden gesetzte feste 2000-ms-Frist. Ein Future-Adapter muss diese Absicht vor
dem Senden persistieren. Der Codec selbst sendet, persistiert und wiederholt nichts.
Lokale Warteschlangenzeit ist Teil der Frist; ein neuer Empfangszeitpunkt
verlängert sie nicht. Frühe/zukünftige Empfangszeit, Zeitrücksprung, Fristablauf,
lokaler Abbruch und Entwertung schließen den Auftrag ohne Wiederbelebung.
Eine gültige Antwort wird genau einmal übernommen; fremde, fehlerhafte und
doppelte Antworten aktivieren keine Freigabe.

Die Originalmetadaten enthalten lokale GATT-Generation **sowie alle Felder** des
F0-BoundConnection-/TrustedAuthority-Belegs: Token, Handle, Epoch, Principal und
Spielleitungsrolle. Empfangskopie und aktuelle Metadaten werden getrennt geprüft.
Entzug/Disconnect oder Ersatz der aktuellen Metadaten entwertet den Auftrag;
eine alte Empfangskopie kann weder neue Authority noch neue Verbindung übernehmen.
Keines dieser Felder steht im DG-Frame. Es existiert noch kein verifizierter
Android-Issuer dieser Metadaten. In Hosttests sind sie ausdrücklich synthetische
Kopien; das Konstruieren der Klasse ist kein Authentisierungsnachweis. Der spätere
Adapter muss Aufnahme, aktuelle Beobachtung und Sendung im passenden Owner
serialisieren. Ein Parservergleich erzeugt keinen Threadschutz und kein Rollenrecht.

Ein Parser akzeptiert ausschließlich seine feste Framegröße und prüft die Länge
vor Feldzugriff und Allokation. Das Aufnahmebudget beträgt 128 Byte; es gibt keine
Fragmente, Verkettung oder Prepared Writes. Der zukünftige BLE-Adapter muss die
Länge **vor** seiner eigenen Callbackkopie begrenzen und mindestens 88 effektive
ATT-Wertbytes bestätigen (MTU-Abzug eingeschlossen). Größere ausgehandelte
Kapazität ist erlaubt, größere DG-Frames sind abgewiesen. Kleine MTUs bleiben bis
zu einem eigenständigen geprüften Segmentierungsvertrag ausgeschlossen.

## Unabhängige Goldenbytes und Prüfung

`generate_vectors.py` erzeugt 14 Bytevektoren ausschließlich mit Pythons
`struct` und `uuid`, ohne den Kotlincodec zu importieren oder auszuführen.
Die versionierte Testressource enthält jede Anforderung und Antwort als Hex,
Felddeskriptoren und Firmware-Provenienz. Sie deckt alle drei Controlarten,
alle Ergebnisnummern, 32-/64-Bit-Grenzen, maximales TTL und die letzte exakt
darstellbare Gerätezeit ab. `firmware-grant` ist die vollständige tatsächliche
C++-Grantantwort, die der unabhängige Firmware-Pythonrunner bereits vergleicht.
LF ist für den vollständigen Ressourcendigest festgelegt.

```
python companion/contract/diagnostic-control-v1/generate_vectors.py --check
```

Aus `companion/android`:

```
gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:assembleDebug :app:lintDebug
```

Elf neue JVM-Fälle prüfen alle Vektoren bytegenau, alle Längen 0..129, große
Frames, Header-/Nullfeld-/UUID-/TTL-/Zeitfehler, unabhängige Ergebnis- und
Anfragekorrelation, alle Originalmetadaten, Fristen, Entzug, Abbruch und
Doppelantworten. Der C++-Runner wurde auf sauberer Quelle `3209275` rein lesend
ausgeführt: 350 Assertions und alle 88 Grantbytes bestanden. Er kompiliert in
einem temporären Verzeichnis; keine fremde Produktdatei wurde geändert.

Der endgültige lokale Prüfstand besteht 55 Protocol-JVM- und 21 App-JVM-Tests
ohne Fehler oder übersprungene Fälle; davon sind elf Controltests neu.
Debugbuild und Debug-Lint bestehen. Lint meldet die drei bereits bekannten
Warnungen `OldTargetApi`, `DataExtractionRules` und `MissingApplicationIcon`.
Die unabhängige Goldenbyte-Prüfung besteht mit allen 14 Vektoren.

Die bestehende bedienbare Android-Debugsimulation und ihr JSON-Berichtcodec
wurden im Controlpaket auf der geprüften Basis `45d3e79` belassen. Der
DG-Controlcodec ist weiterhin nicht an ihre lokale Fake-Freigabe angeschlossen,
weil er ausschließlich Control bestätigt. Der folgende
[Step-/Reassembly-Schnitt](../diagnostic-step-v1/README.md) ergänzt inzwischen
synthetische Schrittbytes in der Debugsimulation. Kein App-Cache oder
simulierter Controlgrant wird zu einer echten Geräteerlaubnis umgedeutet.

## Nächste konkrete Abhängigkeiten

1. Der nachfolgende Android-Step-/Reassembly-Schnitt ist separat dokumentiert;
   maßgeblich ist der korrigierte Vertrag `b5a8c430` auf Wireursprung `04c145f`.
   Echter Serviceanschluss, konsistente Capabilities und ein Wire-AdmissionACK
   bleiben offen. Die in diesem Controlpaket geprüften Bytes sind unverändert.
2. F1: geprüfter Ursprung des restaurierten Bondmodus vor SMP-Reset, aktuelle
   Challenge/P256/Credential-/Registry-ACK und Entwertung derselben Verbindung.
   `d1d3b4e` belegt HCI-Dispatch, reicht für ein Rollenrecht nicht aus.
3. F2: echter Service-/Actor-/Credential-/Registryanschluss im Produkt.
   F0 `src/main.cpp` startet bisher Spiel/Komponenten, keinen Produktions-GATTdienst;
   die bisherige GATT-ABI-Probe ersetzt diesen Anschluss nicht.
4. Echter Diagnoseowner/IPort mit Idleprüfung, exklusivem Komponentenbesitz,
   unterdrückten Spieleingaben, Spielstartfence, garantiertem Timer und sichtbar
   bestätigtem Auszustand. Hardwareprofil mit LED-Revision und Wake-Zuordnung.
5. Bediente Android-Persistenz-/Berechtigungs-/Lebenszyklus-/Dateidialogprüfung und
   anschließend eigenständiger Hardwarelauf durch den Nutzer in der Projekt-App.

Dieser Hostschnitt enthält keine Geräteaktion, APK-Installation, Funkprüfung,
physische Abnahme, Remoteänderung oder Produktionsfreigabe. Android-Repository-
und Fake-Zuständigkeiten folgen der bereits gelesenen offiziellen Basis:
https://developer.android.com/topic/architecture/recommendations (2026-10-10).
