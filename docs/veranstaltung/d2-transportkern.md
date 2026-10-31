# D2: begrenzter transportneutraler Fact-Kern

**Status: Softwareteilschritt zu [#61](https://github.com/Sven401/Larp-Escape/issues/61),
keine produktive Funkanbindung.** Datum: 31.10.2026. Ausgangsbasis:
`8345d65a62a19b4c5236cd5b07207c3b50391676`. Quelle: aktueller Umsetzungsauftrag,
[D0](d0-vertrag-v1.md), [Sicherheitstore](d0-sicherheit-migration-v1.md)
und [D1](d1-persistenz-v1.md). Der genaue geprüfte Head und die CI-Belege stehen
im zugehörigen PR. #61 bleibt offen.

Führender Implementierer und Plattformverantwortlicher: Root-Agent dieser
Sitzung; Tests: `d2_tests`; unabhängiger Plattform-/Security-Grenzreview und
Qualitätsgegenprüfung: `d2_review`. Sitzung:
`01a192dd-375d-7863-baa8-b46b6ff92452`. Keine allgemeine Sicherheitszertifizierung.

## Ausführbarer Umfang

[`EventTransport.h`](../../include/Companion/EventTransport.h) und
[`EventTransport.cpp`](../../src/Companion/EventTransport.cpp) stellen einen
C++11-Kern ohne SDK, Gerätezugriff, Codec, dynamische Allokation oder Warteaufrufe
bereit. Er wird über die vorhandene gemeinsame Quellliste auch für die native
ESP32-Prüffirmware kompiliert, aber weder instanziiert noch produktiv angeschlossen.
Bestehende BLE-/ESP-NOW-Laborformate und Fähigkeiten bleiben unverändert.

- Vier Ausgangsplätze, jeweils für ein Fact/Ziel-Paar, und vier Eingangsplätze;
  jeweils höchstens 2048 vollständige, unveränderte D0-Nachrichtenbytes. Das ist
  ausdrücklich **keine** zulässige ESP-NOW-Einzelframegröße.
- Volle Puffer melden Rückstau, verdrängen keinen Eintrag und wachsen nicht.
  Ausgangsübernahme benötigt den D0-Verifizierer und D1-Nachweis der dauerhaften
  Veröffentlichung. D1 behält seine Outbox bei `Full`, fehlendem Kontakt oder
  Neustart und füllt den flüchtigen Kern später erneut.
- Ein Poll wählt höchstens einen fälligen Kandidaten reihum. Höchstens ein
  Sendeversuch pro 100 ms; pro Eintrag Abstände 100, 200, 400, 800, dann 1600 ms.
  Kein Nachholen aufgelaufener Versuche in einem Burst. Ohne Kontakt kein Versuch
  und kein Fortschreiben des Backoffs. Wiederkehr und Wechsel ESP-NOW/WLAN
  setzen weder Frist noch Kennung zurück. Fakten haben keine Ablaufzeit;
  Wiederholung läuft mit begrenzter Rate weiter, bis D1 den Zielstand bestätigt.
- Ein Versuch kopiert die ursprünglichen Bytes und Metadaten. Ein lokales,
  nicht umlaufendes Versuchstoken korreliert Adapterantworten. Verspätete,
  doppelte und nach Platzwiederverwendung eintreffende Rückrufe ändern nichts.
  MAC-/Adaptererfolg und -fehler verändern weder D1 noch Wiederholungsfrist.
- Vor jedem Versuch und jedem erneuten Eingangsversuch werden Ursprung und
  aktuelle Berechtigung erneut geprüft. `Unavailable` verschiebt mit Backoff;
  Ablehnung sperrt den Ausgang bzw. verwirft den ungeprüften Eingang ohne
  fachliche Bestätigung. Gesperrte Ausgänge bleiben sichtbar belegt, bis der
  Eigentümer den Kern nach geklärtem D1-/Vertrauensstand bewusst neu aufbaut.
- Ausgangsduplikate verwenden verifizierte Fact-ID und Payload-Digest,
  einschließlich Runbindung. Alternative gültige Signaturbytes desselben Inhalts
  erzeugen keinen neuen Eintrag. Gleiche ID mit anderem Inhalt sperrt alle
  zugehörigen Zielplätze und weitere Ziele. Dies ist flüchtige Quarantäne;
  dauerhafte Ursprungskonfliktbehandlung bleibt D1.
- Eingänge gehen nach D0 an D1. D1 entscheidet dauerhaft über Duplikate,
  Reihenfolge, historische Kontexte, Lücken und Konflikte. `Deferred` und
  `Unavailable` bleiben mit Backoff im Puffer; keine automatische Regelwirkung
  und keine vorgetäuschte Annahme. Ein bekannter, noch nicht angewandter Eingang
  bleibt auch nach `AlreadyDurable` Aufgabe des D1-Eigentümers.

Die Grenzen sind lokale Implementierungsparameter dieses nicht angebotenen
Kerns, keine neue Wireversion oder Funk-/Durchsatzgarantie. Sein eigener
statischer Speicher ist auf 20 KiB begrenzt (`static_assert`); dazu kommen
Aufruferkopie `Attempt`, Verifizierer, D1 und später SDK-/Fragmentpuffer.
Die Instanz gehört in den dauerhaften Eigentümerspeicher, nicht auf einen kleinen
Taskstack. Vollständige RAM-/Stack-/Flash- und Funklastmessung bleibt D0-G4.

## Getrennte verweigernde Anschlüsse

| Grenze | Jetzt | Erforderlicher Anschluss |
| --- | --- | --- |
| D0-Ursprung/Vertrauen | `OriginVerifier` liefert standardmäßig `Unavailable`; kein COSE-/CBOR-/Kryptografieersatz | D0-G1/G2: echte Originalbyteprüfung, aktuelles Register, Kontext/Bindung/Epoche/Erlaubnis und Entzug; eigene Goldenbytes/Negativnachweise |
| D1-Veröffentlichung/Annahme | `DurablePort` liefert standardmäßig `Unavailable` | Bereits dauerhaft belegte Originalbytes/Ziele; Annahme mit Entzug am Commitpunkt, Replay/Outbox und atomarer Anwendung nach D1-B/C |
| D1-Quittungszusammenführung | Jede verifizierte Quittung erreicht `recordReceipt`, auch ohne flüchtigen Slot | Gegen **bereits bekannte dauerhafte** Fact-/Run-/Digest-/Ziel-/Bestätiger-/Adressatenreferenz prüfen; fremde Referenz `Unmatched`; widersprüchliche Ergebnisse dauerhaft sichtbar, frühere Stufe ohne Rücknahme |
| Challenge/Grant | Kein Einlass für Observation-/Command-Verkehr und keine Fähigkeit angeboten | Eigenständiger authentifizierter Kontrollvertrag D0-FRESH/D0-G1; Frische, Rechte und Budgets gesondert implementieren. Keine Umdeutung von Facts oder BLE-HMAC |
| Transportadapter | `poll` liefert nur eine Kopie an den serialisierten Eigentümer | Gepinnte SDK-Grenzen, Kanal/Peerbudget, begrenzte Fragmentierung/Reassemblierung, Timeout, Queues, BLE-Koexistenz; kein neuer Hüllenentwurf in diesem Schritt |

`recordReceipt` muss auch späte Ablehnungen nach Erfolg und Erfolg nach Ablehnung
an seinem dauerhaften Beleg erkennen. Der flüchtige Scheduler ist kein
Quittungsindex. Nur ein bestätigter D1-Commit `TargetDurable` **und** D0-Stufe
`Durable`, `Applied` oder `Observed` kann den exakt passenden, nicht gesperrten
Sendepuffer freigeben. Das löscht keine Journal-, Replay-, Outbox- oder
Anwendungsbelege. Transportannahme, Ablehnung, Warten und unklare Wirkung sind
keine höhere Erfolgsstufe. Der Kern erzeugt oder signiert keine Rückmeldung.
Der Rückgabewert `Released` beschreibt ausschließlich die flüchtige Belegung.
Grund und vollständige signierte Quittungsbytes werden an D1 weitergegeben,
damit dessen Commitpunkt die ursprüngliche Schlüssel-/Bindungs-/Paketzuordnung
erneut prüfen und den Beleg erhalten kann. Ports müssen Bytes vor einer späteren
Verwendung kopieren; Zeiger sind nur während des jeweiligen Aufrufs gültig.

## Ausführung, Fristen und Ressourcen

Alle Methoden gehören demselben serialisierten Eigentümer; auch Portaufrufe
sind nicht reentrant. Keine Thread-, ISR-, FreeRTOS- oder SMP-Sicherheit wird
behauptet. Funkcallbacks dürfen den Kern nicht direkt parallel aufrufen.
Ein späterer Adapter braucht einen kopierenden, begrenzten Übergang und muss
dessen eigenen Kontext-/Queuebeleg liefern; hier wird kein RTOS-Port geändert.

`poll`, `processIncoming` und `receipt` erhalten dieselbe monotone 64-Bit-Zeit
in Millisekunden. Rücklauf und bevorstehender Überlauf verweigern Arbeit.
Kein unsigned Wrap als Frist, keine Uhrzeitordnung zwischen Boxen.
`processIncoming` und `receipt` teilen ein Prüfbudget von einem Eingang pro
100 ms. `RateLimited` bei Quittungen heißt erneut vorlegen, kein Annahmebeleg.
Der Adapter muss faire Wahl zwischen beiden Eingangsarten, seine eigene
Callbackrate und Länge/Framing vor dem Kopieren begrenzen. `enqueue` ist nur
für den lokalen D1-Eigentümer, kein externer Funk-Eingang.

Jeder Aufruf durchsucht ausschließlich die festen Viererarrays;
`poll` und `processIncoming` bearbeiten höchstens einen Kandidaten.
Aufwendige D0-/D1-Ports müssen
selbst einen begrenzten, nicht wartenden Eigentümervertrag einhalten; dieser
Kern misst keine Signatur- oder Flashlatenz. Die Tests ersetzen diese Nachweise
nicht. Gezielte Vermittlung/Relay, historische Abgleichseiten, Fragmentcodec und
Kanalwechsel bleiben nach D0-G4 offen; aktuell nur direkte Fact/Ziel-Paare.

## Softwareprüfung und Grenzen

[`test_event_transport`](../../test/test_event_transport/test_main.cpp) verwendet
ausdrücklich simulierte D0-/D1-Testanschlüsse. Die Testdaten sind **keine** COSE-
Goldenbytes und kein Kryptografie- oder Flashnachweis. Geprüft werden fehlende
Freigaben, Unterbrechung/Wiederkehr, volle Queues, unveränderte Transportwechsel,
Duplikate/vertauschte Eingänge, Wiederholungsgrenzen, späte Rückrufe, Quittungs-
korrelation, Widersprüche, Entzug und Zeit-/Längen-/Speichergrenzen.

Reproduzierbare Eingänge:

```text
python -m platformio test -e host-regression -f test_event_transport -v
cmake -S . -B private/host-build
cmake --build private/host-build
ctest --test-dir private/host-build -R event-transport --output-on-failure
python scripts/check_architecture.py
python scripts/check_headers.py
python scripts/check_d0_contract.py
```

Die bestehende Host-/Native-CI führt die neue Suite aus; Pflichtchecks und
Workflows werden nicht verändert. Die gemeinsame Quellliste nimmt den Kern in
die vorhandenen nativen SDK-Builds auf. Keine neue Bibliothek oder SDK-Migration.
Der parallele D1-Speicherkern #60 und der Paketcompiler/Planer #93/#31 werden
nicht umgestaltet; `DurablePort` ist der dokumentierte spätere Adapterauftrag.
Reale Funkabnahme, Kanalwechsel, BLE-Koexistenz, Ressourcenlast, Stromausfälle,
F5a-Wirkung und die Mehrbox-Produktabnahme sind ausdrücklich **offen**.
