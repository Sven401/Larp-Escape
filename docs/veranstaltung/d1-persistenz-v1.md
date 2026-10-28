# D1 v1: dauerhafter Fortschritt, Anwendung und Abgleich

**Status: ausgearbeiteter technischer Vorschlag zu #60 mit ausführbarer
Rechnerreferenz; kein Firmwareanschluss oder physischer Speichernachweis.**
Stand 18.10.2026. Ausgangsbasis nach Fetch:
`dead12040c196ff74ab4be17ba71451a9351a676` (D0 über #72 integriert).
Auftrag: D1 auf Basis des integrierten D0-Vertrags ausarbeiten, ausschließlich
ohne direkten Gerätezugriff. Quelle: Nutzerauftrag in Sitzung
`01a14f48-8628-7930-a8b6-78f3b955a13b`.

Maßgeblich: [D0-Identität/Anwendung](d0-vertrag-v1.md),
[D0-Sicherheit/Migration](d0-sicherheit-migration-v1.md),
[VA-01](../veranstaltungsarchitektur.md) und
[Zugangskonzept](../companion/veranstaltungszugang.md).
Dieser Vorschlag ändert weder D0 noch die Zugangsrollen, Wirebytes, Profile oder
das NFC-Testformat. Erst seine beauftragte Integration übernimmt ihn ins Soll.

## 1. Lieferumfang und Besitz

D1 wird für die Abnahme in drei Teile getrennt:

| Teil | Ergebnis / heutiger Stand |
| --- | --- |
| D1-A · Vertrag und Rechnerreferenz | Hier ausgearbeitet; zustandsbehaftete, dateibasierte Transaktionsreferenz samt Prozessabbruchtests. Simulierte Verifizierer-/Rechte-/Profilbeobachtungen, keine Produktfunktion. |
| D1-B · Firmware-/F5a-Anschluss | Offen: begrenzter Speicherport und konkretes ESP-Speicherformat, vollständige Profilpläne, echtes D0-Verifizieren/#28, Anwendungseigentümer, Ressourcen-/Zeitbudgets. Auch hardwarefreie Implementierungsarbeit ist damit noch nicht als erledigt behauptet. |
| D1-C · Speicher-/Betriebsabnahme | Offen: echte Neustarts/Stromunterbrechungen, Flashverschleiß, beschädigte Medien, Last und Gerätewirkungen; eigener Geräteauftrag erforderlich. |

Das [Profiljournal](../configuration/profile-journal.md) bleibt unverändert für
Profile zuständig. Sein zulässiger Rückfall auf ein älteres Profil ist **kein**
Rückfallrecht für bereits bestätigten Fortschritt. Rechte-/Schlüsselspeicher und
Profiljournal werden nicht in ein vermeintlich atomar gemeinsam speicherndes
Objekt umgedeutet. Ihr gemeinsamer Ordnungsvertrag liegt beim Koordinator.

Ein Anwendungseigentümer aus #31 F5a entscheidet über Fakten und vollständige
lokale Übergangspläne. Ein Speicherworker besitzt den Fortschrittsspeicher.
Funkcallbacks kopieren nur begrenzte Werte in eine Eingangsqueue. Sie öffnen
keine Datei, führen keine Spiellogik aus und bestätigen keine dauerhafte Annahme.
Der Worker wartet weder auf Funk noch auf eine Zentrale. Die synchrone API der
Rechnerreferenz ist keine in einen Spieltick einzubauende SQLite-/Dateioperation.

## 2. Dauerhafter Zustand und Vergabe

| Bereich | Schlüssel und unverzichtbarer Inhalt |
| --- | --- |
| Aktiver Kontext | Veranstaltung, eigene Box/Epoche, Durchlauf, Manifestdigest, Station/Bindung samt Generation und lokale Fortschrittsrevision |
| Eigene Quelle | Letzte dauerhaft vergebene uint64-Sequenz je event/Box/Epoche; kein Boot-/Lease-Zähler |
| Eigener Fortschritt | Monotone lokale Ergebnisse je event/run/Station/Objekt; bleiben nach Freigabe der Ausgangsnutzdaten erhalten |
| Eingangsbelege | Vollständige verifizierte Fakten, D0-Inhaltsdigest, Kontext und Status `pending`, `applied`, `history` oder `control` |
| Streambelege | Je Ursprung/Epoche bewiesener Präfixboden, vorhandene Positionen, authentifizierte unbenutzte Bereiche, persistente Konfliktsperre; Maximum allein reicht nicht |
| Freigaben | Latch `(event, run, localStation, ruleId)` samt Auslöser/Digest und ausgeführter Regelgeneration; kein neuer Latch bei anderem Fakt, Paket oder Regelgeneration |
| Ausgangsablage | Unveränderlicher eigener Fakt mit vorab festgelegten Zielboxen und deren getrennten Empfangs-/Ergebnisständen |
| Lokale Vorgänge | Stabile Vorgangskennung, vollständige Anfrage-/Kontextbindung und zugewiesene Fact-ID/Digest; verhindert Neuvergabe nach verlorener Antwort |
| Effekte | Stabile Intentkennung, Aktion/Ressource, Profil-/Kontext-/Ursprungsbindung und `prepared`, `attempted`, `observed`, `unknown` oder `cancelled` |

**Gewählter Vergabeweg:** die von D0-ID ausdrücklich erlaubte Vergabe pro Fakt,
ohne reservierte Nummernblöcke. Eine Transaktion übernimmt lokale Zustandsänderung,
`lastIssued+1`, vollständigen Fakt, Vorgangsbeleg und Ausgangsablage gemeinsam.
Vor bestätigtem Commit wird weder ID/Fakt veröffentlicht noch Erfolg gemeldet.
Ein nur im RAM konstruierter Kandidat ist noch keine vergebene dauerhafte ID.
Nach einem vollständig verlorenen Vor-Commit-Versuch darf deshalb derselbe
Kandidat erneut konstruiert werden; eine dauerhaft vergebene ID wird nie für
anderen Inhalt eingesetzt. Verlorene Antwort nach Commit: denselben gebundenen
Vorgang nachlesen, keine neue Nummer vergeben. Bei unklarem Speicherausgang keine
weitere Vergabe bis zur geklärten Wiederaufnahme.

UINT64_MAX sperrt weitere Vergabe; weder Wrap noch spontane neue Epoche.
Run-/Paket-/Regelwechsel setzen Quelle und Streambeleg nicht zurück. Fremde,
verifiziert erzeugte `unused_range`-Fakten bleiben für den D0-Abgleich unterstützt,
obwohl dieser eigene Allocator normalerweise keine Reservierungslücken erzeugt.
Ein vollständiger alter Flashrestore bleibt die Rollbackgrenze aus D0-ID; ohne
nachgewiesenen monotonen Anker ist bewusstes Recovery mit neuer zugelassener
Ursprungsinkarnation nötig. Ein zweites Profil oder Backup löst das nicht.

## 3. Drei Übernahmepunkte

| Vorgang | Atomar zu übernehmen | Zulässige Antwort / nach Absturz |
| --- | --- | --- |
| Verifizierten Eingang aufnehmen | Inhalt/Digest, Kontext, Replay-/Streambeleg und wartende Anwendung oder reine historische/control-Aufnahme | Erst danach `durable_received`. Ein gleiches Duplikat nennt den bestehenden Stand; `pending` ist kein `applied`. |
| Vollständigen Anwendungsplan übernehmen | Erwartete Fortschritts-/Profil-/Rechterevision, alle betroffenen Latches, fachlicher Zielzustand, neue eigene Fakten samt Vergabe/Outbox und vorbereitete Effektintents | Erst danach `applied`. Keine teilweise Freigabe bei fehlender Abhängigkeit, vollem Speicher oder ungültigem Teilplan. Wiederanlauf übernimmt den gespeicherten Zustand und arbeitet verbleibende `pending`-Eingänge erneut gegen aktuelle Rechte ab. |
| Physische Ausführung beanspruchen | Eindeutiger Intent wird vor Übergabe an den Gerätebesitzer zu `attempted` | Ein Absturz danach ergibt `unknown`, auch wenn der Geräteaufruf womöglich noch nicht begonnen hatte. Kein automatischer zweiter Aufruf. Nur ein eigener belastbarer Wirkungsbeleg erlaubt `observed`. |

Fehlende AND-Abhängigkeiten und geordnete Regeln mit Lücke bleiben wartend.
Eine ausdrücklich konfigurierte monotone Regel darf trotz fremder Lücke arbeiten,
wenn ihre konkreten Abhängigkeiten vorliegen. Historische Fakten können nur den
Streambeleg füllen; sie erfüllen keine aktuellen Profilbedingungen. `control`
schließt nur authentifizierte Lücken und erzeugt keine Spielwirkung.

Eine lokale Ersatzfreigabe benötigt separat aktuelle Spielleiterrechte oder eine
ausdrücklich konfigurierte lokale Ersatzregel. Ihr eigener Fact und derselbe
Ziel-Latch werden gemeinsam übernommen. Ein späteres Original hat dadurch keine
zweite Wirkung. Es gibt keinen Netzwerkparameter „Abhängigkeiten ignorieren“.

Alle auf denselben Eingang passenden Regeln gehören in **einen vollständigen
Plan**; `applied` darf nicht nach der ersten Regel eines unvollständigen Plans
gemeldet werden. Die Referenz zeigt Einzelpläne und eine atomare Mehrregel-API;
die Ableitung des vollständigen Plans aus dem wirklichen Profil bleibt D1-B.

## 4. Rechtefence, Queue und Anwendungseigentümer

Ein Vorschlag bindet event/run/Paket, ursprüngliche Fact-IDs/Digests, erwartete
Fortschrittsrevision, Profilrevision, Vertrauensrevision und seine vollständigen
Regel-/Ressourcenbezüge. Die Produktionsintegration erhält folgende Grenzen:

1. Der Eigentümer erzeugt höchstens einen aktiven dauerhaften Übergangsplan.
   Er ordnet nachfolgende konkurrierende Zustandsänderungen dahinter ein.
   Sensor-/Geräteservice bleibt bedienbar; eine vorbereitete RAM-Änderung ist
   noch kein sichtbarer verbindlicher Spielzustand und erzeugt keinen Effekt.
2. Der Worker bereitet die begrenzte Übernahme vor. Vor seinem endgültigen
   Commit erteilt der Eigentümer eine an genau Plan und Revisionen gebundene
   Commitfreigabe. Rechteänderungen, Profilaktivierung und D1-Commits werden
   durch denselben Koordinator geordnet. Keine zeitlich ungeschützte Kopie
   eines alten „authorized=true“ zwischen Tasks.
3. Nach dieser Freigabe darf eine konkurrierende Rechteänderung ihren eigenen
   Übernahmepunkt erst nach geklärtem D1-Commit erreichen. Sie bleibt davor
   sichtbar ausstehend. Ein vorher übernommener Entzug sperrt den Plan; ein
   danach übernommener Entzug löscht bereits angenommene Fakten nicht.
4. Vor tatsächlicher Effektbeanspruchung wird die aktuelle lokale Zulässigkeit
   erneut geprüft. Eine alte vorbereitete Wirkung nutzt nach Entzug keine alten
   Rechte. Beim neuen Durchlauf werden unbeanspruchte alte Intents abgebrochen;
   bereits beanspruchte/unklare Wirkungen bleiben separat sichtbar.
5. Ein nicht mehr passendes oder verworfenes Ergebnis darf keine andere aktive
   Sitzung, Planrevision oder Box erfüllen. Queue-/Arbeitskapazität voll:
   vor Annahme klar ablehnen/zurückstellen, keinen bestehenden Vorgang verdrängen.

Die Referenz simuliert diese Fence durch vertrauenswürdige Policycallbacks unter
einem Besitzer und prüft sie unmittelbar vor Commit erneut. Sie ist **kein**
RTOS-Implementierungsnachweis dieser fünf Regeln. Ein freigegebener Plan lässt
den Eigentümer nicht synchron auf eine Datei warten; Latenz/Queue und das Verhalten
bei dauerhaft ausfallendem Speicher brauchen begründete Produktbudgets in D1-B.

## 5. Abgleich, Outbox und Kompaktierung

Ein Streambericht nennt Ursprung/Epoche, lückenlosen belegten Präfix, zusätzliche
Intervalle, offene Bereiche, unbenutzte Abschlüsse und Kontextbezug. Listen sind
begrenzt und bei Bedarf paginiert. Das ist D1-Zustandsinhalt; das D2-Kontroll-/
Transferformat einschließlich Authentifizierung wird damit nicht still erfunden.

Jede eingehende Fact-Position wird gegen geschlossene unbenutzte Bereiche geprüft,
auch bei später Lieferung. Ein neuer Bereich darf keine belegten oder als reale
Historie verdichteten Positionen umetikettieren. Gleiche ID mit anderem bekannten
Digest und solche Bereichskonflikte sperren den Ursprungspfad dauerhaft; keine
automatische Reparatur nach numerisch größter Sequenz. Unabhängige lokale Regeln
werden davon nicht pauschal gestoppt.

Eine Zielquittung bindet Referenz, D0-Inhaltsdigest, tatsächliche Zielbox und
verifizierten Bestätiger. Ein Relay/MAC-Erfolg befreit keine Outboxdaten.
`transport_received`, `rejected`, `deferred` und `effect_unknown` implizieren
keine dauerhafte Zielannahme. Ein späteres niedrigeres Ergebnis nimmt einen
bereits belegten dauerhaften Empfang nicht zurück. Widerspruch von Ablehnung und
Erfolg wird in beiden Empfangsreihenfolgen sichtbar; Daten bleiben erhalten.
Ablehnung und dauerhafte Annahme werden unabhängig von der jeweils letzten
Zwischenmeldung behalten; auch nach Neustart darf keine spätere Meldung den
Widerspruch verdecken.

Kompaktierung ist eine eigene atomare Transaktion:

- Sie darf einen **intern bewiesenen** Präfixboden nur erhöhen, nie allein aus
  einem hohen Empfangszähler ableiten. Vor einer noch wartenden Anwendung bleibt
  der Boden stehen; deren Inhalt und Abhängigkeiten bleiben erhalten.
- Sie erhält Allocator, Latches, lokalen Fortschritt, ungeklärte Effekte,
  Vorgangskorrelationen und unbenutzte Bereiche. Aktuelle Profil-/Abhängigkeits-
  anker werden ausdrücklich gepinnt. Nicht bestätigte oder widersprüchliche
  Zielstände halten zugehörige Ausgangsnutzdaten fest.
- Gelöschte Historiennutzdaten werden unter dem Boden nicht erneut angewandt.
  Ohne erhaltenen individuellen Digest wird ein altes Paket nur als durch den
  Boden abgedeckt behandelt, **nicht** als inhaltsgleiches Duplikat oder neuer
  Anwendungsbeleg. Noch vorhandene Digestbelege werden weiterhin auf Konflikt
  geprüft. Später benötigter Inhalt ohne erhaltenen Anker sperrt die abhängige
  Regel, statt einen Erfolg zu erfinden.
- Ein externes Snapshot-/Bootstrapverfahren muss die vollständige D0-Bindung,
  Freigabe-/Replay-/Konfliktbelege und explizite lokale Zulassung erhalten. Die
  Referenz nimmt **keinen extern vorgegebenen Boden** an. Bootstrapcodec und
  sichere Übernahme bleiben D1-B/#30; kein pauschales Nachladen in die Outbox.

Geändertes Paket im selben Durchlauf erhält alte Latches. Neuer Durchlauf setzt
die Ursprungssequenz nicht zurück. Die Referenz behält auch alte Vorgangs-/Latch-
belege, bis ein separat spezifizierter, bewusst bestätigter Ruhestand möglich ist.
Sie bietet keine automatische Löschung alter Durchläufe. Bei erfüllten Quoten
wird weitere Annahme gesperrt. Eine spätere Ruhestandsmigration braucht einen
dauerhaften Sperrbeleg gegen erneute Aktivierung alter Durchlauf-IDs, geklärte
Restzustellungen und ausdrücklichen Daten-/Wirkungsabgleich (D1-B/#30).

## 6. Rechnerreferenz und Ressourcen

[Referenzkern](../../scripts/d1_journal_model.py) und
[Tests](../../scripts/tests/test_d1_journal.py) verwenden ausschließlich die
Python-Standardbibliothek. Das Persistenzformat der **Rechnerreferenz** ist eine
SQLite-Datenbank mit `application_id=0x4C443131`, `user_version=1` und genau einer
Tabelle `snapshot(id=1, payload BLOB, digest TEXT)`. `payload` enthält den
vollständigen kanonischen ASCII-JSON-Zustand (sortierte Schlüssel, keine Leerzeichen,
keine NaN); `digest` ist dessen SHA-256. Er erkennt veränderte Referenzbytes,
liefert aber keine Authentifizierung oder Rollbackerkennung.

Im Snapshot liegen genau `context`, `issued`, `revision`, `streams`, `facts`,
`operations`, `latches`, `outbox`, `effects`, `localProgress`. Fact-IDs werden
lesbar als event:origin:epoch:16-stellige Hex-Sequenz geführt. Der **Modelldigest**
eines Facts hasht dessen kanonische JSON-Projektion. Er ist **nicht** der
produktive D0-Digest über CBOR-Payloadbytes; eine Verwechslung ist im D0-Port
auszuschließen. Datenbankpfade und alle Policy-/Profilbeobachtungen sind lokal
vertrauenswürdige Testeingaben, niemals Netzwerkfelder.

| Grenze der Referenz | Wert / Verhalten |
| --- | --- |
| Stream-/Fact-/Vorgangstabellen | 8 Streams, 32 Nutzdaten-Facts, 32 lokale Vorgangsbelege |
| Lokaler Zustand | 64 Regel-Latches, 64 lokale Fortschrittsbelege, 16 Effektintents |
| Ausgang | 32 Fakten, jeweils höchstens acht vorher benannte eindeutige Ziele |
| Abhängigkeiten/Bereiche | Acht Abhängigkeiten und acht Bereiche pro Fakt; höchstens 16 normalisierte unbenutzte Bereiche je Stream |
| Bytes | Einzelne JSON-Fact-Projektion höchstens 2048 Byte; Snapshot höchstens 128 KiB. Das ist kein D0-COSE-Größennachweis. Quoten sind zusätzliche Grenzen, keine Zusage, alle Maxima gleichzeitig mit beliebigen Daten zu erreichen. |
| Hostbackend | 4096-Byte-Seiten, höchstens 256 Datenbankseiten (1 MiB), Rollbackjournal zusätzlich. `journal_mode=DELETE`, `synchronous=FULL`; keine WAL-Datei. Physische temporäre Journal-/FS-Kosten sind zusätzlich zu budgetieren. |

Diese Zahlen begrenzen die Referenz und erzeugen gezielte Erschöpfungsfälle.
Sie sind keine festgelegte Veranstaltungs-/Handy-/Boxkapazität und kein gemessener
ESP32-RAM-/Flashbedarf. Die Referenz hält vollständige JSON-Zustandskopien im
Rechnerheap. Kein solcher Puffer ist für die Firmware zugesagt.

Jede Mutation ist `BEGIN IMMEDIATE` → validierter Zustand → vollständiger neuer
Zustand samt Prüfdigest → letzter Rechtecheck → Commit. Ein unverändertes Duplikat
schreibt nicht erneut. Öffnen verwendet `mode=rw` und erzeugt keinen fehlenden
Speicher. Nur ausdrücklich neue Hostfixtures werden exklusiv erzeugt; vorhandene
Daten oder verwaiste Journaldateien sperren diese Erstellung. Unbekannte Version,
unerwartetes Schema, Integritäts-/Zustandsfehler oder Quotenüberschreitung erlauben
kein leeres Ersatzjournal. Speicherfehler/unklarer Commit sperren weitere Writes
der Instanz; keine blinde Wiederholung. Der Test „verlorene Antwort“ liegt nach
einem bestätigten SQLite-Commit, ist kein simulierter Beweis für defekte Flushes.

Die SQLite-Referenz nutzt die dokumentierten
[atomaren Transaktionen und Backendannahmen](https://www.sqlite.org/atomiccommit.html)
sowie [Synchronisierungseinstellungen](https://www.sqlite.org/pragma.html#pragma_synchronous).
Prozessabbruch ist kein Stromverlust von Rechner, Controller oder ESP-Flash.
SQLite wird weder in Firmware eingebaut noch als ESP32-Speicherbackend ausgewählt.

## 7. Verschleiß, Fehler und Implementierungstore

Flashverschleiß darf nicht aus dem kleinen logischen Datensatz abgeleitet werden.
Für den Zielport sind zu belegen: Commitrate R, logische/physische Bytes je Commit,
Erase-/Write-Amplifikation W, Partition/Reserve F, spezifizierte Erasezyklen E,
Sicherheitsreserve und geforderte Veranstaltungsdauer. Ein grober Planungswert
`E * F / (R * BytesProCommit * W)` ersetzt weder Messung noch garantierte Lebensdauer.
Verdichtung darf mehrere Entscheidungen nur dann zusammenfassen, wenn vor dem
Commit keine einzelne davon als dauerhaft bestätigt oder physisch ausgelöst wird.
Keine Speicherwrites pro LED-Frame, Statuspoll oder identischem Replay.

| Tor | Vor Abschluss von #60 zusätzlich nötig |
| --- | --- |
| D1-B1 · #28/D0-G1/G2 | Echte verifizierte D0-Digests/Bytes, offline eingeführte Rechte und Kontext-/Revisionsfence; manipulierte Eingaben ändern keine Persistenz. |
| D1-B2 · #60/#31 | Konkreten begrenzten ESP-Speicherport und versioniertes Format wählen/implementieren; atomaren Transaktions- oder gleichwertigen Intent-/Wiederanlaufvertrag mit keiner Rücknahme bestätigter Fortschritte nachweisen. Fehlendes Mounten, unklarer Flush, beschädigter neuer Stand, unbekannte Version und voller Speicher bleiben geschlossen. |
| D1-B3 · #31/#30 | Vollständige Profilpläne, Commitfreigabe zwischen Tasks, Wiederaufnahme und getrennte Effektbeanspruchung anschließen; alte Profile, Latches und Zustellungen bei Aktivierung/Ruhestand/Bootstrap prüfen. |
| D1-B4 · #60/#61 | Konkrete Queue-, RAM-/Stack-/Flash-/Latenz- und Verschleißbudgets am gepinnten Stand ausweisen; Regelanker, Paginations- und Rückstaugrenzen in den D2-Abgleich überführen. |
| D1-C · #60/#63 | Physische Stromunterbrechungen vor/nach Aufnahme, Anwendung, Claim, Wirkung und Quittung; Kaltstart, voller/beschädigter Speicher, wiederholte Kompaktierung und kombinierte Last getrennt abnehmen. |

Die vier Kriterien von #60 sind durch diese Ausarbeitung **nicht als vollständige
Produktabnahme erfüllt**. Der [Prüfbericht](../../handoff/D1-AUSARBEITUNG-2026-10-18.md)
ordnet tatsächlich ausgeführte Rechnerprüfungen, Modellannahmen und offene
Integration/Hardware zu. Keine neue Geräte-, Funk- oder Stromausfallprüfung,
kein Upload und kein Frameworkwechsel in diesem Paket.
