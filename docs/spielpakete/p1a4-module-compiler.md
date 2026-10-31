# P1a-4: generische Modulbindung und Expansion

Status: **implementiert und hostseitig gegengeprüft; native PSA-PR-Prüfung offen**,
31.10.2026.
Auftrag: [SP-01 / #93](https://github.com/Sven401/Larp-Escape/issues/93).
Arbeitsbasis: `77540756f7f7383aa7117fbeb52c7bfea8b34a83`, ausdrücklich auf
dem offenen [PR #121](https://github.com/Sven401/Larp-Escape/pull/121) gestapelt;
dessen Abhängigkeit #117 bleibt erhalten. Entwickler: `developer_p1a3`, anderer
Gegenprüfer: `quality_p1`, Maintainer/Koordination: übergeordnete Sitzung.
Gezielte Hash-Grenzprüfung: `module_digest_review`. Keine Integration oder
Gerätefreigabe aus dieser Planung ableiten.

## Ziel und verbindliche Quellen

Der nächste fehlende Pfad ist die generische Prüfung und Expansion von
Moduldefinitionen und Instanzen gemäß [P0 §3](p0-formate-v1.md), einschließlich
Parameterbindung, Constraints, verschachtelter Verdrahtung und DAG-Grenzen.
Die [Referenzmodule](p0-referenzspiele-v1.md) und umbenannte Varianten müssen
denselben Codepfad verwenden. Metadatenprüfung allein erfüllt diesen Auftrag
nicht. Vollständiger Planer, Deployment-/Rechteprüfung und Geräteausführung
bleiben darüber hinaus eigene offene Anforderungen von #93.

P1a-3 besitzt absichtlich konservative lokale Konfliktgrenzen. Diese dürfen
nicht als vollständiger Prüfer für expandierte Referenzmodule verwendet werden:
Regeln mit gleichem Trigger und Ziel, aber sich ausschließenden Guards sind
beispielsweise für `threshold` erforderlich. Typ-/Strukturprüfung und vollständiger
Planer müssen ihre jeweilige Aussage getrennt ausweisen.

## Technische Präzisierungen vor der Implementierung

Die folgenden Entscheidungen konkretisieren bislang nicht bytegenau festgelegte
Stellen des P0-Vertrags. Sie sind am tatsächlichen Ergebnis gegenzuprüfen.

- `content.modules` benennt den vollständigen gepinnten Distributionsabschluss.
  Identische Pins werden dedupliziert; gleiche ID/Version mit anderem Digest ist
  ein Konflikt. Verschachtelte Abhängigkeiten müssen dort auflösbar sein. Der
  Provider wird nur nach gelisteten Pins gefragt; kein Netzwerkfallback.
- Alle gelisteten Definitionen werden geprüft, auch unbenutzte Regeln und
  Constraints sowie Zyklen in nicht instanziierten Zweigen. Unbenutzte Constraints
  werden typgeprüft, aber nicht still mit Defaults ausgewertet. Jede tatsächliche
  Instanz liefert vollständige Parameter-/Port-/Ressourcenmaps.
- Instanzparameter an der Inhaltswurzel sind bestehende Literalobjekte
  `{op:"literal",type,value}`. Innerhalb eines Moduls ist zur Weitergabe
  `{op:"parameter",id}` zulässig. Parameter-Metadaten `default`, `min` und `max`
  enthalten die zugehörigen primitiven Werte. `{parameter:ID}` ist ausschließlich
  an typisierten Wertinitialisierungs- und Timerdauerpositionen vorgesehen;
  keine freie Text-, ID- oder Labelsubstitution.
- Constraints werden nach tatsächlicher Bindung vollständig und streng typisiert
  ausgewertet. Überlauf auch in einem sonst wahren `any`-Zweig ist ein Fehler.
- Modulinputs und -outputs bilden eine eindeutige Portnamensmenge. Andere
  Symbolklassen bleiben getrennt typisiert. Physische Ports behalten ihre
  Ein-/Ausgangsrichtung. Ein verschachtelter `solved`-Input darf einen deklarierten
  Parent-Input oder -Output konsumieren; ein `solved`-Output bindet ausschließlich
  einen Parent-Output. Die endgültige Erzeugung muss auf ein deklariertes Subject
  mit lokaler Stationsherkunft zeigen; keine erfundenen internen Subjects.
- Die äußerste Instanz zählt als Expansionstiefe 1. Instanzbudgets zählen
  expandierte Vorkommen statt lediglich verschiedene Definitionen. Zusätzliche
  Speicherprofile und ihre Reichweite sind getrennt von P0-Gültigkeit zu melden.
- Autoren-IDs bleiben auf 48 ASCII-Bytes begrenzt. Generierte interne IDs halten
  strukturierte Pfadsegmente. Ihre kanonische Ausgabe verwendet für **alle**
  internen IDs, einschließlich direkter Definitionen, einen führenden Slash und
  JSON-Pointer-Escaping je Segment (`~0`, `~1`). Vier Instanzsegmente plus lokale
  ID benötigen höchstens 485 Bytes. Keine Abschneidung auf 48 Bytes. Öffentliche
  Stationsports und Subjects behalten ihre ursprünglichen Identitäten.

## Hash- und Übernahmegrenze

SHA-256 bezieht sich auf die tatsächlich gelesenen kanonischen Rohbytes,
einschließlich Stringschreibweise. Providerbehauptungen oder lediglich gleiche
Referenzstrings ersetzen keinen Hashnachweis. Modul-ID und uint16-Semver aus den
Bytes müssen zusätzlich zum Digest mit dem Pin übereinstimmen.

Der reine Kern verwendet eine explizit fehlschlagende inkrementelle Hashschnittstelle;
ein realer Adapter verwendet öffentliche PSA-Multipart-APIs am bereits
qualifizierten nativen Buildpfad. Kein neues handgeschriebenes Hashverfahren.
Setup-/Update-/Finish-/Abortfehler dürfen keine Annahme ergeben. Quellen- und
Kanonikfehler behalten Vorrang, auch unmittelbar nach vollständigem Rootende.
Die Quelle wird einmal gelesen; ein nachfolgender Durchlauf über eigenes
typisiertes IR ist keine Wiederholung der Bytequelle.

Ausgaben werden erst nach vollständiger Prüfung transaktional veröffentlicht.
Ein Hash ist Identitäts-/Integritätsprüfung, keine Signatur, Berechtigung oder
Aktivierungsfreigabe. Mockhashes dürfen ausschließlich Fehlerpfade prüfen;
reale Hash-Gegenvektoren bleiben obligatorisch.

Compiler, Quellen, Hashinstanz und beide Workspaces haben für den gesamten
synchronen Aufruf einen exklusiven Besitzer. Provider-, Digest- und Sinkcallbacks
dürfen weder Quelle noch Workspace verändern oder den Compiler erneut aufrufen;
Update-/Writepuffer dürfen nicht über den Callback hinaus gespeichert werden.
Ein fehlgeschlagener Transaktionsbeginn kann bereits provisorischen Zustand
angelegt haben und benötigt ebenfalls Abort. Fehlgeschlagene Hashinitialisierung
und Finalisierung müssen sicher abbrechbar bleiben. Providerfehler dürfen keine
offenen Quellhandles hinterlassen.

Lokaler Umgebungsbefund vom 31.10.2026: Der vorhandene Ordner
`esp-idf-migration/Larp Escape/private/esp-idf-v6.1` wurde mit
`scripts/check_idf_sources.py` geprüft und wegen abweichendem Commit abgewiesen.
Er ist kein Nachweis am aktuellen Pin
`9a97f6c54ec638111ce55cd36581b3c192f15207` und wurde nicht verändert.
Die reale native Adapterprüfung benötigt eine getrennte passende Quellbasis
oder den unveränderten nativen CI-Prüfpfad am veröffentlichten Commit.

## Ergebnis, Betriebsprofil und Prüfstand

`PackageModuleCompiler::compile` liefert `Rejected` oder
`ExpandedUnauthenticated`, keine vollständige Paketgültigkeit oder Ausführbarkeit.
Die eigentliche Bindung, Constraintauswertung, DAG-Expansion und kanonische
Stationsausgabe sind implementiert. Alle Stationen werden vor dem Beginn der
Ausgabetransaktion vorgeprüft; allein der erfolgreiche Sinkcommit veröffentlicht
die gesamte Ausgabe. Der Decoder liest jede Quelle einmal. Wiederholte
Expansion aus eigenem typisiertem IR wiederholt keine Bytequelle.

Die semantischen Speicherpools gehören dem Aufrufer und haben explizite
Kapazitäten. Überlauf gibt `OperatingProfileLimit`, keine Trunkierung oder
Teilannahme. Zusätzliche aktuelle Grenzen: 1024 dekodierte Bytes je lexikalischem
Token, 64 Parameter und 128 Interfaceeinträge je Kontext, höchstens 65534
indizierte Einträge je Pool und 256 KiB kanonische Bytes je Quelle. Das sind
Betriebsprofilgrenzen, insbesondere für Moduldateien keine erfundenen globalen
P0-Formatgrenzen. Die 32 Vorkommen werden pro expandierter Station gezählt;
die Wurzeltiefe ist 1. Die vollständige P0-Metadatenbudgetprüfung bleibt offen:
insbesondere wurde kein gesondertes 32-KiB-Metadatenlimit für Moduldateien
implementiert. Der erfolgreiche Hashvergleich ersetzt keine Trustentscheidung.

Frisch geprüft nach finaler Quellformatierung mit MinGW GCC 5.1.0/C++11:

- CMake/Ninja mit Werror: **7/7 CTest-Ziele** erfolgreich; bestehende Reader-,
  Deklarations- und Direktregeltests bleiben enthalten.
- PlatformIO 6.2.0 `host-regression`: **16 Suiten / 157 Fälle erfolgreich**.
  Die neue Suite wird tatsächlich über den expliziten Quellfilter gelinkt.
  Die PSA-Bibliothek ist ausschließlich in dieser Hostumgebung von der
  Bibliotheksautosucheinbindung ausgenommen: PlatformIO erkannte sonst den
  bedingten PSA-Testinclude trotz nicht gesetztem Native-Testmakro. Es wurde
  keine Testsuite entfernt. Reale PSA-Tests bleiben eigene native CTest-Ziele.
- P0-Modell **18/18**; Architektur **153 Dateien / 318 Includes**;
  **90 unabhängige C++11-Header**; Formatbaseline **343 Dateien** mit
  clang-format 19.1.7. Adapter und neuer Unity-Test zusätzlich direkt formatiert
  und geprüft.
- Anderer Bearbeiter `quality_p1`: **89 feste kanonische Gegenvektoren**, darunter
  bytegenaue Ausgaben aller drei Referenzbausteine, umbenannte Varianten,
  verschachtelte Solved-Verbindungen, 485-Byte-Namen und die relevanten
  Grenzen exakt/+1; **2843 Quellenfehler**, **2842 Präfix-/späte Quellenfehler**,
  **30 Hashfehler-/Wiederverwendungsfälle**, **10 Ausgabefehlerfälle** bestanden.
  Erfolgreiche Fälle benutzen echtes Windows-BCrypt-SHA-256. Der konstante
  Mockhash dient ausschließlich negativen Zyklusproben, deren erwarteter Fehler
  ausdrücklich `Cycle` ist; kein positiver Integritätsnachweis mit Mockhash.
  [Reproduzierbare Quellen und Aufrufe](../../test/fixtures/package-modules-review/README.md).
- Keine verbleibenden P1/P2 im dokumentierten unabhängigen Reviewumfang.
  Die gefundenen Fehler bei unbenutzten Factguards, später Quellenzuordnung und
  fehlgeschlagenem Transaktionsbeginn sind korrigiert und gegengeprüft.

Host32/GCC5.1 ohne Optimierung: Compiler **23928 B**, Resultat **308 B**,
Workspace-Metadaten **132 B / 72 B**, jeweils **ohne** die vom Aufrufer
bereitgestellten Pools. Größter eigener statischer Compilerframe **1648 B** in
`expandContext`; rekursive Aufrufe kommen hinzu. Das sind keine Gesamtstack-,
Geräte-Heap- oder Latenzmessungen. Der PSA-Backendbedarf ist ebenfalls gesondert
zu qualifizieren; der reine Kern benötigt keinen Heap oder JSON-DOM.

Geprüfte SHA-256 nach Formatierung:
PackageModules.cpp `5ee6de694e1794a690e8c827536c562a7492c58983ff261ce0843a33b956a878`,
PackageModules.h `5d28e5ed14bfade9162de9125d5af4b473ba4520e8667152b78e325bb895fd4c`.
Commit und PR-Nachweise werden im veröffentlichten PR zugeordnet.

## Noch offen

Die nativen CTest-Ziele `package-modules-psa` und `package-modules-review-psa`
sind angeschlossen: bekannte leere/abc/Mehrblock-Hashvektoren, Wiederverwendung
und derselbe vollständige unabhängige Compilerfixturelauf über den realen
produktiven PSA-Adapter. Deren tatsächlicher Lauf auf dem exakt gepinnten SDK
und die zwölf aktuellen PR-Pflichtchecks stehen vor Veröffentlichung noch aus.
Die Quellprüfung allein ersetzt diese Ausführung nicht.

Vollständiger Planer/Konfluenz, Trace-Konformität, Deployment-/Berechtigungs-
prüfung, Aktivierung, Geräte-/Mehrboxbetrieb und der spätere Drittspielnachweis
bleiben Anforderungen des vollständigen Ziels #93. Dieser Teilschnitt hebt
keines dieser Tore auf.
