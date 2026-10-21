# VA-01 und SP-01: Verträge, Produktplan und Abnahme

## 0. Produktpriorität ab 21.10.2026

**Status: bestätigt, verbindliches Architektur-Soll; Umsetzung und Abnahme offen.**
[SP-01](spielpakete-architektur.md) ist der neue Produktmaßstab: eigene Spiele
vollständig in Larp GM erstellen und als Spiel-/Regelpakete nachladen;
kooperierende Boxen bereits in der ersten nutzbaren Produktversion;
nach zwei Referenzspielen ein drittes, fachlich anderes Spiel bei eingefrorener
App und Firmware durch Pakete/Inhalte ergänzen. Einzelbox- und Baugruppenläufe
sind Zwischenbelege. Die neue Priorität ersetzt frühere Reihenfolgen, soweit
diese allgemeine Autorenschaft oder Mehrboxbetrieb auf spätere Produktversionen
verschieben. Bestehende Kriterien und datierte Nachweise bleiben erhalten.

Quelle ist der ausdrückliche Nutzerauftrag vom 21.10.2026 in SP-01.
Ausgelesene Basis: `e61d7642d035ccdcf8eca689d6c50a82bcf1560b`, nach frischem
Fetch; Quell-/Dokument- und GitHub-Backlogprüfung, keine neue Laufzeitprüfung.
[Produktplan #25](https://github.com/Sven401/Larp-Escape/issues/25) führt die
operative Priorität; [Arbeitspaket #93](https://github.com/Sven401/Larp-Escape/issues/93)
bündelt neue Paket-/Laufzeit-/Autorenarbeit und das Drittspiel-Tor.
Diese Matrix hält Vertragszuordnung und Prüftore fest.

| Folge | Arbeit und überprüfbares Ergebnis | Anschluss / Abhängigkeit |
| --- | --- | --- |
| P0 | Zwei unterschiedliche Referenzspiele und kleinsten allgemeinen Paket-/Sprachvertrag festlegen: Ereignisse, Zustände, Werte, Timer, Aktionen, zusammengesetzte Module und deren Bearbeitungsbeschreibung. Bindungen, Ressourcen, Rechte, Versionen und Budgets definieren. | SP-01, bestehender Profilkern und D0; keine neue Lua-/Wasm-/SDK- oder Wireformatentscheidung durch diesen Plan |
| P1, parallel | Allgemeine begrenzte Laufzeit und reale Gerätebesitzer anschließen; GM-Editor, Bausteine, Paketprüfung und semantisch passende Vorschau gegen dieselben Referenzfälle entwickeln. D1/D2 und allgemeiner Zugang werden parallel angeschlossen. | #31 F5a, #27/#28, #60/#61; neue Autorenarbeit bleibt getrennt von #36 |
| P2 | Paket aus GM je Zielbox prüfen, übertragen, bewusst aktivieren und bestätigten Speicher-/Laufstand lesen; Ressourcen und Zuordnungen nachvollziehbar vorbereiten. | #30 nach nutzbarem F5a-Pfad; #37/#33/#34, Fähigkeiten- und Rechteprüfung. Kein Warten auf eine fertige Zentrale |
| P3 | Beide Referenzspiele auf kooperierenden realen Boxen ausführen; Offline-, Neustart-, Wiederholungs-, Durchlaufwechsel- und Lastfälle prüfen. | #31 F5b, #60/#61 und zentralenunabhängiger Teil #63; relevante Hardware-/Baugruppenrestprüfungen |
| P4, Release-Tor | App, Firmware und beteiligte Ausführungswerkzeuge einfrieren; danach drittes Spiel neu entwerfen, in GM erstellen, prüfen, verteilen und im Mehrboxbetrieb spielen. Nur Pakete, Ressourcen und Bindungen ändern; unveränderte Binärhashes und neue Mechanik belegen. | SP-T-Kriterien aus SP-01; getrennte Software-/Geräte-/Nutzerbelege; Freigabe bleibt beim Nutzer |
| Optionaler Ausbau | Zeitweise erreichbare Zentrale und ihr eigener Abnahmeanteil. | #62 und zentralenspezifischer Teil #63/VA-T08; keine Voraussetzung des vorbereiteten lokalen Mehrboxspiels |

**Konkreter nächster Schritt:** P0 als versionierten Paket-/Sprachvertrag mit
zwei unterschiedlichen Mehrbox-Referenzfällen und offenen Ressourcenbudgets
ausarbeiten. Den dritten Abnahmefall erst nach dem Software-Freeze auswählen.
Die ersten beiden Fälle müssen die allgemeinen Fähigkeiten erproben; keine
versteckte Vorimplementierung des dritten Spiels im Laufzeitcode.

**Integrationsfolge ohne Kreis:** F5a kann lokal bereitgestellte Referenzpakete
über denselben Validator und dieselbe Laufzeit ausführen. Ein fertiger
BLE-Pakettransfer ist dafür noch nicht nötig. Der reale Übernahmepfad folgt in
#30; danach #31 F5b. Vorbereitung von Editor/Vorschau, D1/D2 und Zugang läuft
parallel. Teilabschlüsse schließen weder #31 noch #63 pauschal ab.

D0 #59 bleibt als Spezifikation abgeschlossen; D0-G1–G5 bleiben offen.
D1-A in PR #73 ist weiterhin ein eigener offener Review, kein eingebauter
Firmware-Speicher. #36 bleibt Erhalt/Import alter Szenariodaten. #29/#35/#39/#54
und übrige Restprüfungen bleiben erhalten; ihre Reihenfolge richtet sich nach
den berührten Fähigkeiten der Referenzspiele. #62 bleibt im Backlog. Ohne
Zentralenimplementierung bleiben dessen D4-/VA-T08-Kriterien ausdrücklich offen;
der erste SP-01-Produktabschluss ist kein Abschluss aller VA-01-Arbeiten.

## Herkunft der VA-01-Matrix

**Status: abgeleitete verbindliche Planungsanforderungen; technische Umsetzung offen.**
Stand: **18. Oktober 2026**. Quelle und maßgeblicher Beschluss:
[VA-01](veranstaltungsarchitektur.md). Codebasis `d61f77f`; geprüft durch Lesen
der unten benannten Verträge, `IdentityAccess.h`, `GameControl.h`, `src/main.cpp`,
`ICommunicator.h` und `EspNowCommunicator.h`. Backlog #25/#27–#37/#39/#54 und
Zugangskonzept PR #58 am selben Tag gelesen. Keine Laufzeitprüfung.

## 1. Vertragsmatrix

Für D0 ist jetzt der [versionierte Vertrag](veranstaltung/d0-vertrag-v1.md)
mit [Format-/Sicherheits-/Migrationsprofil](veranstaltung/d0-sicherheit-migration-v1.md)
und [negativen Modellfixtures](veranstaltung/fixtures/d0-v1.json) über
[PR #72](https://github.com/Sven401/Larp-Escape/pull/72) integriert, Ausgangsbasis
`5f90d10`, fachlich korrigierter Inhaltsstand `a9481e0`, 111 Modellfälle.
D0 #59 ist als Spezifikationspaket abgeschlossen. Implementierungs-/Geräteabnahme
bleiben getrennt. D0-G1–G5 weisen die Nachweise D1/D2/#28/#30/#31/D3/D4 zu;
die offenen VA-T01–VA-T12 werden dadurch nicht als bestanden markiert.

| Bestehender Vertrag / Einstieg | Beibehalten | Erforderliche Anpassung / neues Anschlussdokument | Paket |
| --- | --- | --- | --- |
| [Identität/Zugang](companion/identity-access.md), [Register](companion/peer-registry-v1.md), [Geräteidentität](companion/device-identity-store-v1.md) | Stabile Boxidentität, ausdrückliche lokale Rechte; zunächst eine aktive Steuerverbindung je Box | Veranstaltung, Station, Zuordnungsgeneration und Durchlauf trennen; BLE-Übergabe; Rechtekapazität von Gleichzeitigkeit trennen. Boxereignisvertrauen ist eigener Vertrag, kein automatisches GM-Recht. PR #58 berücksichtigen. | #27/#28, D0 |
| [Peer-/SDK-Sicherheit](companion/peer-security-sdk-v1.md), [Nachweis](companion/peer-proof-v1.md), [Grant](companion/peer-proof-grant-v1.md) | Ursprüngliche Verbindungs-/Credentialbindung und Schutz vor verspäteten Belegen | Offline-Prüfung der Zielbox; Nachweis anderer Boxen/weitergeleiteter Ereignisse separat definieren. Entzug wirkt erst mit Kenntnis an Zielbox. Kein zentrales Online-Prüferfordernis. | #27/#28, D0 |
| [Configured v2](companion/configured-v2.md), [Befehlscodec](companion/command-codec-v2.md), [Receipts](companion/command-receipts-v1.md) | Boot/Lease/Request, erwartete Revisionen und lokale geordnete Ausführung | BLE-Befehlsfolge bleibt sitzungslokal; dauerhaftes Ereignis hat eigene Kennung. Kein Fernereignis als alte BLE-Anfrage wiedergeben. Versions-/Fähigkeitsvertrag für Veranstaltungskontext, ohne v2-Fixtures still zu ändern. | #29/#37, D0/D1 |
| [Status v1](companion/status-v1.md), [Statusrouting](companion/status-routing.md), [Antworten](companion/reply-routing.md) | Kopierte Snapshots, passende aktuelle Zielverbindung | Alten WLAN-Appentwurf durch BLE-Entscheidung einordnen; lokale und vermittelte Beobachtung samt Quelle/Alter trennen. Zentrale ist nur zeitweise aktuell; Reply bleibt am ursprünglichen Zielauftrag. | #33/#34/#37, D3 |
| [Baugruppen-Wire v2](led/ble-wire-v2.md) | Bestehende Bytes, HMAC-/Sitzungsbindung, Fristen, Fähigkeiten und manuelle Stopregeln | Boxauswahl/Anmeldung immer eindeutig. BLE-Abbruch manueller Ausgabe vom autonomen Spielbetrieb trennen; kein Umdeuten als veranstaltungsweites Protokoll. | #29/#33/#34/#37, D4 |
| [Laufzeit](runtime.md), `GamePorts/ICommunicator.h`, `EspNowCommunicator`, `src/main.cpp` | Ein Besitzer je Ressource, Callback kopiert nur, begrenzte Queues | Einzelpeer-/Sendepfad zu beidseitigen Transportadaptern ergänzen. Ereigniskern, Speicherworker und Anwendung entkoppeln; Empfang ist keine direkte Hardwareaktion. Transportannahme/MAC-Erfolg/fachliche Verarbeitung unterscheiden. | #31 F5a, D1/D2 |
| [Profilmodell](configuration/profile-model.md), [Ausführung](configuration/execution.md), [Session](configuration/session.md) | Boxlokaler Graph, Komponenten-IDs, Geräteport und Revisionen | Ereignisbedingungen/-ausgaben und lokale Ersatzregeln versioniert spezifizieren; Quellstation/Durchlauf/Zuordnung prüfen. Veraltete externe Tatsachen dürfen keinen neuen Durchlauf freischalten. | #31 F5a, D0/D1 |
| [Profilanwendung](configuration/profile-application.md), [Aktivierung](configuration/activation.md), [Profiltransfer](companion/profile-source-v1.md) und [Kontrollen](companion/profile-controls-v1.md) | Validierter Entwurf, bewusste Aktivierung, lokale Bestätigung, kein automatischer Spielstart | Veranstaltungspaket mit Inhaltsbindung, lokalen Ressourcen und Kompatibilität; unabhängig bestätigte Boxaktivierung. Keine atomare Aktivierung aller Boxen behaupten. Unvollständiger Download bleibt Entwurf. | #30, D0/D3 |
| [Profiljournal](configuration/profile-journal.md), [Fehler](configuration/journal-faults.md) | Vorhandener Profil-Speichervertrag und dessen Aussagegrenzen | Eigenes Fortschritts-/Ereignisjournal einschließlich Empfangslücken, Outbox, Wiederholungserkennung, Wiederanlauf, Kompaktierung und Speichererschöpfung. Profilrollback darf alte Freigaben nicht neu auslösen. | #30/#31, D1 |
| [Android-Einstieg](../companion/android/README.md) | Vordergrund-BLE, Sitzungs-/Callbackbindung, bewusste Bedienung | Mehrere gespeicherte Boxzuordnungen; getrennte Zustände pro Box und expliziter Zielwechsel. Erster Ausbau verlangt keine gleichzeitigen BLE-Verbindungen eines Handys zu allen Boxen. | #33/#34/#37, D3/D4 |
| [NFC-Testformat](../companion/contract/nfc-v1/README.md), [PN532-Paketvertrag](nfc-packet-contract.md) | UID-/Payloadbindung, frisches Rücklesen, unklarer Schreibausgang ohne blindes Wiederholen | Späteren Spielobjektvertrag veranstaltungs-/durchlaufbezogen planen; Konfliktregel bei Bearbeitung durch verschiedene Stationen. NFC ist kein automatisch gewählter Nachrichtenkurier. Bestehendes Testformat bleibt unverändert. | #35, D0 |
| [Lokale Szenarien](../companion/android/README.md), #36 | Datenerhalt und Import lokaler Entwürfe | Lokaler Entwurf ist kein veröffentlichtes Veranstaltungspaket. Freigabe/Verteilung in D3; #36 bleibt Datenübernahme ohne neuen Mehrbenutzereditor. | #36, D3 |
| [Prüfgrundlage](quality-checks.md), [Hardwaregrenzen](../handoff/BAUGRUPPEN-NACHWEISE.md) | Historische Belege und Software-/Geräteabnahme getrennt | Partition, Neustart, Duplikate, Lücken, Weiterleitung, Speichergrenzen und Funkkoexistenz zusätzlich prüfen. Vorhandene Teilbelege nicht zurücksetzen. | #31 F5b/#39, D4 |

Die Matrix ist die kanonische Liste des Anpassungsbedarfs. Verweise in bestehenden
Verträgen kennzeichnen das neue Soll; ihre implementierten Versionen bleiben
lesbar. Binär-/JSON-Schemata und Code werden erst in den jeweiligen Folgepaketen
geändert, mit expliziten Fähigkeiten, Versionsgrenzen und Migration.

## 2. Bestehendes Backlog anpassen

| Arbeitspaket | Verbindliche Ergänzung |
| --- | --- |
| #25 Produktplan | SP-01 priorisiert nachladbare Pakete, allgemeine Autorenschaft, Mehrboxbetrieb ab erster Produktversion und den Drittspiel-Nachweis. VA-01/D0 bleiben Grundlage; Zentrale ist kein Betriebszwang. |
| #27 F1 / #28 F2 | Offline-Zugang je Box und Veranstaltung, klare BLE-Übergabe, keine zentrale Rechteprüfung. Zugangskonzept aus #58 erhalten; Ereignisursprungsprüfung zusätzlich D0. |
| #29 F3 / #37 A3 | Mehrere Handys bedienen gleichzeitig verschiedene Boxen. Je Ziel aktuelle Berechtigung/Revision/Antwort; #37 verbindet GM mit Paketinstallation und Spielbedienung. BLE-Abbruch manueller Ausgabe stoppt nicht pauschal das Spiel. |
| #30 F4 | SP-01-Pakete mit lokalen Profilen/Ressourcen, Fähigkeits-/Inhalts-/Veranstaltungsbindung und individuell bestätigter Aktivierung. Profilpersistenz bleibt getrennt von D1-Fortschrittspersistenz; Teilverteilung ist kein gemeinsamer Aktivierungserfolg. |
| #31 F5a | Paketbestimmte allgemeine Laufzeit mit realen Besitzern und begrenztem Ereigniseingang verbinden; kein Warten des lokalen Spieltakts auf Funk/Zentrale. Lokaler Nachweis mit Referenzpaketen über denselben Validator/Laufzeitpfad erlaubt Vorbereitung vor #30. |
| #31 F5b | Lokale Autonomie, Wiederanlauf mit D1 und kombinierte Ressourcen prüfen. Erste Produktversion zusätzlich mit D4-Mehrboxkern und eingefrorenen Binärdateien für das dritte Spiel nach SP-01; lokale Abnahme allein genügt nicht. |
| #60 D1 / #61 D2 / #63 D4 | Dauerhafter Fortschritt, authentifizierter Ereignisaustausch und zentralenunabhängige verteilte Abnahme liegen auf dem Pfad zur ersten SP-01-Produktversion. Bestehende vollständige Kriterien einschließlich späterem Zentralenanteil bleiben erhalten. |
| #62 D3 | Optionale Zentrale mit eigenem Abnahmeumfang; Autorenschaft, direkte Paketverteilung und lokaler Mehrboxbetrieb setzen sie nicht voraus. |
| #33 A1a / #34 A1b | Quelle/Box/Empfangsalter je Beobachtung, getrennte Sitzungen und Schutz vor alten Antworten beim Boxwechsel; Cache ist keine aktuelle Fernerreichbarkeit. |
| #35 NFC | Bestehende Resttests erhalten; produktiven Mehrstations-Spielobjektvertrag vor Spielintegration D0 zuordnen. |
| #36 A2 | Altdatenübernahme bleibt erhalten und zurückgestellt; keine automatische Veröffentlichung oder gemeinsame Bearbeitung hinzufügen. |
| #39 H0 / #54 H1 | Hardware-/Matrixrestarbeit erhalten. Funk-/Koexistenzmessung in D4 verwendet geklärte Hardware; Architekturarbeit verlangt keinen neuen vollständigen Matrixlauf. |

## 3. Neue Pakete und Reihenfolge

Die Kürzel D0–D4 sind stabile Dokumentreferenzen, keine Fertigstellungstermine.
GitHub #25 und die verlinkten Issues führen Status und operative Priorität.

| Paket | Ergebnis / Abschlusskriterium | Voraussetzung |
| --- | --- | --- |
| **[D0 / #59](https://github.com/Sven401/Larp-Escape/issues/59) – Veranstaltungs- und Ereignisverträge** | **Spezifikationspaket durch #72 abgeschlossen.** Versionierte Identitäts-/Durchlauf-/Zuordnungsbegriffe, Nachrichtenarten und Ursprungsvertrauen; Regeln für Trennung und verspätete Daten. Gegenbeispiele/Fixtures, Format-/Fähigkeits-/Migrationsplan und offene technische Entscheidungen vor Umsetzung geklärt. | VA-01 und #28/#58; Konzeptarbeit ohne Hardware möglich |
| **[D1 / #60](https://github.com/Sven401/Larp-Escape/issues/60) – Dauerhafter Fortschritt und Ereignisabgleich** | Begrenztes Journal/Outbox, stabile IDs, Eingangslücken, dauerhafte Wiederholungserkennung und geordnete lokale Anwendung. Unterbrechungen zwischen Speichern/Anwenden/Quittieren; voller/beschädigter Speicher und Kompaktierung geprüft. | D0; Integration in #31 F5a, eigener Fortschrittsspeicher statt Umdeuten von #30 |
| **[D2 / #61](https://github.com/Sven401/Larp-Escape/issues/61) – ESP-NOW-/WLAN-Transport und Kontakte** | Gemeinsamer Vertrag über beide Wege; zuerst Direktkontakte, dann begrenzte Vermittlung. Peer-/Kanal-/Transferlimits, Quittierung, Lastkontrolle und BLE-Koexistenz aus gepinntem SDK ableiten und messen. | D0; Abgleich aus D1, Ressourcenbesitzer #31 F5a |
| **[D3 / #62](https://github.com/Sven401/Larp-Escape/issues/62) – Zentrale und Veranstaltungsansicht** | Eigenständiger persistenter Knoten mit zeitweise vollständiger Sicht, bestätigten Einzelaufträgen und Paketverteilung; getrennte lokale Handyzugänge. Hardwarewahl und Bedienvertrag vor Umsetzung konkretisieren. Ausfall blockiert keine Box. | D0; D1/D2 für tatsächlichen Abgleich, #30/#33/#34/#37 für Anschluss |
| **[D4 / #63](https://github.com/Sven401/Larp-Escape/issues/63) – Verteilte Referenz- und Betriebsabnahme** | Zwei Boxen, zwei BLE-Handys, zeitweise getrennte Zentrale; nachvollziehbarer Offline-/Neustart-/Wiederholungs-/Transportwechselablauf und Last-/Funkmessung. Softwaremodell und physische Nachweise getrennt. | D1/D2, #27/#28/#31; D3 für Zentralenanteil, geklärter Prüfaufbau #39 |

**Aktuelle Ausführungsfolge:** maßgeblich ist Abschnitt 0 / SP-01. D0 ist über
#72 spezifiziert und integriert. Paket-/Autorenvertrag zuerst; Editor/Vorschau,
F5a, allgemeiner Zugang und D1/D2 anschließend parallel gegen denselben Vertrag.
Die Anschlussfolge #31 F5a → #30 → #31 F5b bleibt erhalten. Der D4-Mehrboxkern
und das Drittspiel-Tor sind vor dem ersten Produktabschluss nachzuweisen.
D3 sowie sein D4-Zentralenanteil folgen unabhängig; ein Teilabschluss darf
nicht als vollständige D4-/VA-Abnahme ausgegeben werden.

## 4. Gemeinsame offene Abnahmekriterien

| ID | Gefordertes Ergebnis |
| --- | --- |
| VA-T01 | Zwei Handys steuern gleichzeitig verschiedene identifizierte Boxen per BLE; Antworten/Rechte/Revisionen bleiben je Ziel getrennt. Zweites Steuerhandy an derselben Box wird klar behandelt. |
| VA-T02 | Lokales Spiel läuft ohne Handy, WLAN, Nachbarbox und Zentrale nach vorbereiteter Regel weiter. Manuelle Baugruppenausgabe behält ihre gesonderte Abbruchregel. |
| VA-T03 | A hält offline einen Erfolg fest, startet neu, liefert später an B; B wendet die Freigabe trotz Duplikaten, eigenem Neustart und Empfang über beide Transporte nur einmal an. |
| VA-T04 | Fehlende, vertauschte und lückenhafte Ursprungsfolgen werden erkannt. Wiederverbindung und Zusammenfassung erklären Vollständigkeit; ein hoher Zähler überspringt keine fehlenden Tatsachen. |
| VA-T05 | Falsche Veranstaltung, alter Durchlauf, alte Stationszuordnung, unerlaubter Ursprung und manipuliertes weitergeleitetes Ereignis bewirken keine Freigabe. |
| VA-T06 | Unterbrechung vor/nach dauerhafter Annahme, Anwendung und Antwort ergibt nachvollziehbaren Wiederanlauf. Physischer Effekt mit unklarem Ausgang wird nicht als eindeutig ausgeführt/unausgeführt erfunden. |
| VA-T07 | Zeitkritischer Auftrag wird nach Funklücke nicht beliebig verspätet ausgeführt. Trennungs-/Ersatzregel jeder geprüften Interaktion ist sichtbar. |
| VA-T08 | Zentrale fällt aus und kehrt zurück: Boxspiele bleiben unabhängig; Abgleich stellt Verlauf/Einzelstände her. Alter/Quelle und ausstehende/unklare Zielergebnisse bleiben sichtbar. |
| VA-T09 | Begrenzte Weiterleitung erzeugt weder Schleifensturm noch unbegrenzte Queues. Volles Journal, Kompaktierung, Rückstau und Flash-/RAM-/Stackbudgets haben geprüfte Ergebnisse. |
| VA-T10 | BLE-Bedienung, ESP-NOW und WLAN-Kontaktphasen werden gemeinsam am tatsächlichen SDK-/Gerätestand gemessen; Kanalwechsel und Paketverlust verändern keine fachliche Kennung. |
| VA-T11 | Neue Pakete/Profile werden lokal vollständig geprüft und bewusst aktiviert. Versions-/Fähigkeitskonflikt, altes Profil und alte Nachrichten setzen keinen neuen Durchlauf frei. |
| VA-T12 | Lokaler Rechteentzug und spätere Rückkehr getrennter Boxen erfüllen #28/#58. Zentrale und Vermittlung umgehen keine Zielberechtigung oder bewusste Bestätigung. |

Alle zwölf Kriterien sind **offen**. Diese Dokumentation liefert keine
Implementierungs-, Build-, Funk-, Stromausfall- oder Produktfreigabe.
