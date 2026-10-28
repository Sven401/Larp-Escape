# Verifizierte Antwortauswahl aus dem synchronisierten Antwortpuffer

**Stand:** 7. Oktober 2026. **Status:** schmaler tatsächlicher Queue-/Host-/Probenpfad;
physischer Transport, Identität/Pairing und tatsächliche Peerbestätigung offen.

**Korrektur vom 24.10.2026:** Der Antwortpuffer ist ein eigener Wert-Ring mit
vier Plätzen unter der Authoritysperre. Aufnahme, aktuelle Auswahl und rohe
Diagnoseentnahme verwenden dieselbe Sperre, ohne verschachtelte RTOS-API.
Die ursprünglichen Prüfzahlen unten sind historische Belege; der
[aktuelle Hostprüfweg](../../test/test_configured_control/README.md) ergänzt
FIFOumlauf, Vollgrenze, Zielerhalt und echten Host-Threadkonflikt.

GameControl kopiert ursprüngliche Principal+connectionEpoch in jede
RoutedGameReply. Der verifizierte Transporteigentümer benutzt nun
FreeRtosGameControlChannel::takeCurrentReply anstelle einer blinden Rohentnahme.
Unter derselben kurzen Authoritysperre prüft es aktuelle GM-Verbindung und
entnimmt höchstens vier kopierte Antworten aus dem Wert-Ring, ohne wartende API. Nicht passende
Principal/Epoch oder unavailable/suspendierte/Observer-Verbindung verwirft
bereits gepufferte Antworten. Eine passende wird kopiert und true geliefert.
False erhält das Ziel unverändert; kein Gerät/Netzwerk wird innerhalb der
Sperre aufgerufen. Keine neue Queue, kein Heap oder zusätzliches Profil.
Roh-takeReply bleibt ausdrücklich Diagnose-/Testentnahme ohne Routingzusage.

## Beobachtungspunkt und spätere tatsächliche Sendelogik

Auswahl ist der letzte gemeinsame synchronisierte Beobachtungspunkt beim
Entnehmen; sie ist keine physische Übertragung. Nach Freigabe der Sperre kann
die Verbindung verschwinden/wechseln. Die kopierte ursprüngliche epoch darf
nie auf einen neuen Connectionhandle umetikettiert werden. Der spätere
Beareradapter muss vor einer tatsächlichen Übertragung nochmals genau die
gebundene verifizierte Verbindung prüfen und sein Lifecycle-/Sendelock im
eigenen Kontext definieren. Eine bereits vom Funkstack angenommene Übertragung
kann nicht rückwirkend als abgebrochen behauptet werden. Kein SDK-Sendaufruf
unter dieser Queue-/Authoritysperre.

Diese Funktion ist kein allgemeiner Multiclientrouter: der aktuelle Vertrag
hat eine verifizierte GM-Verbindung. Ein entzogenes/altepoch Ergebnis kann
daher bewusst verloren gehen; neue Verbindung erhält frischen Boot/Lease/
nextRequest-Status. Das Spiel läuft autonom weiter. Unavailable oder Observer
erhält keine per-GM-Kontrollantwort. Eine Unauthorized-Spielantwort mit
weiterhin **passenden aktuellen** Routingmetadaten darf zur zugehörigen
aktuellen Verbindung (z.B. falsche Clientlease); outcome ist kein Ersatz für
die Routingprüfung.

## Vertikaler Nachweis

Vier zusätzliche Funktionen im tatsächlichen host-configured-control-Pfad:
Start Executed gepuffert → disconnect/reconnect mit recyceltem Principal →
aktueller Stop → nur epoch3-Stopantwort ausgewählt; fremder Principal/Epoch,
fehlende Initialisierung und False-Zielerhalt; volle echte Replyqueue →
Entzug vor Auswahl → alle alten Antworten verworfen; Auswahl → späterer
Entzug/Reconnect → alte kopierte epoch bleibt1 statt neu3. Das tatsächliche
Spiel bleibt beim Entzug aktiv. RTOS-Ports sind kopierende deterministische
Fakes; keine SMP-/physische Routing-/Zeitabnahme.

Isolierte Profilprobe verwendet diese Auswahl für synthetische Startantworten,
keinen zusätzlichen Spieltick. Kein Upload, Funk oder Androidzugang.
55/55 gezielte Hostfunktionen ohne Fixtureexport erfolgreich/Exit0,
%TEMP%/larp-reply-routing-host-final.log. Zwei betroffene finale Builds
profile-tools und esp32-profile-runtime-probe SUCCESS/Exit0,
%TEMP%/larp-reply-routing-final-build.log. Neue Probe99.056 RAM/386.637 Flash
plus57.272 Profilheap/Allocator/FS; keine Boardreserve. Produktion/alte Probe
wurden seitc9654c3 nicht erneut gebaut, keine betreffenden Einstiegssourceänderungen.
Architektur83/193, Format141/clang19.1.7, diffcheck sauber; kein Cppcheck dieses
Schnitts. Typbelegung/Statusschemata unverändert, keine neue Queue oder Heap.

Der folgende [Antwortframepfad](reply-framing-v1.md) übernimmt die tatsächliche
verbindungsgebundene Auswahl, friert JSON ein und bearbeitet pro Owneraufruf
höchstens einen kopierten Frame. Seine synthetische Bearerprüfung ist ein
Host-/Probenbeleg; physische SDK-Sendebindung bleibt gesondert abzunehmen.
