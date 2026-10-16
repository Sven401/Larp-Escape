# Sammeldurchgang: Vorbereitung und Prüfstand

Stand: **16. Oktober 2026, Europe/Berlin**. Basis `e4b2ce0`, Branch
`codex/matrix-collection`. [Ablauf und Wiederaufnahme](collection-run-2026-10-16.md).
Getestete Quellen und Artefakt:
[Maschinenbeleg](evidence/collection-preparation-2026-10-16.json).
Die Prüfsummen bezeichnen exakte lokale Quelldateien inklusive Zeilenenden.

## Frisch geprüft

Mit JDK 21.0.8, Gradle 8.9 und vorhandenen Offline-Abhängigkeiten:

```powershell
./gradlew.bat :core:protocol:test :app:testDebugUnitTest :app:lintDebug :app:assembleDebug --offline --no-daemon
```

- 68 Protokoll- und 59 App-Tests bestanden; null Fehler oder ausgelassene Fälle.
- Sieben neue Sammellauffälle: alle 81 Felder, Halten/Freigabe/Wiederholung,
  explizite Bestückungsbestätigung, leere Läufe, erneut erfassbare deaktivierte
  Felder, Nachprüfung, ältere Laufdateien, Mehrfach-/ungültige Werte,
  Neustart/Gerätewechsel, Speicherfehler, Codec und vollständiger Export.
- Lint: null Fehler; drei bestehende Warnungen `OldTargetApi`,
  `DataExtractionRules`, `MissingApplicationIcon` und bestehender Hinweis
  `AutoboxingStateCreation`.
- Debug-APK gebaut: 23,283,438 Byte;
  SHA-256 `d1a12043c38d19008965530478c5834a4342d2774e3a9b3d6735f548437d694a`.
- Kein Firmwarecode verändert; kein Firmwarebuild oder Upload beansprucht.
  GitHub-CI ist getrennt am veröffentlichten Commit zu lesen.

## Tatsächlich erreichter Gerätestand

Das angeschlossene Moto G14 wurde frisch identifiziert. Das alte installierte
Matrix-APK entsprach der dokumentierten Prüfsumme
`fbdb0439aef6eb950b39c9eaf1b042865654e0865e46cce36ae91c5dc6910cd6`.
Eine private APK-Kopie wurde als Rückweg gesichert. Signaturvergleich,
Installation mit `-r` und Hashprüfung des vollständig zurückgelesenen APK
waren für die erste Sammelfassung erfolgreich (SHA-256
`67024c3462da73d0f3d33e252458b4f252c3052a614f2e70728f08420593f63e`).
Verschlüsselter Boxzugang und Wake-Anordnung blieben bytegleich.
Eine frische UI-Abfrage zeigte die neue Sammelansicht mit null Treffern.

Danach verlor das Handy die USB-Verbindung. Die endgültige Fassung ergänzt
zusätzlich den Neubeginn nach einem leeren abgeschlossenen Lauf bei Erhalt
der alten Laufdatei; sie wurde erneut vollständig wie oben geprüft.
**Diese endgültige APK ist noch nicht installiert. Keine Boxverbindung,
kein gestarteter Sammellauf und keine echte Kontaktbetätigung in dieser
Sitzung belegt.** Der Nutzer wurde um Wiederanschließen gebeten.
Die vorhandene Prüffirmware wurde weder beschrieben noch zurückgesetzt.

Nächster Schritt: Handy wieder identifizieren, endgültiges APK mit Datenerhalt
installieren und zurücklesen. App öffnen, Box suchen/verbinden, frischen
leeren Matrixstatus prüfen, Sammlung starten und ihren gespeicherten
`RECORDING`-Stand mit null Treffern kontrollieren. Erst dann zum Rundgang
auffordern. Das ist kein abgeschlossener Kontakt- oder Dauertest.

Private Messbelege, alte/neue APKs und Bedienhilfe liegen unter
`.local/matrix-collection/` in der Arbeitskopie und bleiben unversioniert.
