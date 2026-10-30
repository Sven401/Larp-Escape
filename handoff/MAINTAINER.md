# Maintainer-Einstieg

## Geprüfter Projektstand · 30.10.2026

**Belegt durch frischen Fetch und Git-/PR-Abgleich:** Integrationsbranch
`plan/projektuebergabe-kuratierter-stand` bei
`7585e1573240ac92da6e925f484a93429c219673`.
Diese Momentaufnahme vor neuer Arbeit am Remote-Head und im tatsächlich
verwendeten Arbeitsbereich prüfen. Lokale Branch- oder Worktree-Namen
belegen keine Aktualität. `main` bleibt ein gesonderter Integrationsauftrag.

[Mandat](../AGENTS.md) · [Sitzungseinstieg](PROJEKT-SETUP.md#sitzungseinstieg)
· [Auftrags-/Abnahmevorlage](../docs/quality/verantwortlichkeiten.md#kurzvorlage-für-auftrag-und-abnahme)
· [technischer Wegweiser](README.md)

| Bereich | Integriert / vorbereitet | Offene Grenze und maßgeblicher Beleg |
| --- | --- | --- |
| Produktziel und Spielpakete | SP-01 (#94) und P0-Verträge, zwei Referenzspiele, Semantikmodell (#104) integriert | Allgemeine Laufzeit, GM-Autorenschaft, Mehrboxbetrieb und Drittspiel-Nachweis offen. [P0-Prüfstand](SPIELPAKETE-P0-2026-10-29.md), [Plan](../docs/veranstaltungsarchitektur-umsetzung.md) |
| BLE-/Profilvorbereitung | #100 integriert: Android-Transferkern und originalgebundener Rückkanal | Durchgängiger Produktzugang, reale Profilgeräte und Geräteabnahme offen. [Paket und Nachweise](PR100-MERGEVORBEREITUNG-2026-10-29.md) |
| Native Plattform | #101 integriert: native ESP-IDF-/PSA-Buildbasis und geprüfter Hersteller-Wartungspin | Keine abgeschlossene Produktmigration. [Qualifikation und Grenzen](PR101-MERGEVORBEREITUNG-2026-10-29.md) |
| TLS-/BLE-Bausteine | #103 integriert: begrenzter TLS-Worker, NimBLE-Quelladapter, separate Prüfziele | Produktstart aktiviert diese Strecke nicht; S1–S6 bleiben offen. [Paketbeleg](PR103-MERGEVORBEREITUNG-2026-10-30.md), [Bereitschaft](BLE-BEREITSCHAFT-2026-10-28.md) |
| D1 und reale Geräte | D1-A-Rechnerreferenz (#73), Baugruppen, Resonanzkreis (#71), Kristallsiegel-Labor (#89) integriert | D1-B/C, Firmware/F5a, echte Speicher-/Ausfallprüfung und gemeinsamer Spielbetrieb offen. [D1](D1-AUSARBEITUNG-2026-10-18.md), [Baugruppenbelege](BAUGRUPPEN-NACHWEISE.md) |
| Android und Gestaltung | Getrennte Dev-/GM-Apps, lesender Stationspilot und UI/UX-Prototyp integriert | Prototyp und begrenzte Gerätebelege ersetzen keine native vollständige Spielsteuerung. [Android](../companion/android/README.md), [Stationspilot](../docs/companion/spielleiter-stationspilot.md), [UI/UX](../docs/ui-ux/BOX-PILOT.md) |
| Facharbeit und Audit | Fachmandate, Laufzeitkorrekturen und verschlankter Fachaudit-Auftrag dokumentiert | Geräte-/SMP-/Zeitgrenzen und Zusatznutzen des Wochenbetriebs offen. [Mandate](../docs/quality/verantwortlichkeiten.md), [Korrekturen](../docs/quality/runtime-fixes-2026-10-24.md), [Auditbetrieb](../docs/quality/fachaudit-betrieb.md) |

Seit dem früheren Einstieg bei `191d081` kamen #102 (Übergabe), #104 (P0),
#100, #101 und #103 hinzu. Ihre tatsächlichen Mergecommits und die
Quellenkonflikte sind im [Abgleich dieses Pakets](KI-PROJEKTEINSTIEG-2026-10-30.md)
festgehalten. Softwarebelege in den verlinkten Berichten gelten für deren
jeweilige Prüfcommits; hier wurden keine Produkt- oder Gerätetests wiederholt.

## Offene Arbeit und Fortsetzung

- [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25) führt die
  operative Planung, [#93](https://github.com/Sven401/Larp-Escape/issues/93)
  das Spielpaketziel. **Abweichung am 30.10.:** Die dortigen datierten
  Integrationsangaben reichen noch nicht bis #100/#101/#103. Für integriert
  versus Entwurf gelten der frische Git-/PR-Abgleich und der Code; der
  Produktbeschluss wird durch eine Statuskorrektur nicht verändert.
- [PR #105](https://github.com/Sven401/Larp-Escape/pull/105) ist am Ablesetag
  ein offener Entwurf zur BLE-/TLS-Bridge, nicht Teil dieser Integrationsbasis.
  Vor Fortsetzung dessen aktuellen Head, Umfang und Nachweise lesen.
- Der [BLE-Fortsetzungsplan](BLE-FORTSETZUNGSPLAN-2026-10-28.md) bindet die
  Produktfortsetzung an S1–S6. Integrierte Vorarbeiten erfüllen diese Tore
  nicht automatisch. Reale Profiladapter, Androidkopplung, physische
  Pairingbestätigung, Daten-/Rückweg- und Gerätebelege bleiben gesondert nötig.
- Bestehende Hardware-/Baugruppenrestprüfungen (#39/#54 und Fachberichte),
  Rückkehrkonflikt #34, D1-B/C und F5a → Profilübernahme → F5b bleiben offen.
  Der konkrete nächste Schritt richtet sich nach dem beauftragten Paket und
  dem aktuellen Plan, nicht nach alten Fortsetzungsabsätzen.

## Nachweise und Pflege

[Prüfwege](../docs/quality-checks.md), Fachverträge und Geräteberichte bleiben
kanonisch. Vor Upload konkreten Auftrag, tatsächliches App-/Firmwarepaar und
chipgebundenen Flash-Rückweg nach [Hardwareplanung](../Hardware/HARDWAREPLANUNG.md)
prüfen. Hosttest, SDK-Build, Upload und beobachtetes Verhalten getrennt belegen.

Bei einer beauftragten Fortschreibung diesen Kopf ersetzen: Datum, geprüfte
Basis, Status und Links. Ausführliche Laufbelege bleiben im Paketbericht;
keine weitere Chronik voranstellen. Die technische Übergabe verlinkt diesen
Stand, statt ihn unabhängig zu duplizieren. Neue Nachweise schließen nur
die tatsächlich geprüften Kriterien. [Erste Anwendung und Rückweg](../PROMOTION.md).

## Historische Belege

Die [unveränderte frühere Fassung](MAINTAINER-ARCHIV-2026-10-30.md) enthält
Integrationsfolgen, damalige CI und frühere Fortsetzungen. Ihr Wortlaut ist
historisch; er erteilt keine neuen Aufträge. Herkunft: Dateifassung aus
`7585e1573240ac92da6e925f484a93429c219673`.

### Integrierter Stand · 28.10.2026

Bestehendes Verweisziel: [damalige Mergefolge und CI](MAINTAINER-ARCHIV-2026-10-30.md#integrierter-stand--28102026).

### Start für weitere App- und Firmwarearbeit

Bestehendes Verweisziel: [historische Bestandsaufnahme](MAINTAINER-ARCHIV-2026-10-30.md#start-für-weitere-app--und-firmwarearbeit).
Für neue Arbeit gilt der [Sitzungseinstieg](PROJEKT-SETUP.md#sitzungseinstieg).
