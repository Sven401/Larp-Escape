# Konfigurierbarer Spielgraph – Laufzeitvertrag

Stand: 5. Oktober 2026. Hardwarefreier Graphkern und Rechner-Vorschau implementiert. Der Produktionsloop verwendet weiterhin `ReichstagGame`. Rollen-/Kristallfortschritt, Geräteaktionen und Komponentenaufbau müssen noch an den neuen Kern angeschlossen werden; E2 ist nicht abgeschlossen.

## Eigentümer und Schnittstellen

- `include/GameExecution/Engine.h`, `src/GameExecution/Engine.cpp`: fester Ausführungsplan, laufender Zustand, Uhren, Eingabecache und Aktionsdiagnose. Keine Arduino-, FreeRTOS-, Netzwerk- oder Heapabhängigkeit.
- `IExecutionInputs` liefert bereits erfasste Werte. Keine Geräteabfrage oder Ereignisverarbeitung innerhalb eines Predicate. Pro Tick wird jedes Paar aus Feld und Komponenten-ID höchstens einmal abgefragt; Zeitfelder gehören dem Kern. Adapter liefern definierte Werte/Verfügbarkeit, boolesche Felder nur 0/1.
- `IExecutionActions` erhält Profil, Aktion und Kontext (Zustand, Übergang, Entry/Tick/Exit/Transition, Zeit). Beide Ports melden Ergebnisse ohne Exceptions. Geliehene Daten bleiben nur für den synchronen Aufruf gültig; Gerätequeues müssen benötigte Parameter kopieren.
- Alle Engine-Aufrufe gehören einem Anwendungseigentümer. Reentrante Änderungs-/Tickaufrufe aus Ports werden mit `busy` abgewiesen. `snapshot()` kopiert Werte, synchronisiert keine Tasks; Companion braucht weiterhin eine Mailbox.
- `start()` verlangt eine gestoppte Engine und ein gültiges Profil. Das Profil bleibt unverändert und lebendig bis `stop()` zurückkehrt. Stop führt gegebenenfalls Exit aus und löst den Profilbezug; der Snapshot behält kopierte Revisionen. Engine/Profile gehören nicht auf einen kleinen Taskstack.

## Tick, Reihenfolge und Grenzen

1. Aktive Zeit aktualisieren. Pausiert laufen keine Zustandsaktionen oder Guards.
2. Bei anstehendem Eintritt: Entry einmal ausführen, danach Tickaktionen in Listenreihenfolge.
3. Übergänge des aktuellen Zustands in aufsteigender Priorität prüfen. Prioritäten sind pro Quelle eindeutig; Reihenfolge der JSON-Tabelle bestimmt sie nicht.
4. Beim ersten Treffer: Exit, Übergangsaktionen, Ziel wählen und dessen Uhren zurücksetzen. Ziel-Entry/Tick folgen erst im nächsten Tick. Das bewahrt den bisherigen zentralen Maschinenablauf. Eine Selbstkante durchläuft ebenfalls Exit/Entry.

Pro Tick höchstens ein Übergang und 8 Entry + 8 Tick + 8 Exit + 4 Übergangsaktionen. Bis 64 Übergänge, je Guard bis 96 unterschiedliche Bedingungsknoten und Pfadtiefe acht. Geteilte Knoten werden pro Guard nur einmal ausgewertet; kompilierte Byteindizes ersetzen Referenzsuchen im heißen Bedingungspfad. AND/OR werten zuerst `first` aus und kürzen ab; NOT negiert seinen Kindwert. Fehlende Eingaben oder ungültige Boolwerte melden ein Tickresultat und beenden weitere Guardauswertung. Bereits ausgeführte Aktionen werden dadurch nicht zurückgenommen.

Aktionsresultate `applied`, `queued`, `rejected`, `unavailable` sind getrennt. Eine abgewiesene Anfrage stoppt die restliche Aktionsliste nicht; Zähler und letzter fehlgeschlagener Aktionsbezug sind sichtbar. Dadurch bleiben bestehende Reihenfolgen wie zwei direkt nacheinander angeforderte Bursts erhalten. `queued` beweist keine tatsächliche Hardwarewirkung. Zähler sättigen an uint32-Maximum.

