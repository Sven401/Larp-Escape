# Näheglut: konkreter Spielanschluss

Stand 18.10.2026. **Technischer Umsetzungsvorschlag, noch nicht implementiert.**
Auftrag: nach dem [Stopbefund](naeheglut-stop-diagnose-2026-10-18.md) den
Spielanschluss konkretisieren. Codebasis `7e5f4f38e88ed76358e31dc66d267da9cb0fbc1f`.
Grundlage: [Effektvertrag](effect-command-contract.md),
[Rolle](rollen/led-effekt-architekt.md), [Entwurf](naeheglut-design.md),
[Profilübernahme](../configuration/activation.md) und VA-01.
Farbfreigabe und das unten markierte Pausenverhalten bleiben Nutzerentscheidungen.

**S3-Stationen · Entwurf vom 31.10.2026:** Das
[Konzept für Proximity-Spielstationen](esp32-s3-proximity-konzept.md) ergänzt
diesen Anschluss um zwei bewegte Requisiten, autonome Näheeffekte und
Spielaktionen nach bestätigter grober Funknähe mit Verweilen. Es führt die
bestätigte Näheglut-Semantik fort; neue Kontakt- und Umsetzungsdetails bleiben
als Entwurf gekennzeichnet.

**Bestätigte Spielbedeutung, Nutzerantwort vom 18.10.2026:** Näheglut ist wie
Funkeln ein durchlaufender Effekt im Spiel. Startende Spielaktion ist die
Erkennung einer korrespondierenden Spielstation. Es ist kein an ein
automatisches Phasenende gebundener Einmaleffekt.

## Zielpfad und tatsächliche Lücken

Lokaler Spielbesitzer → bewerteter Nähewert → kopierter LED-Auftrag → ein
LED-Besitzer → gemeinsamer Näheglut-Kern → Mischpuffer → ein Ausgabetreiber.
Kein Hostskript und keine dauerhafte BLE-Verbindung im autonomen Spiel.

| Bestehende Stelle | Nötige konkrete Ergänzung |
| --- | --- |
| `GameConfiguration/Profile.h`, `EffectKind` | Eigenes `NearGlow` mit Parametern und expliziter Eingangsbindung; Parser/Validator/Export gemeinsam erweitern. Bestehende Enum-/Profilsemantik erhalten und Formatversion gezielt entscheiden. |
| `GameExecution/Session.cpp` | Explizite Start-/Update-/Stopaktion und Laufzeitbindung je gestarteter Instanz. Nicht jeden Tick erneut starten. |
| `DeviceRuntime/Channel.h`, `Broker.cpp` | Kopierte Effektoperation mit Instanzidentität und getrenntem Annahme-/Ausführungsstatus. `ApplyEffect` und dessen kurzes Ticket allein reichen nicht. |
| `GamePorts/ILightEffects.h`, `GameLightEffects` | Entsprechender Port für das feste Spiel; keine eigene zweite LED-Task. Bestehende void-Aufrufe ohne Einzelresultat sind dafür unzureichend. |
| `NearGlow.h/.cpp` | Bestehende Berechnung und Zustandsmaschine wiederverwenden. Slotwahl und Generationen kapselt der Produktadapter. |
| `Runtime::Park`, `IChannel::quiescent` | Lang lebende Instanzen, offene Stops und laufende Ausgabe zusätzlich zu Queue-/Ticketzuständen berücksichtigen. |

Das bestehende Profil kennt Twinkle/Burst/Ripple/BrightnessRamp/Palette,
jedoch weder NearGlow noch einen laufenden skalaren Näheeingang. Der bestehende
Broker kopiert `Effect` und sendet an `Owner::Led`; das erzeugt noch keinen
realen NearGlow-Produktpfad. `Session::stop/pause` steuern derzeit die Engine,
liefern aber keinen neuen LED-Instanzvertrag. Der manuelle RMT-Baugruppenpfad
und das feste FastLED-Spiel sind verschiedene Ausgabepfade; gemeinsame
Hardware darf nie von beiden zugleich betrieben werden.

## Eingang und Spielbedeutung

