# Bestätigte Registerübergabe an den ursprünglichen SDK-Akteur

Aktuelle Fortsetzung: [Verifizierter Workerauftrag zum Originalaktor](peer-proof-grant-v1.md).
Die nachfolgenden Prüfzahlen und offenen Punkte beschreiben den jeweiligen
historischen Schnitt; dessen Nachweise werden hier nicht neu behauptet.

Status: schmaler Firmware-/Host-/Toolpfad; kein Produktionszugang und keine Geräteabnahme. Datum: 2026-10-07, Basis Firmware7398d2d. Der öffentliche [404-Byte-Registervertrag](peer-registry-v1.md) und die App-v2-Verträge bleiben erhalten.

## Besitzer und vertikaler Pfad

Ein IOworker besitzt `PeerRegistryOwner` und den tatsächlichen `Esp32PeerRegistryStore`. Der ursprüngliche BTU-/SDK-Akteur besitzt `IdentityAccess`. Ein Kryptoworker liest ausschließlich geschützte Kopien aus `FreeRtosPeerRegistryRuntime` und `Esp32GattsSource`; er liest keine veränderlichen Owner-/Policyfelder eines anderen Tasks.

Der IOworker friert die vertrauenswürdige Änderung ein, prüft Struktur, Generationen und echte Kurvenpunkte und beginnt einen begrenzten geschlossenen Rollenauftrag. `Pending` bedeutet ausschließlich ausstehende Bearbeitung. Erst die Bestätigung des tatsächlichen ursprünglichen Akteurs erlaubt NVSlesen/-schreiben. Der Store vergleicht den gesamten bisherigen Datensatz, schreibt/committet und liest exakt zurück; der Owner prüft ein zweites Mal. Erst eine weitere bestätigte geschlossene Rolleninstallation veröffentlicht Credentials und ergibt `Saved`, `Loaded` oder `Empty`. Eine gespeicherte, aber nicht bestätigte Installation ergibt weder Saved noch eine lesbare Veröffentlichung.

`PeerRegistryOwner::service()` führt höchstens drei Phasen pro Aufruf aus und wartet nicht auf SDKarbeit. Der vorhandene direkte `PeerRegistryRuntime` bleibt für eine einzige serialisierte Startup-/Hostausführung geeignet. Dessen Getter sind keine Synchronisation. Eine zweite Produktionstask darf diesen direkten Port nicht verwenden.

## Privater fester Auftrag

`PeerRegistryRoleJob` ist ein interner typisierter Vertrag, kein Appformat und kein Structdump:

- Original: Gerät, SDKregistrierung, Interface, Verbindungslebenszyklus und **volle eigene** Rechtebindung einschließlich Epoche, Principal und GM.
- Nicht umlaufende Auftragssequenz, eingefrorene Aufnahmezeit und Operation Close oder Install.
- Genau vier Rollenplätze, kompakt, sortiert und eindeutig; leere Plätze sind null. Close enthält ausschließlich eine leere Tabelle.
- Ein fester belegter SDKslot mit unveränderter vollständiger Anforderung und geschlossenem ACK. Identische Wiederholung postet nicht erneut; ein anderer Auftrag während Pending ist Busy.

Die SDKqueue erhält einen nicht umlaufenden globalen 32-Bit-Token als opaken Callbackkontext. Sie erhält keinen Zeiger auf den später wiederverwendeten Auftrag. Ein verspäteter alter Callback kann deshalb den neuen Slot nicht konsumieren. SDKinstanz, ursprüngliche Task, Registrierung, Lebenszyklus, Alter und vollständige Bindung werden vor und nach Ausführung geprüft. SDKposting erfolgt außerhalb des lokalen Mux mit Wartezeit0. Kein NVS oder Kryptografie im BTUcallback und keine automatische Wiederholung.

Jeder Auftrag hat eine eigene **5000-ms-Frist**. Die gesamte angenommene Close-/IO-/Installoperation hat **30000 ms** ab dem ersten Close. Polling erneuert keine Frist. Das sind Abbruchregeln, keine gemessenen Funk-/NVSlatenzen: Ein blockierender NVSaufruf kann dadurch nicht unterbrochen werden. Die Befehlsfrist2000 ms wird nicht übernommen. Prozesslebensdauer und einmalige SDKregistrierung sind erforderlich; kein Stop/Restart oder Objektabbau bei ausstehender SDKarbeit.

