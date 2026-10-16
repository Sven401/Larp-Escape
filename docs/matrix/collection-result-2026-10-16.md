# Ergebnis des Tastenmatrix-Sammeldurchgangs

Stand: **16. Oktober 2026, Europe/Berlin**. Status: **Lauf abgeschlossen;
68 Koordinaten erfasst, vier Problemstellen vorhanden, neun Felder unbestückt.**

Lauf `5805f942-5e90-4948-9c2b-5a256f5d0307`. Aufgezeichnet mit APK aus Quellcommit
`20ac696f75630919949edceec975d593d46a4218`. Aus der vollständigen privaten
Laufdatei gelesen, über die App nach `REVIEW` abgeschlossen und vor jeder
weiteren Appänderung gesichert. Kein neuer Sammellauf gestartet.

## Ergebnis und Bestückung

**68 verschiedene Koordinaten**, **110 registrierte Betätigungen**.
Null protokollierte Unterbrechungen und null übersprungene Mehrfachkontakte.
Die Zähler zählen beobachtete Freigabe-/Kontaktfolgen, keine unabhängig
gemessene Zahl mechanischer Betätigungen oder Aussage zur Entprellung.

**✓** im Lauf erkannt · **!** vorhanden, Kontakt-/Kabelproblem laut Nutzer ·
**×** ohne Kontakt, unbestückt laut Nutzer. Alle 81 Koordinaten bleiben erhalten.

| Zeile | A | B | C | D | E | F | G | H | I |
|---|---|---|---|---|---|---|---|---|---|
| 1 | ✓ | × | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 2 | × | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 3 | × | ✓ | ! | ✓ | ✓ | ✓ | ! | ! | ✓ |
| 4 | ✓ | × | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 5 | ✓ | ✓ | ✓ | ✓ | × | ✓ | ✓ | ✓ | ✓ |
| 6 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 7 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 8 | ! | ✓ | × | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| 9 | ✓ | ✓ | ✓ | ✓ | × | ✓ | × | ✓ | × |

**Problemstellen, bleiben offen und werden nicht als unbestückt deaktiviert:**
C3, G3, H3, A8.

**Unbestückt, für die Deaktivierung ausdrücklich bestätigt:**
B1, A2, A3, B4, E5, C8, E9, G9, I9.

**Erkannte Koordinaten:**
A1, C1, D1, E1, F1, G1, H1, I1, B2, C2, D2, E2, F2, G2, H2, I2, B3, D3, E3, F3, I3, A4, C4, D4, E4, F4, G4, H4, I4, A5, B5, C5, D5, F5, G5, H5, I5, A6, B6, C6, D6, E6, F6, G6, H6, I6, A7, B7, C7, D7, E7, F7, G7, H7, I7, B8, D8, E8, F8, G8, H8, I8, A9, B9, C9, D9, F9, H9.

## Quelle und Grenzen

Die vollständige Trefferliste und Zeitpunkte stammen aus dem gespeicherten
Sammellauf. Nutzerbestätigung in dieser Sitzung: alle vorhandenen Kontakte
durchprobiert; C3, A8, G3 und H3 haben ein Kabel- oder Kontaktproblem; die
übrigen offenen Koordinaten besitzen keine Kontakte. Die Unterteilung
zwischen Störung und fehlender Bestückung ist damit **nutzerbestätigt**.
Die konkrete elektrische Fehlerursache wurde nicht unabhängig gemessen.
Physische Raumpositionen werden aus Koordinaten nicht hergeleitet.

[Bereinigte Messdaten](evidence/collection-result-2026-10-16.json) enthalten
Treffer, Zähler und Zeitpunkte; [CSV](evidence/collection-result-2026-10-16.csv)
enthält alle 81 Felder. Die Rohdatei und alte APKs bleiben privat unter
`.local/matrix-collection/private/completed/`.

## Appstand und nächster Schritt

Die gezielte App-Ergänzung erlaubt, vorhandene stumme Kontakte als
Problemstellen auszunehmen. Nur die verbleibenden unbestückten Felder
werden deaktiviert; Problemstellen erhalten ein **!**. Alte Laufdateien
ohne dieses optionale Feld werden mit unveränderten Treffern geladen.
**Frisch geprüft und auf dem Handy übernommen:**

- 68 Protokoll- und 62 App-Tests bestanden, null Fehler/Skips. Drei neue
  Fälle prüfen die Teilbestätigung mit offenen Problemstellen, ungültige
  Ausnahmelisten und das verlustfreie Laden älterer Laufdateien.
- Lint ohne Fehler, drei bestehende Warnungen und bestehender Hinweis.
  Debug-APK gebaut, mit passender Signatur und Datenerhalt installiert und
  vollständig zurückgelesen. SHA-256:
  `c13841e8a2047425df796e39bad8b01df2dda66ad17fae5b0912c7806bca2bd6`.
- Eingefrorene Laufdatei war nach dem Appupdate bytegleich. Boxzugang und
  Wake-Anordnung unverändert. Keine Firmwareänderung oder neuer Sammellauf.
- Nutzerangaben über die reguläre Appauswertung eingetragen und gespeichert.
  Danach private Appdatei erneut gelesen: Phase `CONFIRMED`, exakt neun
  deaktivierte und vier offene Problemkoordinaten. Alle 68 Treffer samt
  Zählern, Zeitpunkten und Laufkennung unverändert. Die App bestätigt
  „9 unbestückt deaktiviert · 4 Problemstellen offen“. Ein frischer, eindeutig
  zugeordneter UI-Abzug nach Prozessneustart bestätigt denselben Ergebnisstand.

Der Maschinenbeleg führt die Prüfsummen der tatsächlich getesteten Quellen.

Nächster physischer Schritt: Kabel-/Kontaktprüfung gezielt für C3, G3, H3
und A8. Danach bestehenden Lauf zur Nachprüfung öffnen, in Ruhe fortsetzen
und diese Kontakte erneut auslösen. Die neun unbestückten Felder und die
bisherigen 68 Treffer erhalten; keine vollständige Wiederholung nötig.