Vorgeschlagener kopierter Eingang: `sourceId`, `sourceEpoch`, `sampleSequence`,
`observedLocalMs`, `level` (0–1000), `valid`, `reason` und Spiel-/Profilgeneration.
Keine Profilzeiger oder aus Fremdpaketen ungeprüft übernommenen Zeitstempel.
Der lokale Quellenbesitzer prüft Identität, Herkunft und Reihenfolge. Der
Spieladapter bewertet den Eingang; die LED-Instanz kennt keine Funkpakete.

Eine konfigurierte Zuordnung verbindet Spielstation und Lichtbindung, etwa
`stationId → componentId/instanceId/sourceId`; keine Startberechtigung allein
aus starkem RSSI. Der lokale Spielbesitzer meldet `StationRecognized` erst
nach Prüfung dieser Korrespondenz. Für eine noch nicht gestartete, freigegebene
Bindung startet das Ereignis genau eine Instanz. Wiederholte Erkennung derselben
Station aktualisiert nur Eingang/Status; sie startet weder Phase noch Rampe neu.
Erkennung und kontinuierlicher Nähewert sind unterschiedliche Eingänge.
Frische Werte müssen derselben gebundenen Station/Quelle zugeordnet sein.
Eine andere erkannte Station ersetzt den laufenden Beitrag nicht stillschweigend;
Parallelbindungen benötigen eine konfigurierte Zuordnung und freie Kapazität.
Nach ausdrücklichem Stop bleibt die Bindung gesperrt, bis das Spiel sie bewusst
neu freigibt; bloß fortlaufende Stationsmeldungen sind kein Neustart.

Für den ersten technischen Anschluss ist der bereits erprobte RSSI-Weg ein
geeigneter Kandidat: gleicher Q8-Filter, bestätigter Peer, ganzzahliger Wert,
`level=clamp(20*(RSSI+90),0,1000)`; **kein Metermaß**. Fehlerantworten und
Duplikate erneuern Frische nicht. Gültig nur bei lokalem Alter **<3000 ms**;
bei genau 3000 ms wird der Eingang ungültig. Die Frische wird vom lokalen
Spielbesitzer auch ohne neue Funkmeldung geprüft. Der Produktionsfilter und
das Verhalten bei einem Peer-Neustart müssen als eigener Quellenvertrag
festgelegt werden; der Laborhost ist kein automatisch übernommener Produktadapter.
Eine Quellenepoche wechselt nur nach bestätigter lokaler Quellenannahme;
aufgespeicherte Veranstaltungsereignisse sind keine frischen Nähemessungen.

Ungültig: aktive Instanz blendet ihren Beitrag nach null. Frisch gültig:
derselbe aktive Lauf kehrt weich zurück. Beendet/gestoppt: Messungen dürfen
keinen Start auslösen. Der Eingangsadapter darf Messungen unabhängig weiter
beobachten, ohne eine nicht aktive Lichtinstanz zu erzeugen.

## Auftrags- und Stopzustellung

Vorgeschlagener Schlüssel: `(ownerKind, ownerEpoch, componentId, instanceId,
startGeneration)`. Auftragssequenz je Instanz streng monoton. `ownerEpoch`
grenzt Spiel-/Profilbindung von manueller Sitzung ab; die kurze Brokersequenz
und `stateRevision` bleiben davon getrennt. Ein Zustandswechsel stoppt nur
explizit daran gebundene Instanzen, nicht jede gültige Beobachtung eines
weiterlaufenden Effekts. Alte Rückmeldungen bestätigen nie eine neue Revision.

- `Start`: vollständige Parameter, freie Kapazität, neue Generation. Startet
  bei null mit dem aktuellen frischen Eingang; ohne frischen Eingang mit
  `valid=false` und damit Ziel null.
- `Update`: nur aktiver passender Schlüssel. Übernahme atomar an der Rendergrenze.
  Identisches Ziel startet die Rampe nicht erneut. Ungültiger Wert ist fachlich
  erlaubt; Parameter außerhalb der Grenzen werden ohne Teilwirkung abgewiesen.
