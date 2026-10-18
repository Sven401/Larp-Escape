# Laufzeitvertrag

**Neue verbindliche Zielarchitektur · 18.10.2026:** [VA-01](veranstaltungsarchitektur.md) und [Vertragsmatrix](veranstaltungsarchitektur-umsetzung.md). Lokaler Spieltakt und Ressourcenbesitzer bleiben unabhängig von Funk und Zentrale. Dauerhafte Ereignisaufnahme/-abgleich und beidseitige ESP-NOW-/WLAN-Adapter sind Folgearbeit #60/#61. Die folgenden Queue-/Sendepfade belegen noch kein dezentrales Veranstaltungssystem.

**Stand:** 5. Oktober 2026. Quellcode im Arbeitsbranch ist maßgeblich; die folgenden Grenzen sind Implementierungsverträge, keine gemessenen Gerätelatenzen.

## Besitzer und Kontexte

| Kontext | Besitzer | Kommunikation / Grenze |
| --- | --- | --- |
| Arduino-Anwendungslauf | `LarpEscapeApp`, `ReichstagGame` | Audio bedienen → ESP-NOW-Rückmeldungen verarbeiten → genau ein Spieltick → Statuskopie veröffentlichen |
| Anwendungslauf | `DFMinniHandler` | Alle Wake-Pin- und UART-Schreibzugriffe; keine eigene Audio-Task |
| LED-Task, Core 1, Priorität 1 | `GameLightEffects` | Einziger Besitzer von Effekten, LED-Puffer und FastLED nach Start; Queue 32 |
| WiFi-Callback | `EspNowRuntime::onSent` | Nur Statuswert kopieren, Queue 8, keine Spiellogik oder Serial-Ausgabe |
| Anwendungslauf | `EspNowRuntime::service` | Höchstens acht Rückmeldungen pro Tick; Erfolg/Fehler zählen |
| Statusleser, künftig Netzwerk | `CompanionStatusStore` | Einelementige Queue; Veröffentlichung überschreibt alten Wert, Lesen liefert Kopie |

Alle Referenzen in Anwendung und Adaptern leben bis zum Neustart. Queuehandles werden vor Nutzung eingerichtet und danach nicht ausgetauscht. Dynamische Deinitialisierung oder Wiederanmeldung von Netzwerk-Callbacks ist durch diese API nicht abgedeckt.

## Zeit und Überlast

- Die LED-Task strebt 33 ms pro Frame an. Pro Frame werden höchstens 32 Befehle bearbeitet. Bei überfälligem Frame wird ein Deadlinezähler erhöht; alte Frames werden übersprungen und mindestens ein Tick abgegeben.
- LED-Befehle sind Wertkopien. Volle Queue: Befehl verwerfen, Dropzähler erhöhen. Ein fälliger Helligkeitsschritt wird bei der nächsten Anwendungsausführung erneut angeboten, falls sein Einreihen scheiterte. Andere Effekte haben keine Zustellgarantie.
- Helligkeitsintervalle werden einmal im Producer geprüft. Der LED-Consumer wendet fällige Schritte ohne zweite Zeitprüfung an.
- ESP-NOW-Statusqueue voll: Rückmeldung verwerfen, Dropzähler erhöhen. Die Callback-Queue meldet Funkzustellung; der Rückgabewert von `ICommunicator::send` bedeutet nur, dass das lokale Senden angenommen wurde.
- DFPlayer-Wakeup: sofort HIGH, danach sieben Flanken im Abstand von mindestens 100 ms; abschließend LOW mindestens 100 ms. Nominal 800 ms. Verspätete Updates komprimieren keine Impulse. Wiederholte Wake-Anforderungen verlängern eine aktive Sequenz nicht.
- DFPlayer-Start wartet diese Sequenz einmal vor Beginn des Spiels ab. Im Spiel läuft sie nichtblockierend. Die zusätzliche periodische Wake-Funktion bleibt bis zur Hardwareprüfung erhalten.
- Uhren vergleichen vorzeichenlose Differenzen; die Tests enthalten den 32-Bit-Millisekundenüberlauf. Ein vollständiger Umlauf ohne zwischenzeitlichen Service liegt außerhalb des Timervertrags.

## Diagnose

Alle zehn Sekunden: maximales Loopintervall inklusive Diagnosekosten, minimale LED-Stackreserve in Bytes, beobachtete Queuehöchststände, maximale Renderdauer, verpasste Frames und Drops. Der Aufwand des gerade ausgegebenen Berichts ist erst im nächsten Bericht sichtbar.

Queuehöchststände sind Stichproben. Bei einem festgestellten vollen Puffer wird seine Kapazität erfasst. Loopdauer ist Ausführungszeit, keine Garantie für Inputreaktion oder Tasklatenz. LED-Stackreserve und Deadlines müssen am ESP32 unter Last erhoben werden.

ESP-NOW-Erfolgs-/Fehlerzähler gehören ausschließlich zum Anwendungslauf; ihre Getter sind keine taskübergreifende Schnittstelle. Drop-/Peakwerte sind atomar. Für die App werden ausschließlich veröffentlichte Statuskopien gelesen.

## Noch am Gerät zu prüfen

Wake-Pulsbreite und periodischer Wake-Bedarf; Busy-Pin-Verzögerung und Audioausfall; LED-Framerate/Stackreserve; Matrix- und NFC-Abfragekosten; ESP-NOW-Last und Kanal; Queueverluste sowie Kaltstart und Wiederanlauf. Eine Firmwareübersetzung ersetzt diese Prüfung nicht.
