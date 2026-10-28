# Zweiter Fachaudit-Probelauf und Vorbereitung des Wochenlaufs

**Status: manueller Zweitlauf abgeschlossen; begrenzte Wiederholungsprobe
übereinstimmend; regelmäßiger Auftrag pausiert vorbereitet.**
24.10.2026, Europe/Berlin. Nutzerauftrag in Sitzung
`01a16ab1-34d7-73b0-924a-fc2d245b9027`.

## Basis, Methode und Zeit

Geprüft wurde der integrierte Commit
**`cee968a7bf4c97eb68c9fe6f705bb5a06eef76b0`**, Git-Baum
`6c5529feaec6a9d8eddd3985bfbc73dc9096545c`.
Remote-Head und Arbeitsbaum wurden vor und nach der Recherche abgeglichen.
Der erfasste Prüfzeitraum war 01:24:25–01:36:07 MESZ (23:24:25–23:36:07 UTC
am Vortag), damit innerhalb des 30-Minuten-Budgets. Die anschließende
Dokumentation/Automationsvorbereitung ist ein eigener Arbeitsschritt.

Der [manuelle Auftrag](../../fachaudit-entwurf.md#8-dateien-pilotauftrag-und-rückweg)
blieb unverändert. Der historische Erstpilot hatte aber den inzwischen
korrigierten Code auf `e61d7642`. Deshalb gilt **nicht** die Behauptung,
beide Sitzungen hätten identische Produkteingänge gehabt.

Zusätzlich wurden die heutigen Eingänge eingefroren und zweimal zugeordnet:

- **2A:** Maintainer wertet die aktuellen Fachbeiträge von `official_sources`
  (Plattform) und `repo_automation` (Geräteintegration) aus.
- **2B:** `responsibility_map` wiederholt unabhängig die Zuordnung aus
  denselben Eingängen, ohne 2A einzusehen oder erneut Quellen abzurufen.
- Der Maintainer vergleicht danach Identitäten, Mandate, Statusgrenzen und
  Wiedervorlagen. Es ist eine manuelle Wiederholungsprobe, kein ausgeführter
  Scheduler und keine automatische Idempotenzprüfung.

[Eingangsmanifest](eingang.json), [beide Zuordnungen und Gegenfälle](pruefung.json)
und [Startregister nach der anschließenden Zuordnung](register.json) bleiben
getrennt. Manifest-SHA256:
`3834d224e143391ecb86f902e065def5446a9e3204f013127158db14e845f529`.
Alle 14 eingetragenen Quelldatei-Hashes wurden von 2B bestätigt.

Der Auftragshash `3845018f2c35ff046a8c11a0f33f0619709e483a25ccdb3eb7238c04841965c9`
wird aus UTF-8-Text **nach**
`**Wiederverwendbarer Text des manuellen Auditauftrags:**` und **vor**
`## 9.` gebildet: Rand-Leerzeilen und Blockquote-Präfixe erhalten,
CRLF nach LF normalisieren, UTF-8 SHA256. Der anfangs unklare Hashzuschnitt
wurde mit dieser Regel unabhängig reproduziert; das Manifest blieb unverändert.

## Dubletten, Verantwortung und Wiedervorlage

**Ergebnis: fünf vorhandene Finding-IDs und ein vorhandener Hardwarefall
wiedererkannt; null neue oder doppelte Finding-IDs; alle sechs führenden
Mandate übereinstimmend.** Softwarestatus und offene Abnahmegrenzen stimmen
inhaltlich ebenfalls überein.

| Fall | Führendes Mandat | Ergebnis und nächste Wiedervorlage |
| --- | --- | --- |
| A1-01 | Plattform/Systemintegration | Software verifiziert. Bei Task-/Queue-/Spinlock-/Callback-/ISR-Änderung erneut prüfen; vor produktiver Kanalübernahme fehlen SMP-/ISR-/Latenzbelege. |
| A1-02 | Plattform, Maintainer; gezielter Security-Review | Supportentscheidung offen. Nächster beauftragter Planungs-/Migrationsabgleich, spätestens Produktfreigabe oder Pinänderung. Keine Ausnahme oder Zielversion erfunden. |
| A2-01 | Geräteintegration/Hardwarevertrag | Software verifiziert. Vor Legacy-Wake-Freigabe/Adapterübernahme sowie bei Sensor-/Fehlergrenzenänderung; reale Busunterbrechung und elektrische Recovery offen. |
| A2-02 | Geräteintegration, Qualität | Parser-/Patchregression verifiziert; Geräte-Rest offen. Beide Durchgänge erkennen die fehlende explizite Wiedervorlage für diesen Rest. |
| A2-03 | Geräteintegration, Plattform für Frist | Vertragspräzisierung umgesetzt; vor jeweiliger Adapterfreigabe vollständigen Transport-/Zeitpfad prüfen. Bool-/Timeout-Zusagen bleiben begrenzt. |
| #39/B7 | Geräteintegration mit Hardware/Firmware | Bekannter Fall: Wake `0x20/B7` und Matrix `0x21/B7` getrennt identifizieren. Nächster H0-Zuordnungsauftrag, nötiger Pinvertrag vor Netzliste/Layout. #54 bleibt der separate Kontaktfall. |

**Anschließende Maintainer-Zuordnung:** Der Geräte-Rest aus A2-02 wird im
Startregister ausdrücklich mit dem bereits vorhandenen Adapterfreigabetor
aus A2-03 verknüpft. Das behebt die Zuordnungslücke der Vorbereitung; es
behauptet weder einen neuen Test noch ein neues Abnahmekriterium oder Datum.
Die unveränderten Beobachtungen 2A/2B bewahren die zuvor gefundene Lücke.

Für neue Folgeaufträge muss der Maintainer konkrete Bearbeiter und andere
Gegenprüfer besetzen. Ein leeres GitHub-Assignee-Feld in #39/#54 wird nicht
als neue fachliche Rolle oder bereits beauftragter Gerätelauf interpretiert.

## Quellen- und Projektabgleich

Das Manifest enthält 18 begrenzte Primärquelleneinträge: 17 gelesen,
ein Supportartikel weiterhin nicht lesbar. Es kopiert keine vollständigen
fremden Handbücher. Revisions-/Abrufangaben und Beobachtung stehen je Quelle.

- **Microchip:** DS20001952D ist jetzt erreichbar; Seite 1 bestätigt die
  Herstellergrenze der beiden IO7-Pins des MCP23017. Der Supportartikel liefert
  weiterhin Loading/CSS Error. Dies verengt die Quellenlücke des bekannten
  Falls #39; Chipidentität, Verdrahtung und elektrische Anwendbarkeit bleiben
  offen. Historische fehlgeschlagene Abrufe werden nicht umgeschrieben.
- **Adafruit:** Frische Release-/Versionsquellen bestätigen die vorhandenen
  Pins PN532 1.3.4, MCP23017 2.3.2 und BusIO 1.17.4. Skalare MCP-Fehlerwerte
  und SPI-Bool-Semantik werden erneut erkannt, nicht als neue Produktfehler
  angelegt. Echte Original-/Patchhashes wurden lesend geprüft.
- **ESP32:** Versionspassende Portregel und IDF-4.4-EOL erneut bestätigt.
  Aktuelle Arduino-Releases/Prerelease bleiben getrennt von einer noch nicht
  ausgewählten Migrationsbasis. Ein zusätzlicher A2DP-Advisoryhinweis gehört
  zum bestehenden A1-02-Prüfauftrag; Konfiguration allein belegt keine erreichbare
  Schwachstelle. Kein vollständiger Binary-/Advisoryausschluss.
- **Planung:** #25 ist gelesen; SP-01 ist beschlossen, PR #94 bleibt offen.
  Beschlossenes Soll wird nicht als integrierter Produktcode ausgegeben.
  PR #95/#96 sind integriert. Der vorhandene GitHub-Abgleich bleibt unverändert.

Die sieben manuellen Gegenfälle decken falsches SDK, WLED als vermeintliche
Dependency, Draft/Prerelease als Final, unzugängliche Quelle, B7-Dublette,
HEAD-Wechsel und fehlende Gerätemessung ab. Sie sind in `pruefung.json`
mit beobachteter Behandlung dokumentiert. Kein künstlicher HEAD-Wechsel,
kein Softwaretest und kein neuer NIST-/Normenvolltextaudit wurde dafür ausgeführt.

## Nachweise und Nutzungsbewertung

Die eigene [Merge-CI](https://github.com/Sven401/Larp-Escape/actions/runs/40038277888)
auf `cee968a7` ist mit drei Jobs erfolgreich; sie wurde im vorausgehenden
CI-Auftrag ausgelesen und hier als vorhandener Beleg verwendet.
Dieser Audit führt keine Tests/Builds oder Geräteaktionen erneut aus.

Die Dokumentprüfung bestätigt 14 Quellhashes gegen den ursprünglichen
Commit, den unveränderten Auftrag, drei gültige JSON-Dateien und 124
vorhandene lokale Markdown-Verweisziele. Linkanker wurden dabei nicht
separat geprüft. Der gespeicherte Automationsprompt stimmt exakt mit der
vorbereiteten Fassung überein. Das unabhängige Abschlussreview fand keine
weiteren Beanstandungen; auch die spätere A2-02-Zuordnung bleibt klar von
den unveränderten Prüfdurchgängen getrennt.

Beobachteter Nutzen: stabile Wiedererkennung trotz historischer offener
Pilotabsätze, konkrete Wiedervorlagelücke gefunden und zugeordnet,
zugängliche/fehlende Quellen und Software-/Gerätebelege getrennt.
Null neue IDs ist allein kein Erfolgskriterium. Langfristige Fehlalarmrate,
Zeit bis Fachentscheidung und Zuverlässigkeit des Wochenbetriebs sind offen.

Das manuelle Erprobungstor ist mit dieser begrenzten Aussage erfüllt.
Ein vollständiges Komponenten-/Transitivenregister, A3–A6-Recherche,
Normauswahl, Hardware-/Timing-/Security-Folgearbeit und operative
Schedulerprüfung bleiben offen.

## Vorbereitete Automatisierung und Rückweg

**`larp-escape-fachaudit` / „Larp-Escape Fachaudit“** wurde nach unabhängigem
Promptreview als **Heartbeat im bestehenden Chat, Status PAUSED**, angelegt
und zurückgelesen. Vorgesehen ist Montag 10:00; lokal ist Berlin als
Windows-Zone eingestellt. Nächster hypothetischer Termin nach heutiger
Vorbereitung: 26.10.2026 10:00 MEZ = 09:00 UTC. Keine Aktivierung oder
Schedulerzustellung erfolgt; tatsächlichen nächsten Termin vor Aktivierung
in der App verifizieren.

[Auftrag](../../fachaudit-automation-prompt.md) und
[Betriebsvorbereitung](../../fachaudit-betrieb.md) begrenzen den Lauf auf
Recherche und eigene lokale Laufentwürfe. Zwei unabhängige Reviewhinweise
wurden vor Anlage korrigiert: Anfangszustand datieren und neuere Entscheidungen/
Codebefunde voranstellen; bloßes Ende eines unveränderten Laufs bleibt still.
Danach keine verbleibenden substanziellen Promptbefunde.

Gespeicherter Prompt-SHA256 (UTF-8, ohne abschließenden Dokument-Zeilenumbruch):
`84216bd07c59a4912903a4829bbe44d6e6a3a5ae62a835beb00af51635de1943`.
Exakter Vergleich mit dem zurückgelesenen Automationsprompt erfolgreich.
Der bestehende aktive GitHub-Abgleich behält seinen Prompt-Hash
`ed34b0a306432fef0b2500f31e6e43a5904bb6c42094183a5a7072f9d35cf9a0`.

Vor Aktivierung Vorbereitung integrieren und Termin/Zeitzone bestätigen.
Rückweg ist Pausieren desselben Auftrags; Mandate, CI, GitHub-Abgleich und
Beleggeschichte bleiben erhalten. Nach vier tatsächlich geplanten Läufen
Nutzen/Takt prüfen. Kein automatischer Produkt-, Rollen- oder GitHub-Schreibauftrag.
