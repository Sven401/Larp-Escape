# VA-01: betroffene Verträge, Arbeitspakete und Abnahme

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
| #25 Produktplan | Mehrbox-/Mehrspielleiter-Veranstaltung, Infrastrukturgrundsatz, VA-01 und D0–D4 aufnehmen. Zentrale ist kein Betriebszwang. |
| #27 F1 / #28 F2 | Offline-Zugang je Box und Veranstaltung, klare BLE-Übergabe, keine zentrale Rechteprüfung. Zugangskonzept aus #58 erhalten; Ereignisursprungsprüfung zusätzlich D0. |
| #29 F3 / #37 A3 | Mehrere Handys bedienen gleichzeitig verschiedene Boxen. Je Ziel aktuelle Berechtigung/Revision/Antwort. BLE-Abbruch manueller Ausgabe stoppt nicht pauschal das Spiel. |
| #30 F4 | Lokale Profile/Ressourcen, Inhalts-/Veranstaltungsbindung, individuell bestätigte Aktivierung; Profilpersistenz von D1-Fortschrittspersistenz trennen. |
| #31 F5a | Anwendungseigentümer und Gerätebesitzer um begrenzten Ereigniseingang ergänzen; kein Warten des lokalen Spieltakts auf Funk/Zentrale. Lokaler F5a-Abschluss braucht noch keine fertige Zentrale. |
| #31 F5b | Lokale Autonomie nach Handy-/Zentralen-/Nachbarverlust; Wiederanlauf mit D1, kombinierte Ressourcen und spätere verteilte Abnahme D4 getrennt ausweisen. |
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

**Ausführungsfolge:** D0 ist über #72 spezifiziert und integriert; technische Zugangsklärung #27/#28
und bestehende Baugruppenrestabnahme laufen als getrennte Stränge weiter.
Anschließend D1 und D2 vertikal mit zwei Boxen anschließen, D3 darauf aufbauen
und D4 gestuft abnehmen. Die bestehende lokale Folge #31 F5a → #30 → #31 F5b
bleibt erhalten. Lokale Abschlüsse und verteilte Veranstaltungsfreigabe werden
getrennt gezählt; kein pauschales Warten aller Arbeiten auf alle neuen Pakete.

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