## Tatsächlicher Besitzer statt beobachtetem fremden Publisher

`IdentityAccess` liefert von seinem alleinigen Akteur eine feste `IdentityAccessSnapshot`-Kopie: Gerätekennung, ursprüngliche Verbindung, eigene volle Rechtebindung, aktuelle/verbundene/Proofberechtigung. Eine einmalig gebundene Beobachterstelle kopiert unter dem Source-Mux; sie startet keine SDKarbeit und führt keine Speicherung/Kryptografie aus. Vor externen Rechteänderungen und nach jeder öffentlichen Mutation wird der lokale Zustand beobachtet. Der Source prüft diese **eigene** Bindung gegen die aktuelle Veröffentlichung; er übernimmt deren möglicherweise fremde Bindung nicht als seine eigene.

Eine neue Hostprüfung hat vor Sicherung des Schnitts gezeigt, dass die ursprüngliche Implementierung bei fremder Veröffentlichung diese fälschlich als Closeziel übernehmen konnte. Roter Beleg und grüne Wiederholung sind erhalten. Jetzt lehnt der alte Besitzer den Auftrag ab und erhält die fremde volle Bindung. Ebenso erhalten verspätete alte Aufträge eine neu verbundene und berechtigte Sitzung.

Der kopierte Proofkontext bleibt auf derselben ursprünglichen Verbindung nach erfolgreicher GMauthentisierung gültig. Identitätsnachweis und Rolle sind getrennt: direkter Rollenentzug schließt die GMroute; eine vollständige geschlossene Registerinstallation sperrt zusätzlich den alten Proof bis Disconnect/Reconnect. Eingabekopien entstehen vor jeder externen Beobachtung; erfolglose Getter/ACKs verändern ihre Ausgaben nicht.

## Geschützte Veröffentlichung

Die Veröffentlichung enthält einen festen 408-Byte-Snapshot und dessen vollständigen bestätigten Installauftrag. Leser kopieren unter dem Mux, prüfen den Installauftrag am Source und prüfen die Veröffentlichung nochmals. Während jeder angenommenen Änderung, bei Fehler, Timeout oder Entzugssperre sind Keys/Snapshot nicht lesbar. Kein NVS, keine Kryptografie und kein SDKposting im Mux. Neue Verbindungen können denselben bestätigten Credentialbestand lesen; sie übernehmen keinen alten Proof. Registry-/Schlüsselgenerationen und Originalbinding bleiben getrennt.

Ein Fehler sperrt Veröffentlichung, ausstehende ursprüngliche Rollenarbeit und den gemeinsamen NVSstore für den Boot. Die genaue erfasste eigene Autorität darf geschlossen werden; andere Publisher werden erhalten. Bei realem Neustart nach unbestätigter Speicherung kann weiterhin ein alter oder neuer gültiger Datensatz lesbar sein. Die RAMsperre garantiert keinen dauerhaften Entzug über Stromausfall.

## Belege und noch offene Abnahmen

Die neue native Prüfung verwendet echte Mbed-TLS2.28.4-Kurvenpunkt-/Signaturprüfung sowie tatsächliche NVS-, FreeRTOS-, Source-, Queue-, Registry- und Spieladapter gegen Hostmodelle. Sie lädt die ausdrücklich gespeicherte GMrolle mit einem neuen Owner, verarbeitet sechs Proofframes, authentisiert mit ausdrücklich **modellierter** ursprünglicher SDKsecurity, führt13 originale SDK-Befehls-/ATTantwortframes bis zum laufenden Spiel aus und entzieht anschließend die Rolle. Das Spiel läuft weiter; der alte Proof und die alte GMroute sind gesperrt.

