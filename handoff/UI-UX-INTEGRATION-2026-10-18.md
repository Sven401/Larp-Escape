# UI/UX-Pilot: Integrationsabschluss und Fortsetzung

Status: **belegt, 18. Oktober 2026**. Quelle: ausdrücklicher Nutzerauftrag,
PR #78 in den kuratierten Integrationsbranch zu mergen und Projektstand samt
Übergabe zu aktualisieren. Sitzung `01a1509f-5f56-7232-8765-d0d8453fd25f`.
Operativer Projektstand: [Issue #25](https://github.com/Sven401/Larp-Escape/issues/25).

## Mergebeleg

[PR #78](https://github.com/Sven401/Larp-Escape/pull/78) integriert bei
[`42853bc1aa93de23b5fcfc72e66d2c1a397361af`](https://github.com/Sven401/Larp-Escape/commit/42853bc1aa93de23b5fcfc72e66d2c1a397361af)
auf `plan/projektuebergabe-kuratierter-stand`. Frischer Fetch nach Merge:

- Eltern: Zielbasis `7e72411bb1c416b5864036c79a1e17d9cf46b5ac` (#77)
  und PR-Head `3b2f6b4ffd561681dc5b14d13d2f0225eb640cd4`.
- Vollständiger Git-Dateibaum `2bf9ec0f9eaf1863562783352737085ad2caafee`
  identisch zum geprüften Head; keine Integrationsauflösung nötig.
- `main`, bestehende Geräteinstallation und fremde lokale Änderungen unberührt.

## Integriert und geprüft

[Rollenvertrag](../docs/ui-ux/ROLLE.md), Projektagent/Skill,
[aktueller Zustands-/Akzeptanzvertrag](../docs/ui-ux/BOX-PILOT.md),
[isolierter Webpilot und Figma-Einstieg](../design/box-pilot/README.md).
Der Stationsklick verbindet automatisch; Lichtaktionen werden bewusst ausgelöst.
Unterbrechung entwertet den Status und verbindet neu, ohne Befehle zu wiederholen.
Konkrete Einrichtungshilfe, erfolglose Wiederholung ohne Scheinkorrektur sowie
Start/Stop mit ausgeführten und nicht angekommenen Aufträgen sind nachgeführt.

[PO-Korrektur](../design/box-pilot/po-review.md) und
[Figma-/PR-Nachprüfung](../design/box-pilot/figma-sync-review.md) bleiben kanonische
Belege statt hier duplizierter Screenshots: 56 editierbare Figma-Screens,
148 gültige Verknüpfungen; repräsentative Presentation-Pfade im Browser bedient.
19 Zustandsregeltests, vier Packagingtests, Typprüfung, Produktionsbuild und
Integrität aller 28 geschützten Runtime-Dateien auf unverändertem Prototypcode
1dc9227 bestanden. Die Hashmanifeste halten den jeweiligen Prüfstand fest.

**Unmittelbar vor Merge frisch bestätigt: alle drei PR-Workflows am Head 3b2f6b4 erfolgreich.**
[Quality](https://github.com/Sven401/Larp-Escape/actions/runs/39164422680),
[LED/Android/Kamera](https://github.com/Sven401/Larp-Escape/actions/runs/39164422724),
[NFC](https://github.com/Sven401/Larp-Escape/actions/runs/39164422681).
Die [Quality-Push-CI des tatsächlichen Merges](https://github.com/Sven401/Larp-Escape/actions/runs/39165151009)
war beim Entwurf dieser Übergabe noch aktiv. Andere Push-Workflows wurden durch
ihre Pfadfilter nicht ausgelöst. PR-, Merge- und nachfolgende Dokumentations-CI
getrennt bewerten; neuester Ablesestand und Dokumentationsmerge in #25.
Keine Geräteprüfung oder Installation durch diesen Mergeauftrag.
Dokumentationsabschluss lokal geprüft: UTF-8, 97 lokale Markdownziele,
Whitespace-/Diffprüfung; ausschließlich drei Übergabe-Markdown-Dateien.

## Offene Abhängigkeiten und nächster Schritt

**Integriertes Design-/Prototyppaket; keine native Produktfreigabe.** Android-App
und Firmware wurden durch #78 nicht geändert. Reale Einrichtung/Rechte und
Stationszuordnung, mehrere Stationen/Wechsel, native Android-/BLE-, TalkBack-,
Lifecycle- und Nutzerabnahme bleiben offen. P3-Abstände/Icons und nominale
Figma-/Browserbreiten sind dokumentiert; keine pixelidentische Compose-Vorgabe.
#27/#28/#33/#34/#36/#37 werden durch diesen Entwurf nicht abgeschlossen.

Bei einem gesonderten Folgeauftrag:
1. Aktuellen Integrationsbranch und #25 frisch abgleichen, dann Rollenvertrag,
   Pilot-Zustandsvertrag und letzte PO-/Figma-Nachprüfung lesen.
2. Den fokussierten Stationsablauf auf bestehende Compose-Komponenten und echte
   BLE-/Rechtezustände abbilden. Fehlende reale Stationszuordnung als offene
   Voraussetzung klären; keine neue Autorität oder Erfolgsaussage erfinden.
3. Normalweg, beide Auftragsausgänge, Einrichtungsausnahmen und Wiederaufnahme
   nativ prüfen; 320 dp/200 % Schrift, TalkBack und Lifecycle gezielt abnehmen.
   Geräteupload erst mit gesondertem Auftrag und chipgebundenem Rückweg.

Dokumentationsabschluss auf `codex/ui-ux-integrationsstand` im Worktree
`C:/Dev/Larp Escape/.worktrees/pr78-po`. Vor Fortsetzung Status/Branch/HEAD neu
prüfen; der Integrationsbranch und #25 sind maßgeblich. Der Hauptcheckout bleibt
auf seinem eigenen Arbeitsbranch samt fremder `.vscode/extensions.json` erhalten.
