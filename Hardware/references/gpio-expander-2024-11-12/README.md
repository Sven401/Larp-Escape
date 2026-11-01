# GPIO-Expander: Referenz für zukünftiges Hardwaredesign

**Bestätigt · 1. November 2026:** Der Nutzer bestimmt diesen Entwurf als
Ressource für zukünftiges Hardwaredesign im Projekt Larp-Escape.
Quelle: Nutzerauftrag in Codex-Sitzung `01a19920-4757-7e12-adbb-829cf546a119`.
Dies ist eine Entwurfsreferenz, keine Freigabe zur Fertigung oder verbindliche
Vorgabe für neue Platinen.

## Herkunft und Erhaltung

Das [Originalpaket](Gerber_Gpio-Expander_PCB_Gpio-Expander_2024-11-12.zip)
wurde am 1. November 2026 vom Nutzer bereitgestellt und bytegleich übernommen.
Zuvor bezeichnete der Nutzer den JLCPCB-Eintrag
`Gpio-Expander_PCB_Gpio-Expander` mit dem Status
„Order expired as it hasn't been submitted for a month.“
Der Shop-Inhalt und die Identität mit diesem ZIP wurden nicht unabhängig geprüft.
Das Datum 2024-11-12 ist dem Dateinamen entnommen, keine verifizierte Revision.

**Gemessen:** SHA-256 des Originalpakets und der Projektkopie:
`dfd3c2878d0d13eca65ba1e2b2674cebf4bef80b5ecab4c720969e4f20c0e8b8`.

## Ausgelesener Inhalt

ZIP-Verzeichnis und beide tabulatorgetrennten CSV-Dateien wurden am
1. November 2026 mit .NET ZipArchive/StreamReader ausgelesen.
Projektbasis: `c4f1f824c6274e9a8d7676212a2a96be4d4a77b9`.

- `BOM_Gpio-Expander_2024-11-12.csv`: sechs Positionen mit insgesamt
  22 Bauteilen; 16 dreipolige Anschlüsse A0–A7/B0–B7, zwei vierpolige
  Anschlüsse CN1/CN2, ein dreipoliger Anschluss CN3 sowie U1/U2/U3
  als Stift-/Buchsenleisten. LCSC-Teilenummern sind enthalten.
- `PickAndPlace_PCB_Gpio-Expander_2024-11-12.csv`: Positionen,
  Footprints, Oberseite und Rotation der 22 Bauteile.
- Verschachteltes `Gerber_Gpio-Expander_PCB_Gpio-Expander_2024-11-12.zip`:
  `Gerber_TopLayer.GTL`, `Gerber_BottomLayer.GBL`,
  `Gerber_TopSilkscreenLayer.GTO`, `Gerber_TopSolderMaskLayer.GTS`,
  `Gerber_BottomSolderMaskLayer.GBS`, `Gerber_BoardOutlineLayer.GKO`,
  `Gerber_DocumentLayer.GDL`, `Drill_PTH_Through.DRL`,
  `Drill_PTH_Through_Via.DRL` und `How-to-order-PCB.txt`.

## Verwendung und offene Prüfungen

Die Fertigungsdaten können künftig als Ausgangsmaterial für Layout-,
Anschluss- und Bestückungsvergleiche dienen. Das Originalpaket bleibt erhalten.
Ein Schaltplan oder editierbare CAD-Projektquellen sind darin nicht enthalten.
Vorhandene [Hardwarequellen](../../QUELLENINVENTAR.md) sind separate Eingänge;
eine Übereinstimmung mit ihnen oder der real bestückten Box ist offen.

**Offen:** visuelle Gerberprüfung, Netz-/Pinzuordnung, elektrische Bewertung,
ERC/DRC, Importierbarkeit und Fertigungsabnahme. Das Einlagern beinhaltet keine
Bestellung und keinen Gerätetest. Für spätere Änderungen möglichst die
ursprünglichen editierbaren CAD-Quellen ergänzen und die Revision abgleichen.