## Bedingungen und Zeit

| Bedingung | Bedeutung |
| --- | --- |
| `predicate` | Typisierter Vergleich eines vorab erfassten Felds mit einem uint32-Operand |
| `all` / `any` / `not` | UND / ODER / Negation über referenzierte Knoten, mit definierter Reihenfolge |
| `state_elapsed_ms` | Aktive Zeit seit tatsächlichem Zustandseintritt, unabhängig von Guardabfragen |
| `transition_elapsed_ms` | Fortlaufende aktive Zeit seit erster Abfrage dieses Felds in diesem Übergang; pro Übergang eigene Uhr |
| `wait` | Einmaliger Treffer nach `operand` aktiven Millisekunden; nächste Abfrage beginnt ein neues Intervall und liefert zunächst false |

`wait` ist eine neue benannte Bedingungsart im noch nicht als API veröffentlichten v1-Profilvertrag: nur `id`, `kind="wait"`, `operand` (1..2³¹−1). Höchstens ein **unterschiedlicher** Wait-Knoten je Übergang; derselbe Knoten darf im Guard geteilt werden. Ein Wait-Knoten, der mehrere Übergänge bedient, erhält je Übergang eine eigene Uhr. Ungültige Formen oder mehrere unabhängige Waits in einem Guard werden vor Start abgewiesen. Appfähigkeiten müssen unterstützte Bedingungsarten nennen, bevor Profile übertragen werden.

Der bisherige `TransitionWait::elapsed` beginnt bei erster Guardprüfung, trifft nach 500 ms und setzt seine Beobachtung zurück, auch wenn nachfolgend Audio-BUSY den Zustandswechsel blockiert. Das Standardprofil verwendet deshalb jetzt `wait(500)`. Ein fortlaufender `transition_elapsed_ms >= 500` hat eine andere Bedeutung und wäre kein identischer Ersatz. Standardprofilrevision **2**, Hinweisprofilrevision **3**; Schema bleibt v1 mit zusätzlicher benannter Bedingungsart. Alte Profile mit Predicate-Zeitvergleich behalten dessen fortlaufenden Vertrag.

Clock-Delta verwendet uint32-Differenz und erlaubt einen Uhrüberlauf. Abstand aktiver Tick-/Steuerzeitpunkte höchstens 2³¹−1 ms; größere/ rückwärts wirkende Sprünge werden abgewiesen. Verstrichene Zeiten akkumulieren sättigend bei uint32-Maximum statt nach 49 Tagen wieder null zu werden. Zeitoperanden bleiben auf 2³¹−1 begrenzt. Pause friert Uhren ein; Resume setzt die Zeitbasis auf den übergebenen Zeitpunkt. Pause verändert keine bereits angeforderten Audio-/Lichteffekte; deren Betriebsvertrag liegt beim Gerätebesitzer.

## Manuelle Bedienung

`changeState` prüft Ziel und dessen `manualAllowed`; `takeTransition` zusätzlich die aktive Quelle und die manuelle Freigabe der Kante. Bei manuellen Wechseln werden automatische Bedingungen nicht abgefragt, Exit/Übergangsaktionen/Entry bleiben konsistent. Ziel-Entry bleibt bis zum nächsten aktiven Tick anstehend. Wechsel in einen pausierten Graphen bleiben pausiert.

Stop, Pause, Resume und Wechsel verlangen die beobachtete `stateRevision`. Veraltete Revisionen werden abgewiesen. Revision steigt bei Start/Stop, Wechsel und tatsächlichem Pause/Resume, nicht bei jedem Eingabewert oder Aktionszähler. Kein stiller Revisionsüberlauf; am uint32-Maximum werden weitere Steueränderungen abgewiesen. Boot-/Request-ID, Zugangsprüfung, Befehlsqueue und Netzwerkschnittstelle folgen in E4; diese Methoden allein erlauben noch keine App-Fernsteuerung.

`errorState` bleibt der deklarierte Profilbezug für den späteren Betriebs-/Recoveryvertrag; normale Fehlerkanten stehen explizit im Graphen. Der Kern erfindet keinen Geräteausfallübergang. Gerätestatus, Ausfallbehandlung und Wiederherstellung sind am kommenden Aktions-/Beobachtungsadapter zu konkretisieren.

