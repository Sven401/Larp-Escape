# Näheglut mit echtem Funkwert · 18. Oktober 2026

Status: **Laboranbindung softwaregeprüft; neue Papier-Kamerakalibrierung untersucht;
Gehversuch mit dem Powerbank-ESP wartet auf die Bereitschaft des Nutzers.**
Dies ist ein laufender Arbeitsstand, kein abgeschlossener Geräte- oder Produktnachweis.

## Auftrag und Arbeitsbasis

Der Nutzer beauftragt den neuen Näheglut-Effekt gegen denselben Powerbank-ESP
wie beim früheren Funkel-Gehversuch. Vorher verlangt er wegen neu aufgelegtem
weißem transparentem Papier die Prüfung der Kamerakalibrierung.
Der frühere Rollenauftrag und Merge #74/#75 sind abgeschlossen; dieser Zusatz
ist eine neue Laborprüfung ohne Änderung des produktiven Spieladapters.

Arbeitsbranch `codex/naeheglut-espnow-test` im bestehenden Worktree
`C:/Users/seide/.codex/worktrees/led-effekt-architekt/Larp Escape`, Basis
`42d986ed353277d861df07ae59bd1c06345d3b52` plus eigener Arbeitsbaum.
PR-Ziel bleibt `plan/projektuebergabe-kuratierter-stand`; kein neuer Mergeauftrag.
Die fremde `.vscode/extensions.json` im Hauptcheckout bleibt unangetastet.

## Geprüfter und offener Stand

- [Versuchsplan](../docs/espnow/naeheglut-test.md): bestehender Funkpfad liefert
  gefilterten ganzzahligen RSSI; `p = clamp(20 * (RSSI + 90), 0, 1000)`.
  Frische unter 3000 ms ab letzter gültiger Antwort; Veraltung blendet aus.
  Gezielter Stop verhindert automatische Updates und Wiederanlauf.
- Neue isolierte Umgebung `larp-led-camera-rssi-lab`, separater Messauftrag
  und Hostadapter `scripts/led_camera_rssi.py`. Produktpfad, Funkkern,
  Filter und Näheglut-Rechenkern unverändert.
- Softwareprüfung vor Installation: 10 neue RSSI-Laborfälle, 9 bestehende
  Camera-Fälle sowie 7 NearGlow-Gruppen mit 2002 Werte-/Kurvenfällen bestanden.
  Host: 14 neue RSSI- und 13 bestehende Fälle bestanden. Diese Ausgaben liegen
  im Sitzungsprotokoll; keine separate Testlogdatei behauptet.
  RF-Laborbuild erfolgreich, RAM 45336/Flash 710985 Byte.
- [Papier-Kamerabefund](../docs/led/paper-camera-calibration-2026-10-18.md):
  neue 23 Bereiche; Voll-CAL bei Belichtung −8 bestanden. Goldmaximum in den
  festen Weiß-CAL-Masken nur 12,58–21,55 Kameracodes über Hintergrund;
  19/23 unterschreiten das vorab festgelegte 20-Code-Kriterium. Keine volle
  quantitative Goldfreigabe. Deutliche Helligkeitsänderungen, Drift und Ende
  sind beobachtbar; unterste Stufen und harte Timing-/Flimmeraussagen bleiben offen.
- Der vollständige Goldlauf `paper-gold-e8-complete` endete mit bestätigtem
  schwarzem Transfer und DONE. Kein offener Aufnahmeprozess. Echter RSSI-
  Gehversuch und Ausfall/Wiederkehr-Lauf haben noch nicht begonnen.

## Geräte und verpflichtender Rückweg

LED-Box `A8:42:E3:CD:4B:18` an COM3. Frische 4-MiB-Sicherung am selben Chip
vor Installation verifiziert, SHA-256
`b703f4b13aaaaa3e8c3bc68d8e8134b1cec0a77df957ea06175ac147d1d03c71`.
Sie entspricht der zuvor wiederhergestellten Originalinstallation.
Nur App ab `0x10000` wurde temporär ersetzt; Boot-/Schlüsselbereiche geprüft
unverändert. Aktuell ist die Laborfirmware installiert und nach STOP inaktiv.
Binary-SHA-256:
`8c8ee844b9f7f18bfe20846f0a07946352677fdae27a998eab4e306f7fc0d27a`.

Powerbank-Peer `A8:42:E3:CE:DE:D8`, Kanal 1; keine Peerinstallation vorgenommen.
Historische COM-Nummern sind keine Geräteidentität.

Private Belege und Werkzeug unter
`C:/Dev/Larp Escape/private/naeheglut-espnow-20261018/`:
`source-manifest.json`, `before-full-flash.bin`, `backup.json`, `install.json`,
`lab-firmware.bin/.elf`, `rssi-lab-build.log`, Papieraufnahmen und Auswertung.
Kamera-Python: `C:/Dev/Larp Escape/private/led-live-20261018/venv/Scripts/python.exe`.
Nach den temporären Versuchen `device.py restore --port COM3` aus diesem
Belegordner ausführen: chipgebundene Original-App wiederherstellen,
Boot/Schlüssel vergleichen und vollständige 4 MiB gegen Sicherung verifizieren.
Der Rückweg dieses Zusatzversuchs ist **noch offen**, nicht durch den alten
Wiederherstellungsbeleg ersetzt.

## Nächster konkreter Schritt

Die Bereitschaftsfrage an den Nutzer ist gestellt; erst nach seiner Rückmeldung
den abgestimmten kurzen Gehversuch beginnen. Box, Kamera und Papier nicht bewegen.
1280×720, Belichtung −8, `paper-rois-e8.json`, derselbe Binaryhash.
Interaktiver Host mit `--rssi-peer A8:42:E3:CE:DE:D8 --rssi-slot 0`;
bewusster Start mit p0/valid0, RGB(24,8,0), Gain1000, Smoothstep1000ms.
Bewegungsphasen gemeinsam markieren; danach Ausfall/Wiederkehr und Stop prüfen.
Keine Meterkalibrierung oder Erfolgskurve voraussetzen.

Bei automatischer Ablaufsteuerung stdout des Hosts direkt in Datei schreiben
oder ständig lesen: ein nicht geleerter PIPE blockierte einen verworfenen
Goldversuch bis zum KEEP-Watchdog. Der abgeschlossene Wiederholungslauf verwendet
direkte Dateiprotokollierung. Eine weitere Vorprobe scheiterte vor COM-Öffnung
im eingeschränkten Ausführungskontext; sie enthält keinen Geräte-/Optiknachweis.

Danach Funk-/Auftrags-/TX-Spur und Kamera getrennt auswerten, Nutzerurteil
festhalten, Originalzustand verifiziert wiederherstellen, Dokumentation und
Quellfingerabdrücke abschließen und eigene Änderungen als PR vorlegen.
Rohbilder und Flash-/Schlüsselmaterial bleiben privat.
