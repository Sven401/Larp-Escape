# Atomarer Entzug einer ursprünglichen Rechtebindung

Stand: 7. Oktober 2026, nach c510b9f. Hardwarefreier Firmware-/Toolpfad bestätigt; SDK-Originalzuordnung und Geräteabnahme bleiben offen.

## Nachgewiesener Fehlerpfad

Im gezielten Austausch-/Ausfallmodell hat ein alter serialisierter IdentityAccess-Eigentümer noch eine ursprüngliche Verbindung. Der synchronisierte Kanal wird inzwischen durch einen neueren vertrauenswürdigen Produzenten aktualisiert. Der alte Disconnect veröffentlichte bislang uneingeschränkt einen neuen geschlossenen Epochwert. Dadurch überschrieben seine alten Daten die neue Bindung.

Der neue Regressionstest war vor der Korrektur rot: AuthorityUnavailable14 erwartet, Disconnected5 beobachtet. Danach erhält der gleiche Pfad die neue Authority, führt einen echten Start durch den Anwendungsbesitzer aus und erzeugt einen aktuellen v2-Status. Es ist kein gemessener SDK-/Funkfehler. Der reguläre Vertrag bleibt genau ein serialisierter vertrauenswürdiger Zugangseigentümer; der Test schützt die Verarbeitung übrig gebliebener alter Arbeit bei einem Austausch.

## Neuer Port

`IAuthorityPublisher::closeAuthorityIfCurrent(expected,published)` und `AuthorityState::closeIfCurrent` vergleichen die gesamte ursprüngliche Authority: Epoch, Principal und GMbit.

| Zustand | Ergebnis | Wirkung |
|---|---|---|
| ursprüngliche Authority aktuell | Published | neue geschlossene Epoch, Principal0/GMfalse |
| inzwischen andere Authority | Conflict | aktuelle Authority/Highest und Zielwert unverändert |
| Kanal geschlossen/uninitialisiert | Suspended | Zielwert unverändert |
| maximale aktuelle Epoch | Suspended | Kanal geschlossen, kein Wrap und Zielwert unverändert |

Der tatsächliche FreeRTOS-Kanal friert expected vor der ersten externen Beobachtung ein. Vergleich und neue geschlossene Epoch laufen unter derselben kurzen Authoritysperre wie Aufnahme und Rechtevergabe. Keine Queuewartezeit, kein SDK-, Datei-, Parser- oder Spieltickaufruf wird hinzugefügt. Eigene Laufzeit/SMP/Gesamtstackreserve wurden nicht gemessen.

Ein Entzugsmismatch ist eine abgelehnte Erwartung des alten Produzenten. Er ist kein Update einer neuen Authority und erhöht keine Highwatermark. Historisch blieb in diesem Schnitt der Vergabepfad `advanceAuthorityIfCurrent` unverändert und schloss bei Konflikt die Authority. Der nachfolgende [Original-Proof-Grant-Schnitt](peer-proof-grant-v1.md) korrigiert dies anhand eines bestätigten Gegenbeispiels: Auch ein alter Vergabekonflikt erhält nun den Ersatzpublisher und Zielwert; der alte Identitybesitzer wird lokal retired. Allgemeines `update`/Faultfencing bleibt unverändert.

## Zugangseigentümer

Disconnect entfernt seine alte lokale Verbindung vor dem Portaufruf. Ein Konflikt setzt authorityRetired für die Lebenszeit des alten Eigentümers. Er kann sich danach weder durch begin noch durch connect, neue Rollenvergabe oder Rollenentzug wieder einsetzen. Die öffentliche Gerätekennung bleibt lesbar. Ein bereits suspendierter Kanal kann dagegen mit einer neuen originalen Verbindung und einer frischen Epoch wieder aufgebaut werden.

Gebundene Sicherheitsfehler und Rollenentzug verwenden denselben atomaren Schließport. Alte Fehler können deshalb die neue Bindung ebenfalls nicht überschreiben. GMvergabe und echte Credentialprüfung werden daraus nicht abgeleitet.

Die zu vergleichende Authority ist der im Eigentümer gespeicherte vollständige Wert. `BoundConnection.token` ist die Epoch beim Verbindungsbeginn; Authentisierung und Rollenänderung erhöhen die Authorityepoch innerhalb derselben Verbindung. Die beiden Werte sind deshalb kein austauschbares Abbruchmerkmal.

## Vertikale Prüfung

13 neue Hostfunktionen, finale Suite324/324 erfolgreich ohne Fixtureexport. Neue Fälle prüfen aktuellen Entzug, Produzentenaustausch, alte Abbrüche/Sicherheitsfehler/Rollenentzüge, nicht wieder einsetzbaren Eigentümer, eingefrorene Erwartungen, Austausch am atomaren Port, normale Wiederverbindung, Zielwerterhalt, maximalen Zähler und unveränderte Vergabepolicy. Ein zuvor kopierter alter Raw-Befehlsframe bleibt unausführbar und erhält keine neue Authority.

