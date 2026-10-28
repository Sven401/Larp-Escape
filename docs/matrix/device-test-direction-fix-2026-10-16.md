# Tastenmatrix: historischer Installationsbeleg zur Richtungskorrektur

**Historischer Zwischenstand vom 16. Oktober 2026; am 20. Oktober 2026 aus
erhaltenen lokalen Quellen kuratiert übernommen.** Die damalige Installation
und eine begrenzte Ruhebeobachtung sind dokumentiert. Diese Übernahme enthält
keinen heutigen Gerätezugriff, Upload oder neue physische Abnahme.

Für die weitere Arbeit gilt der spätere
[abgeschlossene Sammeldurchgang](collection-result-2026-10-16.md): 68 erfasste
Koordinaten, neun unbestückte Felder und vier offene Problemstellen. Die alte
Einzelkontakt-Fortsetzung aus der wiedergefundenen Notiz wird nicht reaktiviert.

## Damaliger Installationsstand

Die vier erhaltenen Laufbelege dokumentieren am 16. Oktober die Anwendung aus
`e4b2ce024655ae9e951e04542dc7f473e1bc6e9c` in der Umgebung `larp-matrix-test`.
Sie ergänzen den [ersten Gerätetest](device-test-2026-10-16.md) und den
[Software-Korrekturbericht](scan-direction-correction-2026-10-16.md).

- Vor dem Schreiben wurde ein 4-MiB-Vollflashabzug des vorherigen
  `c613acebfb07805d1c363b52a325721f2b6f8077`-Standes erstellt und laut damaligem
  Beleg am Gerät verifiziert. Chip, Partitionierung und Schlüsselpartition
  wurden mit dem vorgesehenen Rückweg abgeglichen.
- Der laufgebundene Installer schrieb ausschließlich die Anwendung ab
  `0x10000`, prüfte sie am Gerät und las alle 1.139.856 Bytes zurück. Der Beleg
  hält denselben seriellen Zugriff während der Prüfung und Installation fest.
  Partitionstabelle, Schlüsselpartition und übrige Flashbereiche waren vor
  dem Neustart unverändert; Laufzeitänderungen nach dem Neustart sind davon
  nicht erfasst.
- Das vorhandene Matrix-APK blieb unverändert. Weder eine Appinstallation noch
  ein Rollback sind in diesem Lauf protokolliert. `physicalAcceptance` bleibt
  im damaligen Installationsbeleg ausdrücklich `false`.

| Damaliges Artefakt | SHA-256 |
| --- | --- |
| Anwendungsrücklesung, 1.139.856 Bytes | `a15c2c698c527c1b919ffde7cc31b213c91570856100a16d9d5d7741d27beb8d` |
| Unverändertes Matrix-APK | `fbdb0439aef6eb950b39c9eaf1b042865654e0865e46cce36ae91c5dc6910cd6` |
| Vorab-Vollflash, 4.194.304 Bytes | `7daf56a99bf0b89dc49472893056af8f4c0c938ca006ef58b7d1fa91c7fb35f4` |

Diese Angaben identifizieren historische Artefakte. Sie bestimmen nicht das
heute installierte App-/Firmwarepaar.

## Ruhebeobachtung und Grenzen

Der damalige Nutzerbeleg bestätigt entfernte Magnete und freie Reedkontakte.
Zwei erhaltene UI-Dateien enthalten „Live · elektrische Kontaktkoordinaten“
und „Beobachtete Kontakte: keine“. Bei der heutigen Dateiprüfung waren diese
beiden Dateien bytegleich; ihr zeitlicher Abstand wurde nicht neu belegt.
Das stützt eine begrenzte dokumentierte Ruheanzeige, keine vollständige
Kontaktprüfung oder räumliche Zuordnung.

Weitere erhaltene private XML-Dateien zeigen A9 beim bezeichneten Halten und
Wiederholen sowie keine Kontakte nach dem bezeichneten Loslassen. Dateinamen
und Anzeigetexte ersetzen keine unabhängige Bestätigung der physischen
Handlung oder Position. Dieser Import erklärt daraus keine neue positive
Geräteabnahme. Der spätere Sammellauf bleibt der maßgebliche Ergebnisbericht.

## Herkunft und Prüfung der Übernahme

Die ursprüngliche unversionierte Berichtsfassung wurde unverändert privat
aufbewahrt. Ihr SHA-256 lautet
`4b623a19f7d7131cabbcc93b76c12b0d9ba85acd492faebea685b001d98424dd`.
Ihr vorgesehener Sammel-JSON-Beleg fehlte sowohl im Quellklon als auch am
geprüften kanonischen Commit `97a2584cc108ebb9c0f667fc46f898d163e00144`.

Der neue [Quellen- und Prüfbeleg](evidence/device-test-direction-fix-2026-10-16.json)
führt die vier tatsächlich vorhandenen bereinigten Originaldateien mit ihren
Prüfsummen: `device-preflight.json`, `direction-fix-install.json`,
`flash-backup.json` und `user-baseline.json`. Er trennt deren damalige
Aussagen von der Importprüfung am 20. Oktober. Der lokale Anschlussname ist
weggelassen; die Originalprüfsummen beziehen sich auf die unveränderten
privaten Dateien.

Heute geprüft wurden die Originaldatei-Hashes, der lokale Vollflashabzug,
die lokale Anwendungsrücklesung und die genannten UI-Texte. Es gab keine
Geräteverbindung und keine Wiederholung der damaligen Tests. Flash, APKs,
Schlüsseldateien, UI-Rohdaten und Installationslogs bleiben privat.
Chipidentität, B7-/IO7-Eignung und physische Zuordnung erhalten durch diese
Dokumentation keine zusätzliche Freigabe.
