# P1a-1: begrenzte kanonische JSON-Byteprüfung

Stand: **31.10.2026**. Quelle: #93 und ausdrücklicher PM-Folgeauftrag nach
Integration von #111. Basis: `0de42307044c62fc29805b5431050a791c9e72ad`.
Führende Umsetzung: Firmware-Entwickler `p1a1_developer`; technische Entscheidung:
`p1a1_maintainer`; anderer Gegenprüfer: `review_111_p1a`.

## Umfang und technische Entscheidung

`larp::configuration::CanonicalPackageJsonReader` prüft genau ein Rootobjekt
gemäß dem [kanonischen Byteprofil](p0-formate-v1.md#1-kanonische-bytes-und-identitäten).
Die gemeinsame explizite CMake-Quellliste bindet dieselbe Implementierung in
Host und native SDK-Ziele ein. Bestehende Profil-/Commandreader bleiben erhalten.

Zwei synchrone Eingänge sind vorhanden: `validate(const char *, size_t)` und
`validate(IPackageByteSource &)`. Die Quelle liefert `Byte`, `End` oder `Failure`;
Fehler werden nicht als EOF behandelt, auch direkt nach einem gültigen Root.
Das Ergebnis enthält ausschließlich Fehlercode und nullbasierten Byteoffset.
Erfolg meldet die vollständige Dokumentlänge. Keine geliehenen Zeiger verbleiben
nach Rückkehr. Der Reader hat genau einen exklusiven Besitzer pro Aufruf;
Quelle und Bytes müssen währenddessen stabil bleiben und dürfen den sich
ändernden Readerzustand nicht überlagern. Jeder Aufruf setzt den Zustand zurück.

Der Maintainer hat ein explizites **Betriebsprofil** festgelegt:

| Grenze | Verhalten |
| --- | --- |
| 262144 Quellbytes | EOF exakt am Limit zulässig; erstes weiteres Byte meldet `DocumentLimit` bei Offset 262144. |
| 32 gleichzeitig offene Container | Root zählt als eins; öffnender Token des 33. Containers meldet `DepthLimit`. |
| 0–48 dekodierte ASCII-Keybytes | Erstes weiteres Zeichen meldet `KeyTooLong` an seinem Quellbyte; bei einem erlaubten Escape am Backslash. |

256 KiB sind im [Hauptvertrag §6](p0-vertrag-v1.md#6-ressourcen-und-überprüfbare-startbudgets)
für den kanonischen Inhalt benannt. Gesamtgrößen von Modul- und Deploymentdateien
sind dort nicht vollständig festgelegt; das Betriebsprofil behauptet deshalb
keine vollständige Formatfähigkeit für beliebige solche Dateien. Es ist kein
automatisches Gerätebudget oder Aktivierungsversprechen. Die Ausdruckspfadtiefe 8
ist keine JSON-Containertiefe; gültige Ausdruckshüllen werden separat getestet.

Keys werden nach **dekodierten ASCII-Bytes** verglichen, einschließlich leerem
Key und DEL. Gleichheit wird abgelehnt; ein kürzeres Präfix sortiert vorher.
Wertstrings erlauben unverändertes gültiges UTF-8, jedoch keine C0-Zeichen.
Nur Quote und Backslash dürfen escaped sein. Zahlen werden ausschließlich
lexikalisch auf kürzeste Integer geprüft; keine Konvertierung nach int32/uint64.
Insbesondere werden uint64-Generationen nicht vom Byteprüfer abgeschnitten.

Das ist **keine Schema-/Feldmengen-/Wertebereichsprüfung**. `{}` und unbekannte,
sortierte Keys können bytekanonisch sein, obwohl sie kein gültiges Paket bilden.
Schema, ID-Arrayreihenfolge, Modulbindung, Rechte, Digests/Signaturen, Planer,
Capabilities und Aktivierung bleiben nachfolgende eigene Prüfungen. Es entsteht
weder DOM noch teilweise übernommener Paketinhalt oder Verbraucher-Callback.

## Fehleroffsets und Ressourcen

Der erste Fehler bleibt erhalten. EOF/Quellenfehler melden die Anzahl vorher
gelesener Bytes; `InvalidArgument` meldet 0. Keyduplikat/-ordnung melden die
öffnende Quote des zweiten Keys. Verbotene Escapes melden den Selektor;
UTF-8 meldet den illegalen Lead beziehungsweise die erste widersprechende
Continuation, Trunkierung EOF. `-0` meldet die Null, `01` die zweite Ziffer;
Plus, Punkt und Exponent melden ihr jeweiliges Quellbyte.

Fester Containerstack im Readerobjekt, kein rekursiver Abstieg, Heap oder DOM.
Unter lokalem **MinGW GCC 5.1.0 / 32 Bit** beträgt `sizeof(Reader)` **1656 Byte**;
das Testziel erzwingt das vereinbarte Layoutziel von höchstens 4096 Byte.
Der ergänzende `-O2 -fstack-usage`-Bericht weist maximal **128 Byte pro eigener
Funktion** (`readKey`) aus. Dies ist kein gemessener Gesamt-Callstack oder
ESP32-Tasknachweis; virtuelle Quellimplementierung und Aufrufer kommen hinzu.
Das Readerobjekt selbst gehört zum Speicherbudget seines Besitzers.

## Ausgeführte lokale Prüfung

- Frischer CMake-/Ninja-Hostbuild mit C++11 und `-Wall -Wextra -Wpedantic -Werror`;
  exakt gepinnte Unity-Quelle `cbcd08fa7de711053a3deec6339ee89cad5d2697`.
- Neues CTest-Ziel `package-json`: zwölf Unity-Testgruppen mit festen Bytefällen
  und erwarteten Offsets, Memory-/Streamgleichheit, allen Trunkierungspräfixen,
  Quellenfehler an jeder Position einschließlich Rootende, Wiederverwendung,
  exakten drei Grenzen und jeweils Überschreitung erfolgreich.
- Vollständiger lokaler Standard-Hostlauf: **4/4 CTest-Ziele** erfolgreich
  (`native-journal`, `package-json`, `configured-control`, `led-wire`).
- **18/18 P0-Modelltests** einschließlich sechs Referenztraces erfolgreich.
- Architektur: **147 Dateien / 305 Includes**; **87 unabhängige C++11-Header**;
  Formatbaseline **337 Dateien** mit clang-format **19.1.7** erfolgreich.

Ein erster nicht erhöhter Windows-Prozessstart für die ergänzende Messung
scheiterte an einer DLL-Laufumgebungsbeschränkung; derselbe Compiler-/Testpfad
lief im genehmigten Hostkontext erfolgreich. Kein Produktfehler daraus abgeleitet.
Lokale Logs, Buildausgaben und Stackbericht liegen regenerierbar unter `private/`.
Unabhängiger Review und CI werden am veröffentlichten Commit separat zugeordnet.
Keine lokale PSA-/SDK-Neuqualifikation, kein Upload, Geräte- oder Gesamtspieltest.
