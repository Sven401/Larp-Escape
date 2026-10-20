# Stationspilot: Integration und verbleibende Abnahmen

Stand: **19. Oktober 2026**. Status: **PR #79 integriert; eigene Merge-CI 7/7 erfolgreich**.
Quelle: ausdrücklicher Nutzerauftrag zum Merge von #79, CI-Nachprüfung und
Projektstandsabgleich. Kein neuer Gerätezugriff oder Upload in diesem Auftrag.

## Merge und Softwarebelege

[PR #79](https://github.com/Sven401/Larp-Escape/pull/79) ist in
`plan/projektuebergabe-kuratierter-stand` bei
`7e19c955aaa54bcc7ed318d594a6f69476de4aa9` integriert. Nach frischem Fetch
geprüfte Eltern: Zielbasis `494f1c4c93f92df967b9c26aa106a891b26f5766` und
PR-Head `98488ea61117cba88b87803eb4711add3490edcc`. Vollständiger Dateibaum
identisch zum PR-Head: `448887f8e32ad44f241460323ac57a412c500331`.
Die älteren Integrationen bleiben erhalten; main wurde nicht geändert.

Vor dem Merge waren alle neun PR-Jobs erfolgreich. Die Android-Checkoutlogs
belegen Prüfmerge `4989efdad5dead04b8032c8177117e2f8f73b865` gegen obige Basis.
Diese PR-Läufe bleiben vom eigenen Merge-Nachweis getrennt:

| Prüfung | PR-CI | Eigene Push-CI am Merge `7e19c95` |
| --- | --- | --- |
| Quality: Host/Kryptografie, Firmware/ABI, Android | [3/3 erfolgreich](https://github.com/Sven401/Larp-Escape/actions/runs/39171408270) | [3/3 erfolgreich](https://github.com/Sven401/Larp-Escape/actions/runs/39172888974) |
| Baugruppen, Android, synthetische Kamera | [4/4 erfolgreich](https://github.com/Sven401/Larp-Escape/actions/runs/39171408305) | [4/4 erfolgreich](https://github.com/Sven401/Larp-Escape/actions/runs/39172888978) |
| NFC-Probe | [2/2 erfolgreich](https://github.com/Sven401/Larp-Escape/actions/runs/39171408314) | Durch Pfadfilter nicht ausgelöst |

Alle sieben ausgelösten Push-Jobs am tatsächlichen Merge sind abgeschlossen
und erfolgreich. Der Android-Checkoutlog bestätigt `7e19c95`; beide
Android-Jobs prüfen Tests, Debug/Release, Lint, zehn Appgrenzen-Fälle und die
vier APKs.

ESP-NOW-Probe durch Pfadfilter ebenfalls nicht ausgelöst. Nicht ausgelöste
Läufe sind keine neuen bestandenen Prüfungen. Ein späterer Dokumentationshead
benötigt seine eigene Laufzuordnung; diese Tabelle gilt nur für die genannten
Commits. Kein neuer lokaler Produktbuild durch diesen Dokumentationsabschluss.

## Nutzbarer Stand und frühere Gerätebelege

Larp Dev und Larp GM sind getrennte Anwendungen. Larp GM bietet lokale
Stationsvorbereitung, Stationsliste, gezielte BLE-Anmeldung und authentisierten
aktuellen Zustand. Abbruch, Hintergrund, bewusste Pause, alte Rückrufe und
begrenzte Wiederverbindung sind berücksichtigt. Dieser Pilot liest Zustände;
Licht-/Spielaktionen und allgemeine Spielleitungsrechte bleiben Folgeumfang.

Der [kanonische Stationsbericht](../docs/companion/spielleiter-stationspilot.md)
belegt den früheren begrenzten Gerätelauf: lokale Vorbereitung und Anmeldung,
Bluetooth aus/ein, Hintergrund/Rückkehr, Pause, Prozessneustart, große Schrift,
Querformat und getrennte verschlüsselte GM-Daten bei erhaltenen Dev-Daten.
Das sind keine neuen Gerätebeobachtungen am Mergecommit. Eine authentisierte
Lichtmeldung ersetzt keine optische Bestätigung des Lichts.

## Gerätefolgeprüfungen des Stationspiloten

| Offen | Konkreter nächster Nachweis |
| --- | --- |
| TalkBack | Stationsliste, Vorbereitung, Statuswechsel und erreichbare Aktionen auf dem realen Handy prüfen. |
| Mehrere reale Stationen | Verschiedene vorbereitete Boxen nacheinander bedienen; keine alten Zustände nach Stationswechsel, falsche Zuordnung wird nicht bestätigt. |
| Funkverlust bei Bluetooth an | Erreichbarkeit der Box verlieren; Status wird unbekannt, Wiederkehr prüft frisch, automatische Versuche bleiben begrenzt. Bluetooth aus/ein ersetzt diesen Fall nicht. |
| Langzeitbetrieb | Statusfrische, wiederholte Unterbrechung/Rückkehr sowie Ressourcen und Sitzungsabbau am tatsächlichen Paar beobachten. |

Vor einem Gerätelauf tatsächliche App-/Firmwareversionen und Boxzuordnung
feststellen. Ein Upload benötigt einen eigenen Auftrag und den chipgebundenen
Flash-Rückweg. Die vorliegenden Nachweise sind keine vollständige Nutzerabnahme.

## Weitere Projektgrenzen und Fortsetzung

- Allgemeiner Zugang/Bond/Rollen (#27/#28), Licht-/Spielaktionen in GM (#29/#37)
  sowie vollständige Status-/Sitzungsfehlerfälle (#33/#34) bleiben offen.
- Matrix C3/G3/H3/A8, Chip/B7 und elektrische/CAD-Freigabe (#39/#54),
  NFC-Fehlerfälle (#35) und Audio-R3 bleiben offene Baugruppenabnahmen.
- Näheglut benötigt weiterhin die unabhängige Dunkelkontrollmessung,
  Produktadapter, Quellen-/Pausenentscheidung und optische Nutzerabnahme;
  [bestehender Befund](../docs/led/naeheglut-stop-diagnose-2026-10-18.md).
- Geräteausführung F5a, reale Profilübernahme, F5b-Ressourcen-/Betriebsabnahme
  (#30/#31), Reichweite/Dauerlast und verteilte Veranstaltung bleiben offen.
  D1-A/#73 und Resonanzkreis/#71 sind eigenständige offene PRs.

Nächster kleiner Geräteumfang: TalkBack und gezielter Funkverlust am bekannten
Paar; Mehrstationsprüfung nach Bereitstellung weiterer zugeordneter Boxen.
Neue Licht-/Spielaktionen sind ein eigener Implementierungsauftrag. Der
[Projektplan #25](https://github.com/Sven401/Larp-Escape/issues/25) führt den
operativen Status; kein Gesamtissue wird allein durch diesen Merge geschlossen.

## Nachprüfung und PR-Vorbereitung · 20. Oktober 2026

**Status: vorbereitete Fortschreibung fachlich bestätigt und auf aktuelle Zielbasis übernommen.**
Quelle: Nutzerauftrag in Sitzung `01a15add-4ca0-7c12-9db7-c42e4c87d633`.
Der erhaltene Vorbereitungscommit `499ec949177b9fb1836bcf2dd1d5304847a59029`
liegt bereits in [PR #86](https://github.com/Sven401/Larp-Escape/pull/86).
Der saubere vorhandene Worktree wurde wiederverwendet und Zielbasis
`a4c83cb7351759808f85ede612abc7c2280212fc` ohne Umschreiben des Ausgangscommits
übernommen. Diese Basis enthält den inzwischen gemergten Wartungs-PR #87.

Erneut geprüft am 20.10.2026:

- Eltern und identischer vollständiger Dateibaum von `7e19c95` und `98488ea`
  durch Git geprüft; Baum-ID `448887f8e32ad44f241460323ac57a412c500331` bestätigt.
- Alle fünf oben verlinkten historischen Läufe über ihre Jobs erneut abgefragt:
  neun PR-Jobs und sieben ausgelöste Merge-Jobs erfolgreich. Die beiden
  PR-Android-Checkoutlogs bestätigen `4989efd` mit Head `98488ea` und Basis
  `494f1c4`; der Windows-Android-Checkoutlog am tatsächlichen Merge bestätigt
  `7e19c95`. Historische Belege werden nicht als neue Builds ausgegeben.
- Nutzbarer Pilotumfang und die vier Gerätefolgeprüfungen mit dem
  [Stationsvertrag](../docs/companion/spielleiter-stationspilot.md) abgeglichen.
  D1-A/#73 und Resonanzkreis/#71 sind weiterhin offene PRs.
- Wartung #87: [Quality 3/3](https://github.com/Sven401/Larp-Escape/actions/runs/39489965753),
  [LED/Android/Kamera 4/4](https://github.com/Sven401/Larp-Escape/actions/runs/39489965803)
  und [NFC 2/2](https://github.com/Sven401/Larp-Escape/actions/runs/39489965993)
  am Wartungshead `44e7098` erfolgreich, danach Merge `a4c83cb`.
  [Lokale Wartungsprüfung und verbleibende Grenzen](WARTUNG-PRUEFSTAND-2026-10-20.md).

Die Einstiege nennen nun sowohl die Produktintegration #79 als auch die spätere
Dokumentationsbasis #87. Die frühere pauschale Aussage zur noch fehlenden nativen
Stationsbedienung ist durch den tatsächlich vorhandenen lesenden Pilotumfang und
seine offenen Folgeprüfungen ersetzt. Inhalt aus parallelem PR #88 wird nicht
übernommen; dessen ergänzende Einstiegsverweise bleiben eigenständige Arbeit.

Dieses Paket ändert nur drei Übergabedokumente. UTF-8, 116 lokale Dateiverweise,
der neue Markdown-Anker, ausgeglichene Codeblöcke und Diff am neuen
Dokumentationsstand geprüft; keine fehlenden Ziele oder Whitespacefehler. Kein neuer
lokaler Firmware-/Android-Build, Upload oder Gerätelauf. Die neue PR-CI von #86
ist nach dem Push separat zu bewerten; ein Merge dieses PRs ist nicht beauftragt.