Gemäß den ausgeführten Assertions: fünf modellierte Taskidentitäten; zehn Rollen-SDKposts plus13 ATTposts, drei bestätigte NVSänderungen. `fixtures/peer-registry/actor-observation.json` stammt aus diesem tatsächlichen C++-Fall. Export ausschließlich über `LARP_EXPORT_REGISTRY_ACTOR_FIXTURE=1`; normale Nativeprüfungen vergleichen vorhandene Bytes. `scripts/check_peer_registry_actor.py` korreliert die Beobachtung unabhängig mit den drei öffentlichen Datensätzen und verwirft60 falsche Beobachtungen plus sechs falsche Datensatzkorrelationen. Der bisherige CLI- und Kurvenpunktabgleich bleibt erhalten.

Die geschlossene SDKprobe linkt auch den tatsächlichen neuen Runtimeadapter, startet keine Registrierung/BTU/RF und öffnet keinen NVSstore. Eine negative Beginprüfung ist kein Geräteausführungsergebnis. Produktion verwendet weiterhin den bisherigen Controller. Ursprüngliche echte SDKsecurity, eigene Vor-GM-Authentisierungsattribute, gegenseitiger Servernachweis, vertrauenswürdige Einrichtung, echte Ausgangsattribute/CCC, Android und Produktionsanschluss bleiben offen. Die vorhandenen beiden Schreibattribute setzen bereits GM voraus.

### Abschließender Prüf- und Compilerstand

424/424 Basis-Hostfunktionen und429/429 im separaten Ziel mit echter Kryptografie, jeweils abschließend ohne Fixtureexport nach der Stackverbesserung. Gegenüber7398d2d achtzehn neue Basis- und eine weitere native Funktion. Fünf Buildziele danach erfolgreich. Architektur136 Dateien/287 Includes,81 unabhängige C++11-Header;284 Formatdateien. Die bisherigen unabhängigen Gegenstellen einschließlich432 Register-/246 Kryptonegative bleiben erfolgreich. Zwölf App-v2-Dateipaare frisch byte-/SHAgleich; keine Appänderung/-prüfung.

| Tatsächlicher ESP32-Typ | Byte |
|---|---:|
| Registryowner mit eingefrorener ausstehender Änderung |1644|
| Geschützte Registryruntime |772|
| Ursprünglicher SDKsource inkl. Auftragsplatz und Policykopie |604|
| IdentityAccess |148|
| Registrysnapshot / Record |408 /404|
| Original / Rollenauftrag / Policykopie |40 /132 /40|

Die Besitzer-, Source- und Runtimesymbole wurden auch am tatsächlichen SDK-ELF abgeglichen. Der Owner benötigt gegenüber832 Byte früher jetzt812 Byte zusätzlich für die ausstehende eingefrorene Änderung. SDKprobe46.360 RAM /1.069.737 Flash (81,6% Appslot), gegenüber7398d2d1808 RAM /6076 Flash zusätzlich. Produktion55.148 /1.229.077 (93,8%), USB21.480 /291.881, Profilprobe107.560 /407.069 plus57.272 Startup-Profilheap und Allocator/Dateisystem. Getrennte Proben sind keine kombinierte Produktionsreserve.

Eigene einzelne ESP32-Compilerframes: Ownerbegin64, Ownerreplace480, service32, advance32 plus ausgelagerter advance-Teil848; Registryruntimeclose208 /replace592 /readPublished576; Sourceapply208 /post192 /ACK208 /Origin160 /Proof144; Identityobserver80, Rollenreplace176; NVSreplace1712 unverändert. Nach erster Größenrechnung entfernt: Ownerbegin464→64 (**400 Byte**), RegistryruntimeReplace880→592 (**288 Byte**). Eingabekopien und bestätigte Veröffentlichung bleiben erhalten; feste semantische Snapshotvergleiche unter dem Mux benötigen keinen zweiten408-Byte-Stacksnapshot.

Konservative Summe eines bekannten eigenen Änderungspfads:480+32+848+1712+848+160 = **4080 Byte** vor Bibliotheks-/NVS-/Aufruferkosten. Über den normalen service-Pollaufruf ist derselbe bekannte eigene IOpfad kleiner; vollständige Ketten und echte Workerstack-/Heap-/Zeitspitzen bleiben offen. Diese statischen einzelnen Frames sind keine Messung von SMP, Flashdurabilität, Funk oder Energie.
