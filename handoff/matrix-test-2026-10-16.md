# Tastenmatrix: Übergabe des offenen Gerätetests

Stand: **16. Oktober 2026, Europe/Berlin**. Sitzung
`01a141ae-0601-7570-9555-79479dbe81d1`. Status: **offen; Fortsetzung in neuer
Sitzung vom Nutzer gewünscht**. Kein weiterer Gerätetest nach diesem Wunsch.

## Auftrag und letzter belegter Stand

Gemeinsamer Test der Reedkontaktmatrix nach dem LED-/Wake-/NFC-Muster.
App und Matrix-Prüffirmware aus Quellstand
`c613acebfb07805d1c363b52a325721f2b6f8077` sind verifiziert installiert.
**Diese Matrix-Testversion bleibt der zuletzt belegte Gerätestand; es wurde
kein Rückweg ausgeführt.** Der Test ist nicht bestanden und keine physische
Position hat eine bestätigte Koordinate.

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

## Zu klären, bevor die Kontaktserie weitergeht

1. An einem Reed-/Diodenzweig die **beiden konkreten Expander-Klemmen** und
   die Ring-/Kathodenseite feststellen. Letzte offene Nutzerfrage: Ring-Ende
   an `0x20 B1/A0–A6/B5` oder an `0x21 B7–B0` / `0x20 B0`? Der Nutzer hat
   genau diesen Abgleich auf die neue Sitzung verschoben.
2. Ursprünglicher Scanner: Spalte LOW, Zeile mit Pull-up lesen. Testscanner:
   Zeile LOW, Spalte mit Pull-up lesen. Der Richtungswechsel ist eine
   **vermutete Ursache**, keine bewiesene Beschädigung oder Diodenverpolung.
   Alte Kommentare verwechseln die Begriffe; konkrete Anschlussnamen verwenden.
3. Die B7-Ausgangsbegründung des Testscanners ist unvollständig: dessen
   Freigabe setzt IODIR wieder auf Eingang. Siehe Herstellerbeleg im
   Gerätebericht. Keine behauptete Vollmatrix-Gültigkeit aus einer Teilprobe.
4. Ein früherer Wechsel von Spalte I (`0x20 A7` nach `0x20 B5`, Commit
   `61ef6c6` vom 19.02.2025) ist gegebenenfalls mit dem tatsächlich erfolgreichen
   Altstand abzugleichen. Er erklärt allein nicht jede leere Position.

Nach geklärter Richtung eine begrenzte Korrektur beziehungsweise Rohprobe
vorbereiten und prüfen; dann denselben konkreten Kontakt bewusst halten,
lösen und wiederholen. Erst danach reale Positionen zuordnen und weitere
Kontakte/Mehrfachkontakte testen. Die 81 Rasterpositionen sind logische
Kandidaten, keine bestätigte Anzahl realer Reedkontakte.

## Arbeitsverzeichnis und lokale Belege

Repository: `https://github.com/Sven401/Larp-Escape.git`, eigener Branch
`codex/matrix-test-preparation`, Draft [PR #49](https://github.com/Sven401/Larp-Escape/pull/49),
Ziel `plan/projektuebergabe-kuratierter-stand`. Vor Fortsetzung Status, Branch,
HEAD und Remote im **eigenen Klon** prüfen. Die Übergabefassung ergänzt nur
Dokumentation; der auf den Geräten geprüfte Quellstand bleibt `c613ace`.

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
- Nutzerkopie: `outputs/Tastenmatrix-Uebergabe.txt`.

ADB-Handy und COM-Port vor Wiederaufnahme frisch identifizieren; keine alte
Portannahme ungeprüft übernehmen. Vor weiterem Firmwarewechsel erneut
chipgebundenen Rückweg und aktuellen Gerätestand prüfen. Der private
4-MiB-Abzug vor Matrixinstallation ist am Chip verifiziert; Anwendung,
Partitionstabelle und Schlüssel wurden nach Installation verifiziert.
Eine Wiederherstellung oder ein Abschalten der Box wurde zum Sitzungsende
**nicht** ausgeführt und darf nicht nachträglich behauptet werden.
