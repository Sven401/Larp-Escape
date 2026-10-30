# Technische Projektübergabe

Der [Maintainer-Einstieg](MAINTAINER.md) führt den datierten geprüften
Integrationsstand, offene Arbeit und Nachweisgrenzen. Diese Seite ist der
Wegweiser zu Fachunterlagen; sie führt keine zweite Statusliste.

Vor Fortsetzung: [Arbeitsbasis und Umgebung prüfen](PROJEKT-SETUP.md#sitzungseinstieg),
dann den passenden Vertrag und Paketbericht lesen. Der Maintainer verwendet
die [kurze Auftrags-/Abnahmevorlage](../docs/quality/verantwortlichkeiten.md#kurzvorlage-für-auftrag-und-abnahme)
im bestehenden Auftrag oder Arbeitsbericht.

| Aufgabe | Passender Einstieg |
| --- | --- |
| Produktziel und Reihenfolge | [SP-01](../docs/spielpakete-architektur.md), [Umsetzungsmatrix](../docs/veranstaltungsarchitektur-umsetzung.md), [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25) mit Quellenabgleich |
| Allgemeine Spiel-/Regelpakete | [P0-Vertrag](../docs/spielpakete/p0-vertrag-v1.md), [Paketbericht](SPIELPAKETE-P0-2026-10-29.md) |
| BLE, Rollen, Profile und Plattformmigration | [Fortsetzungstore](BLE-FORTSETZUNGSPLAN-2026-10-28.md), [Bereitschaft](BLE-BEREITSCHAFT-2026-10-28.md), [Migrationsplan](ARDUINO-ESP32-3-MIGRATION.md) |
| Dev-/GM-App und Bedienabläufe | [Android](../companion/android/README.md), [UI/UX-Rolle](../docs/ui-ux/ROLLE.md), [Box-Pilot](../docs/ui-ux/BOX-PILOT.md) |
| Reale Geräte und Baugruppen | [Hardwareplanung](../Hardware/HARDWAREPLANUNG.md), [gesicherte Baugruppenbelege](BAUGRUPPEN-NACHWEISE.md), [V2-Vertrag](../docs/led/ble-wire-v2.md) |
| Laufzeit, Profile und Fortschritt | [Geräteausführung](../docs/configuration/device-runtime.md), [D1-A](D1-AUSARBEITUNG-2026-10-18.md), [D0](../docs/veranstaltung/d0-vertrag-v1.md) |
| Prüfung und Fachreview | [Prüfwege](../docs/quality-checks.md), [Verantwortlichkeiten](../docs/quality/verantwortlichkeiten.md) |
| Wiederkehrende Aufgaben | [Fachaudit-Betrieb](../docs/quality/fachaudit-betrieb.md), [GitHub-Abgleich](GITHUB-AUTOMATISIERUNG-2026-10-20.md) |

## Historische Übergaben

Die [unveränderte technische Übergabe vor dieser Kürzung](README-ARCHIV-2026-10-30.md)
bewahrt sämtliche datierten Einträge aus der Dateifassung an
`7585e1573240ac92da6e925f484a93429c219673`, einschließlich des verworfenen
Diagnoseansatzes und seiner Grenzen. Die [frühere Maintainer-Chronik](MAINTAINER-ARCHIV-2026-10-30.md)
enthält die damaligen Integrations-/CI-Stände. Historische Aussagen wie
„offen“, „aktuell“ oder „nächster Schritt“ gelten nur für ihr Datum.

## Quellarchiv und Wiederaufnahme

Bestehendes Verweisziel: [historische Quellarchive und deren Bewertung](README-ARCHIV-2026-10-30.md#quellarchiv-und-wiederaufnahme).
Archivbranches und die frühere separate KI-Werkbank sind Belegquellen.
Eine Wiederaufnahme benötigt einen aktuellen Auftrag und fachlichen Abgleich.
Die am 13.10. verworfene Diagnose-App bleibt verworfen.
