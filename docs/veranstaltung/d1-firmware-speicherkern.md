# D1-B2: begrenzter Firmware-Speicherkern

Status: **begrenzter Softwarekern implementiert**, 31.10.2026. Beauftragte Teilmenge von
[#60](https://github.com/Sven401/Larp-Escape/issues/60), abgeleitet aus
[D1 v1, Abschnitte 2–4 und Tor B2](d1-persistenz-v1.md). D1-A ist integriert.
Dieses Paket liefert eine atomare Transaktionsablage, noch keinen vollständigen
D1-Reducer, D0-Verifizierer oder F5a-Koordinator. Profilpersistenz bleibt getrennt.
[Arbeits- und Prüfbericht](../../handoff/D1-SPEICHERKERN-2026-10-31.md).

## Grenzen und Eigentümer

Ein Speicherworker besitzt `ProgressJournal` und `IProgressStore` exklusiv.
Die API arbeitet synchron, ohne Heapallokation, Task, Queue oder Geräteaufruf.
Sie gehört weder in Funkcallbacks/ISR noch in einen zeitkritischen Spieltick.
Gleichzeitige Aufrufe sind nicht unterstützt; wieder eintretende Aufrufe werden
mit `Busy` abgelehnt. Ein externer Besitzer muss konkurrierende Änderungen ordnen.

| Feste Implementierungsgrenze | Wert |
| --- | --- |
| Reservierter Speicher | genau 256 KiB, 64 Slots zu 4096 Byte |
| Dauerhafte Transaktionen | 63; Slot 0 enthält die Ursprungsbindung |
| Nutzdaten je Transaktion | höchstens 3072 Byte, keine Kürzung/externen Überläufe |
| Eigene Fact-Vergabe pro Commit | 0 bis 8 zusammenhängende Nummern |
| Revision und Fact-Zähler | uint64, geprüft ohne Überlauf; frischer Zähler beginnt bei 0 |
| Wiederfreigabe von Platz | keine; voll bedeutet `Full` vor Mutation |

Dies sind technische Grenzen dieser ersten Implementierung, **keine zugesagte
Veranstaltungskapazität**. Wegen der 63 unveränderlichen Records können höchstens
504 eigene Fact-Nummern vergeben werden. Keine Kompaktierung, Löschung alter
Durchläufe, externe Bootstrapübernahme oder Wiederverwendung einer Partition.
Ein Folgeentwurf muss alle D1-Replay-/Latch-/Vorgangsbelege erhalten, bevor er
Platz wieder freigeben darf. Der Kern beansprucht keine Endloszustellung.

## Transaktionsgrenze und Wiederholung

Der Header bindet die drei nichtnull 128-Bit-Werte `event`, eigene `box` und
`epoch`. Wiederaufnahme verlangt dieselbe ausdrücklich übergebene Bindung;
Run-/Paketwechsel dürfen nicht durch Neuinitialisierung den Zähler zurücksetzen.
Ein Datensatz bindet Vorgangskennung, Anfragedigest, Art, erwartete Fortschritts-
und Zählerstände, Profil-/Vertrauensrevision, Vergabeanzahl und sämtliche Nutzbytes.
Eine unveränderte Wiederholung liefert `Duplicate` mit der ursprünglichen
Revision/Nummernspanne, ohne Write. Dieselbe Vorgangskennung mit anderem Inhalt
liefert `Conflict`, auch bei unverändert behauptetem Digest.

`Received`, `Application`, `EffectClaim`, `EffectResult` und `Control` benennen
getrennte Transaktionsarten. Der Typ allein beweist **keine** fachliche Annahme,
vollständige Anwendung oder beobachtete Gerätewirkung. Insbesondere ist ein
Vorgangs-Replay kein vollständiger D0-Fact-/Stream-/Latch-Replayschutz.
`readRecord` liefert nach erfolgreicher Prüfung kopierte Metadaten/Nutzdaten für
den späteren Reducer; der Verbraucher darf erst bei `Read` damit arbeiten.

Ein vollständiger Transaktionsplan muss alle zugehörigen Kontextdaten,
Eingangsbelege, Latches, lokalen Zustandsänderungen, Folgefakten, Outboxziele und
Effektintents enthalten. Seine kanonische Kodierung und Semantik bleiben der
Integration vorbehalten. Passt er nicht in einen Record, muss der Besitzer ihn
vor Übernahme ablehnen; Aufteilen in bereits bestätigte Teilpläne ist unzulässig.
Eigene IDs werden aus `expectedLastIssued + 1 .. + issueCount` vorhergesagt und
gemeinsam mit den Nutzdaten gespeichert. Vor `Committed` oder bestätigtem
`Duplicate` dürfen sie weder veröffentlicht noch als Erfolg gemeldet werden.

Der Commit validiert das Medium, friert Anfrage und Nutzdaten ein und ruft direkt
vor dem ersten Write `IProgressCommitFence::allow` auf. F5a muss diese Freigabe an
den vollständigen Plan und aktuelle Rechte-/Profil-/Kontextrevisionen binden und
Änderungen bis zum geklärten Ausgang ordnen. Der Callback ist **keine** Umsetzung
der taskübergreifenden D1-Rechtefence. Ein gespeichertes `authorized=true` genügt
nicht. Ein `EffectClaim` muss vor dem Geräteaufruf bestätigt sein; ein späterer
Reducer muss Claims ohne Ergebnis als unklar behandeln und darf sie nicht
selbsttätig erneut ausführen.

## Medium, Übernahmepunkt und Fehler

`initialize` ist ein expliziter Einrichtungsaufruf, kein Boot-Fallback. Er prüft
alle 256 KiB auf `0xff`, bevor der Header geschrieben wird. `recover` initialisiert
niemals. Ein fehlender/ungeöffneter Port ist kein leeres Medium.

Jeder Slot besitzt ein explizites Little-Endian-Format, Versionskennung,
Prüfsumme und Abschlussmarker. Records bilden eine geprüfte Revision-/Zähler-
und Vorgänger-Prüfsummenkette. CRC32 erkennt zufällige Beschädigung, liefert aber
keine Authentifizierung und keinen Schutz gegen das Zurückspielen eines ganzen
alten Flashabbilds. Dafür bleibt D0-ID mit bewusstem Recovery maßgeblich.

Vor jedem Record schreibt der Kern ein eigenes Belegbyte im Header genau einmal
von `0xff` auf `0x00` und synchronisiert. Danach folgen Slotkörper und Sync,
Abschlussmarker und Sync sowie ein vollständiger Kontrollscan. Erst danach wird
`Committed` sichtbar. Die 63 Belegbytes sind vom Header-CRC ausgenommen, werden
aber als lückenloses Präfix gegen die vollständigen Records geprüft. Ein komplett
auf `0xff` verlorener letzter Record bleibt durch seinen getrennten Beleg sichtbar.
Verliert eine bereits geöffnete Instanz das ganze Medium, blockiert auch ihr
bekannter RAM-Stand eine neue Initialisierung. Das ersetzt keinen monotonen
Anker nach einem vollständigen Flashrollback und anschließendem Neustart.
Jeder unklare Programmier-/Synchronisierungsausgang sperrt die Instanz dauerhaft
(`WriteUncertain`). Eine neue Instanz darf erst nach echter Wiederöffnung des
Backends bzw. Neustart das dauerhafte Medium prüfen; eine noch lesbare Schreib-
cachekopie ist keine neue Durabilitätsbestätigung.

Wiederaufnahme prüft die **ganze** Reserve, nicht nur bis zur ersten leeren Seite.
Beschädigter Header, unbekannte Version, ungültige Kette, schmutziger Tail und
ein belegter Slot hinter einer Lücke sperren den Speicher. Es wird weder ein
älterer Stand bestätigt noch ein abgebrochener Eintrag still abgeschnitten.
Ein Abbruch vor jeder Änderung lässt den vorherigen vollständigen Stand zu;
ein vollständiger neuer Record kann nach echter Wiederaufnahme angenommen werden;
ein beschädigter Zwischenstand bleibt geschlossen. Das priorisiert den Erhalt
bestätigten Fortschritts vor automatischer Verfügbarkeit.

## Konkreter ESP-Port

`Esp32ProgressPartition` verwendet die native ESP-IDF-Partitions-API am im
Repository qualifizierten Wartungspin. Er sucht ausschließlich `larp_progress`,
Typ `data`, Subtyp `undefined` (0x06), exakt 256 KiB, 4096-Byte-Ausrichtung und
Erasegeometrie. Verschlüsselte oder schreibgeschützte Partitionen werden abgelehnt.
Der Adapter prüft das Schreibziel auf vollständig gelöschte Bytes, programmiert
synchron und liest zurück. Es gibt keine Datei-/NVS-Pufferung und keinen eigenen
Partition-Flush-Aufruf. `sync()` bestätigt den fehlerfreien Abschluss dieser
synchronen Aufrufe, keine hier schon gemessene physische Stromausfallgarantie.
Herstellerbasis: [gepinntes Partitions-API](https://github.com/espressif/esp-idf/blob/9a97f6c54ec638111ce55cd36581b3c192f15207/components/esp_partition/include/esp_partition.h).

Der native Qualifikationsbuild kompiliert und verlinkt den echten Adapter. Sein
Start fragt nur die Kapazität des **ungeöffneten** Adapters ab. Es wird keine
Partition geöffnet, gelesen, angelegt oder gelöscht. Die vorhandenen
Partitionstabellen bleiben unverändert; ohne gesonderte Bereitstellung gibt es
noch keine nutzbare Produktpartition. Layoutwahl, Migration und Gerätefreigabe
sind Folgearbeit, keine stillschweigende Speicherumwidmung.

## Begründete Softwarebudgets

Der Kern liest bei neuer Übernahme höchstens zweimal 256 KiB, programmiert
4097 Byte (Beleg, Slotkörper und Marker) und verlangt drei Synchronisierungen.
Wiederaufnahme liest 256 KiB; Recordlesen oder unverändertes Replay höchstens
260 KiB. Der Adapter liest zusätzlich die jeweiligen Schreibbereiche vor und
nach dem synchronen Schreiben. Das sind logische API-Bytes, keine gemessene
physische Write-Amplifikation, Lebensdauer oder Latenzzusage. Der Kern prüft
höchstens 63 Records und deren Kennungen; keine unbeschränkte Suche/Allokation.
Konkrete Objektgrößen und ausgeführte Prüfergebnisse stehen im Arbeitsbericht.

## Anschluss an parallele Arbeiten

- **D0/#28:** verifizierte kanonische Factbytes und echte Inhaltsdigests liefern;
  ungeprüfte Eingänge erreichen den Speicher nicht. Fact-ID-/Streamkonflikte,
  Lücken, historische Kontexte und Quittungsbindung dauerhaft reduzieren.
- **#93/F5a:** vollständigen Mehrregelplan in eine begrenzte Transaktion kodieren,
  aktuelle Revisionen und Rechte koordinieren, nach Recovery alle Records zuerst
  prüfen/reduzieren und erst danach Spielzustand freigeben. Latches unabhängig
  von auslösendem Fakt/Paket erhalten. Claims ohne Ergebnis bleiben unklar.
- **D2/#61:** bestätigte Outboxdaten wiederholen; Transporterfolg ist keine
  Zielannahme. Vorgangs- oder Empfangsduplikate lösen keine zweite Anwendung aus.
- **D1-B4/C:** tatsächliche Worker-/Queue-/Stack-/Zeitbudgets, Flashaufteilung,
  Schreibverstärkung, Verschleiß, physische Stromunterbrechungen und kombinierte
  Last nachweisen. Hier weder Gerätezugriff noch physischer Abnahmenachweis.

Die integrierte SQLite-Rechnerreferenz bleibt semantische Referenz für diese
Anschlussarbeiten. Sie wird weder portiert noch durch dieses Primitive ersetzt.
#60 und die Produktabnahmekriterien bleiben offen.