- `Stop`: am Adapter atomar die weitere Updateproduktion dieser Generation
  sperren und den vollständigen Instanzschlüssel zustellen. Der Besitzer
  verwendet seine eigenen zuletzt angewandten Parameter für `Scene::apply(Stop)`;
  ein Produzent mit altem Parameterschnappschuss darf nicht am Kernvergleich
  `stop-parameters` scheitern. Wiederholter Stop verlängert die Ausblendung nicht.
- `Start` nach Stop: bewusster Auftrag mit neuer Generation, erst nachdem der
  bisherige Beitrag beendet und dessen entsprechende Ausgabe abgeschlossen ist.
  Frühere Updates werden sichtbar als veraltet verworfen.

**Begrenzter erster Adaptervorschlag:** zwei Näheglut-Instanzen entsprechend
dem vorhandenen Kern. Keine Zusage universeller Effektkapazität. Start/Update
nutzen die bestehende 16er-Owner-FIFO; pro Service höchstens 16 Übernahmen,
danach Rendern. Volle Queue lehnt sichtbar ab. Je Instanz existiert zusätzlich
eine reservierte Stop-Latch mit genau ihrem aktuellen Schlüssel, Sequenz und
Resultat; eine laufende Latch wird nicht durch einen anderen Schlüssel überschrieben.
Annahme und Sperre erfolgen serialisiert/atomar, bevor Erfolg gemeldet wird.
Solange der Slot belegt ist, ist diese Stopkapazität verfügbar. Vor dem
FIFO-Drain verarbeitet der Besitzer Stops und verwirft eingereihte ältere
Updates/Starts der gesperrten Generation. Ein Stop vor erstmaliger Startübernahme
annulliert den Start; neue Starts für diese Instanz bleiben bis terminaler
Rückmeldung gesperrt. Kontroll-Park hat Vorrang vor beiden Datenpfaden.

Angenommen, angewandt, ausblendend, beendet, Frame übertragen und Ausgabefehler
sind getrennte Zustände. Ein reservierter Stop ist kein sofortiges physisches
Aus. Ausgabefehler lässt den Stop ausdrücklich **unbestätigt**, auch wenn der
berechnete Beitrag null ist. Fehler-/Statuszähler sättigen; Generationen und
Sequenzen laufen nicht über. Vor Erschöpfung neue Aufträge sichtbar ablehnen.
Resultatspeicher pro Slot: letzter angenommener Auftrag mit vollständigem
Inhalt/Resultat; gleiche Sequenz mit anderem Inhalt ist Konflikt. Außerhalb
dieses Fensters ältere Sequenzen abweisen, nie erneut ausführen.

## Wirkung, Mischung und Betriebsübergänge

Arbeitsvariante RGB `(24,8,0)`, Gain 1000, Smoothstep 1000 ms, 23 LEDs.
Gleicher Kern wie im Labor; `Scene::value` liefert den einzelnen Festkommabeitrag.
`Scene::render` enthält bereits Hintergrund und ADD32-Klemmung und wird deshalb
**nicht** als einzelner Beitrag in einen weiteren Mixer addiert.
Breit addieren, einmal am festgelegten Ausgangsbudget begrenzen. Beispiel im
begrenzten ADD32-Prüfmodus: Hintergrund `(4,4,4)`, Blau `(0,0,8)`, volle Näheglut
`(24,8,0)` ergeben `(28,12,12)`; nach gezieltem Stop `(4,4,12)`.
Zwei volle Nähegluten mit `(4,4,4)` ergeben `(32,20,4)` nach Klemmung.

Der erste Produktversuch bleibt im begrenzten ADD32-Prüfmodus. Das erlaubt
keine globale Begrenzung bestehender FastLED-Effekte auf 32 und keine doppelte
Helligkeits-/Gammaanwendung. Für Zusammenspiel mit den bisherigen RGB8-Effekten
müssen Farb-/Helligkeitsraum und elektrisches Gesamtbudget separat festgelegt
werden; ohne das ist der gemischte Vollspielpfad nicht abnahmebereit.

