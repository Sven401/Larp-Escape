# Aus der Firmware erzeugte Profilfähigkeiten v1

**Architektur-Soll · 21.10.2026:** [SP-01](../spielpakete-architektur.md)
fordert versionierte nachladbare Regelbausteine samt Bearbeitungsbeschreibung.
Der heutige Katalog beschreibt kompilierte Primitive; er lädt keine neuen
Operationen. Zusammengesetzte Module, generischer Editor und Fähigkeitenabgleich
bleiben Folgearbeit. Bestehende v1-Bytes und Prüfbelege werden nicht verändert.

**Stand:** 7. Oktober 2026. **Status:** schmaler Firmware-, Werkzeug- und Hostpfad.

scope=profile_language beschreibt die kompilierte Profilsprache. Aktuell
angeschlossene und verfügbare Komponenten stehen im getrennten Laufstatus.
Der Katalog ist kein Nachweis einer Geräteidentität oder eines Funkzugangs.

encodeProfileCapabilities verwendet dieselben Limits, ImportLimits,
ProfileVocabulary und Befehlsnamen wie Reader, Writer und Spielkern. Es
allokiert kein Profil und keinen Heap. Der Aufrufer besitzt den Ausgabepuffer;
maxProfileCapabilitiesJsonBytes=3072 enthält den abschließenden NUL-Byte.
written zählt ausschließlich die JSONbytes. Bei Fehler bleibt written=0
und der erste verfügbare Pufferbyte ist NUL. Auf einem Firmwaretask muss
der Aufrufer die Puffergröße ausdrücklich in seinem Speicherplan berücksichtigen.

## Inhalt und Verwendung

Der Katalog liefert Profil-, Spiel- und Frameversion; Board-, Komponenten-,
Kristall-, Feld-, Vergleichs-, Bedingungs-, Aktions- und Effekttypen; sechs
GM-Befehle sowie 28 Mengen-, Dokument-, UTF8-, Tiefen- und Transportgrenzen.
Die Kernbedingungen verlangen ein gestopptes Spiel für Konfigurationswechsel,
einen bestätigten gespeicherten Startstand und eine aktuelle GM-Verbindung.

Die App verwendet diese Daten mit dem vollständigen
[ProfilJSONschema](../configuration/profile-v1.schema.json). Referenzen,
GPIO- und Buskonflikte und Graphsemantik entscheidet weiterhin die tatsächliche
Firmwarevalidierung. Katalog und Editorprüfung ersetzen Import, Apply und Save
nicht. Neue Typen müssen auch Reader, Writer und Validator tatsächlich erweitern.

## Bedingte Grenzen

limits.leds=256 gilt ausschließlich für aktivierte led_strip-Komponenten.
Der strukturelle uint16-Wertebereich von ledCount bleibt 0..65535:
deaktivierte und andere Komponententypen benutzen diesen Parameter nicht.
Der Schemagenerator liefert x-maxEnabledLeds und x-maxConditionDepth
als semantische Hinweise; x-maxDocumentBytes und x-maxContainerDepth beschreiben
den Reader. Allgemeine JSONschema-Validatoren setzen diese Annotationen nicht
automatisch durch. Ein gültiges Schema ersetzt validateProfile deshalb nicht.

Die anfängliche Prüfannahme, ledCount.maximum müsse gleich limits.leds sein,
war falsch. Der aktuelle Check vergleicht die bedingte Grenze mit der erzeugten
Annotation. Ein tatsächlicher Hostfall prüft aktiviert256, aktiviert0/257
abgewiesen, deaktiviert65535 sowie ein irrelevantes Feld65535.

## Werkzeug und gemessene Prüfung

Nach pio run -e profile-tools liefert
.pio/build/profile-tools/program.exe --capabilities den tatsächlichen
Firmwareencoder ohne Profilaufbau oder Netzwerk. Seine Ausgabe stimmt bytegenau
mit [profile-capabilities-v1.json](fixtures/profile-capabilities-v1.json) überein:
1732 JSONbytes. [Antwortschema](profile-capabilities-v1.schema.json).

58/58 gezielte Hostfunktionen ohne Fixtureexport erfolgreich, Exit0;
%TEMP%/larp-profile-capabilities-final-host.log. Encoderprüfungen umfassen
Null-/Kleinstpuffer, Fehlerzustand, genaue Mindestgröße, Sentinelgrenzen und
bytegleiche Referenzdaten. Acht v2-Fixtures und der Katalog sind schema-valid;
alle neun Vokabulare und 22 zugehörige Profilgrenzen stimmen überein.
Erzeugtes Profilschema entspricht dem aktuellen Generator. Der bestehende
CLI-Befehlsproduzent liefert weiterhin dieselben 13 Referenzrahmen.

Vier finale Builds SUCCESS, terminal Exit0:
%TEMP%/larp-profile-capabilities-final-build.log.
Produktion55.148 RAM/1.228.417 Flash, alte Probe21.480/291.673,
Profilprobe99.056/386.637 plus57.272 einmaliger Profilheap und Allocator/FS.
Profilkatalog wird auf diesen ESP32-Einstiegen noch nicht abgefragt; der Linker
entfernt unbenutzte Funktionen. Buildwerte belegen keinen künftigen Funkbedarf.
Eigener Xtensa-Encoderframe48 Byte, weitere Aufrufkette und Aufruferpuffer kommen
hinzu; keine gemessene Taskreserve. Architektur84 Dateien/198 Includes,
Header54, Format142/clang19.1.7; diffcheck sauber.

Direkter BLE-Endpoint, reale Identitätsprüfung, Hardware- und Produktionsanschluss
sowie SMP-, Laufzeit-, Stack- und Energieabnahme bleiben offen. Kein Upload,
Androidlauf, Remote-CI oder Cppcheck dieses Schnitts.
