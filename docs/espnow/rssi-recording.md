# Vollständiges Messprotokoll und Kameravergleich

## Geprüfter Stand

Code `6e6f09326ba9dda2136674de7300fe7eafc605b3`: 76 App-JVM-Tests ohne Fehler
oder ausgelassene Fälle, Debug-Build und Debug-Lint erfolgreich (bestehende drei
Warnungen/eine Info, null Fehler). Neue Journalfälle prüfen 201 Ergebnisse
einschließlich Fehlern/später Antwort, Wiederöffnung/neuen Lauf und Speicherfehler.
Keine Firmwareänderung in diesem Schritt.

APK SHA-256 `80a24cad9b0d3e7e91b6bd29b00fdbad1cd4ff4c5f8d4a4405438823329f999b`
nach frischer APK-/Datensicherung mit gleicher Signatur installiert und
zurückgelesen; alle acht vorherigen Dateien unverändert. APK/Manifest:
`outputs/espnow-rssi-journal-20261018/` im Hauptcheckout.

Begrenzter Gerätelauf: reine Messung bewusst gestartet/beendet, UI meldet
25 gespeicherte Punkte. Private CSV vollständig ausgelesen: 25 eindeutige
Anfragen, 25 Ergebnisse mit Ausgang 0, keine offenen Anfragen, 52 Ereignisse
einschließlich Start und Stop-Anforderung. CSV SHA-256
`b2d7a08ae3277b5f97dd963adf4269706fe9ed9fa38c6b46c1c19afd06e55c6b`.
Belege: `private/espnow-app/journal-install-20261018/`, CSV/Prüfsumme unter
`exports/123917-386726/`. Dies war der technische Probelauf; der spätere
vollständige Gehversuch ist im unten verlinkten Messbericht dokumentiert.
Nächster Start erzeugt eine eigene Datei; der technische Probelauf bleibt erhalten.

## Auftrag und Ablauf

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

## USB-Kamera: abgeschlossener Gehversuch

**Belegt, 18.10.2026:** Kameraaufnahme und Auswertung sind inzwischen eingerichtet.
Der Nutzer hat den kombinierten Gehversuch durchgeführt und beendet. Sämtliche
92 Anfragen und Ergebnisse wurden ausgelesen; der Kameraverlauf bestätigt
Abdunkeln und Wiederaufhellen sowie das abschließende Aus.

Der [vollständige Messbericht](distance-camera-result-2026-10-18.md) ist die
kanonische Quelle für Zeitabgleich, Ergebniszahlen, Bildprüfungen, Aufnahme-
bedingungen und Grenzen. Die isolierte Kalibrierung stammt aus dem getrennten
[Kamera-PR #68](https://github.com/Sven401/Larp-Escape/pull/68).

Die frühere Einschätzung fehlender OpenCV-/Aufnahmemöglichkeiten ist durch den
inzwischen eingerichteten Kamera-Messweg überholt. In diesem Versuch wurden
feste LED-Aufnahmeflächen bei rund 30 Bildern/s und regelmäßige Kontextbilder
gespeichert. Kein lückenloser Vollbildfilm und keine Luxmessung. RSSI-Zielwert
und relative Kamerahelligkeit haben unterschiedliche Prozentbezüge.
