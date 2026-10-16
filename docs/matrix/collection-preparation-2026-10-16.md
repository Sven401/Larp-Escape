# Sammeldurchgang: Vorbereitung und Prüfstand

**Abgeschlossener Folgeprüfstand:** [Sammelergebnis](collection-result-2026-10-16.md)
mit 68 erkannten Koordinaten, vier offenen Problemstellen und neun
nutzerbestätigt unbestückten, in der App deaktivierten Feldern.
Die folgenden früheren Vorbereitungs-/Startangaben sind historische Belege.


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

Die USB-Unterbrechung wurde nach Nutzerbestätigung „angeschlossen“ behoben.
Die endgültige APK aus Quellcommit `20ac696f75630919949edceec975d593d46a4218`
ist inzwischen mit Datenerhalt installiert und vollständig zurückgelesen;
ihre oben genannte Prüfsumme stimmt. Die Signatur passt zum vorherigen APK.
Verschlüsselter Boxzugang und Wake-Anordnung sind erneut bytegleich.
Keine Änderung oder Rücksetzung der Box-Firmware.

Die Box wurde frisch gesucht und über den vorhandenen Zugang authentisiert.
Die App zeigte einen frischen Matrixstatus. **Der Sammellauf ist gestartet**;
der Start verlangt eine frische leere Matrix. Die private Laufdatei wurde
direkt aus dem Appbereich gelesen und abgeglichen:

- Laufkennung: `5805f942-5e90-4948-9c2b-5a256f5d0307`.
- Phase `RECORDING`, null gespeicherte Treffer, null Unterbrechungen,
  null übersprungene Mehrfachkontakte zum Startzeitpunkt.
- UI bestätigt „Sammlung läuft“, null von 81 Koordinaten und nach Scrollen
  „Beobachtete Kontakte: keine“. Raster und Zähler sind auf dem Handy sichtbar.
- [Bereinigter Startbeleg](evidence/collection-start-2026-10-16.json).

**Offen:** Der Nutzer geht jetzt alle vorhandenen Kontakte durch und meldet
sich einmal mit „Fertig“. Noch keine Betätigung oder vollständige Bestückung
für diesen neuen Lauf behauptet; der Startnachweis ist keine Kontaktabnahme.
Beim nächsten Zugriff bestehenden Lauf auslesen/abschließen, keinen neuen
Lauf starten. Nach bestätigter Vollständigkeit unbestückte Koordinaten über
die einmalige Gesamtbestätigung deaktivieren; stumme vorhandene Kontakte
bleiben gezielt zu prüfen. Ablauf und Grenzen stehen im verlinkten Sammellauf.

Private Messbelege, alte/neue APKs und Bedienhilfe liegen unter
`.local/matrix-collection/` in der Arbeitskopie und bleiben unversioniert.
