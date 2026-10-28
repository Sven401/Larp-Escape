# Maintainer-Einstieg

**Fachaudit verschlankt · 28.10.2026:** Dokumentationsbasis ist
`f81956faffa4d4c6b0198fdbd0c626b21e137cd4` im Integrationsbranch
`plan/projektuebergabe-kuratierter-stand`. Diese Fortschreibung liegt in
[PR #99](https://github.com/Sven401/Larp-Escape/pull/99); sie ist bis zum Merge
kein integrierter Regelstand. Die ausdrücklich beauftragte Umstellung des
bestehenden Wochenauftrags wird getrennt im
[Betriebsbeleg](../docs/quality/fachaudit-betrieb.md#beobachteter-ausgangspunkt-und-umstellung)
nachgewiesen.

**Neuer Arbeitsumfang:** Kurzcheck höchstens fünf Minuten; nur bei belegtem
Anlass eine vertiefte Frage, insgesamt höchstens 30 Minuten einschließlich
Gegenprüfung und Protokoll. Starre Fachgebietsrotation und Sechswochenziel
entfallen. [Fachmandate](../docs/quality/verantwortlichkeiten.md),
[Prüfpflichten](../docs/quality-checks.md), echte Wiedervorlageauslöser und
quartalsweiser Normabgleich bleiben verbindlich. Eine fachliche Vertiefung
braucht weiterhin eine tatsächlich durchgeführte Gegenprüfung; ein unveränderter
Kurzcheck erneuert keinen vollständigen Fachprüfstand.

**Nutzenkontrolle:** Nach vier abgeschlossenen geplanten Läufen werden belegte
Maintainerentscheidungen, angenommene Änderungen und geschlossene Nachweislücken
getrennt den Laufzeiten, offenen Entwürfen, Fehlalarmen und Quellenlücken
gegenübergestellt. Der 26.10. zählt als erster Lauf vor der Verschlankung.
Keine Befundquote, keine Wirkung aus bloßer Recherche ableiten.
[Zählregeln und frühere Entscheidungsauslöser](../docs/quality/fachaudit-betrieb.md#messbarer-nutzen-und-entscheidung-nach-vier-läufen)
sowie [Übernahme/Rückweg](../PROMOTION.md) sind festgehalten.

**Erstlauf und Nachkontrolle abgeschlossen · 26.10.2026:** Der geplante A3-Lauf
auf `f81956faffa4d4c6b0198fdbd0c626b21e137cd4` dauerte im Chat 20 Minuten
18 Sekunden. Keine neuen/geänderten Findings, keine daraus angenommene
Änderung und keine neue Geräteabnahme. Die gesondert gegengeprüfte
Erstlaufkontrolle bestätigte:

| Bereich | Ergebnis |
| --- | --- |
| Dubletten | Fünf bekannte Finding-IDs und #39/B7 erhalten; keine Wiederöffnung korrigierter Software allein aus Pilottexten; #54 getrennt. |
| Zuständigkeiten | A3-Mandat, tatsächliche andere Qualitätsgegenprüfung und kanonische Ziele belegt; Fachreview und Maintainerentscheidung getrennt. |
| Wiedervorlagen | Vorhandene Auslöser erhalten, A2-02-Geräterest an A2-03 gekoppelt; keine erfundenen Freigabetermine. |

Laufbelege liegen lokal unter
`outputs/fachaudit/2026-10-26T18-25-59Z-f81956f/README.md` und `lauf.json`.
[Zeiten, Chatbelege und Grenzen](../docs/quality/fachaudit-betrieb.md#beobachteter-ausgangspunkt-und-umstellung)
zeigen die Betriebsabweichung: Auditstart 19:25:47 statt 10:00 MEZ,
Nachkontrolle 19:46:48 statt 11:00 MEZ. Ursache ungeklärt; beide wurden
abgeschlossen. Die einmalige Nachkontrolle ist beendet und entfernt.
Ein leerer Finding-Zähler allein begründet weder Erfolg noch Nutzlosigkeit.

**Weiter offen:** Geräte-/SMP-/Zeitnachweise, IDF-4.4-Supportentscheidung,
Chipzuordnung #39/B7, Adapterabnahmen und der messbare Zusatznutzen des
Wochenbetriebs. Die nicht vertieften Fachgebiete bleiben sichtbar ungeprüft.
Nächster Solltermin und rückgelesene Auftragseinstellungen stehen im
[Betriebsbeleg](../docs/quality/fachaudit-betrieb.md#beobachteter-ausgangspunkt-und-umstellung).
Der GitHub-Abgleich und die Produkt-/CI-Prüfungen bleiben unverändert.

Die folgenden Einträge bewahren ihre **historischen** Integrations- und
Prüfstände; frühere Terminvormerkungen und Betriebsregeln sind keine aktuellen
Aufträge.

**Historischer Integrations- und Aktivierungsstand · 24.10.2026:**
[PR #97](https://github.com/Sven401/Larp-Escape/pull/97) ist in
`plan/projektuebergabe-kuratierter-stand` integriert. Remote-Head und geprüfter
Mergecommit: [`c582ed7048cf7c386f240eca20e09142b86c7e00`](https://github.com/Sven401/Larp-Escape/commit/c582ed7048cf7c386f240eca20e09142b86c7e00).
Der Dateibaum ist identisch zum PR-Head `a8f5ec45dccf6a2d83347bb503c70e8e9255a755`
(Baum-ID `9154e19bd36c6d222a037c596e875c10a7951ba7`, lokal erneut verglichen).
Vor dem Merge waren zwölf Checks erfolgreich: neun PR-Jobs und drei
Quality-Jobs des Pushes auf den Arbeitsbranch. Davon getrennt ist die eigene
[Merge-CI, Quality-Lauf 40042256785](https://github.com/Sven401/Larp-Escape/actions/runs/40042256785)
jetzt **3/3 erfolgreich** (Push-Ereignis, abgeschlossen am 24.10.2026).

| Prüfung auf dem tatsächlichen Merge | Ergebnis aus dem aktuellen Jobprotokoll |
| --- | --- |
| [Hardwareunabhängige Prüfungen](https://github.com/Sven401/Larp-Escape/actions/runs/40042256785/job/120715805046) | 122 allgemeine, 11 MCP-, 513 Steuerungs- und 523 Kryptografiefälle erfolgreich; 10 PN532-Patchtests, RTOS-Kontextprüfung und 20.000 FIFO-Befehle/-Antworten mit Hostthreads; Architektur-, 86 Header- und Formatprüfung erfolgreich |
| [ESP32-Builds](https://github.com/Sven401/Larp-Escape/actions/runs/40042256785/job/120715805025) | Spiel-Firmware, Runtime-, Profil-Runtime- und GATTS-ABI-Probe erfolgreich; acht native und zwei SDK-Negativfälle der Kryptografie-Quellenprüfung abgewiesen |
| [Android](https://github.com/Sven401/Larp-Escape/actions/runs/40042256785/job/120715804818) | Protokoll-/Unit-Tests, Debug-/Release-Builds und Lint für Dev und GM erfolgreich; zehn Trennungsregressionen und Prüfung beider App-Pakete erfolgreich |

Alle drei Protokolle nennen `c582ed7` als tatsächlich ausgecheckten Commit;
vier nicht abgelaufene Belegartefakte sind vorhanden. LED-, NFC- und
ESP-NOW-Push-Workflows wurden durch ihre Pfadfilter bei diesen reinen
Dokumentänderungen nicht ausgelöst. Das sind keine zusätzlichen grünen
Merge-Läufe. Diese Fortschreibung wertet vorhandene CI aus; lokal wurden
keine Produkt-, Build- oder Gerätetests erneut ausgeführt.

**Damals rückgelesener Automationsstand (24.10.2026):** Der [Fachaudit-Wochenauftrag](../docs/quality/fachaudit-betrieb.md)
`larp-escape-fachaudit` ist nach ausdrücklichem Folgeauftrag **ACTIVE**.
Prompt und Zielchat wurden zurückgelesen; der bestehende GitHub-Abgleich ist
unverändert. Nächster Solltermin: **26.10.2026, 10:00 MEZ / 09:00 UTC**,
wöchentlich Montag in Europe/Berlin. Tatsächliche Scheduler-Vormerkung,
Startversatz und Prüfweg stehen im Betriebsbeleg. Eine erste geplante
Ausführung ist noch nicht erfasst; nach vier abgeschlossenen Wochenläufen
folgt die Nutzungsbewertung.

**Offen bleiben:** Geräte-/SMP-/Zeitnachweise, IDF-4.4-Supportentscheidung,
Chipzuordnung #39/B7, Adapterabnahmen und die operative Bewährung des Audits.
Fachmandate und Prüfpflichten gelten weiter. CI und Aktivierung sind keine
Geräte- oder Gesamtproduktabnahme. Die folgenden Einträge bewahren die
jeweiligen früheren Prüfstände.

**Zweitlauf und Automationsvorbereitung · früherer Stand vom 24.10.2026:** Auf Integrationscommit
`cee968a7bf4c97eb68c9fe6f705bb5a06eef76b0` wurde der
[zweite Audit-Probelauf mit unabhängigem Wiederholungsabgleich](../docs/quality/audits/2026-10-24-probelauf-2/README.md)
durchgeführt. Fünf Findings und #39/B7 wiedererkannt, keine Dubletten; offene
A2-02-Gerätewiedervorlage dem bestehenden Adapterfreigabetor zugeordnet.
[Fachaudit-Wochenauftrag](../docs/quality/fachaudit-betrieb.md) **pausiert vorbereitet**,
nicht aktiviert. Die [eigene CI von PR #96 am tatsächlichen Merge](https://github.com/Sven401/Larp-Escape/actions/runs/40038277888)
ist mit Quality 3/3 erfolgreich; andere Push-Workflows wurden durch ihre
Pfadfilter nicht gestartet. Kein neuer Geräte- oder Gesamtproduktnachweis.

**Fachaudit integriert · 24.10.2026:** [PR #95](https://github.com/Sven401/Larp-Escape/pull/95)
ist in `plan/projektuebergabe-kuratierter-stand` übernommen, tatsächlicher
Mergecommit [`882c9d613ed6e63b33b8e4dffcd2b207ea4d66d3`](https://github.com/Sven401/Larp-Escape/commit/882c9d613ed6e63b33b8e4dffcd2b207ea4d66d3).
Der vollständige Dateibaum ist identisch mit dem geprüften PR-Head
`d17e699e548346051f0777a022171be545e76b9b`; lokal mit `git diff --exit-code`
und gleicher Git-Baum-ID `1cb1bf53e280881ae203be4c44aefbd4d9c56800` verifiziert.

- **PR-Prüfstand:** neun erfolgreiche Jobs am PR-Zusammenführungsstand
  `a4957905ba4081c28256230c05ebab6f2c6de229`, Head `d17e699e` gegen Basis
  `e61d7642`: [Quality 3/3](https://github.com/Sven401/Larp-Escape/actions/runs/40035232919),
  [LED/Baugruppen 4/4](https://github.com/Sven401/Larp-Escape/actions/runs/40035232741),
  [NFC 2/2](https://github.com/Sven401/Larp-Escape/actions/runs/40035232726).
- **Eigener Merge-Prüfstand:** sieben erfolgreiche Jobs (Quality 3/3, LED/Baugruppen 4/4).
  [Quality](https://github.com/Sven401/Larp-Escape/actions/runs/40036526891) und
  [LED/Baugruppen](https://github.com/Sven401/Larp-Escape/actions/runs/40036526833)
  prüfen den tatsächlichen Mergecommit `882c9d6` über das Push-Ereignis.
  Der separate NFC-Workflow wurde wegen seines Pfadfilters nicht neu gestartet;
  sein grüner PR-Lauf bleibt ein eigener Nachweis. Die CI dieser reinen
  Maintainer-Fortschreibung ist wiederum davon getrennt.
- **Lokaler Softwareprüfstand vor dem Merge:** Codecommit `c4b84fc`, 122 allgemeine,
  513 Steuerungs-, elf MCP- und 523 Kryptografiefälle, Kanalprüfung mit echten
  Hostthreads und vier erfolgreiche ESP32-Buildvarianten;
  [vollständige Befehle, Commitbezug und Grenzen](../docs/quality/runtime-fixes-2026-10-24.md).

Damit sind die [Fachmandate unter dem Maintainer](../docs/quality/verantwortlichkeiten.md),
die FreeRTOS-/MCP-Softwarekorrekturen und die zusätzlichen PN532-/MCP-/
Kontextprüfungen integriert. Der [Pilot](../docs/quality/audits/2026-10-24-pilot/README.md)
bewahrt die ursprünglichen Fehlerbelege. Hardwareabnahme, ESP32-SMP-/Timingnachweise,
IDF-4.4-Supportentscheidung und bestehende Chip-/Fristgrenzen bleiben offen.
[Automationsbestand](AUTOMATISIERUNGSINVENTAR-2026-10-23.md) und
[Koordinatorentwurf](../docs/quality/fachaudit-entwurf.md) bleiben abgegrenzt.
Dieser Übernahmeschritt richtete keinen Scheduler ein; die spätere Aktivierung
ist oben dokumentiert. Kein Geräteupload und keine Übernahme nach `main`.

## Start für weitere App- und Firmwarearbeit

**Integration und Bereinigung · 21.10.2026:** #91 ist bei `f174cee` integriert,
mit neun erfolgreichen PR-Prüfungen und sieben erfolgreichen Prüfungen auf dem
Mergecommit. Der lokale Checkout `.worktrees/kanonischer-start` liegt sauber
auf diesem Stand. [Abschlussbeleg, private Sicherung und erhaltene Arbeit](BEREINIGUNG-2026-10-20.md).
Die folgenden Angaben bewahren den fachlichen Prüfstand der ursprünglichen
Bestandsaufnahme; für neue Arbeit weiterhin den Remote-Head prüfen.

**Ausgelesen und am Code abgeglichen · 20.10.2026:** Kanonischer Ausgangspunkt
ist `origin/plan/projektuebergabe-kuratierter-stand`, in dieser Bestandsaufnahme
bei `97a2584cc108ebb9c0f667fc46f898d163e00144` einschließlich #86/#87/#88.
Vor neuer Arbeit den Remote-Stand und den tatsächlich geöffneten Checkout prüfen.
Der Hauptordner stand beim Audit auf dem älteren Resonanzkreis-Branch;
auch der gleichnamige lokale Integrationsbranch war zurückgeblieben.
[Bestandsaufnahme, Quellenzuordnung und Aufräumvorschlag](ENTWICKLUNGSSTART-2026-10-20.md).

| Arbeit | Einstieg | Noch offene Grenze |
| --- | --- | --- |
| Dev-App und Baugruppen | [Android-Anleitung](../companion/android/README.md), Modul `app`; [Baugruppenfirmware](../firmware/led/README.md), [Wire-V2](../docs/led/ble-wire-v2.md) | Einzeln belegte Funktionen sind keine gemeinsame Geräte-/Spielabnahme |
| Spielleiter-App | Modul `gamemaster`; [Stationspilot](../docs/companion/spielleiter-stationspilot.md), [UI/UX-Vertrag](../docs/ui-ux/BOX-PILOT.md) | Lesender Pilot vorhanden; Licht-/Spielaktionen und allgemeine Rechte fehlen |
| Spiel-/Profilfirmware | [Profilmodell](../docs/configuration/profile-model.md), [Geräteausführung](../docs/configuration/device-runtime.md), `src/main.cpp` und getrennte Profilprobe | F5a-Ausführung, reale Profilübernahme, F5b-Betrieb nicht abgeschlossen |
| Dezentrale Veranstaltung | [VA-01](../docs/veranstaltungsarchitektur.md), [D0](../docs/veranstaltung/d0-vertrag-v1.md), [Plan #25](https://github.com/Sven401/Larp-Escape/issues/25) | D1-A in PR #73 separat bewerten; D1-B/C und D2–D4 bleiben offen |
| LED-Effektentwicklung | [Rolle](../docs/led/rollen/led-effekt-architekt.md), [Spielinformation](../docs/led/spielinformation-2026-10-18.md), [Live-Anleitung](../docs/led/interactive-development.md), [Näheglut-Fortsetzung](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md) | Labor, Produktadapter und optische Freigabe getrennt; offene Entwürfe nicht automatisch integrieren |

Die [Prüfanleitung](../docs/quality-checks.md) und der
[Wartungsprüfstand](WARTUNG-PRUEFSTAND-2026-10-20.md) ermöglichen den Start ohne
Hardware. Der Audit prüft Git, Quellen, Dokumentation und vorhandene CI-Belege;
er ist kein neuer Firmware-/Android- oder Gerätetest. Die Produktquellen an
`97a2584` sind gegenüber dem Stationspilot-Merge `7e19c95` unverändert.
PR #88 hatte neun erfolgreiche PR-Jobs; sein Merge hat denselben Git-Baum wie
der geprüfte Head `a23ecd1`. Ein eigener CI-Lauf auf `97a2584` war beim Audit
nicht vorhanden; die Nachweise werden nicht gleichgesetzt.

## Datierte Fortschritte und Herkunft

Die folgenden Einträge beschreiben den jeweiligen damaligen Stand.
Für neue Arbeit gilt der Einstieg oben zusammen mit den aktuellen Fachquellen.

**Setup und GitHub-Abgleich · 20.10.2026:** [Projekt, Werkzeuge, Prüfwege und Werkbank-Automatik](PROJEKT-SETUP.md). Die anschließende [beauftragte Umstellung mit Verlaufsaudit und Lese-Probelauf](GITHUB-AUTOMATISIERUNG-2026-10-20.md) löst den alten KI-Werkbank-Bezug ab. Bestehender Heartbeat wieder aktiv; 23 Issues und 16 Schutzfälle geprüft, keine Issue-/Boardänderung im Probelauf. Fachlicher Konflikt #34 bleibt offen.

**Stationspilot #79 integriert · 19.10.2026:** Merge `7e19c95` in den kuratierten Integrationsbranch; Dateibaum identisch zum geprüften Head `98488ea`. [Mergebeleg, getrennte CI und offene Geräteabnahmen](STATIONSPILOT-INTEGRATION-2026-10-19.md). Der lesende native Pilot und die Apptrennung sind integriert; TalkBack, Mehrstationsbetrieb, echter Funkverlust und Langzeitbetrieb bleiben offen. Keine neue Geräteprüfung durch den Merge.

**Wartung #87 integriert · 20.10.2026:** Aktuelle Integrationsbasis `a4c83cb`; der Dokumentationsmerge erhält die Produktquellen von `7e19c95`. [Frische lokale Tests/Builds und korrigierte Prüfanleitungen](WARTUNG-PRUEFSTAND-2026-10-20.md) ergänzen die historischen Stationsbelege. Neun PR-Jobs am Wartungshead `44e7098` erfolgreich; eigene Merge-CI und die CI dieser Maintainer-Fortschreibung bleiben getrennte Nachweise.

**Nativer Stationspilot · 19. Oktober 2026:** [Lokale Vorbereitung, Stationsauswahl, automatische BLE-Anmeldung und bestätigter Zustand](../docs/companion/spielleiter-stationspilot.md) in Larp GM implementiert und an der lokalen Box/Moto G14 geprüft. Bluetooth-Wiederkehr, Hintergrund/Rückkehr, bewusste Pause, Prozessneustart, große Schrift und Rotation geprüft. Dev-Quellen und alle zehn erfassten Dev-Dateien unverändert; Licht-/Spielaktionen sind Folgearbeit.

**Android-Apptrennung · 18. Oktober 2026:** [Zwei Anwendungsprojekte und Erhalt der Dev-Daten](../docs/companion/android-app-trennung.md): Dev behält Paketkennung/Speicher; Spielleitung erhält eine eigene Kennung und einen leeren Veranstaltungseinstieg. [Beide Apps am Moto G14 installiert, Dev ohne Deinstallation aktualisiert und Daten erhalten](../docs/companion/android-app-geraetepruefung-2026-10-18.md). App-Auswahl: Larp Dev mit DEV-Badge und Larp GM mit übernommenem GM-Companion-Icon. Neue Spielsteuerung bleibt Folgearbeit.

**UI/UX-Entscheidung · 18. Oktober 2026:** [UX-01 und Projektreferenz](../docs/ui-ux/PROJEKTREFERENZ.md): Dev-App für Entwicklung/Tests/Experimente und Spielleiter-App für Endnutzer werden als zwei separate Apps ausgeliefert. Hauptabläufe für beide Apps sind ausgearbeitet; technische Trennung ist im oben verlinkten Folgeschritt umgesetzt; Installation/Datenerhalt sind oben geprüft; der lesende native Stationspilot ist über #79 umgesetzt; Licht-/Spielaktionen bleiben offen. Der über PR #78 integrierte Stationspilot bleibt die Grundlage der Spielleitungsbedienung.

**Merge-Dokumentation #84 integriert · Stand 19.10.2026:**
[PR #84](https://github.com/Sven401/Larp-Escape/pull/84) nach 9/9 grünen PR-Jobs
bei `8af3ffaac7208ba3944b927bb472195e18b63510` übernommen; Dateibaum identisch
zum geprüften Head `0a8bb41`.
[Merge-/CI-Belege und unverändert offene Folgearbeit](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).

**Näheglut-CI-Status integriert · 18.10.2026:**
[PR #83](https://github.com/Sven401/Larp-Escape/pull/83) nach 9/9 grünen PR-CI-Jobs
bei `9d8bf44aff9da1d4c47f109c451cb251a9d626cd` übernommen. Vollständiger
Dateibaum identisch zum geprüften Head `0fb6ab0`.
[Merge-/CI-Belege und unverändert offene Folgearbeit](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).

**Näheglut-Stopdiagnose und Spielanschluss integriert · 18.10.2026:**
[PR #81](https://github.com/Sven401/Larp-Escape/pull/81) nach 9/9 erfolgreichen
PR-CI-Jobs bei `c4870215a155b9abff2e36fccfe93d42e1a03043` übernommen.
**Zusätzlicher CI-Abschluss geprüft:** #82-PR-CI 9/9 und Quality am tatsächlichen
#82-Merge `8648f57` 3/3 erfolgreich; LED am #81-Merge 4/4 erfolgreich.
Der durch den Folgemerge abgebrochene alte Quality-Lauf bleibt als solcher dokumentiert.
[Befund, Spezifikation und nächste Schritte](NAEHEGLUT-STOP-SPIELANSCHLUSS-2026-10-18.md).
650 Originalbilder erneut geprüft; digitaler Stop bestätigt, flächiger Anstieg
auch außerhalb der CAL-Masken. Physische Ursache/optische Freigabe weiter offen.
Konkreter Produktadaptervorschlag liegt vor; keine Produktimplementierung oder
neue Geräteprüfung. Nutzerbestätigter Start: korrespondierende Station erkannt;
Näheglut läuft durch wie Funkeln. Quellenzuordnung/Pausenregel bleiben offen.

**UI/UX-Pilot integriert · 18. Oktober 2026:** [PR #78](https://github.com/Sven401/Larp-Escape/pull/78)
ist bei `42853bc1aa93de23b5fcfc72e66d2c1a397361af` in den kuratierten
Integrationsbranch übernommen. [Rolle](../docs/ui-ux/ROLLE.md),
[aktueller Zustandsvertrag](../docs/ui-ux/BOX-PILOT.md),
[Prototyp/Figma](../design/box-pilot/README.md) und
[PO-/Figma-Nachprüfung](../design/box-pilot/figma-sync-review.md) sind integriert.
Drei P2-Punkte behoben; 56 editierbare Figma-Screens, 148 geprüfte Verknüpfungen.
**Design-/Prototyppaket, keine native Produktumsetzung oder Geräteabnahme.**
[Mergebeleg, Prüfnachweise und konkrete Fortsetzung](UI-UX-INTEGRATION-2026-10-18.md).

**Näheglut mit realem ESP-NOW geprüft · 18. Oktober 2026:** Drei Laborläufe
belegen Funkanbindung, Veraltung/Wiederkehr und gezielten Stop ohne Wiederanlauf.
Optische Dunkelprüfung und zwei Übergangsabweichungen bleiben offen;
[Ergebnis und Diagramm](../docs/espnow/naeheglut-result-2026-10-18.md).
Originalinstallation vollständig wiederhergestellt und verifiziert.
[Arbeitsstand, Belege und Grenzen](NAEHEGLUT-ESPNOW-2026-10-18.md);
keine Produkt- oder Gesamtspielabnahme.
[PR #76](https://github.com/Sven401/Larp-Escape/pull/76) ist nach grüner CI
bei `1646bc37d43f29f0fb48b3cf13adf597dfdc2cf4` im kuratierten Integrationsbranch gemergt.
[Integrationsbeleg und Fortsetzung](NAEHEGLUT-ESPNOW-INTEGRATION-2026-10-18.md).

**LED-Effektentwicklung integriert · 18. Oktober 2026:**
[PR #74](https://github.com/Sven401/Larp-Escape/pull/74) ist bei
`d4dd7c67cd6ba0e1903d8bf6c5df256c49fe5c3e` in
`plan/projektuebergabe-kuratierter-stand` gemergt.
[Mergebeleg, Prüfstand und offene Folgearbeit](LED-EFFEKT-ARCHITEKT-INTEGRATION-2026-10-18.md).
Die [Architektenrolle 2.0](../docs/led/rollen/led-effekt-architekt.md) wurde mit
gemeinsamem Effektkern, seriellen Liveparametern und parallelem Kamerafeedback
praktisch erprobt. [Näheglut-Versuchsbericht](../docs/led/naeheglut-live-results-2026-10-18.md):
Farb-/Kurveniteration, zwei vollständige Folgeläufe und verifizierter Flash-Rückweg.
[Ausführbare Anleitung](../docs/led/interactive-development.md).
Produktadapter, produktseitiger Nähe-/Spielwertvertrag und optisches Nutzerurteil bleiben offen;
die [Rechnerprobe 1.0](LED-EFFEKT-ARCHITEKT-ERPROBUNG-2026-10-18.md) bleibt
historischer Beleg.

**D0 integriert · 18. Oktober 2026:** [Vertrag zu #59](../docs/veranstaltung/d0-vertrag-v1.md)
mit Ereignisidentitäten, Offline-/Replayregeln, Ursprungsvertrauen und Migration.
[PR #72](https://github.com/Sven401/Larp-Escape/pull/72) übernimmt den fachlich
nachgeprüften Inhalt `a9481e0` auf Basis `5f90d10` samt dieser Statusfortschreibung.
Quelle: ausdrücklicher Merge-/Projektstandsauftrag vom 18.10.2026;
[Review, 111 Modellfälle und offene Implementierungstore](D0-REVIEW-2026-10-18.md).
Das D0-Spezifikationspaket ist abgeschlossen; D0-G1–G5 sowie Implementierungs-
und Gerätenachweise bleiben offen. Tatsächlicher Mergecommit und neue CI: PR #72/#25.

**Integration abgeschlossen · 18. Oktober 2026:** [geprüfte PR-Stände,
Konfliktentscheidungen und Prüftore](INTEGRATION-2026-10-18.md).
[PR #69](https://github.com/Sven401/Larp-Escape/pull/69) hat #58/#64/#65/#66/#68
bei `0b0843d5fce475d14705c42c21f4f8bf4314df30` in den kuratierten Branch
integriert. Alle fünf Original-PRs sind als gemergt geschlossen; ihre Heads erhalten.
Der abgeschlossene [RSSI-Gehversuch](../docs/espnow/distance-camera-result-2026-10-18.md)
und die [isolierte Kameraabnahme](../docs/led/camera-results-2026-10-18.md)
ergänzen die bisherigen Teilbelege. Gesamtspiel, Reichweite und Dauerlast bleiben offen.
Der [Resonanzkreis-Kameralauf](../docs/led/resonance-camera-results-2026-10-18.md)
belegt zusätzlich alle 16 synthetischen Wake-Masken und Übergangs-/Stop-Fälle
auf allen 23 LEDs. Reale Sensor-/BLE-Abnahme bleibt offen; ursprünglicher
RSSI-Boxstand anschließend vollständig verifiziert wiederhergestellt.

**Neue verbindliche Produktentscheidung · 18. Oktober 2026:** [VA-01](../docs/veranstaltungsarchitektur.md) legt autonome Boxen, BLE-Bedienung je Box, unterbrechbare ESP-NOW-/WLAN-Kommunikation und die Zentrale ohne Betriebszwang fest. [Vertrags-/Backlogmatrix](../docs/veranstaltungsarchitektur-umsetzung.md), neue Pakete #59–#63. Zugangskonzept PR #58 bleibt ergänzend maßgeblich. **Architektur-Soll; keine Implementierung oder neue Geräteabnahme.**

Stand: **20. Oktober 2026**. Mandat: [AGENTS.md](../AGENTS.md).
Quelle: vorbereiteter #79-Abschluss vom 19.10.2026 und beauftragte Nachprüfung
gegen den um #87 fortgeschriebenen Zielstand am 20.10.2026. Frühere Integrationen bleiben erhalten.
Vorheriger Integrationsabschluss: beauftragter Merge von #69 und Planabgleich;
[Integrationsabschluss](INTEGRATION-2026-10-18.md). Frühere Hygiene:
[Prüfbericht und Bestandsaufnahme](REPOSITORY-HYGIENE-2026-10-18.md).

Aktuelle fachliche Ergänzung vom **18. Oktober 2026**:
[abgestimmtes veranstaltungsbezogenes Zugangskonzept](../docs/companion/veranstaltungszugang.md)
mit Rechteverwaltung, Schlüsselwechsel, Wiederherstellung, Unterbrechungspunkten
und offenen Abnahmekriterien. **Sollkonzept, keine Implementierung.**

## Aktuelle Arbeitsbasis

- **Dokumentationsbasis:** PR #87 ist bei `a4c83cb7351759808f85ede612abc7c2280212fc`
  integriert; Dateibaum identisch zum geprüften Wartungshead `44e7098`.
  [Wartungsprüfstand](WARTUNG-PRUEFSTAND-2026-10-20.md). Die damalige Maintainer-Fortschreibung
  aus `499ec94` ist inzwischen durch #86 bei `5b85965` integriert; #88 ergänzt
  bei `97a2584` die Setup-Dokumentation. Beide ändern keine Produktquellen.
- **Belegt:** PR #79 ist bei `7e19c955aaa54bcc7ed318d594a6f69476de4aa9`
  integriert. Frischer Fetch bestätigt Zielbasis `494f1c4` und Head `98488ea`
  als Eltern sowie identischen vollständigen Dateibaum zum geprüften Head.
  [Integrationsbeleg und eigene Merge-CI](STATIONSPILOT-INTEGRATION-2026-10-19.md).
- LED, Wake, Matrix, Audio und NFC sind auf dem
  [gemeinsamen V2-Vertrag](../docs/led/ble-wire-v2.md) zusammengeführt.
  [Herkunft und Erhaltung aller acht Ergebnis-IDs](KONSOLIDIERUNG-2026-10-17.md)
  sowie [historische Gerätebelege](BAUGRUPPEN-NACHWEISE.md) bleiben maßgeblich.
- Die früheren Einzel-PRs sind Herkunft, keine erneut einzuspielende Mergekette.
  Die [Hygieneübersicht](REPOSITORY-HYGIENE-2026-10-18.md) dokumentiert ihre
  Schließung und erhaltenen Branches/Sicherungen.
- `main` bleibt der historische Default-Branch bei `29da937c`; eine Integration
  dorthin benötigt einen gesonderten Auftrag. Neue Arbeit auf eigenem Branch
  gegen den kuratierten Integrationsbranch vorbereiten.
- Fremde Änderungen und private lokale Artefakte erhalten. Die frühere separate
  KI-Werkbank bleibt historische Quelle; Projektwissen wird hier gepflegt.

## Nächste Arbeit und Abnahmegrenzen

[Issue #25](https://github.com/Sven401/Larp-Escape/issues/25) führt die operative
Planung. [#53](https://github.com/Sven401/Larp-Escape/issues/53) dokumentiert
abgeschlossene Softwarekonsolidierung und Integration durch #56.

- **Nativer Stationspilot integriert:** Lokale Vorbereitung, automatische
  Anmeldung und bestätigter Zustand sind implementiert und begrenzt am Gerät
  belegt. [Gerätefolgeprüfungen und Fortsetzung](STATIONSPILOT-INTEGRATION-2026-10-19.md):
  TalkBack, mehrere reale Stationen, Funkverlust bei Bluetooth an und Dauerlauf.
  Licht-/Spielaktionen und allgemeine Spielleitungsrechte bleiben Folgearbeit.

- **D0 #59 abgeschlossen als Spezifikationspaket:** Nach Übernahme von #72
  D0-G1–G5 in #28/D1/D2/#30/#31/D3/D4 erfüllen. D1/D2 können nun gegen den
  integrierten Vertrag konkretisiert werden; die Zugangsspur #27 → #28 → #37
  bleibt eigenständig. Kein Beginn dieser Folgearbeiten durch den Mergeauftrag.
  D1–D4 bleiben offen; App-Messjournal und Funkprobe schließen
  weder dauerhaften Box-Spielfortschritt noch Veranstaltungsbetrieb ab.

- **LED-Architektenrolle 2.0 erprobt und integriert:** Für weitere Effektarbeit
  [Rolle](../docs/led/rollen/led-effekt-architekt.md), [Live-Anleitung](../docs/led/interactive-development.md)
  und [Näheglut-Bericht](../docs/led/naeheglut-live-results-2026-10-18.md) wiederverwenden.
  Konkreter Spieleinsatz/Nähequelle, Produktadapter mit Epochen-/Profilgrenzen,
  produktseitige Mischung sowie Nutzerurteil am realen Licht bleiben offen.
  Der Laborprototyp schließt #29/#31/#37 oder die optische Abnahme nicht ab.
- **D1-A separat in Review:** [PR #73](https://github.com/Sven401/Larp-Escape/pull/73)
  ist weiterhin offen; vor einer eigenen Integration gegen die dann aktuelle
  Basis prüfen. D1-B/C und #60 bleiben offene Folgearbeit.

- **Gemeinsame Geräteabnahme offen:** passendes V2-App-/Firmwarepaar gezielt
  prüfen. Historischer RSSI-Beleg: App-Journalstand `6e6f093` und
  Firmware `e012314`; der neuere GM-Pilot hat einen eigenen
  [begrenzten Gerätenachweis](../docs/companion/spielleiter-stationspilot.md).
  Historische [Installation und Gehversuch](ESPNOW-UEBERGABE-2026-10-18.md).
  Vor erneutem Gerätezugriff tatsächliches Paar feststellen. Ein Upload
  braucht eigenen Auftrag und chipgebundenen Flash-Rückweg.
- **Hardwareplanung fortgeschrieben, 18.10.2026:** [Übernahme aller acht
  Ergebnis-IDs](../Hardware/HARDWAREPLANUNG.md) mit bestätigten As-built-Pins,
  zusätzlicher Audioversorgung und gezielten Restnachweisen. Teilbefunde sind
  erhalten; elektrische/CAD-Freigabe und gemeinsamer Gerätelauf bleiben offen.
- **Matrix #54 / Hardware #39:** C3, G3, H3 und A8 im erhaltenen Sammellauf
  nachprüfen; 68 Koordinaten/110 Beobachtungen, neun unbestückte Felder und
  vier Problemstellen erhalten. Chip/B7 und räumliche Zuordnung bleiben offen.
- **LED #29/#37, NFC #35, Status/Sitzung #33/#34:** berührte V2-Bedienung und
  Fehlerfälle prüfen. Historische Teilabnahmen bleiben gültig für ihren Umfang.
  Audio-R3 (kurzes natürliches Titelende) bleibt ausdrücklich ausgenommen.
- **Allgemeiner Spielzugang #27/#28:** Bond, Rollen und Einrichtung bleiben
  eigenständige Aufgaben; der Baugruppen-HMAC-Zugang ersetzt sie nicht.
- **Spielintegration #31/#30:** zuerst F5a-Geräteausführung, dann reale
  Profilübernahme, anschließend F5b-Ressourcen-/Betriebsabnahme.
- **Notwendige zukünftige Framework-Aktualisierung:**
  [Arduino-ESP32 3.x](ARDUINO-ESP32-3-MIGRATION.md) am 18. Oktober 2026 vom
  Nutzer zur Einplanung beauftragt. Umsetzung, Zielversion und Termin offen;
  jetzt ausschließlich dokumentieren, keine Migrationsarbeiten beginnen.

Softwareintegration, begrenzte historische Gerätebeobachtung und gemeinsame
Produktabnahme sind getrennte Nachweise. Der Baugruppenstand ist kein
abgenommenes Gesamtspiel. Keine neue Geräteprüfung in der Hygienesitzung.

## Aktuelle Prüfwege und CI

**PR #79:** 9/9 PR-Jobs erfolgreich; eigene Push-CI am Merge `7e19c95`
mit 7/7 Jobs ebenfalls erfolgreich. Getrennte Belege im [Integrationsbericht](STATIONSPILOT-INTEGRATION-2026-10-19.md) und #25.

**UI/UX #78, Head `3b2f6b4`: alle drei PR-Workflows erfolgreich.**
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39164422680),
[LED/Android/Kamera](https://github.com/Sven401/Larp-Escape/actions/runs/39164422724),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/39164422681).
Die [eigene Quality-Push-CI am Merge `42853bc`](https://github.com/Sven401/Larp-Escape/actions/runs/39165151009)
war beim Dokumentationsentwurf noch aktiv. Nachfolgende Dokumentations-/Push-Läufe
werden gesondert in #25 ausgewiesen; kein übertragener grüner Status.

Der [vollständige B0-Prüfweg](../docs/quality-checks.md) enthält echte
Kryptografie, konfigurierte Hosttests, vier Legacy-/ABI-Firmwareziele,
Android Debug/Release und Quellen-/Fehlerbelege. Dazu kommen die
[gemeinsame Baugruppen-CI](../.github/workflows/led-product.yml) für Wire,
LED/Wake/Matrix/Audio/NFC/ESP-NOW, vier getrennte Produktvarianten und die
isolierte Kamera-Laborfirmware sowie die [unabhängige NFC-Probe](../.github/workflows/nfc-probe.yml)
und [ESP-NOW-Probe](../.github/workflows/espnow-probe.yml).

**Merge #74, `d4dd7c6`:** Eigene Push-Prüfungen sind
[Quality, drei Jobs](https://github.com/Sven401/Larp-Escape/actions/runs/39145590308)
und [LED/Baugruppen, vier Jobs](https://github.com/Sven401/Larp-Escape/actions/runs/39145590228).
Den abschließend ausgelesenen Status und den anschließenden Dokumentationsstand
führt [Projektplan #25](https://github.com/Sven401/Larp-Escape/issues/25).
[Integrationsbeleg mit getrennten PR-Nachweisen](LED-EFFEKT-ARCHITEKT-INTEGRATION-2026-10-18.md).
Es gab bei diesem Merge-/Dokumentationsauftrag keinen Gerätezugriff oder Upload.

**Historisch am Zielcommit `784984d`: alle acht Push-Jobs erfolgreich.**
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/38980094469),
[Baugruppen](https://github.com/Sven401/Larp-Escape/actions/runs/38980094470),
[NFC-Probe](https://github.com/Sven401/Larp-Escape/actions/runs/38980094423).
Das sind Läufe des tatsächlichen Mergecommits. Alte PR-Läufe bleiben historische
Belege. Nach Änderungen den neuen Head und PR-Prüfmerge gesondert bewerten.

Lokal ohne Hardware: die Befehle in [quality-checks.md](../docs/quality-checks.md)
und [Android-Anleitung](../companion/android/README.md) verwenden. Fehlende
Werkzeuge oder Sandboxfehler sind weder bestandene Tests noch Produktfehler.

## Historischer Prüfstand der Übernahme vom 14. Oktober 2026

- **Historisch belegt:** [Quality-Run 38470091132](https://github.com/Sven401/Larp-Escape/actions/runs/38470091132)
  auf Diagnosearchiv-Commit `8e1fc9ab` war erfolgreich. Der Vergleich vollständiger
  Git-Bäume am 13. Oktober 2026 zeigte gegenüber `42610abf` ausschließlich sechs
  zusätzliche Archivdateien unter `handoff/`; Produktdateien und Workflow waren
  identisch. Das ist kein CI-Lauf auf dem neuen Maintainer-Commit.
- **Damals verwendeter CI-Prüfort:** Die Checks des
  [Maintainer-PRs](https://github.com/Sven401/Larp-Escape/pulls?q=is%3Apr+head%3Acodex%2Fmaintainer-uebernahme)
  liefern das Ergebnis am jeweiligen veröffentlichten Commit. Nach einem neuen
  Commit dessen eigenen Quality-Lauf auswerten; einen früheren grünen Lauf
  nicht auf den neuen Stand übertragen.
- **Gemessen am 14. Oktober 2026:** Android-Aufruf oben mit `--offline --no-daemon`
  auf Basiscommit `42610abfef07c971d9902ea6ca5a350ed68981bf`, JDK 21.0.8 und
  Gradle 8.9 erfolgreich; 58 Aufgaben ausgeführt. 45 JVM-Fälle bestanden
  (29 Protokollkern, 16 App), keine Fehler oder ausgelassenen Tests; Debug-APK
  gebaut. Lint meldet null Fehler und drei bestehende Warnungen:
  `OldTargetApi`, `DataExtractionRules`, `MissingApplicationIcon`.
  Belege sind die erzeugten JUnit-XML-Dateien unter
  `companion/android/{core/protocol,app}/build/test-results/` und
  `companion/android/app/build/reports/lint-results-debug.html`.
  Diese Buildausgaben sind lokal regenerierbar und werden nicht versioniert.
  Der vorhandene Gradlecache genügte für den Offline-Lauf; der anfängliche
  Sandbox-Schreibfehler wurde durch genehmigten Cachezugriff behoben.
  Kein Handy-, Bluetooth-, NFC- oder sonstiger Hardwarelauf wurde durchgeführt.
- **Am 14. Oktober noch offen, durch B0 inzwischen belegt:** Ein erneuter echter Kryptografie-Hostnachweis benötigt
  das gepinnte Mbed-TLS-2.28.4-Archiv und den Quellpfad gemäß
  [native_peer_mbedtls.py](../scripts/native_peer_mbedtls.py).

## Branchschutz und spätere Integration nach main

**Ausgelesen am 18. Oktober 2026:** Das aktive
[Ruleset „Main“](https://github.com/Sven401/Larp-Escape/rules/24680694)
gilt für den Default-Branch. Es sperrt Updates ohne Bypass und verlangt
signierte Commits, lineare Historie sowie einen Review, Code-Owner-Review,
Freigabe des letzten Pushs und aufgelöste Reviewthreads. Erlaubte Methode
im Ruleset: Squash. Die Liste konkret erforderlicher Statuschecks ist leer.
Repositoryweit sind Merge, Squash und Rebase aktiviert; Auto-Merge ist aus.

Der kuratierte Integrationsbranch wird von diesem Ruleset nicht erfasst und
wird von GitHub als ungeschützt gemeldet. CI läuft, wird dort aber durch den
sichtbaren Regelsatz nicht erzwungen. Eine passende Review-/Check-Policy für
diesen Branch ist als Entscheidung vorgemerkt; Schutzregeln wurden nicht geändert.
Die zusätzliche klassische Branch-Protection-Abfrage ist mit der App-Verbindung
nicht zugänglich (403); das ist keine Aussage über deren Abwesenheit.

Vor einem beauftragten späteren Main-Update Regelsätze neu prüfen und die
Update-Sperre fachlich klären. Diese Hygiene ändert weder Main noch Regeln.
