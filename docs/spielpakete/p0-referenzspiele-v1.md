# P0 v1: zwei unterschiedliche Mehrbox-Referenzspiele

**Status: technische Referenzentwürfe, keine Geräte-/Nutzerabnahme.**
Basis/Version wie im [Vertrag](p0-vertrag-v1.md). Die Namen sind Beschreibungen,
keine Laufzeit-Schalter. Keine Auswahl eines dritten Spiels vor dem Freeze.
[Prüfvektoren](fixtures/p0-v1.json) sind absichtlich dekodierte Projektionen,
keine installierbaren vollständigen Pakete oder echten D0-Nachrichten.

## A. Energiebrücken: unabhängige Erfolge zusammenführen

Drei logische Stationen Quelle, Linse und Tor; mindestens zwei reale Boxen
für den späteren Lauf, vorzugsweise eine je Station. Quelle zählt drei gültige
lokale Impulse, Linse zwei. Jede erzeugt einmal ihr task_solved-Subject.
Tor erfüllt die AND-Regel aus beiden aktuellen belegten Erfolgen und erzeugt
ein eigenes Subject sowie einen Lichtintent. Es braucht keine globale Uhr
oder Reihenfolge zwischen Quelle und Linse. Bei zwei Boxen dürfen Quelle und
Linse getrennte logische Eingangsgruppen auf derselben Box sein; der reale
Mehrboxbeleg muss weiterhin eine entfernte Tatsache verwenden.

| Station / Regel | Guard und vollständiger Plan |
| --- | --- |
| Quelle q_count, each | Gültiger lokaler Impuls; count<2: count+1 |
| Quelle q_finish, once | Gültiger lokaler Impuls; count==2: count=3, emit source_solved, Lichtintent |
| Linse l_count/l_finish | Dasselbe generische threshold-Modul mit threshold=2; eigene IDs und Ports |
| Tor gate, once | fact(source_solved) AND fact(lens_solved): emit gate_solved, Lichtintent (open wird aus gate_solved abgeleitet) |

Bei threshold=1 ist die count-Regel niemals passend und finish arbeitet aus
count=0. Ungültige Beobachtung ist unknown und erhöht nichts. Nach Erfolg
weitere Impulse ohne neue Wirkung. Während Partition arbeiten die beiden
Quellen unabhängig weiter. Tor wartet ausschließlich auf fehlende Fakten;
local_continue gilt für seine übrige Bedienung. Keine Ersatzfreigabe in
Referenz A. Streamlücken werden gemäß D0 behandelt; nur die ausdrücklich
monotone gate-Regel darf bei vorhandenen konkreten Belegen unabhängig von
anderen Lücken arbeiten. Der neue gate-Fakt referenziert beide FactRefs.

Erwarteter Trace: Quelle Impulse 1/2/3 → count 1/2/3, genau ein source_solved;
Linse 1/2 → 1/2, genau ein lens_solved. Tor empfängt Linse vor Quelle oder
Quelle vor Linse → erster Fakt wartet, zweiter übernimmt gate_solved und einen
Intent. Replay beider Facts und Neustart nach Commit erzeugen keine neue
Freigabe. Alter Run/falsche Bindung/unberechtigter Ursprung erfüllen nichts.

## B. Resonanzfolge: lokale Reihenfolge, Zeitfenster und Alternative

Station Echo verlangt zwei verschiedene lokale Eingaben in der Reihenfolge
1 dann 2 innerhalb 5000 ms aktiver Spielzeit. Falsche Eingabe im aktiven
Fenster setzt die lokale Folge zurück; der fällige Timer tut dasselbe.
Nach Erfolg entsteht einmal echo_solved. Station Archiv kann lokal entweder
drei gültige Impulse sammeln oder eine ausdrücklich konfigurierte zweite
lokale int32-Eingangsquelle mit gültigem Wert 1 verwenden. Beide Wege erzeugen dieselbe
fachliche archive_solved-Freigabe. Finale benötigt beide Erfolge wie in A.

| Echo-Regel | Snapshotguard → Plan |
| --- | --- |
| e_begin, each | local value=1 AND phase=0 → phase=1, arm window |
| e_win, once | local value=2 AND phase=1 → phase=2, cancel window, emit echo_solved, Lichtintent |
| e_wrong, each | local value!=2 AND phase=1 → phase=0, cancel window |
| e_timeout, each | timer window AND phase=1 → phase=0 |

Weil alle Guards denselben Vorzustand lesen, führt e_begin nicht im selben
Eingang zu e_wrong. Ein zweites 1 während phase=1 ist falsch, kein Timerrefresh.
Bei genau 5000 ms ist der Timer zuerst; eine dann eintreffende 2 löst nicht.
Pause bei 2000 ms und Resume später erhält 3000 ms Restzeit. Nach Crash keine
automatische Fortsetzung; berechtigte Recoveryentscheidung wie im Hauptvertrag.

