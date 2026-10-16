# Tastenmatrix: Sammeldurchgang

Stand: **16. Oktober 2026, Europe/Berlin**. Auftrag aus Sitzung
`01a145f4-3cbc-7971-b8aa-26e0a9437eb1`: alle vorhandenen Kontakte ohne
Einzelbestätigungen erfassen und nach einer Abschlussmeldung auswerten.
Basis: `e4b2ce024655ae9e951e04542dc7f473e1bc6e9c` (#49), eigener Branch
`codex/matrix-collection`. Maßgeblicher neuer Ablauf; die früheren
Einzelschritte M2–M4 sind für diese Bestückungserfassung ersetzt.

## Bedienung

1. App öffnen, Box suchen und verbinden. Alle Magnete entfernen und einen
   frischen, leeren Matrixstatus abwarten. Erst dann ist der Start möglich.
2. **Neuen Sammeldurchgang starten.** Alle tatsächlich vorhandenen Kontakte
   nacheinander mindestens eine Sekunde halten, danach mindestens eine
   Sekunde lösen. Reihenfolge und Wiederholungen sind beliebig. Die App
   bleibt dabei im Vordergrund; während der Sammlung bleibt der Bildschirm an.
3. Jeder eindeutige Einzelkontakt wird sofort dauerhaft gespeichert. Das
   Raster behält ein Häkchen nach dem Loslassen. Anzahl und Zeitpunkte werden
   mitgeführt; Live-Punkte und gespeicherte Häkchen bedeuten Unterschiedliches.
4. Nach dem gesamten Rundgang einmal **„Fertig, alle vorhandenen Kontakte
   durchprobiert“** melden. Das Wechseln zur Chat-App kann die Sammlung
   pausieren; alle gesicherten Treffer bleiben erhalten. Codex liest danach
   den gespeicherten Lauf aus und schließt ihn zur Auswertung ab. Alternativ
   selbst **Durchgang abschließen** drücken und den Sammelbericht exportieren.
5. Erkanntes und unbeobachtete Koordinaten gemeinsam abgleichen. Eine einzelne
   globale Bestätigung **Übrige sind unbestückt → Gesamtbestätigung speichern**
   setzt die unbeobachteten Felder auf deaktiviert. Nur bestätigen, wenn jeder
   vorhandene Kontakt einen Treffer erzeugt hat. Falls vorhandene Kontakte
   ohne Reaktion blieben, zunächst **Kontakte nachprüfen**; keine pauschale
   Deaktivierung aus Schweigen. Keine Fragerunde pro Koordinate.

Ein leerer Lauf kann nicht alle 81 Felder deaktivieren. Ein abgeschlossener
Lauf kann zur Nachprüfung wieder geöffnet werden; bestehende Treffer bleiben
stehen. Ein neuer Lauf beginnt mit leerer Trefferliste und eigener Kennung;
ältere Laufdateien bleiben erhalten. Auch bisher deaktivierte Koordinaten
werden im Erfassungsmodus weiterhin geprüft. Eine spätere Gesamtbestätigung
ersetzt die Bestückungsliste korrigierbar. A1 bis I9 werden nie umnummeriert.
Die Deaktivierung betrifft die lokale Matrixanzeige/Bestückungsliste der App;
es erfolgt keine Änderung des Firmware-Scanners oder eines Spielprofils.
Elektrische Koordinaten belegen keine räumliche Zuordnung an der Box.

## Erfassung und Belege

`MatrixCollectionRecorder` erhält jeden angenommenen vollständigen,
authentisierten Matrixstatus direkt aus `LedBleSession`; die Erfassung hängt
nicht vom Scrollzustand, einer Compose-Neuzeichnung oder einem späteren
Bildschirmfoto ab. Der bestehende Sequenz- und Frischevertrag bleibt erhalten.
Eine gezählte Betätigung erfordert eine beobachtete leere Matrix vor dem
Einzelkontakt. Halten erhöht den Zähler nicht; ein direkter Wechsel ohne
gesehene Freigabe erzeugt keinen weiteren Treffer. Mehrfachkontakte und
Mehrdeutigkeit werden übersprungen; anschließend lösen und einzeln wiederholen.
Es gibt keine neue Zusage zu Entprellung, Ghosting oder kurzen Impulsen.

Bei Trennung, ungültigen/überholten Werten und Verlassen der App wird pausiert.
Unterbrechungen werden vermerkt. Neue Werte allein setzen den Lauf nicht
fort: nach Rückkehr und frischem Leerstatus **Sammlung fortsetzen** drücken.
Ein Prozessneustart lädt vorhandene Treffer und pausiert einen zuvor laufenden
Durchgang. Schreibfehler stoppen die Sammlung mit sichtbarem Hinweis; ein
fehlgeschlagener Treffer wird nicht als gespeichert ausgegeben.

Die privaten Dateien liegen unter `no_backup/matrix-collection/` im Appbereich:
`<run-uuid>.properties` mit Schema 1 und einem gerätegebundenen `*-current`-
Verweis. `AtomicFile` schützt jede Datei; eine neue UUID bewahrt ältere Läufe.
Sie enthalten keine Zugangsschlüssel. Der Export enthält die Laufkennung,
Status, Zeiten, Unterbrechungen, übersprungene Mehrfachkontakte und alle
81 Koordinaten mit Zählern und Deaktivierungsstatus, ohne Gerätekennung.
Die lokalen Rohdateien und APK-Rückwege bleiben unveröffentlicht.

## Wiederaufnahme durch Codex

Arbeitskopie: `C:/Dev/Larp Escape/.worktrees/matrix-collection`.
Vor Fortsetzung dort Status, Branch und HEAD prüfen. Hauptcheckout und
ursprünglicher Testklon bleiben unangetastet. Die fremde Änderung an
`.vscode/extensions.json` gehört nicht zu dieser Arbeit.

Nach „Fertig“ Handy frisch identifizieren, App öffnen und bei laufendem oder
pausiertem Lauf **Durchgang abschließen** bedienen. Kein neuer Lauf und kein
Löschen von Appdaten. Vollständige Laufdatei über `adb exec-out run-as
 de.larpescape.companion` aus dem privaten Appbereich lesen (Debug-App) oder
den expliziten Export verwenden. Geräte-/Laufzuordnung prüfen; Daten nicht
nur aus sichtbaren Rasterfeldern rekonstruieren. Treffer, offene Felder und
bekannte Unterbrechungen auswerten. Eine physisch vorhandene, aber stumme
Taste bleibt offen. Erst die ausdrücklich bestätigte Bestückungsliste
übernehmen. Bereinigten Ergebnisbericht mit Laufkennung erstellen.

## Prüfstand

Frische Softwareprüfungen, APK-Prüfsumme und der tatsächlich erreichte
Gerätestand stehen in [collection-preparation-2026-10-16.md](collection-preparation-2026-10-16.md).
Die ursprüngliche Scanrichtung, Firmwareinstallation und offene IO7-Frage
bleiben separate Nachweise; dieser Auftrag ändert ausschließlich Android.
