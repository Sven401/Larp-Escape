# Projektfestlegungen: Übernahme und Korrektur

## 15. Oktober 2026 — Fehlinterpretation zur LED-Grundlage zurückgenommen

- **Bestätigt:** Der Projekt-Owner hat klargestellt, dass WLED ausschließlich
  eine externe Beispielimplementierung ist. Die verbindliche Grenze steht in
  [LED-Funktionalität: externe Beispielimplementierung](docs/led/implementation-reference.md).
  Quelle: Klarstellungsauftrag in Codex-Sitzung
  `01a13c85-2371-7bd1-8b80-37104cb58c04`.
- **Korrektur:** Codex hatte den vorherigen Auftrag in `54933ea` fälschlich als
  Einbindungsvorgabe dokumentiert und diese dem Owner zugeschrieben. Diese
  Deutung ist zurückgenommen; es handelt sich nicht um einen nachträglichen
  Wechsel der Produktentscheidung des Owners.
- **Durchgeführter Rückweg:** Übernahme-, Migrations-, Usermod- und
  Schnittstellenpflichten entfernt; den Zusatz zum Laufzeitvertrag
  zurückgenommen; die Quellen als externe Beispiele eingeordnet. App und
  Firmware wurden in diesem Dokumentationsbranch nicht verändert.
- **Bewertung:** Der Quellenvergleich ist als Lernmaterial nützlich. Daraus
  folgt keine Freigabe zur Nutzung als Projektkomponente. Abweichende
  Implementierungsbranches und installierte Software benötigen eine eigene
  Korrektur; dieser Eintrag behauptet deren Bereinigung nicht.
