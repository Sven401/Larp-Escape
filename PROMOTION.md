# Projektfestlegungen: Übernahme und Korrektur

## 18. Oktober 2026 — LED-Rolle 2.0 nach realer Iteration

- **Auftrag erweitert:** Nutzer verlangt Idee bis praktisch verfeinerte Umsetzung
  mit früher serieller/Kamera-Iteration. Die Beschränkung der Fassung 1.0 ist damit
  ersetzt; die historische Nutzungsbewertung bleibt unten erhalten.
- **Beobachtete Anwendung:** [Näheglut-Bericht](docs/led/naeheglut-live-results-2026-10-18.md)
  mit einem Firmwareimage und drei frühen Kameraläufen vor Commit: Farbauswahl,
  Kurvenvergleich, zwei Instanzen, Parameteränderungen, gezielter/globaler Stop.
  Vorschau-Lock im ersten Lauf aufgedeckt und behoben; zwei Folgeläufe vollständig.
- **Bewertung/Übernahme:** [Rolle 2.0](docs/led/rollen/led-effekt-architekt.md) und
  [interaktiver Weg](docs/led/interactive-development.md) sind ein nützlicher
  Projekteinstieg. Gemeinsamer Kern statt Kamera-Sonderalgorithmus, Vorschaufehler
  getrennt von Aufnahmebelegen, offene Aufträge beim Abschluss explizit und
  gestalterisches Feedback von metrischer/Produktabnahme getrennt. Eine Anwendung
  belegt keinen dauerhaften Zeitgewinn. Kein globaler Skill und keine Hookänderung.
- **Gültigkeit:** Sitzung `01a14f9a-35fb-72e1-9acb-132b0d16599d`, Basis `aa887f5`,
  Quell-/Binaryhashes und aktuelle Prüfwege im Bericht. Originalfirmware und voller
  Flash verifiziert wiederhergestellt. Produktadapter und optisches Nutzerurteil offen.
- **Einfacher Rückweg:** Rollen-/Anleitungsverweise im Maintainer-Einstieg inaktiv
  setzen; ursprüngliche CAL/RUN-Laborroute weiterverwenden. Neue LIVE-Route und
  NearGlow-Dateien lassen sich getrennt entfernen. Produkt-USB/BLE wurden nicht
  erweitert; die frühere Installation ist bereits wiederhergestellt. Fachquellen,
  Entwurf und historische Versuchsbelege erhalten.


## 18. Oktober 2026 — LED-Effekt-Architektenrolle nach erster Anwendung

- **Beauftragt und angewandt:** Projektgebundenes
  [Rollenprofil 1.0](docs/led/rollen/led-effekt-architekt.md) für Verhalten,
  Berechnung, Prüfplanung und den fachlichen Spiel-/Effektvertrag.
  Quelle: Nutzerauftrag und Antworten in Sitzung
  `01a14f9a-35fb-72e1-9acb-132b0d16599d`; Ausgangsbasis `dead12040c196ff74ab4be17ba71451a9351a676`.
- **Beobachteter Einsatz:** Kandidat 0.1 auf den neuen Näheglut-Entwurf angewandt;
  [Erprobungsbericht](handoff/LED-EFFEKT-ARCHITEKT-ERPROBUNG-2026-10-18.md)
  belegt 23 Modelltests und unabhängigen Review. Stop während Ausblenden,
  fortlaufende Zieländerungen, Gesamt-Aus und Ticket-/Instanzleben präzisiert.
- **Bewertung:** Quellenrouter und Pflichtfragen deckten konkrete
  Mehrdeutigkeiten auf; erste nützliche Anwendung, keine gemessene langfristige
  Zeitersparnis oder optische Freigabe.
- **Übernahme:** Rollenprofil bei beauftragter LED-Effektentwurfsarbeit als
  Einstieg nutzen. Keine Änderung an AGENTS, globalen Skills oder Hooks.
  Fachvertrag bleibt Entwurf; die einmalige Python-Probe wird nicht zum
  allgemeinen Produktwerkzeug oder CI-Standard befördert.
- **Einfacher Rückweg:** Verweise auf das Rollenprofil aus den beiden
  Maintainer-/Übergabeeinstiegen entfernen und Profil als inaktiv markieren.
  Fachquellen, Entwurf und Prüfbelege bleiben erhalten; keine Firmwareänderung
  muss rückgängig gemacht werden.

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