| Ereignis | Vorschlag / bestehende Grenze |
| --- | --- |
| Korrespondierende Station erkannt | Bestätigter Starttrigger für einen durchlaufenden Effekt; pro freigegebener Bindung genau einmal starten. |
| Wiederholte Erkennung / Phasenwechsel | Kein impliziter Neustart oder Stop. Ein Stop muss ausdrücklich vom Spiel beauftragt werden. |
| Pause | **Produktvorschlag, unbestätigt:** Beitrag weich ausblenden, Startbindung merken, neue Effektstarts sperren. Quellenfrische läuft auf monotoner Realzeit weiter. |
| Bewusstes Fortsetzen | Zum Pausenvorschlag: neue Generation aus vorgemerkter Bindung starten, nur mit frischem Eingang; zuvor ausdrücklich gestoppte Bindungen bleiben entfernt. |
| BLE-/WLAN-Verlust | Autonomen Spielbeitrag nicht pauschal stoppen; manuell besessene Beiträge nach eigenem Zugangsvertrag beenden. |
| Profilwechsel/Park | Annahme sperren, Epoche einzäunen, Spielbeiträge entfernen und veraltete Jobs terminal behandeln. Hardwareausgabe leeren und frisches Schwarz bestätigen, bevor Park/Quieszenz Erfolg meldet. |
| Park-/Ausgabetimeout | Fehler/Barrierefehler, keine behauptete Stilllegung; bestehender Runtime-Rückweg unter neuer Generation. |
| Boot/Spielreset | Keine Wiederherstellung alter Lichtinstanzen oder letzter Nähemesswerte als frisch. Nur bewusster Spielstart aktiviert neue Bindungen. |

## Umsetzbare Pakete und Prüftore

1. **NG1 Quellen-/Spielvertrag:** bestätigten Stationserkennungs-Start abbilden;
   Stations-/Quellenzuordnung, Peer/Eingangsquelle und Pausenregel konkretisieren,
   Frische/Reset und Profilformat festlegen. Ein
   synthetischer Näheproduzent ermöglicht unabhängige Softwarearbeit.
2. **NG2 Besitzeradapter:** obige Schlüssel, Stop-Latches, Wertkopien und
   Zwei-Slot-Kapazität implementieren. Tests: volle FIFO plus Stop, Stop vor
   Startübernahme, Update während Stop, verspätete alte Jobs/Callbacks,
   Duplikat/Konflikt, Slotwiederverwendung und Zählererschöpfung.
3. **NG3 Spiel-/Profilanschluss:** Parser/Validierung, explizite Aktionen und
   dynamische Eingänge; Quellenfrische bei 2999/3000 ms, Uhrüberlauf,
   Peerboot/Replay, Pause/Fortsetzen und zustandsübergreifende Bindungen prüfen.
   Stationsfälle: falsche Korrespondenz ohne Start, passende Station genau einmal,
   wiederholte Erkennung ohne Rampenneustart, fremde Station ohne Übernahme,
   Veraltung/Wiederkehr bei gleicher Instanz und Erkennung nach explizitem Stop.
4. **NG4 Ausgabe/Barriere:** gemeinsamer Kern, genau ein Treiber,
   Übertragungsrevisionen; gezielter Stop erhält Hintergrund/zweite Instanz.
   Park bei belegter Queue, laufender Rampe, In-flight-TX und Treiberfehler.
   Kein erfolgreicher Park nur wegen leerer FIFO oder abgeschlossener Tickets.
5. **NG5 Produktversuch:** bestehende Abnahmen erhalten; anschließend passende
   neue Geräte-/Optik-, Leistungs-/Dauerprüfungen am tatsächlichen Paar. Der
   [offene Dunkelkontrolllauf](naeheglut-stop-diagnose-2026-10-18.md) bleibt
   eigenes Tor. Kein Vollspielabschluss aus Laborbelegen.

Ein neuer App-/Wirevertrag ist nur nötig, wenn die Spielleitung einzelne
NearGlow-Instanzen direkt bedienen soll. Vorhandene V2-LED-Nachrichten und
deren exklusiver Effektstatus werden nicht stillschweigend umgedeutet.
Dieser Auftrag liefert Diagnose und Umsetzungsspezifikation; NG1–NG5 sind
damit nicht implementiert und es erfolgte kein Gerätezugriff.
