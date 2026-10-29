# #93 P0: Vertragsausarbeitung und Prüfstand

Stand 29.10.2026; Auftrag: P0 ausarbeiten und bis zum fertigen PR führen.
Ausgangsbasis nach Fetch `191d08164614bd6472db241336d263da77691f49`, eigener
Branch `codex/spielpakete-p0`, Ziel `plan/projektuebergabe-kuratierter-stand`.
Fremde lokale Dateien im ursprünglichen Checkout unangetastet.

## Auftrag, Verantwortung und Ergebnis

Führend: Maintainer als Plattform-/Systemintegrationsverantwortlicher.
Anderer Bearbeiter: unabhängiger Qualitäts-/Plattformreview (`p0_review`).
Geprüft werden neue Verträge, Ausdrucks-/Plansemantik, Referenzspiele und
Nachweisgrenzen. Varianten: künftiger allgemeiner IR-Kern und GM-Vorschau;
bestehende C++-/Android-/Laborproduktquellen unverändert. P0 ändert keine
Zugangs-/Kryptografieimplementierung oder bestehenden Vertrauensgrenzen.
D0-/D1-Sicherheitsregeln sind ausdrücklich Voraussetzungen, keine Modellleistung.

Ergebnis: [technischer Sprach-/Paketvorschlag](../docs/spielpakete/p0-vertrag-v1.md),
[Formate, Modulparameter und D0-kompatibles Deployment](../docs/spielpakete/p0-formate-v1.md),
[zwei unterschiedliche Mehrbox-Referenzen](../docs/spielpakete/p0-referenzspiele-v1.md).
Der Vorschlag setzt SP-01 um, erfindet keinen dritten Abnahmefall und bewirbt
keine neue Laufzeitfähigkeit. Technische Übernahme ist im PR reviewbar;
Merge und Produktfreigabe bleiben beim Nutzer.

## Quellenabgleich und bestätigte Anschlussgrenzen

Status **am Code/Vertrag bestätigt**, 29.10.2026, Basis `191d081`:

- `include/GameConfiguration/Profile.h`, `src/GameConfiguration/ProfileValidation.cpp`
  sowie `docs/configuration/profile-model.md`/`execution.md`: bestehendes
  rollen-/kristallbezogenes Profil und Graphsemantik sind kein allgemeiner
  atomarer Mehrregelplaner. Neue Sprache hat eigene Format-/Fähigkeitsversion.
- `docs/veranstaltung/d0-vertrag-v1.md` und `d0-sicherheit-migration-v1.md`:
  v1-Fakten sind positive task_solved/local_override, kein Remote-Integerbus.
  D0 packageDigest bindet ein event/run-/bindungsbezogenes Manifest; teilbarer
  Inhalt benötigt separaten contentDigest. Keine Änderung der D0-Bytes.
- `docs/veranstaltung/d1-persistenz-v1.md` und `scripts/d1_journal_model.py`:
  vollständiger Mehrregelplan und Latcherhalt erforderlich; lokale wiederholbare
  Schritte/Timer brauchen stabile Vorgangsbelege und D1-B-Anschluss.
- Offene PRs #100/#101/#103 enthalten Zugang/Migrationsvorarbeit. P0 übernimmt
  deren Code nicht und wählt keinen neuen SDK-Pin. #102 führt eine getrennte
  allgemeine Maintainer-Übergabe; hier nur aufgabeneigener Bericht.

Projektgebundene Befunde verbleiben in diesen Fachverträgen und diesem Bericht;
keine Übernahme in globale persönliche Skills oder Werkbank.

## Prüfungen und Review

Die lokale Semantikprojektion ist **kein vollständiger Interpreter**. Feste
JSON-Traces enthalten unabhängig festgehaltene erwartete Zwischenstände für
beide Referenzen, zwei Empfangsreihenfolgen und die exakte Timergrenze. Tests
prüfen zusätzlich atomaren Zielerhalt bei Konflikt, Overflow, vollem Intentbereich
und Planbudget, unknown/NOT, Snapshotsemantik, Vorgangsdeduplizierung, Latches
und dauerhafte Timerkonsumgrenze als RAM-Modell. Kryptografie, persistente
Transaktionen, Modulauflösung, alle Formatgrenzen und physische Wirkung bleiben
separate P1-/Gerätenachweise. [Befehle](../docs/quality-checks.md#p0-regelvertrag-ohne-hardware-prüfen).

Lokal erfolgreich: 14 P0-Unittests einschließlich sechs Referenztraces;
111 D0-Vertragsfälle; 45 D1-Tests einschließlich 36 echter Prozessabbrüche
und Wiederaufnahme am Rechner. P0/D0 laufen mit Python-Standardbibliothek;
D1 verwendet den bestehenden Hostjournalpfad. Keine neue Bibliothek nötig.
Editierbare Quellen sind `scripts/p0_rules_model.py`,
`scripts/tests/test_p0_rules_model.py` und die referenzierte JSON-Fixture.
Herkunft: neuer aufgabengebundener Semantikausschnitt aus diesem Vertrag;
keine Promotion zum allgemeinen Werkzeug oder produktiven Interpreter.

Die unabhängige Gegenprüfung las Verträge/Modell und führte die 14 P0-Tests
selbst aus. Korrigierte Befunde: positive/exklusive externe Freigaben statt
reihenfolgeabhängiger Writes; Timerconsume erst beim Eingangscommit;
kollisionsfreie Eingangskennungen; generische Parameterconstraints und
Ressourcenslots; Multiporttrigger statt verstecktem Eingangsadapter;
vollständige D0-CBOR-Bindung; eigener später lokaler Fact-Eingang nach emit.
Die lokale emit-Rückkopplungsqueue und Pause-/Crashrecovery sind ausdrücklich
nicht Teil des Rechnermodells. Im zuletzt geprüften Umfang verbleibt nach
Korrektur kein bekannter P1/P2; Review ist keine externe Zertifizierung.
Neue PR-CI und deren Commitbezug werden im PR ausgewiesen.

## Noch erforderliche Arbeit

P1a implementiert vollständige Parser-/Typ-/Modul-/Konfluenz-/Budgetvalidierung
und Planer; unabhängige Bytevektoren und Box-/Vorschauparität einschließen.
P1b ergänzt D1-B und F5a mit realen Besitzern, Timercheckpoint/Recovery und
Speicher-/Zeitmessungen. P1c folgt mit eigenem UI/UX-Auftrag für den vollständigen
Autorenweg. P2 bestätigt autorisierte Installation und tatsächliche Ressourcen.
P3 erprobt beide Spiele auf echten kooperierenden Boxen; P4 bleibt Freeze und
erst danach neu entworfenes Drittspiel. #93 und alle SP-T01–SP-T07 bleiben offen.
Kein Gerätezugriff, Upload, Merge, Release oder Hintergrundauftrag in P0.
