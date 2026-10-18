# Vollständiges Messprotokoll und geplanter Kameravergleich

Stand: 18. Oktober 2026. Nutzer bestätigt: Funkeln wurde beim vorherigen
Bewegungstest sichtbar schwächer und anschließend wieder heller. Das ist eine
Sichtbestätigung relativer Änderung, keine kalibrierte Abstandsmessung.

Für den nächsten Lauf ist beauftragt: Start in der App, mit dem ESP an der
Powerbank weggehen und zurückkommen, Messung beenden und anschließend melden.
Währenddessen keine erforderlichen Bildschirmabfragen oder Eingriffe durch Codex.

## Protokoll

Jeder bewusste Messstart legt eine eigene CSV-Datei unter den privaten
App-Dateien `files/rssi-runs/rssi-<Startzeit>-<UUID>.csv` an. Frühere Läufe bleiben
erhalten. Jede Anfrage und jedes abgeschlossene RSSI-Ergebnis werden gespeichert,
auch Funkfehler und Zeitüberschreitungen. Ein Ergebnis zählt erst nach Schreiben
und Dateisynchronisation als gespeicherter Messpunkt. Speicherfehler beenden
die Verbindung/Messung sichtbar statt Punkte still zu verwerfen. Größenlimit
32 MiB je Lauf; beim Erreichen ebenfalls Abbruch. Keine Zugangsschlüssel im Log.

Spalten: Ereignis, Handy-Wanduhr in Millisekunden, monotone Handyzeit,
Auftrag, Box-Millisekunden, Statussequenz, Ausgang, lokale Sendeannahme,
MAC-Zustellung, Empfang, Roh-RSSI, geglätteter RSSI, Zielintensität,
Empfängerzähler/-Bootkennung, LED-Effekt/-Revision, Peer, Funkelsteuerung aktiv.
LED-Ergebnisse, Stop-Anforderung und Verbindungstrennung sind eigene Ereignisse.
Eine bereits offene Probe darf nach `stop_requested` noch abgeschlossen werden;
sie gehört zum gleichen Lauf. Ein Verbindungsabbruch kann offene Aufträge mit
unbekanntem Ausgang hinterlassen; fehlende Antworten nicht als Erfolg auffüllen.

Die App zeigt die Anzahl gespeicherter Messpunkte. Während aktiver Messung bleibt
das Display wach. Appwechsel/Trennen behält den bisherigen Abbruch-/Stop-Rückweg.
Nach Rückkehr startet keine Messung automatisch.

Nach der Beendet-Meldung über ADB `run-as de.larpescape.companion` den neuesten
CSV-Lauf auslesen, Kopie und Quellhash erhalten und offene Aufträge, doppelte IDs,
Zeitsprünge und vollständige CSV-Zeilen prüfen. Bei noch offenen Ergebnissen erst
Abschluss abwarten. Danach gesamte Zeitreihe auswerten, nicht nur sichtbare Werte.
Der Debug-Zugang ist aktuell verfügbar; ohne USB-Verbindung zum Handy bleibt das
Protokoll trotzdem dort erhalten. Für spätere Release-Nutzung wäre ein eigener
Exportweg eine gesonderte Erweiterung.

## USB-Kamera: vorgeschlagener nächster Schritt

Am Rechner als **HD Webcam USB**, VID `0C45`, PID `6366` erkannt. Noch keine
Bildaufnahme, unterstützte Bildrate oder manuelle Belichtungssteuerung geprüft.
In den geprüften Laufzeitpfaden sind FFmpeg/OpenCV aktuell nicht verfügbar.
Die Kameraaufzeichnung/-auswertung ist deshalb noch nicht eingerichtet.

Geeigneter Weg: lokale Videoaufnahme über Windows DirectShow, z. B. mit
[FFmpeg](https://ffmpeg.org/ffmpeg-devices.html#dshow). Vor dem nächsten
gemeinsamen Videolauf zunächst kurze Probeaufnahme und unterstützte Einstellungen
prüfen. Kamera fest auf die LEDs ausrichten, Umgebungslicht stabil halten,
Überbelichtung vermeiden und automatische Belichtung/Verstärkung nach Möglichkeit
ausschalten. Andernfalls ist ein quantitativer Helligkeitsvergleich eingeschränkt.

Video vor dem App-Test starten. Handy-/PC-Zeit mit gemessener Unsicherheit
abgleichen und eine gemeinsame sichtbare Ein-/Aus-Zeitmarke aufzeichnen.
Die Kamerazeitachse dann den protokollierten Start-/LED-Ereignissen zuordnen.
Nach Testende Video und CSV unverändert sichern. Ohne diese Synchronisation
keine exakte Zuordnung einzelner Messpunkte behaupten.

Aus jedem Videobild einen festen LED-Ausschnitt auswerten, Hintergrund abziehen
und relative Helligkeit bestimmen. Über etwa zwei Sekunden mitteln, damit die
normalen Funkelpulse nicht als Regelungsfehler erscheinen. Roh-RSSI, geglätteten
RSSI, Zielintensität und Kamerakurve gemeinsam darstellen; zeitlichen Nachlauf,
Ausreißer, Clipping und fehlende Bilder markieren. Zielintensität ist ein Sollwert,
kein Messwert des tatsächlich ausgegebenen LED-Stroms. Kamerawerte sind ohne
Kalibrierung relative Helligkeit, keine Luxmessung und nicht zwingend linear zur
Prozentanzeige. Messwerte bei bekannten Abständen bleiben eine eigene Beobachtung.