## Rechner-Vorschau benutzen

```powershell
pio run -e profile-tools
.pio\build\profile-tools\program.exe --import docs/configuration/hint-profile.json --preview docs/configuration/hint-preview.txt
```

Ausgabe ist JSON Lines: Vorschaugrenzen, jede Aktionsanforderung und Zustandskopien. Die Beispiele stammen aus denselben C++-Factorys wie der Export. `hint-preview.txt` ist ein bearbeitbares Eingabeskript. Befehle:

| Befehl | Argumente |
| --- | --- |
| `start`, `tick` | Zeit in Millisekunden |
| `set` | Feldname, Komponenten-ID, Wert; ID 0 für globale Fortschrittsfelder |
| `state`, `edge` | Ziel-/Kanten-ID, erwartete Revision, Zeit |
| `pause`, `resume`, `stop` | Erwartete Revision, Zeit |
| `snapshot` | Keine |

Leere Zeilen und `#`-Kommentare erlaubt; maximal 255 Zeichen je Zeile und 100.000 Zeilen. Vorschau führt **nur Graphlogik** aus: Karten-/Kristallabfragen, Reset, Audio und Effekte werden protokolliert. Sie erzeugen weder echte Eingaben noch Rollenfortschritt oder Gerätewirkung. Alle Predicate-Eingaben werden explizit im Skript gesetzt. Ein „applied“ bezieht sich hier allein auf den geschriebenen Trace.

## Beobachteter Gebrauch und offene Abdeckung

Hinweis-JSON importiert und im gebauten Werkzeug ausgeführt: Standby → Idle → Zustand **42** nach 30.000 ms → cyanfarbene Burstanforderung (Effekt 200) → Idle nach 5.000 ms. Anschließend JSON geändert, ohne erneute Kompilierung: Zustand 42 auf **87** umnummeriert und Referenzen angepasst, Hinweiszeit 45.000 ms, RGB `0xffaa00`, Revision 4. Der Graph wählte Zustand 87 und protokollierte die orangefarbene Effektanforderung. Pause/Fortsetzen fror 9.999 ms aktive Zeit ein; manueller Wechsel und Stop mit passenden Revisionen wurden ausgeführt. Das ist beobachtete Vorschauverwendung, kein vollständiger Standardspielvergleich oder Gerätebeweis.

Enginegröße **4.200 Byte** auf MinGW32 und ESP32-Compiler; Profile unverändert **28.636 Byte**. Zwei Profile plus Engine mindestens **61.472 Byte**, ohne Rollenfortschritt, Adapter, Queues, Parserframes und WLAN. `-Os -fstack-usage` meldet im Graphkern maximal 112 Byte je Funktion; rekursive Guards und Portaufrufe kommen zur Gesamttiefe hinzu. Keine gemessene Taskreserve oder maximale Loopzeit daraus ableiten.

Keine neue Unity-Suite ergänzt oder ausgeführt. Vollständige Fehler-, Timer-/Uhrüberlauf-, Reentranz-, Zielerhalts- und Revisionsabdeckung offen. Nächster Schritt: profilabhängigen Rollen-/Kristallfortschritt und Aktions-/Beobachtungsadapter implementieren, dann Standardspielverhalten und zweiten Ablauf durchgängig belegen. Komponenteninitialisierung, Speicherübernahme, API und Android-App folgen; Produktionshardwareabnahme bleibt offen.

Frische Quellenprüfungen: 50 Architekturdateien / 125 Includes, 35 isolierte C++11-Header und 87 Formatdateien bestanden; Cppcheck 2.1 für Graphkern, Profilvalidator und Vorschau ohne Befund. Drei Buildprofile erfolgreich: Spiel RAM 55.156 / Flash 1.225.589 Byte (93,5 % des 1.310.720-Byte-Slots), USB RAM 21.480 / Flash 291.673 Byte, Rechnerwerkzeug mit Warnungen als Fehler. Graphkern im Produktionsloop noch nicht instanziiert und teilweise durch Linker entfernt; kein zukünftiger Gesamtbudgetnachweis. Kein Upload und keine Remote-CI.
