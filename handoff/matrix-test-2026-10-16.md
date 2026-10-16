# Tastenmatrix: Übergabe des offenen Gerätetests

## Aktuelle Fortsetzung: Sammeldurchgang, 16. Oktober 2026

Der [Sammeldurchgang](../docs/matrix/collection-run-2026-10-16.md) ersetzt für die nächste
Bestückungserfassung die Einzelbestätigungen. Umsetzung, Arbeitskopie,
Speicherung und Abschlussauswertung stehen dort. Der folgende frühere
Arbeitsstand ist historisch; den neuen Geräte-/Prüfstand im verlinkten
Sammellaufbericht lesen.


Stand: **16. Oktober 2026, Europe/Berlin**. Fortsetzung in Sitzung
`01a141f2-ef84-7a63-b492-6b1fa2e9d26e`; ursprünglicher Gerätetest in
`01a141ae-0601-7570-9555-79479dbe81d1`.
Status: **Softwarekorrektur nach Originalrichtung vorbereitet;
neue Installation und Geräteabnahme offen**. Keine neue Geräteinteraktion.

## Auftrag und letzter belegter Stand

Gemeinsamer Test der Reedkontaktmatrix nach dem LED-/Wake-/NFC-Muster.
App und Matrix-Prüffirmware aus Quellstand
`c613acebfb07805d1c363b52a325721f2b6f8077` sind verifiziert installiert.
**Diese Matrix-Testversion bleibt der zuletzt belegte Gerätestand; es wurde
kein Rückweg ausgeführt.** Der Test ist nicht bestanden und keine physische
Position hat eine bestätigte Koordinate. In der Fortsetzung hat der Nutzer
bestätigt: **Box vollständig stromlos, Matrixleitungen angeschlossen**.
Diese Nutzerbeobachtung ist kein erneutes Auslesen oder Flashprüfen.

Kanonische Ergebnisse, Prüfsummen, Quellen und Grenzen:
[Gerätebericht](../docs/matrix/device-test-2026-10-16.md) und
[bereinigter Maschinenbericht](../docs/matrix/evidence/device-test-2026-10-16.json).
Die ursprünglichen Softwareprüfungen stehen separat im
[Vorbereitungsbericht](../docs/matrix/preparation-results-2026-10-16.md).

Bei bestätigter Ruhe zeigte die App aktuelle leere Matrixwerte. Beim
Reedkontakt unten links, seiner Wiederholung mit direkt angelegtem Magneten
und einem zweiten Kontakt oben rechts blieb sie ebenfalls leer. Der Nutzer
bestätigt funktionierende Auflösung im Ursprungsprojekt. Frühere Aussage
„Ring an Zeilenleitung“ benennt noch keine konkrete MCP-Klemme.

## Aktueller Auftrag und nächster Schritt

Der Nutzer hat den zunächst geplanten zwingenden Leitungs-/Diodenabgleich
durch den Auftrag ersetzt, **zuerst die Prüffirmware aus der ursprünglichen
Scanrichtung und Pinbelegung zu korrigieren**. Physische Unklarheiten werden
gezielt nach Bedarf geklärt; der vollständige Leitungsabgleich ist keine
Vorbedingung dieser Softwarekorrektur.

Der Quellabgleich von `001eb7b` und `61ef6c6` bestätigt dieselbe Richtung
und Pinbelegung: Spalten `0x20 B1/A0–A6/B5` LOW, Zeilen `0x21 B7–B0` und
`0x20 B0` mit Pull-up lesen. Die genaue historische Binärzuordnung bleibt
offen; der funktionierende Originalbetrieb ist eine Nutzerbestätigung.
Die Korrektur behält inaktive Spalten hochohmig statt sie wie im Original
HIGH zu treiben. Quelle, Umsetzung und frischer Softwareprüfstand stehen im
[Korrekturbericht](../docs/matrix/scan-direction-correction-2026-10-16.md).

Der historische Prüfmodus ist ausdrücklich durch
`LARP_MATRIX_LEGACY_IO7_INPUT=1` aktiviert. Der Herstellerkonflikt von
MCP23017 IO7 mit der B7-Eingangsverwendung bleibt offen; die Korrektur ist
kein datenblattkonformer B7-Ausweg. Kein Acht-Reihen-Teilscan wird als
vollständiger Matrixstatus ausgegeben. Das Standardprodukt scannt keine Matrix.

