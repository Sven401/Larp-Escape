# Entwicklungsstart und Quellenbestand · 20. Oktober 2026

**Status: Bestandsaufnahme belegt; Bereinigung und Integrationen vorgeschlagen.**
Auftrag: alle erkennbaren Projekt-Worktrees, Chats und Quellen auf einen klaren
Startpunkt für weitere App- und Firmwareentwicklung abgleichen.
Sitzung `01a15b13-bc70-7be0-a117-a403268cf261`.
Geprüfte Integrationsbasis nach Fetch:
`97a2584cc108ebb9c0f667fc46f898d163e00144`.
Dies ist eine datierte Aufnahme in einer laufenden Arbeitsumgebung.

## Ergebnis und unmittelbarer Einstieg

Eine nutzbare Entwicklungsbasis ist vorhanden. Das Orientierungsproblem entsteht
durch alte Checkouts, überholte Einstiegsaussagen, parallele Chats und wenige
noch nicht entschiedene Zweige. Ein Neuaufbau oder ein Sammelmerge aller Quellen
ist nicht erforderlich.

Der [Maintainer-Einstieg](MAINTAINER.md#start-für-weitere-app--und-firmwarearbeit)
ordnet die Arbeitsbereiche zu. Die Root-[README](../README.md) nennt jetzt die
beiden vorhandenen Apps und die getrennten Firmwarepfade. Android-/Firmware-
Anleitungen verweisen auf den abgeschlossenen Matrix-Sammellauf; die alte
[UI-Exploration](../docs/ui-ux/EXPLORATION.md) ist als historische Quelle markiert.
Diese Änderungen berichtigen Orientierung, ohne Produktverhalten zu ändern.
Der wiedergefundene [Matrix-Zwischenbericht](../docs/matrix/device-test-direction-fix-2026-10-16.md)
ist mit vier Originalquellen kuratiert ergänzt und am bestehenden
[Baugruppenregister](BAUGRUPPEN-NACHWEISE.md) angebunden. Die
[LED-Entscheidung](../docs/led/spielinformation-2026-10-18.md) ist mit der
projektgebundenen Architektenrolle verknüpft. Beide Übernahmen werden erst
durch den Merge dieses Pakets Bestandteil des kanonischen Branches.

| Gegenstand | Heutiger Stand | Konsequenz |
| --- | --- | --- |
| Kanonischer Remote-Branch | `plan/projektuebergabe-kuratierter-stand` bei `97a2584`, einschließlich #86/#87/#88 | Neue Arbeitsbranches von frisch abgeglichenem Zielstand ableiten |
| Hauptcheckout | `codex/led-resonanzkreis`, `e7ec0f9`; offene `.vscode/extensions.json`, `.tmp/` und private Belege | Kein ungesicherter Branchwechsel oder pauschales Clean; #71 getrennt behandeln |
| Lokaler Integrationsbranch | `fcb9121`, anfangs 89 Commits hinter Remote | Nach Abschluss der parallelen Arbeiten gezielt vorspulen; Branchname allein beweist keine Aktualität |
| `main` | Historischer Default-Branch `29da937c` | Keine automatische Main-Integration oder Änderung des Default-Branches |
| Produktquellen | Zwischen Stationspilot-Merge `7e19c95` und `97a2584` unverändert; nur acht Dokumentdateien geändert | Bestehende Software-/Gerätebelege mit ihrem jeweiligen Umfang nutzen |

## Prüfbereich und Grenzen

Erfasst wurden fünf erkennbare Produktklone, ihre Git-Worktrees,
Branches, Stashes, offenen Änderungen und projektbezogenen Artefaktorte;
zusätzlich die erkennbaren Codex-Chats, archivierte Projektchats, lokale
Sitzungsmetadaten, relevante Abschlussmeldungen, aktuelle GitHub-PRs/Issues,
vorhandene CI und die projektbezogene Automationskonfiguration.

Ursprünglicher Bestand: **64 registrierte Produkt-Worktrees** — 20 im aktuellen
Klon, 41 im historischen Hauptklon sowie je einer in drei zusätzlichen Matrix-/
Audioklonen. Für diese Dokumentation kam ein isolierter eigener Arbeitsbereich
hinzu: **65 Produkt-Worktrees zum Auditabschluss**. Die fünf Klone hatten
ursprünglich zusammen 87 lokale Branch-Referenzen, einschließlich wiederholter
Namen in unterschiedlichen Repositories; keine Stashes. Hinzu kommt ein
historischer Wissensklon mit einem Worktree, getrennt vom Produktbestand.

**Chats:** 115 Einträge aus lokalen Metadaten und App-Abfragen, davon einer
allgemeines CI-Methodenmaterial; 27 historische Evaluationsläufe separat.
65 Chats anhand jüngster App-Inhalte, fünf anhand lokaler Abschlussnachrichten
und 45 nur anhand Metadaten geprüft. Das lokale Chatmetadateninventar enthält
17 bereits archivierte Projektchats. 93 ältere gespeicherte Rolloutpfade waren
lokal nicht auflösbar; ausgewählte Chats ließen sich dennoch über die App lesen.
Das ist eine Sichtbarkeitsgrenze, kein belegter Datenverlust.

Die verwendete Listenabfrage liefert höchstens die jüngsten 50 Einträge. Deshalb wurden
lokale Metadaten und gezielte Inhaltslesungen ergänzt. Metadatenprüfung ist
keine vollständige Transkriptprüfung. Nicht erreichbare Cloudinhalte, unverbundene
Rechner und unbekannte Ablageorte sind nicht als geprüft ausgewiesen. Es wurde
keine pauschale Vollprüfung sämtlicher Dateien des Rechners behauptet.

Lokale Detailinventare und die untersuchten Chat-Abschlüsse liegen privat in
`outputs/entwicklungsstart-audit/` der Audit-Arbeitskopie. Persönliche absolute
Pfade, Gesprächsexporte und Geräte-Rohdaten werden nicht in das öffentliche
Repository übernommen. Für eine spätere Entfernung muss dieser Bestand zuerst
an einem vom zu entfernenden Checkout unabhängigen privaten Ort gesichert sein.

## Offene Arbeit, die erhalten bleiben muss

| Quelle | Befund | Vorgeschlagene Behandlung |
| --- | --- | --- |
| [PR #71: Resonanzkreis](https://github.com/Sven401/Larp-Escape/pull/71), `e7ec0f9` | Offen, GitHub meldet nicht konfliktfrei integrierbar. Neun eigene Commits gegenüber dem gemeinsamen Vorfahren; Produktänderungen, Wire-Erweiterung, Kameranachweis und historische Effektanalysen | Gegen heutigen LED-/Näheglutstand neu prüfen. Umsetzung bewusst übernehmen oder zurückstellen. Analysen und Messbelege unabhängig von der Effektentscheidung erhalten |
| [PR #73: D1-A](https://github.com/Sven401/Larp-Escape/pull/73), `b9e9814` | Offen, GitHub meldet nicht konfliktfrei integrierbar; Vertrag und ausführbare Rechnerreferenz, keine fertige Firmwarepersistenz | Als nächstes Architektur-/Firmware-Reviewpaket bewerten; D1-B/C und #60 nicht durch D1-A schließen |
| [PR #89: Kristallsiegel](https://github.com/Sven401/Larp-Escape/pull/89), während Audit auf `5da6b40` aktualisiert | Neuer Entwurf mit editierbarer Vorschau; zählbare Abschnitte weichen von der früheren Nutzerentscheidung ab | Entwurf an [atmosphärischen Fortschritt](../docs/led/spielinformation-2026-10-18.md) anpassen oder Entscheidung ausdrücklich ändern; nicht automatisch integrieren |
| `codex/audio-test-alternative-local`, `704f544` | Zwei nur lokal erhaltene Entwurfscommits | Als verworfene/ersetzte Alternative mit Herkunft sichern, dann fachlich schließen; kein zweiter Audio-Produktpfad |
| `codex/ble-led-versuch`, `747812b` | Versuchscommit plus zwei nur lokal erreichbare B0-Vorfahren (`b1d423b`, `95212a7`) | Ganze lokale Historie sichern; isolierten Machbarkeitsnachweis nicht als heutige Produktbasis weiterführen |
| Zusätzlicher Matrix-Testklon | Unversionierter Installations-/Ruhebericht vom 16.10. und veränderte Übergabe; eigener privater Flash-/APK-Rückweg | Historischer Bericht in diesem Paket kuratiert; fremde Übergabe und private Belege weiterhin erhalten, keine alte Kontaktaufforderung aktivieren |
| [PR #90: Automationsnachweis](https://github.com/Sven401/Larp-Escape/pull/90), `c4c5a2a` | Während Audit veröffentlicht; bestehender Auftrag korrigiert, schreibfreier Probelauf dokumentiert | Dokumentation gegen aktuellen Stand prüfen und separat übernehmen; kein zweiter Auftrag |

Die alten grünen PR-Läufe von #71/#73 gelten für ihre damalige Basis.
Konfliktauflösung und Prüfung gegen den aktuellen Zielbranch sind noch offen.
Dieser Audit führt keinen dieser PRs zusammen und verwirft keinen Inhalt.

## Aufräummatrix der aktuellen Arbeitskopien

Arbeitsbereichsnamen sind Identifikatoren aus der lokalen Worktree-Liste.
„Entfernungskandidat“ bedeutet: erst nach frischem Statusabgleich, gesichertem
Quellstand und Prüfung der ignorierten Dateien; keine ausgeführte Entfernung.

| Arbeitsbereich | Zugeordneter Stand | Behandlung |
| --- | --- | --- |
| Hauptcheckout | Resonanzkreis #71, fremde Änderungen, private Geräte-/Kamerabelege | Behalten; später nach Sicherung wieder zum klaren Projekteinstieg machen |
| `audio-test` | Audioalternative `704f544` | Lokale Historie sichern, fachlich archivieren |
| `hardware-plan-tests` | Hardwareplanung #67 | Nach Artefaktprüfung entfernbarer Ergebnischeckout |
| `integration-offene-prs` | D1-A #73 | Behalten bis Reviewentscheidung |
| `led-camera` | Kamera-Messweg #65, inzwischen integriert | Ergebnischeckout nach Sicherung etwaiger Rohbelege entfernbar |
| `pr78-po` | UI/UX-Integrationsdokumentation #80 | Nach Abschluss-/Artefaktabgleich entfernbar |
| `pr79-integrationsabschluss` | Dokumentation #86 | Nach Abschluss-/Artefaktabgleich entfernbar |
| `pr88-review` | Setup-Integration #88 | Nach Abschluss-/Artefaktabgleich entfernbar |
| `veranstaltungszugang-konzept` | Zugangskonzept #58, über #69 integriert | Quellen behalten, Checkout nach Artefaktprüfung entfernbar |
| `wartung-pruefstand` | Wartung #87 | Prüfbericht erhalten; regenerierbare Builds nach Abgleich entfernbar |
| `ble-led-versuch` | Lokaler Versuch `747812b` | Lokale Historie sichern, archivieren |
| `espnow-baugruppentest` | ESP-NOW-Appbedienung #66, über #69 integriert | Mess-/Gerätebelege sichern; Ergebnischeckout später entfernbar |
| `github-automatisierung` | Aktive Korrektur der GitHub-Synchronisierung | Bis zum Abschluss behalten; nur ein zuständiger Chat |
| `led-effekt-architekt` | Näheglut-Integrationsstand | Integrierte Rolle/Kern/Belege erhalten; Checkout nach Rohdatenprüfung entfernbar |
| `led-effekt-lichtfaden` | Aktiver Kristallsiegelentwurf #89 | Behalten; während Audit erfolgte eine Aktualisierung auf die neue Basis |
| `ui-ux-pilot` | Älterer Pilotstand `c04a540` | Neuere integrierte Fassung maßgeblich; Quellen/Exports sichern, Checkout später entfernbar |
| `ui-ux-projektreferenz` | Nativer Stationspilot #79 | Integrierte Quellen und Gerätebelege erhalten; Checkout später entfernbar |
| `werkbank-projektsetup` | Frühere #88-Fassung `03f9aeb` | Aktiven Automations-/Setup-Chat zuerst abschließen; danach Ergebnischeckout entfernbar |
| `naeheglut` | Dokumentationsabschluss #85 | Integrierte Ergebnisse erhalten, nach Abschluss-/Artefaktabgleich entfernbar |
| `veranstaltungsarchitektur` | VA-01 #64, über #69 integriert | Verbindliches Konzept erhalten; Checkout nach Artefaktprüfung entfernbar |

Branches müssen nicht wegen jedes entfernten Checkouts verschwinden. Bereits
integrierte Arbeitsbranches können nach Prüfung der Remote-Erreichbarkeit
reduziert werden; die sechs benannten Quellarchivbranches, Sicherungs-Tags und
noch offenen/lokal einzigartigen Entwürfe bleiben bis zu ihrer eigenen
Erhaltungsentscheidung bestehen.

Zusätzlich wurden drei eigenständige Klone gefunden: `matrix-collection`
(`724f0bd`, sauber), der ältere `matrix-test` (`e4b2ce0`, unversionierter Bericht
und geänderte Übergabe) und ein älterer `audio-test` (`d1c7077`, sauber).
Ihre Quellstände sind historische Baugruppenquellen. Beim Matrix-Testklon ist
die Übernahme des neu gefundenen Berichts in diesem Paket vorbereitet;
die privaten Sibling-Verzeichnisse `matrix-01`/`matrix-02-direction-fix` gehören
zur Rückweg-/Belegprüfung und sind kein Git-Buildcache.

## Historischer Klon und Archivbelege

Der historische Produktklon ist als Quellenlager einzuordnen, nicht als zweiter
Entwicklungsstart. Seine 41 Worktrees umfassen die 40 ursprünglichen Quellen
aus [quelleninventar.json](quelleninventar.json) sowie einen zusätzlichen
kuratierten Übergabecheckout. Acht Arbeitsbereiche enthalten offene Änderungen.

**Frisch gemessen:** Alle 48 lokalen Branchspitzen dieses Klons sind durch heute
vorhandene Remote-Branches erreichbar. Die Archivprüfung umfasst 387
Manifesteinträge ohne SHA-256-Abweichung. 380 heute noch vorhandene unversionierte
Quelldateien sowie sechs archivierte Patches stimmen mit dem aktuellen alten
Arbeitsbestand überein. Der zusätzliche Übergabecheckout enthält eine lokale
Editor-Konfigurationsänderung. Die offenen historischen F1-/Diagnose-/LED-Dateien
sind damit kein Beleg eines unbekannten neuen Produktfortschritts.

**Separat frisch geprüft:** Sechs lokale `evidence/2026-10-16/*`-Tags, ihre
Quellbäume und 43 Dokumente aus [baugruppen-nachweise.json](baugruppen-nachweise.json)
stimmen in Commit, Blob, SHA-256 und Größe überein. Die Prüfung erfasst keine
vollständige private Rohdatensicherung und keinen neuen Gerätelauf.

Die sechs veröffentlichten Archivbranches bleiben der gemeinsame Einstieg in
F1, frühere Androidentwürfe, Hardwarequellen, Diagnosehistorie, LED-Entwürfe und
Experimente; Details in der [technischen Übergabe](README.md#quellarchiv-und-wiederaufnahme).
Alte Fortsetzungsaufträge sowie die verworfene Diagnose-App bleiben inaktiv.

**Historische KI-Werkbank separat ausgelesen:** `Larp-Escape-AI`, Branch
`master`, HEAD `e8a8fc3416434a22658cbfb7ea19e90cb638ded5`, ein Worktree.
Fünf versionierte Dateien sind verändert; zusätzliche Kommunikationsentwürfe,
Sitzungsbeiträge, Referenzarchive und Auswertungen sind unversioniert.
Dieser Arbeitsbestand ist nicht durch die Produkt-Archivprüfung abgesichert.
Vor Weglegen/Löschen privat vollständig sichern und die relevanten Beiträge
gezielt bewerten. Alte Behauptungen dortiger Kanonizität werden nicht reaktiviert.

Eine frühere Bereinigung vom 18.10.2026 hat bereits Arbeitskopien entfernt und
private Sicherungen erstellt. Ihr Bericht und Sicherungsort wurden in den
lokalen Auditnachweisen identifiziert. Das ersetzt keine aktuelle Prüfung der
heute vorhandenen Rohdaten. Insbesondere private Flashabzüge, Kameraoriginale,
App-Daten, installierte APKs und Provisionierungsdateien dürfen nicht pauschal
mit erzeugbaren Buildverzeichnissen gleichgesetzt werden.

## Chats, laufende Zuständigkeiten und Automation

Ein Chat ist Herkunft eines Auftrags oder Ergebnisses; der kanonische Arbeitsstand
steht im Repository und in den aktuellen Issues. Ein abgeschlossener Chat darf
aus der aktiven Ansicht verschwinden, sobald seine nützlichen Ergebnisse und
offenen Abhängigkeiten auffindbar gebunden sind. Alte Aufgaben werden nicht allein
durch Öffnen ihres Chats wieder aktiv.

| Gruppe | Behandlung |
| --- | --- |
| Aktuelle Bestandsaufnahme | Bis zur prüfbaren Dokumentation behalten |
| „Entwirf nächsten LED-Effekt“ | Entwurf abgeschlossen, #89 offen; bis Entscheidung zur Spielinformation behalten |
| „Werkbank-Automatisierungen prüfen“ / „Routineaufgaben im Projekt finden“ | Erstgenannter betreut inzwischen die aktive Automation; zweiter ist abgeschlossen und nach Nachweisübernahme archivierbar |
| Resonanzkreis-/D1-Chats | Bis Entscheidung zu #71/#73 behalten oder ausdrücklich als wartend markieren |
| Abgeschlossene Baugruppen-, Kamera-, Integrations-, UI/UX- und Maintainer-Chats | Nach Quellen-/Belegzuordnung archivieren; nicht als parallele To-do-Liste behandeln |
| Alte Diagnose-/F1-/Dashboard-/Planungschats und Evaluationsläufe | Historische Quellen; keine Wiederaufnahme aus alten Prompts oder Goal-Dateien |

Die beiden Automationschats hatten gleichzeitig Aufträge zur gleichen
GitHub-Synchronisierung. Im Verlauf wurde der gespeicherte Auftrag auf den
aktuellen Produkt-Integrationsbranch umgestellt; die alte KI-Werkbank wird
darin als aktuelle Quelle ausgeschlossen. Das ist eine ausgelesene
Konfigurationsänderung der parallelen Arbeit, kein von diesem Audit ausgeführter
Schedulerlauf. Der historische Zustand in [PROJEKT-SETUP.md](PROJEKT-SETUP.md#10-geplante-github-automatisierung-bestandsaufnahme)
bleibt als früherer Ablesestand erkennbar. Vor Abschluss die Änderung, den
schreibfreien Vergleich und den tatsächlichen Aktivierungszustand aus dem
zuständigen Chat übernehmen; keine zweite Automation anlegen.

Der parallele Abschluss ist inzwischen als [PR #90](https://github.com/Sven401/Larp-Escape/pull/90)
veröffentlicht. Sein [Bericht](https://github.com/Sven401/Larp-Escape/blob/c4c5a2a366b6d2c9ed956efa07ea3b7cccc2be14/handoff/GITHUB-AUTOMATISIERUNG-2026-10-20.md)
dokumentiert sieben frühere Heartbeats ohne Schreibaktion, die Umstellung und
einen schreibfreien Vergleich von 23 Issues mit 16 Schutzfällen. Das ist hier
ausgelesener Nachweis der anderen Sitzung, kein erneut ausgeführter Test.
Die dort benannte Abgrenzung zwischen dem alten Rückkehrkriterium in #34 und
der automatischen GM-Wiederverbindung bleibt eine offene fachliche Entscheidung.

Letzte gelesene Definition: `ACTIVE`, täglich 09:00, Zielchat
`01a15af6-8c80-7c61-8926-d3cf0048adae`, SHA-256
`41c16c88f3de15f93cc16f4e372425ed3e2dc91a1bc6d0ce44b27525bac07d53`.
Die frühere Trägersitzung „Projektplan mit GitHub-Issues abglei“
(`01a125a0-0f7f-7c10-a277-bfc0c1048350`) am alten Projektklon ist nach
Abschlussübernahme archivierbar. Die drei jüngsten dort gelesenen Heartbeats
meldeten wegen Quellenkonflikten keine GitHub-Änderungen.

Eine wiedergewonnene Produktentscheidung ist jetzt unter
[Spielinformation durch LED-Effekte](../docs/led/spielinformation-2026-10-18.md)
auffindbar: atmosphärischer Kristallfortschritt und allgemeine Störung ohne
Ursachenhinweis. Ihre Abweichung zum neuen Entwurf #89 zeigt, warum ein
Chatarchiv vor der Archivierung gegen bestätigte Entscheidungen geprüft wird.

Der live gelesene Plan #25 enthält zusätzlich lange historische Fortschreibungen,
darunter ältere „noch offen“-Aussagen zu inzwischen integrierten Arbeiten.
Empfohlen ist eine kurze aktuelle Tabelle oben mit Stand, Beleg, nächstem
konkreten Schritt und Zuständigkeit; die Chronik bleibt darunter. Der parallel
beauftragte Automationsabgleich darf nicht durch einen konkurrierenden Edit
dieses Audits überschrieben werden.

## Nützliches an den kanonischen Stand binden

| Material | Kanonisches Ziel | Erhaltungsnachweis |
| --- | --- | --- |
| Akzeptiertes Verhalten, Rollen und Schnittstellen | Vorhandene passende Fachdatei unter `docs/`, `Hardware/` oder Projektrolle | Entscheidung/Status, Quelle, Datum, geprüfter Commit und offene Grenzen |
| Übernommener Code | Eigener PR gegen den Integrationsbranch | Head, Konfliktentscheidungen, passende Tests/Builds und Merge separat |
| Nützliche Analyse aus nicht übernommenem Code | Fachreferenz mit Link auf ursprünglichen Branch/Commit | Kennzeichnung als Analyse/Entwurf; keine implizite Produktübernahme |
| Gerätemessung | Bestehender kuratierter Bericht samt maschinenlesbarer Auswertung | App-/Firmwarepaar, Hardware-/Chipbezug, Prüfumfang und private Rohdatenreferenz |
| Private Rohbelege und Rückweg | Privates Archiv außerhalb entfernbarer Worktrees | Manifest, SHA-256, Kopier-/Lesekontrolle; keine Zugangsdaten im öffentlichen Repo |
| Figma, Prototyp, Präsentation oder Effektdemo | Editierbare Quelle und verlinkter Export/Entwurf | Herkunft, Erzeugungsweg und tatsächlicher Prüfstand |
| Erledigter Chat | Bestehender fachlicher Bericht/PR plus nachvollziehbarer Abschluss | Offene Reste in Issue oder Übergabe, danach Chat archivieren |

Diese Zuordnung konkretisiert den bestehenden Maintainer-/Werkbankauftrag.
Es wird kein zweites Projektgedächtnis und keine neue globale Arbeitsregel
eingeführt. Die persönliche Werkbank bleibt projektübergreifend; die frühere
`Larp-Escape-AI` bleibt historische Belegquelle.

## Empfohlene Reihenfolge und Fertigkriterien

1. Diesen kurzen Einstieg und die belegten Dokumentkorrekturen prüfen und nach
   eigenem Mergeauftrag übernehmen; Remote-Stand davor erneut abgleichen.
2. Automationskorrektur einmalig abschließen und eine Zuständigkeit festhalten.
   Danach die aktive Chatliste auf laufende Entscheidungen/Arbeit reduzieren.
3. Lokale Audio-/BLE-Entwürfe und einzigartige private Artefakte außerhalb der
   entfernbaren Checkouts sichern; Wiederauffindbarkeit anhand Manifest prüfen.
4. #71 und #73 gegen den aktuellen Stand fachlich bewerten. Bei Ablehnung die
   wiederverwendbaren Erkenntnisse gezielt übernehmen und den Rest archivieren.
   #89 bleibt ein gesonderter neuer Entwurf.
5. Historische und abgeschlossene Worktrees nach der oben benannten Prüfung
   abbauen. Den aktuellen Hauptcheckout erst nach Sicherung der fremden Änderungen
   und Ende paralleler Arbeit auf einen aktuellen, eindeutig bezeichneten
   Einstieg bringen; alten Klon aus der aktiven Projektführung nehmen.
6. Neue Produktarbeit klein schneiden: Zugang `#27 → #28 → #37`, lokale
   Spielintegration `F5a → #30 → F5b` und dezentrale Persistenz getrennt verfolgen.
   Näheglut beginnt mit NG1 (Quellen-/Pausenvertrag), anschließend NG2.

Die spätere Vereinheitlichung von GitHub-Default-Branch und tatsächlichem
Integrationsziel braucht eine eigene Entscheidung. Bis dahin bleibt `main`
historisch und der kuratierte Branch ausdrücklich der Entwicklungseinstieg.

Fertig ist die Bereinigung, wenn jeder aktive Arbeitsbereich einen aktuellen
Auftrag besitzt, jede schützenswerte Quelle einen nachgewiesenen Ablageort hat,
die Starttexte denselben Produktstand nennen und eine neue App-/Firmwareaufgabe
ohne Rückgriff auf einen alten Chat begonnen werden kann. Dieses Dokument ist
der prüfbare Vorschlag dafür; eine vollständige physische Bereinigung ist noch
nicht ausgeführt.

## Prüfbelege dieses Audits

- Git-Status, HEAD, Remotes, Worktree-/Branch-/Stashinventar und Erreichbarkeit;
  frischer Fetch, Vergleich von Produktquellen und vollständigen Git-Bäumen.
- Relevante aktuelle Quellen und Übergaben; historische Archivmanifeste und
  Baugruppen-Sicherungstags wie oben getrennt beschrieben.
- GitHub: zunächst offene PRs #71/#73/#89, später zusätzlich #90; 17 offene Issues,
  #88 als gemergt bestätigt. Dieser Dokumentations-PR kommt erst danach hinzu.
- PR #88 am Head `a23ecd1`: [Quality 3/3](https://github.com/Sven401/Larp-Escape/actions/runs/39494623756),
  [LED/Android/Kamera 4/4](https://github.com/Sven401/Larp-Escape/actions/runs/39494623849),
  [NFC 2/2](https://github.com/Sven401/Larp-Escape/actions/runs/39494624699)
  frisch als erfolgreich ausgelesen; Merge `97a2584` besitzt denselben Git-Baum.
  Kein eigener Workflowlauf an `97a2584` gefunden; kein solcher Lauf behauptet.
- Lokale Dokumentprüfung dieses Pakets: UTF-8, relative Linkziele, Codezäune,
  Konfliktmarker und Diff. Ergebnis wird am veröffentlichten PR-Head festgehalten.

Kein neues Android-/Firmwarebuild, Geräteupload, optischer Test, Merge, Release,
Branch-/Worktreelöschen oder Chatarchivieren durch diesen Audit.
