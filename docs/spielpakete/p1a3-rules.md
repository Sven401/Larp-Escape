# P1a-3: begrenzte Direktregelprüfung

Stand: **31.10.2026**. Auftrag: [SP-01 / #93](https://github.com/Sven401/Larp-Escape/issues/93),
PM-Folgeauftrag P1a-3. Führender Entwickler `developer_p1a3`, unabhängiger
Gegenprüfer `quality_p1`; technische Schnittstellen und Betriebsprofil: Maintainer
in der übergeordneten Sitzung. Keine Geräteprüfung, kein Upload oder Release.

## Basis und Umfang

Dieser Arbeitszweig ist ausdrücklich auf dem exakten offenen #117-Head
`98638eef25ba221329d81cb576a2b1625c8e53e0` gestapelt. Dessen Deklarationsprüfung
wird wiederverwendet; seine Integration wird nicht behauptet. Zielbranch bleibt
`plan/projektuebergabe-kuratierter-stand`. Vor Integration Abhängigkeit #117 und
aktuelle gemeinsame Basis erneut prüfen.

Verbindliche Quellen sind [P0 §2](p0-formate-v1.md#2-stations--und-sprachgrammatik),
[Semantik und Budgets](p0-vertrag-v1.md) und der
[Deklarationsvertrag](p1a2-declarations.md). Geprüft werden ausschließlich direkt
deklarierte Stationsregeln, deren Ausdruckstypen und Referenzen. Ein begrenzter
Erfolg ist weder vollständige Paketgültigkeit noch Capability `rules_ir_v1`,
Ausführbarkeit, Installation oder Aktivierung.

Leere Modul- und Instanzlisten gehören zum Umfang. Nichtleere Listen sowie
`local_fallback` gehören zum späteren Teilschnitt. Eine Meldung UnsupportedScope
bestätigt weder allgemeine P0-Gültigkeit noch P0-Ungültigkeit. Kanonische Byte-
und Quellenfehler behalten auch nach nicht unterstütztem Inhalt Vorrang.

## Technische Entscheidung: Betriebsprofil

Zusätzlich zu den P0-Budgets gelten für diesen Teilschnitt folgende konservative
Kapazitäten: 64 Ports je Station, 128 Subjects je Paket, 64 Triggereinträge je
Regel und 4096 offene Subjectreferenzen je Paket. Diese Zahlen sind eine
Maintainerentscheidung vom 31.10.2026 für feste Tabellen, keine neuen allgemeinen
P0-Formatgrenzen. Überschreitungen müssen als Betriebsprofilgrenze erkennbar sein.
Die 512 Ausdrucksknoten und Pfadtiefe 8 gelten je Station; JSON-Containertiefe
und Ausdruckstiefe bleiben getrennt. Triggerlisten erhalten keine zusätzliche
P0-Regel für Eindeutigkeit oder Nichtleere, die der Vertrag nicht benennt.

Die Metadatenzählung bleibt das begrenzte P1a-2-Profil: kanonische Inhaltsbytes
abzüglich vollständiger rules-/instances-Arraywerte. Auch eine erfolgreiche
Direktregelprüfung behauptet damit keine vollständige P0-Metadatenbudgetprüfung.
Vorwärtsreferenzen werden ohne Zurücksetzen der Quelle in begrenzten Tabellen
aufgelöst. Es gibt keine Übernahme von Teilmodellen und keinen Gerätepfad.

### Konservative Konfliktprüfung

Die lokale Bezeichnertabelle ist zusätzlich auf 1024 verschiedene IDs je Station
begrenzt (Betriebsprofil). Überlappende lokale Trigger mit gemeinsamen
Schreib-/Ausgabezielen werden ohne Guard-Beweis konservativ als
OperatingProfileLimit ausgeschlossen; gegenseitig ausschließende Guards können
somit ebenfalls außerhalb dieses Teilschnitts liegen. Das ist keine allgemeine
P0-Ungültigkeit und kein vollständiger Konfluenzbeweis. Verschiedene lokale
Trigger dürfen denselben Geräteport verwenden. Doppelte Schreib-/Ausgabeziele
innerhalb einer Regel und konkurrierende Ausgaben mit einer Factregel sind
hingegen statische Konflikte.

P0 §3 nennt einen produzierenden once-ruleId je Zielsubject im Absatz über
verteilte Regeln. Bis zur vollständigen Planer-/Deploymentklärung übernimmt
dieser Teilschnitt die konservative Betriebsregel auch für rein lokale
Subjectausgaben: genau ein once-Produzent. Rein lokale each-Produzenten oder
mehrere lokale Produzenten werden als OperatingProfileLimit eingeordnet;
die konkurrierende Ausgabe einer Factregel bleibt Conflict. Diese Einordnung
ersetzt keine neue allgemeine Sprachentscheidung.


## API, Speicher und Nachweise

`PackageRuleReader` liefert `Rejected` oder `DirectRulesOnly`, ohne boolesche
Erfolgskonvertierung oder ausführbares Ergebnisobjekt. Deklarationsfehler werden
im verschachtelten `DeclarationResult` erhalten; kanonische Quellenfehler haben
Vorrang. Die optionale synchrone Observer-Anbindung publiziert keine Teilannahme.
Quelle und Reader bleiben exklusiv und nicht reentrant; kein Callback darf die
Quelle ändern. Es werden keine geliehenen Bytezeiger gespeichert.

Hostmessung mit MinGW GCC 5.1.0, C++11, 32-Bit-Zeigern: Reader **155860 Bytes**,
RuleResult 536 Bytes, DeclarationResult 272 Bytes. Compiler-Stackbericht ohne
Optimierung: größter eigener statischer Funktionsframe 608 Bytes (validate),
reset 564, station/token je 352. Dies ist weder die kumulierte Aufruf-/Rekursions-
stackgröße noch ein ESP32-Heap-, Taskstack- oder Latenznachweis. Insbesondere
überschreitet allein das Readerlayout die frühere P0-Arbeitsreserve 64 KiB; vor
Geräteintegration sind Speicheroptimierung bzw. ein qualifiziertes eigenes
Compilerbudget erforderlich. Keine Eignung für einen vorhandenen Gerätetask
aus diesen Hostmessungen ableiten.

Aktuell ausgeführt: CMake/Ninja mit C++11/Werror, **6/6 CTest-Ziele**,
Architekturprüfung **151 Dateien / 313 Includes**, **89 unabhängige Header**,
Formatbaseline **341 Dateien** mit clang-format 19.1.7. PlatformIO 6.2.0 `host-regression`: **15 Suiten / 150 Fälle erfolgreich**.
P0-Modell: **18/18 Fälle erfolgreich**. Unabhängig: **187 feste Byte-/Semantikvektoren**
und **11581 Quellenfehler-/Präfixprüfungen** erfolgreich, einschließlich genauer
Betriebsprofilgrenzen und verzögerter Diagnosepfade. Keine verbleibenden P1/P2
im begrenzten Gegenprüfumfang. Die zuerst gefundene irreführende Pfadangabe bei
später Referenzauflösung ist korrigiert und unabhängig nachgeprüft.

Die versionierten [unabhängigen Prüfvektoren](../../test/fixtures/package-rules-review/README.md)
prüfen feste kanonische Bytes ohne Produktencoder. Sie sind eine fachliche
Gegenprüfung eines anderen Bearbeiters, keine unabhängige externe Zertifizierung.
SDK-/Firmware-/Android- und zwölf PR-Pflichtchecks werden am veröffentlichten
Stand gesondert zugeordnet. P1a/#93 bleibt über diesen Teilschnitt hinaus offen.

Geprüfte Implementierung nach finaler Formatierung (SHA-256):
PackageRules.cpp `35722e9e648e41ccfa78477f9c61a00bc950e4f73a48a5b6e9ba2c198a0e379b`,
PackageRules.h `66590d99eb63bc83a0ab7288877dc2d55474cc845932faca1b395bf58eb30219`.
Der veröffentlichte Commit und die PR-CI werden im PR zugeordnet; diese
Quellhashes bezeichnen den tatsächlich lokal geprüften Implementierungsstand.