**Nutzerbestätigung vom 16.10.2026:** Einige Kontakte fehlen absichtlich,
weil sie mit anderen Baugruppen kollidieren würden. Die genaue Liste ist
unbekannt. A1 bis I9 bleiben logische Kandidaten; keine Nummerierung
komprimieren. Leere Werte belegen weder einen Defekt noch fehlende Bestückung.

Nach bestandenem Softwaretest ist der nächste Schritt ein **gesondert
beauftragter gezielter Gerätetest mit passender Installation**. Dafür zuerst
angeschlossenes Handy/ESP und den aktuellen Gerätestand frisch identifizieren,
den chipgebundenen privaten Flash-Rückweg prüfen und einen neuen Lauf mit
eigenem Belegverzeichnis vorbereiten. Danach denselben benannten realen
Kontakt bewusst halten, lösen und wiederholen. Erst nach dieser positiven
Kontrolle weitere Kontakte zuordnen oder Mehrfachkontakte prüfen. Die neue
Korrektur wurde weder installiert noch physisch geprüft; aktuell ist die Box
nach Nutzerbestätigung stromlos.

## Arbeitsverzeichnis und lokale Belege

Repository: `https://github.com/Sven401/Larp-Escape.git`, eigener Branch
`codex/matrix-test-preparation`, Draft [PR #49](https://github.com/Sven401/Larp-Escape/pull/49),
Ziel `plan/projektuebergabe-kuratierter-stand`. Vor Fortsetzung Status, Branch,
HEAD und Remote im **eigenen Klon** prüfen. Ausgangsstand dieser Fortsetzung:
`50effd812bb8f2ac30031aa836936a8efd7086ba`. Die neue Korrektur und ihr
Prüfstand werden im Korrekturbericht geführt; der zuletzt auf den Geräten
geprüfte Quellstand bleibt `c613ace`.

Lokaler Chat-Arbeitsordner, relativ zum Windows-Benutzerprofil:
`Documents/Codex/2026-10-16/bereite-den-n-chsten-gemeinsamen-test/`.

- Projektklon: `work/matrix-test/`.
- Bereinigte Laufbelege: `work/evidence/matrix-01/`.
- Private Flash-/APK-Abzüge, Schlüsselvergleich, Rohlogs und UI-Belege:
  `work/private/matrix-01/`; niemals veröffentlichen.
- Installation/Rückweg: `work/matrix_install.py`; das Skript ist auf diesen
  Lauf und dessen Prüfsummen gebunden. Kein blindes erneutes Backup-/Install-
  Ausführen mit denselben Belegnamen. Neuer Versuch erhält ein neues Laufverzeichnis.
- Android-Anzeige: `work/android_ui.py`; vor Aktionen frische Ansicht prüfen.
  Während des Tests war zeitweise ChatGPT im Vordergrund. Nach Rückkehr war
  die Box getrennt; erneut verbinden und erst frische bestätigte Werte auswerten.
- Nutzerkopie des früheren Übergabestands: `outputs/Tastenmatrix-Uebergabe.txt`;
  für die aktuelle Fortsetzung gilt diese kanonische Projektübergabe.

ADB-Handy und COM-Port vor Wiederaufnahme frisch identifizieren; keine alte
Portannahme ungeprüft übernehmen. Vor weiterem Firmwarewechsel erneut
chipgebundenen Rückweg und aktuellen Gerätestand prüfen. Der private
4-MiB-Abzug vor Matrixinstallation ist am Chip verifiziert; Anwendung,
Partitionstabelle und Schlüssel wurden nach Installation verifiziert.
Zum Ende der ursprünglichen Sitzung waren Wiederherstellung und Abschalten
**nicht** ausgeführt. In der Fortsetzung hat der Nutzer das vollständige
Trennen der Stromversorgung bestätigt. Es gab weder eine Wiederherstellung
noch eine neue Installation oder Geräteprüfung durch Codex.
