# D0 v1: Veranstaltung, Identität und Ereignisse

**Status: durch PR #72 übernommener technischer D0-Vertrag zu #59; nicht implementiert.**
Stand 18.10.2026, Basis `5f90d109e3589d74d73ac26f859dcd76c2902439`
nach Fetch. Auftrag: D0 auf aktuellem Integrationsstand ausarbeiten.
Maßgeblich bleiben [VA-01](../veranstaltungsarchitektur.md),
[Vertragsmatrix](../veranstaltungsarchitektur-umsetzung.md) und
[Veranstaltungszugang](../companion/veranstaltungszugang.md).
Dieser Vertrag konkretisiert diese Entscheidungen, ändert sie aber nicht.
Der Nutzer hat am 18.10.2026 nach fachlicher Prüfung die Übernahme von
[PR #72](https://github.com/Sven401/Larp-Escape/pull/72) in den kuratierten
Integrationsbranch und die Fortschreibung des Projektstands beauftragt.
Damit ist D0 als Spezifikationspaket abgeschlossen; D0-G1–G5 bleiben offen.

`MUSS` bezeichnet eine Bedingung der späteren Implementierung; keine Aussage
über vorhandenen Code. D0 liefert Begriffe, Übernahmeregeln, ein neues Format
und Gegenbeispiele. D1/D2/#28 liefern anschließend Speicher, Verifizierer,
Transport und Nachweise; D4 liefert die verteilte Geräteabnahme.

## 1. Getrennte Identitäten

| Begriff | D0-v1-Wert / Bedeutung | Lebensdauer und Änderung |
| --- | --- | --- |
| Veranstaltung `event` | nichtnull 128-Bit-ID | Vorab angelegt; Veranstaltungswechsel je Box mit Einrichtungsschlüssel gemäß #28. Kein Ableiten aus Name/WLAN. |
| Physische Box `box` | nichtnull 128-Bit-ID des vorhandenen öffentlichen DeviceIdentity-Datensatzes | Bleibt bei normalen Neustarts. Die Kennung allein ist kein Vertrauensnachweis. MAC/COM/Advertisingname sind keine Boxidentität. |
| Logische Station `station` | eigene nichtnull 128-Bit-ID | Bezeichnet eine Spielrolle/Station, nicht Gerät, Komponente oder Handy. |
| Stationsbindung | `(event, station, generation, box, bindingDigest)` | `generation` 1..2^64−1; `bindingDigest` SHA-256 des vollständigen Bindungsdatensatzes. Gegenstelle installiert ausdrücklich genau diesen Stand. |
| Durchlauf `run` | eigene nichtnull 128-Bit-ID | Vorab im Veranstaltungspaket vorbereitet; je Box bewusst aktivieren. Kein automatischer Wechsel bei Neustart, Profilimport oder Wiederverbindung. |
| Paket `packageDigest` | SHA-256 der unveränderlichen deterministischen Manifestbytes | Manifest bindet Boxprofile und Ressourcen über deren Hashes sowie Regeln und Bindungsbezüge. Kein Hash eines ZIP-/Transportcontainers; gemeinsame Paketkennung bedeutet nicht gleiche boxlokale Profilbytes. |
| Ereignisepoche `epoch` | nichtnull 128-Bit-ID einer dauerhaft eingerichteten Ursprungsinkarnation | Bleibt über gewöhnliche Boots erhalten; hängt weder an BLE-Lease noch Profilrevision. Neue Epoche nach verlorener/unsicherer Zählerbasis; Gegenstellen müssen sie bewusst zulassen. |
| Ereignisfolge `sequence` | 1..2^64−1, je `(event, box, epoch)` | Nur für dauerhafte Tatsachen; niemals Wrap oder Wiederverwendung. |
| Schlüssel `kid` | SHA-256 des kanonischen öffentlichen COSE_Key | Eigener Wert; Schlüsselrotation ist nicht automatisch Box-/Stations-/Durchlaufwechsel. |

128-Bit-Werte werden neu aus geeigneter Entropie erzeugt, auf Null geprüft und
vor Veröffentlichung dauerhaft übernommen. Das ist probabilistische Eindeutigkeit,
keine mathematische Kollisionsgarantie. Festgestellte Kollision/mehrdeutige
Zuordnung sperrt den betroffenen externen Pfad; kein automatischer Ersatzwert.
Die vorhandene [öffentliche Identität](../companion/device-identity-store-v1.md)
ist 16 Byte, aber weder Credential noch authentifizierter Ursprung.

Eine Stationsbindung enthält zusätzlich die erlaubten Ursprungs-Epochen,
Faktentypen/Objekte und eine stabile `rulesetId` (128 Bit). Ihr Digest umfasst
genau diese Felder und event/station/generation/box, nicht sich selbst,
Signaturbytes oder den Digest des umschließenden Pakets. Das Paket referenziert
die vollständige Bindung und ihren Digest; so entsteht kein Hash-Zyklus.
Kanonischer Bindungsdatensatz: deterministische CBOR-Map
`{0:1, 1:event, 2:station, 3:generation, 4:box, 5:rulesetId,
6:[epoch...], 7:[[factType,subject]...]}`. Kennungen sind bstr16,
generation unsigned64; Listen ohne Duplikate, Epochen byteweise aufsteigend,
Erlaubnisse nach factType und dann subject aufsteigend. Kein unbekanntes Feld.
Maximale Listen-/Datensatzgröße muss das Ziel vor Übernahme begründet ausweisen;
ein größerer Datensatz wird abgewiesen, nicht gekürzt. Das ist keine vorgegebene
Höchstzahl von Handys oder eine globale Verwaltungsinstanz.
Eine höhere Generation aus dem Funk wird **nicht** automatisch installiert.
Gleiche Generation mit anderem Digest ist ein Konflikt; keine Entscheidung über
Uhrzeit oder numerisch größten Digest. Änderungen verlangen je Zielbox einen
aktuell berechtigten, bewussten Verwaltungsablauf mit Ausgangsstandprüfung (#28).

**Ersatzbox:** bisherige Bindung sperren, Ersatzgerät samt Schlüssel prüfen,
neue Generation und neuen Digest je Gegenstelle bestätigen. Kein automatisches
Erben von Zugang, Fortschritt oder Epochen. Fortschrittsübernahme braucht einen
expliziten, belegten Migrationsvorgang; D0 v1 erlaubt keine stille Übernahme.
Eine getrennte Gegenstelle kann die neue Bindung noch nicht kennen und zeigt
deren Übernahme als ausstehend. Das ist kein global atomarer Austausch.

**Durchlauf:** die Spielleitung verteilt denselben vorbereiteten `run` samt Paket
bewusst an die beteiligten Boxen. Partitionierte Boxen erzeugen keinen vermeintlich
gemeinsamen neuen Durchlauf. Lokale Wiederholung unter neuer ID ist ein anderer
Durchlauf. Ein Ereignis aus dem alten Durchlauf bleibt historisch lesbar, wirkt im
neuen aber nicht. Verwaltungs-Wiederherstellung aus #28 erhält den aktuellen
Durchlauf und Fortschritt; sie ist kein Spielreset.

## 2. Drei Nachrichtenarten und ihre Eigentümer

| Klasse | Kennung / Eigentümer | Speicherung, Reihenfolge und Gültigkeit |
| --- | --- | --- |
| `fact` | `id=(event, originBox, epoch, sequence)`; Ursprung behauptet nur eigene Tatsachen | Unveränderlich, vor Veröffentlichung dauerhaft; Ziel entscheidet über eigene Anwendung. Keine automatische Ablaufzeit. Konkreter Durchlauf, Bindung und Paket müssen passen. |
| `observation` | `(originBox, boot, observationSequence)` | Kopierter Status; kein Ereignisjournal und kein Auslöser einer dauerhaften Freigabe. Eigene Boot-/Zählerfolge; nicht aus Fact-Sequenz oder BLE-Request ableiten. |
| `command` | eigene 128-Bit-`commandId` plus zielseitiger Frischegrant | Zeitkritischer Zielauftrag; kein nachzuliefernder Fact. Ziel prüft aktuelle Berechtigung, Zustand, Frische und lokale Zulässigkeit. |

Nur `fact` verbraucht die dauerhafte Ereignisfolge. Beobachtungen, Pings,
BLE-Aufträge und Transportquittungen erzeugen darin keine scheinbaren Lücken.
Die dauerhafte Kennung bleibt bei erneutem Senden und Transportwechsel gleich.
Bootkennung, Lease, BLE-Request, Profilrevision, Bindungsgeneration, Fact-Sequenz
und `commandId` sind unterschiedliche Namensräume und niemals Ersatz füreinander.
Fakt-Objekte `subject` erhalten eigene 128-Bit-Kennungen im Veranstaltungspaket;
die explizite Regelzuordnung verbindet sie mit lokalen Profilobjekten. Weder
16-Bit-Komponenten-/Zustands-IDs noch NFC-UIDs werden dadurch globale Objekte.

### 2.1 Dauerhafte Vergabe ohne Wiederverwendung · D0-ID

Der Ursprung speichert ein monotones Reservierungsende **vor** Nutzung eines
Nummernblocks. Nach Neustart beginnt er oberhalb dieses Endes; unbenutzte
Reservierungen werden nicht erneut vergeben. Blockgröße ist eine D1-Parameterwahl,
kein Wirewert. Nicht publizierte Lücken werden mit authentifizierten
`unused_range`-Tatsachen derselben Epoche geschlossen. Ein solcher Abschluss darf
keine bereits dauerhaft belegte/ausgegebene Nummer umfassen; er steht selbst in
einer späteren, reservierten Fact-Position. Erzeugen/Belegen/Journalaufnahme müssen
unter einem serialisierten Eigentümer erfolgen. Voller Speicher: keine neue
bestätigte Tatsache und keine Veröffentlichung ungesicherter Nummern.

Ein alternatives D1-Verfahren darf ohne Blocklücken pro Fakt dauerhaft vergeben,
muss aber dieselben Crash-/Nichtwiederverwendungsregeln erfüllen. Bei Maximum,
beschädigter Reservierung oder unklarem Commit keine Vergabe und kein Wrap.
Neue Epoche erst nach bewusstem Recovery-/Vertrauensabgleich; kein spontaner
Neustartwert, der von allen Zielen automatisch akzeptiert wird.

**Rollbackgrenze:** beliebiges Zurückspielen eines alten kompletten Flashabbilds
kann rein softwareseitig nicht zuverlässig erkannt werden. Ohne nachweisbaren
monotonen Anker MUSS dieser Restore als neue Ursprungsinkarnation behandelt
werden, bevor wieder gesendet werden darf. Sicherer Recoveryablauf und Sperre
der alten Epoche gehören zu #28/D1. Verwaltungs-Recovery ohne Speicherrollback
dreht die Epoche nicht und darf Fortschritt nicht zurücksetzen.

### 2.2 Annahme und Wirkung · D0-APPLY

Ziel B verarbeitet ein Ereignis von A in dieser Reihenfolge:

1. Begrenzte Struktur und Version prüfen; dann den fest zugeordneten Ursprung
   [kryptografisch prüfen](d0-sicherheit-migration-v1.md). Ungeprüfte Daten ändern
   weder Replayfenster, Empfangsstand noch Anwendung.
2. Aktive Veranstaltung/Durchlauf/Paket und die ausdrücklich installierte
   Stationsbindung einschließlich erlaubter Epoche und Faktentyp prüfen.
   Lokaler Entzug wird erneut unmittelbar am Übernahmepunkt geprüft.
   Ein ausdrücklich hinterlegter historischer Kontext darf ausschließlich den
   unten beschriebenen Streamabgleich bedienen, nicht diesen Anwendungsweg.
3. Fact-Position gegen dauerhaft geschlossene `unused_range`-Bereiche prüfen.
   Ein Fakt in einer solchen Position ist ein Ursprungskonflikt, auch wenn er
   erst nach dem Bereichsabschluss ankommt. Umgekehrt darf ein Abschluss keinen
   bereits belegten Fakt umfassen. Beides sperrt den betroffenen Ursprungspfad;
   Empfangsstand/Freigaben werden nicht still überschrieben.
   Gleiche ID und gleicher kanonischer Fact-Inhalt: bereits bekannt; bestehenden
   Annahme-/Anwendungsstand zurückgeben. Signaturbytes sind keine Faktidentität.
   Gleiche ID mit anderem Inhalt: Ursprungskonflikt quarantänisieren, nicht
   überschreiben, nicht als zweites Ereignis verarbeiten.
   Ein dauerhaft aufgenommener, noch nicht angewandter Eingang bleibt dabei
   ausstehend. D1 nimmt ihn nach fehlenden Abhängigkeiten, Wiederanlauf oder
   freier Kapazität unter erneuter Rechteprüfung zur Anwendung auf. Eine
   Duplikatantwort darf diese ausstehende Arbeit weder löschen noch als
   bereits angewandt zählen; verarbeitet markiert erst der Anwendungscommit.
4. Noch fehlende ausdrücklich benannte Abhängigkeiten zurückhalten. Für jede
   Regel hat das lokale Profil einen stabilen `ruleId` und `ruleGeneration`.
   Der Eingangsbeleg dedupliziert `fact.id`; die fachliche Freigabe hat separat
   den dauerhaften Latch `(event, run, localStation, ruleId)`. Er ist unabhängig
   von auslösendem Fakt, Paket und `ruleGeneration`. Damit lösen zwei verschiedene
   passende Fakten oder eine lokale Ersatzfreigabe mit späterem Original nicht
   zweimal dieselbe Regel aus. Der Beleg hält Generation und Auslöser fest;
   AND-Regeln prüfen alle Abhängigkeiten, nicht nur das letzte Paket.
   Ein Paket-/Regelversionswechsel im selben Durchlauf erhält den Latch. Ein
   neuer `ruleId` ist eine neue fachliche Regel und verlangt bewusste Migration;
   Umbenennen darf keine bereits ausgeführte Freigabe versteckt zurücksetzen.
5. Eingangsbeleg, lokale Zustandsänderung, angewandte Regel und ausgehende
   Folgeereignisse erhalten in D1 einen atomaren Commit oder einen gleichwertig
   wiederaufnehmbaren Intent-Vertrag. Erst danach `durable_received` bzw.
   `applied` bestätigen. D0 behauptet kein vorhandenes Flash-Transaktionsverfahren.

Reihenfolge besteht je Ursprung/Epoche; zwischen Ursprüngen gibt es keine
globale Reihenfolge. Unabhängige monotone Freigaben dürfen trotz einer fremden
Lücke verarbeitet werden, **nur wenn** die Regel das ausdrücklich erlaubt und
ihre konkreten Abhängigkeiten vorliegen. Reihenfolgeabhängige Regeln warten auf
das lückenlose Präfix oder eine zulässige Abhängigkeitsmenge. Kein implizites
„zuletzt empfangen gewinnt“ und keine Ordnung nach Handy-/ESP-Uhrzeit.

**Abgleich:** pro Ursprung/Epoche lückenloses Präfix, zusätzlich vorhandene
Intervalle, offene Bereiche und authentifizierte `unused_range`-Abschlüsse
ausweisen. Maximum allein genügt nicht. Nimmt B Nummer 5 vor 4 an, bleibt das
Präfix höchstens 3. Fremde Quittung oder ein Vermittler-Cache ist kein eigener
dauerhafter Eingangsbeleg. Alle Intervalllisten sind begrenzt/paginiert (D1/D2).
Authentifizierte `unused_range`-Facts werden nach Kontext-/Konfliktprüfung
unabhängig von wartenden Spielregeln aufgenommen; sonst könnte der Lückenschluss
auf genau seine eigene geschlossene Lücke warten. Sie erzeugen keine Spielwirkung.

**Durchlauf-/Paketwechsel:** Allocator, Präfix, belegte IDs/Inhaltsdigests und
geschlossene Bereiche gehören zum Stream `(event, originBox, epoch)`. Sie werden
bei Run-, Paket-, Stationsbindungs- oder Regelwechsel nicht zurückgesetzt.
Beispiel: R1 belegt 1..10, R2 beginnt mit 11; ein neues Ziel darf 11 weder als
vollständigen Stream zählen noch für immer auf abgewiesene R1-Nummern warten.
Für den Streamabgleich dürfen ausdrücklich lokal hinterlegte historische
Kontexte derselben Veranstaltung weiter geprüft werden: vollständige
Run-/Paket-/Stations-/Bindungszuordnung, aktuell zugelassener Ursprung/Schlüssel,
Epoche und inhaltliche Erlaubnis bleiben erforderlich. Deren Fakten füllen
ausschließlich den historischen Streambeleg, niemals aktuelle Abhängigkeiten
oder Freigaben. Historische `unused_range`-Abschlüsse schließen nur tatsächlich
unbenutzte Positionen; benutzte alte Nummern werden nicht als unbenutzt erklärt.
Unbekannter Altstand bleibt abgewiesen/quarantänisiert, nicht automatisch vertraut.

Bei kompaktierter Historie oder einem neuen Ziel ist alternativ ein bewusst
zugelassener, dauerhaft übernommener Snapshot-/Bootstrapstand nötig (D1/#30).
Er bindet Ziel, Stream, Kontext und Inhaltsdigest, den bestätigten historischen
Präfixboden sowie erforderliche Konflikt-/Replay- und Freigabebelege. Der Boden
bestätigt historische Abdeckung, keine nachträgliche Spielwirkung. Unterhalb
des Bodens wird nichts erneut angewandt; notwendige Abhängigkeiten brauchen
ihren ausdrücklich erhaltenen Inhalts-/Anwendungsbeleg. Fehlen diese Belege,
bleibt die abhängige Regel gesperrt. Weder ein hoher Empfangszähler, ein Relay
noch die Aktivierung allein darf diesen Boden setzen. Ein Bootstrap darf keinen
neueren lokalen Stream-/Freigabebeleg durch einen älteren ersetzen. Ohne Historie oder
zugelassenen Bootstrap bleibt die Lücke sichtbar; lokale unabhängige Regeln
laufen nach ihrem Profil weiter. D1 muss Stream- und Latch-Erhalt bei Wechsel,
Neustart und Kompaktierung gemeinsam prüfen; kein neues Snapshot-Wireformat in D0.

Bei unbekanntem Paket/Bindungsstand kann eine begrenzte Quarantäne existieren;
sie bestätigt weder dauerhafte fachliche Annahme noch Zustellung. Kein
unbegrenztes Speichern ungeprüfter Daten. Nach einem Entzug wird ein alter,
bisher nicht angenommener Fakt nicht durch sein selbst angegebenes Erstelldatum
wieder gültig. Bereits dauerhaft angenommene Fakten bleiben im Verlauf; ein
Entzug widerruft nicht rückwirkend ausgeführte Aktionen.

### 2.3 Ergebnisstufen und physische Effekte · D0-ACK

`transport_received` → `durable_received` → `applied` → optional
`effect_observed`. Keine Stufe impliziert eine höhere. Ein MAC-ACK ist keine
dieser fachlichen Bestätigungen. Jede Rückmeldung bindet Zielbox, aktuelle
Veranstaltung/Durchlauf, ursprüngliche ID und **Fact-Digest** sowie eigene
authentifizierte Antwortkennung. Wiederholte Antworten sind keine neuen Effekte.
`rejected`, `deferred` und `effect_unknown` sind eigene Ergebnisse mit Grund.
Bei Widerspruch zweier gültiger Ergebnisstände: Konflikt sichtbar machen.
Die numerischen Codes sind keine Fortschrittsordnung: 5/6/7 bedeuten weder
`applied` noch `effect_observed`. Eine verspätete frühere Stufe darf bestätigte
Verarbeitung nicht zurücknehmen; Ablehnung/Warten/unklare Wirkung bleiben
getrennt. Eine bereits belegte Anwendung wird bei widersprechender Antwort
nicht erneut ausgeführt. Quittung ohne passende ursprüngliche Referenz und
Inhaltsdigest ändert keinen Auftragsstand.

Ein Ursprung verwirft notwendige Wiederanlauf-/Outboxdaten nicht nach einer
Funkquittung. Aufbewahrung hängt von bestätigten Zielständen und dem aktiven
Durchlauf ab; Ziele sind vorher bekannt. Kompaktierung braucht dauerhafte
Zusammenfassungen und Replaygrenzen. Neue Ziele erhalten einen ausdrücklich
zugelassenen Snapshot-/Bootstrapstand; ein später beigetretenes Ziel begründet
keine unbeschränkte historische Zustellgarantie.

Eine Hardwarewirkung kann zwischen Intent und Ergebnisaufzeichnung liegen.
Nach Absturz darf B weder „ausgeführt“ noch „nicht ausgeführt“ erfinden.
`effect_unknown` wird nicht automatisch wiederholt; das Spielprofil benennt
Zustandsabgleich oder bewusste Spielleiterentscheidung. Genau-einmal gilt hier
für fachliche Freigabe/Anwendung, nicht allgemein für Ton/Licht/Mechanik.

## 3. Offline-Regeln der Referenzinteraktionen · D0-OFFLINE

| Interaktion | Ohne Kontakt | Nach Rückkehr / verspätet |
| --- | --- | --- |
| Lokale Matrix/NFC/Wake löst eigenes Rätsel | Nach vorbereitetem lokalem Profil weiterarbeiten; lokale Tatsachen dauerhaft festhalten | Tatsachen nachliefern, keine alten BLE-Aufträge rekonstruieren. |
| A löst Aufgabe, B öffnet vorbereitete Freigabe | B wartet auf fehlenden Fakt; übriges lokales Spiel bleibt unabhängig | Passender Fakt wirkt einmal. Keine Erfindung eines Erfolgs aus Status/Timeout. |
| Vorbereitete lokale Ersatzfreigabe an B | Nur bewusst berechtigte Spielleitung oder explizit profilierte lokale Ersatzbedingung | Eigener belegter Freigabegrund; derselbe Freigabe-Latch verhindert zweite Aktion durch späteres A-Ereignis. |
| Unmittelbarer Ton/Lichtauftrag an B | Kein Kontakt: ausstehend bis Frist, dann abgelaufen; niemals dauerhafte Nachlieferung | Neuer bewusster Auftrag mit frischem Zielgrant; alter Auftrag bleibt gesperrt. |
| Mehrere Boxen gemeinsam starten/stoppen | Einzelbestätigungen; keine globale Atomarität oder Sicherheits-Not-Aus-Zusage | Je Ziel bestätigt/ausstehend/unklar; kein automatischer globaler Neustart. |
| Rechte-/Bindungsänderung | An erreichbarer Zielbox bewusst bestätigen; andere Ziele bleiben ausstehend | Lokalen Verwaltungsstand nach #28 prüfen. Eine Nachricht verleiht selbst keine Rechte. |
| Neues Paket/Durchlauf | Vollständig lokal prüfen und je Box bewusst aktivieren | Alte Fakten/Kommandos nicht in neuen Durchlauf umdeuten; Konflikt/fehlende Fähigkeit sperrt Aktivierung. |
| NFC-Spielobjekt an anderer Station | D0 v1 transportiert damit noch keine verteilte Tatsache | Bisherigen 59-Byte-Test erhalten; produktives Objekt-/Replay-/Mehrfachschreibverfahren braucht eigenes Format und unabhängige Abnahme. |

Jede spätere boxübergreifende Profilregel MUSS eine dieser Ausfallpolitiken
explizit wählen: `wait`, `local_continue` oder `local_fallback`. Kein Default,
der unbemerkte Freigabe erzeugt. Konfligierende nichtmonotone Regeln sind für
verteiltes v1 nicht zugelassen; sie benötigen zuerst einen eigenen Konfliktvertrag.

## 4. Frische ohne gemeinsame Uhr · D0-FRESH

**Beobachtung:** je Quelle Boot und Beobachtungsfolge führen. Empfangszeit ist
lokal monoton; Uhrzeit des Absenders ist nur Diagnose. Eine vermittelte
oder doppelte Beobachtung darf keinen neueren Quellenstand zurücksetzen.
Bei gleichem Boot werden Folgen kleiner/gleich dem letzten Stand verworfen.
Ein unbekannter Boot wird erst nach frischer Quellenchallenge als aktuell geführt.
Eine vermittelte Cachebeobachtung zeigt ursprüngliche Quelle und `freshness=unknown`, auch wenn
der Vermittler sie gerade sendet. Eine frische Aussage benötigt eine zuvor vom
Betrachter erzeugte zufällige Challenge, die der Ursprung in seiner signierten
Beobachtung beantwortet. Der Betrachter begrenzt deren Lebensdauer mit seiner
eigenen monotonen Uhr. Relay-Verzögerung verbraucht diese Frist. Empfang allein
setzt das Quellenalter nicht auf null. Beobachtungen beeinflussen keine
dauerhaften Freigaben und überleben einen Bootwechsel nicht als frisch.

**Sofortauftrag:** Ziel erzeugt einen unvorhersagbaren 128-Bit-Einmalgrant,
gebunden an eigenen Boot, Ursprungsschlüssel, Zielbox, Veranstaltung/Durchlauf,
Paket/Bindungsstand und erwartete lokale Zustandsrevision. Lebensdauer höchstens
5000 ms auf der monotonen Zieluhr; Wiederholung verlängert sie nicht. Quelle
übernimmt den Grant in ihren authentifizierten Auftrag. Ziel prüft am tatsächlichen
Ausführungspunkt, nicht nur beim Einreihen, und verbraucht den Grant vor dem
Effekt. Pro Grant genau ein `commandId`/Inhaltsdigest; Konflikt sperrt ihn.
Nach Zielneustart sind alle alten Grants ungültig. Kein Vertrauen in absolute
UTC-Timestamps des Senders. Offene alte Aufträge werden nach Trennung oder
Neustart nicht automatisch neu erzeugt; verlorene Ergebnisantwort bleibt unklar.
Lokale Berechtigungs-/Revision-/Spielzustandsprüfungen bleiben zusätzlich nötig.
Ein pauschal gestopptes Spiel ist für `play_once`/`light_once` keine Voraussetzung:
das Zielprofil entscheidet über erlaubte Aktion, Ressource und Ausführungszustand.
Die Stopbedingung für Paketaktivierung ist davon getrennt. Wiederholungen eines
bereits verbrauchten Grants dürfen nur dessen bekannten Ergebnisstand liefern,
niemals erneut ausführen; fehlender Ergebnisbeleg bleibt unklar.

**Kontrollaustausch vor Nutzung:** Die vier D0-Nachrichten transportieren noch
keinen Challengeauftrag und keine Grantausgabe. D2/#28 benötigt dafür vor
Anbieten von `observation_v1`/`command_grant_v1` einen separat versionierten,
authentifizierten Kontrollvertrag. Er bindet Challenge/Anfragekennung,
Anfragenden samt Schlüssel, Zielbox, Veranstaltung/Durchlauf/Paket, relevante
Stationsbindungen, Zielboot/-revision und bei Grants die erlaubte Aktion/Ressource.
Nur der adressierte Ursprung beantwortet die Beobachtungschallenge; nur das
Ziel stellt seinen Grant aus und hält dessen ursprüngliche Frist fest.
Wiederholte Anfrage/Antwort verlängert keine Frist und bewirkt keinen Auftrag.
Abbruch, alte Sitzung, Budgeterschöpfung und unklarer Ausgang müssen ohne
automatisches Neuerzeugen behandelt werden. Ein Relayfeld oder eine normale
Statusmeldung ersetzt diesen Nachweis nicht. Das Modell setzt den geprüften
Kontrollaustausch als vertrauenswürdigen Kontext voraus; es implementiert ihn
nicht. D0-G1/G2/G4 müssen dessen Codec, Rechtebindung und Budgets nachweisen.

## 5. D0-Abschluss und Anschluss

Der [Format-, Vertrauens- und Migrationsvertrag](d0-sicherheit-migration-v1.md)
legt Felder, Authentifizierung und Versionsgrenzen fest. Die
[Gegenbeispiele](fixtures/d0-v1.json) und das
[hardwarefreie Vertragsmodell](../../scripts/check_d0_contract.py) prüfen
Annahmeentscheidungen, keine Kryptografie, Flashdurabilität oder Funk.

| D0-Kriterium aus #59 | Fundstelle / späterer Nachweis |
| --- | --- |
| Kennungen, Neustarts, Lücken/Abhängigkeiten | Abschnitte 1–2, Fälle ID/GAP/DEP; Crash-/Restore-/Kompaktierungsbeweis in D1 |
| Jede Referenzinteraktion offline und verspätet | Abschnitte 3–4, Fälle FRESH/OFFLINE; Profilanschluss und lokale Ausführung #31/#30 |
| Vertrauen über Vermittler, Einführung/Entzug | Sicherheitsvertrag Abschnitte 2–3, Fälle TRUST; echte Verifizierer/Schlüsselabläufe #28/D2 |
| Version/Fähigkeit/Migration und Negative | Sicherheitsvertrag Abschnitte 1/4, Fälle FORMAT/MIG; bestehende Bytes unverändert |

Die Kriterien sind damit **als integrierte Spezifikation abgedeckt**, nicht als
implementierte Funktion abgenommen. Offene Implementierungstore sind ausdrücklich
dem Folgepaket zugeordnet; unbekannte Speicher-/Funkkapazität ist keine Zusage.
Das Modell ist absichtlich eine dekodierte Projektion: Verifizierer-/Rechte-/
Abhängigkeitsbeobachtungen werden simuliert. Vollständige Byte-/Tiefen-/
Kurvenprüfung, stationlose Zentralenmeldungen, Quittungs-Referenzcodec und
Crashdurabilität sind nicht implementiert; diese Nachweise liegen in D0-G1–G5.

Fachliche Nachprüfung und korrigierte Gegenbeispiele vom 18.10.2026:
[Reviewbericht zu PR #72](../../handoff/D0-REVIEW-2026-10-18.md).