Der vollständige Austauschpfad erzeugt `fixtures/gatts/retirement-observation.json` und `retirement-status-v2.json`: alter Verbindungstoken2/alteAuthority3, Ersatz4 bleibt4, tatsächliche Antwort und aktueller Status gehören zu4/Principal99; Lease3/nextRequest2 und tatsächliches Running. Die private Produzentenbindung steht nur in der Hostbeobachtung; es gibt keine neuen App-/Statuswirefelder.

`scripts/check_identity_retirement.py` prüft diese Zusammenhänge unabhängig und verwirft23 falsche Beobachtungen/Statuskorrelationen. Der tatsächliche Status erfüllt das kanonische v2-Schema. Bestehende Gegenstellen bleiben gültig. Die isolierte SDK-ABIprobe ruft den tatsächlichen FreeRTOS-Port mit geschlossenen synthetischen Epochen auf; keine daraus vergebenen Peerrechte und kein BLEstart.

## SDK-Befund und nächster Schritt

Der aktuelle Quellenabgleich hat GATTS- und DMereignisse nachvollzogen:

- [GATTS-Bridge v4.4.6](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/host/bluedroid/btc/profile/std/gatt/btc_gatts.c): kopierte BTAanforderung vor SDKcallback; Writewert bleibt nur im Callback geliehen. Nach dem Callback wird dessen Kopie freigegeben.
- [DM-Bridge v4.4.6](https://raw.githubusercontent.com/espressif/esp-idf/v4.4.6/components/bt/host/bluedroid/btc/core/btc_dm.c): Authcomplete kopiert Adress-/Sicherheitsfelder; der öffentliche Callback führt unseren Originaltoken nicht mit. Das ist noch keine Zuordnung zur stabilen Registryidentität.

Diese Quellenbelege machen eine spätere Handle-/MACsuche nicht zur Originalherkunft. Nächster Schritt ist weiterhin der geordnete geschlossene SDK-Verbindungs-/Ereignispfad mit ursprünglicher Bindung und tatsächlichem Sender. Verifizierer, Provisionierung, persistente ausdrückliche GMzuordnung, Produktionsanschluss und Geräteabnahmen sind weitere offene Anforderungen. Der einzelne Entzugsschnitt schließt das Gesamtziel nicht ab.

## Abschließende Builds und Budget

Nach allen C++-Änderungen: 324/324 native Fälle ohne Fixtureexport, unabhängige Gegenstelle/23 negative Korrelationen, Architektur124Dateien/267Includes,75 isolierte C++11-Header und235 Formatdateien erfolgreich. Zwölf unveränderte v2-Vertragspaare zum Appcheckout sind frisch byte-/SHAgleich; keine Appprüfung oder Appänderung dieser Sitzung.

Fünf finale Builds erfolgreich: Produktion55.148ByteRAM/1.229.077ByteFlash(93,8%), alte USBprobe21.480/291.881, Profilprobe107.560/406.865, isolierte SDKprobe26.408/287.905 und Profilwerkzeug. Gegen c510b9f unverändertes statisches RAM; Profilflash+252Byte, SDKflash+592Byte. Die Profilprobe reserviert weiterhin57.272Byte Startup-Profilheap plus Allocator/Dateisystem.

Im tatsächlichen SDK-ELF bleibt IdentityAccess144Byte und der Kanal1264Byte. Eigene Compilerframes: AuthorityState.closeIfCurrent32Byte, IdentityAccess.publish48, disconnect96, synchronisierter Kanalport48/nonvirtualThunk32, SDKsetup560 und loop208. Aufrufketten, Funk/SDKspeicher, Gesamtstackreserve und Laufzeit sind damit nicht gemessen. Keine dynamische Allokation oder neue Queue im Entzugspfad.

Lokale Belege vom7.Oktober2026 unter dem expliziten Tempverzeichnis: `larp-identity-retire-host-final-20261007.log`, `larp-identity-retire-counterpart-20261007.log`, `larp-identity-retire-builds-final-20261007.log`, `larp-identity-retire-{architecture,headers,format}-20261007.log`, Budgetverzeichnis `larp-identity-retire-budget-20261007/summary.txt`. Reproduzierbar mit den bestehenden PIO-Umgebungen, Architektur-/Header-/Formatchecks und `scripts/check_companion_fixtures.py`; Fixtureexport bleibt eine getrennte Hostausführung.

Kein Upload, BLEstart, Androidlauf, physischer Flash-/NVS-/Entropiefehler, SMP-/Zeit-/Heap-/Energie-/Gesamtstacknachweis, cppcheck oder Remote-CI in diesem Schnitt.