Archiv threshold und alternative lokale Quelle verwenden **eine** once-Regel
archive_finish mit Guard `(count==2 AND impulse) OR alternate_true`; bei
alternativer Quelle wird der Zähler nicht künstlich auf 3 gesetzt. Der
Trigger benennt beide int32_sample-Ports `pulse` und `alternate`; `port_is`
unterscheidet sie generisch. Impuls gilt bei pulse/value=1, Alternative bei
alternate/value=1. Keine Datenumkodierung oder neue Adaptersonderlogik:
count-Regel verlangt port_is(pulse), Finish all(port_is(pulse),count==2)
oder port_is(alternate), jeweils mit gültigem value=1.
Gleichzeitig erfasste Eingänge werden serialisiert; der zuerst dauerhaft
übernommene Erfolg gewinnt denselben Latch. Späterer Alternativ-/Originalweg
erzeugt keinen zweiten archive_solved-Fakt und keinen zweiten Intent.

Beispieltrace: Echo 1 bei t=0, 2 bei t=5000 → Timeout, kein Erfolg;
1 bei t=6000, 2 bei t=8000 → Erfolg einmal. Archiv count=1, dann Alternative
→ Erfolg, count bleibt 1; nachgelieferte Impulse ändern den Erfolg nicht.
Finale kann offline warten und bei Wiederkontakt beide Erfolge einmal nutzen.
Dies ist lokale zeit-/reihenfolgeabhängige Logik mit alternativem Weg und
anschließender Kooperation, fachlich verschieden von A's unabhängigen Zählern.

## Wiederverwendbare Module als Daten

| Modul / Ports | Parameter / interne Struktur |
| --- | --- |
| threshold 1.0.0: pulse int32_sample, done solved, feedback light | limit int32 1..1000 count, default 3; count initial 0; each increment bei value=1 und count<limit-1; once finish bei value=1 und count==limit-1: count=limit, done, feedback. Beide Regeln lesen denselben Snapshot. |
| ordered_window 1.0.0: sequence int32_sample, done solved, feedback light | first/second int32 1..255, verschieden (generische constraints); windowMs 1..86400000 ms, default 5000. phase 0/1/2, Timer window; vier Echo-Regeln oben. first/second sind Literalparameter in Guards. |
| all_solved 1.0.0: left/right solved, done solved, feedback light | Keine Parameter; once-Regel guard all(left,right), emit done und feedback. Beide portbezogenen Belege bleiben Abhängigkeiten des erzeugten Facts. |

Archiv ist eine bearbeitbare Zusammensetzung aus denselben Wert-/Bool-/Aktionen,
kein natives neues Modul. Das Beispiel zeigt ausdrücklich den Bedarf an
Bearbeitung vorhandener Bausteine. Module haben Label/Einheit/Beschreibung
und vollständige typisierte Portzuordnung nach dem [Formatvertrag](p0-formate-v1.md).
Bedingung first!=second und count-Grenzen sind Validatorconstraints; unmögliche
Parameterkombination wird vor Expansion abgewiesen.

## Vorbereitete spätere Fehler-/Abnahmematrix

| Fall | Erwartung / Nachweisort |
| --- | --- |
| Not unknown, Sensorfehler nach gesundem Start | Keine positive Aktion; Semantikmodell jetzt, echter Adapter in F5a |
| Empfangsreihenfolge und Duplikate | A gleicher Endstand; D0/D1-Modell jetzt, echte Verifikation/Flash/Funk später |
| Mehrregelkonflikt/Overflow/volle Outbox | Gesamter Plan unverändert, kein Intent; Modellgrenze offen benennen |
| Timerdeadline/Pause/Crash | Lokale Reihenfolge und Recovery sichtbar; echte Uhr/Checkpoints später |
| Unbekannter op/Modulversion, Modulzyklus, zu große Expansion | Vor Aktivierung abweisen; vollständiger Expandierer P1a |
| Unbestätigtes Audioinventar/falsche Portfähigkeit | Installation gesperrt; Geräte-/Medienvorbereitung P2 |
| Paketwechsel gleicher ruleId / neuer Run | Latch erhalten / Sequenz nicht zurücksetzen; D1-B/P2 |
| Effekt attempted, Crash, Wiederkontakt | unknown, keine Wiederholung; D1 und spätere reale Abnahme |

P3 benötigt echte getrennte Boxen, zwei berechtigte BLE-Handys, Ausfall und
Wiederkehr von Funk, lokale Fortsetzung, Neustart, Replay und Ressourcenmessung.
Weder die Traces noch der Modelllauf erfüllen SP-T01–SP-T07. Für die dritte
Mechanik bleiben Sprache und Werkzeuge erst nach bestandenem P3 einzufrieren.
