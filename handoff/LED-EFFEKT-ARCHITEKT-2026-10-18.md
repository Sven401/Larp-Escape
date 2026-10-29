# Übergabe: spezialisierte LED-Effekt-Architektenrolle erstellen

Datum: **18. Oktober 2026**. Herkunft: Codex-Sitzung
`01a14f43-faf0-7853-b130-2953cf0a43b3`.
Status: **Auftrag und Schwerpunkt vom Nutzer bestätigt; Rolle und neuer
Effektentwurf offen.** Diese Sitzung erstellt die Übergabe, nicht die Rolle.

## Auftrag und geklärte Entscheidungen

Die neue Session soll eine spezialisierte LED-Effekt-Architektenrolle für
dieses Projekt erstellen. Sie soll weitere Effektarbeit fachlich gezielter
und mit weniger wiederholter Quellenerschließung vorbereiten.

Mit ausdrücklich aufgerufenem Skill `grill-me` wurden zwei Entscheidungen
abgefragt und vom Nutzer bestätigt:

- **Zuständigkeit:** Verhalten, Effektberechnung und Prüfplanung. Keine
  pauschale Gesamtverantwortung für App-/BLE-/Spielintegration ableiten.
  Benötigte Schnittstellen und Abhängigkeiten soll die Rolle benennen.
- **Praktische Erprobung:** Die Rolle an einem **Entwurf für einen neuen
  LED-Effekt** anwenden und bewerten. Die Originaleffekte liefern Fachwissen
  und Gegenbeispiele; ihre Kameraprüfung ist nicht die gewählte Rollenprobe.

Quelle: direkte Nutzerantworten in dieser Sitzung am 18.10.2026.
Der neue Effekt selbst, sein Einsatzzweck und sein sichtbarer Ablauf sind
noch nicht festgelegt. Aus diesem Auftrag folgt noch kein Produktumbau oder
Geräteupload. Eine neue Codex-Session wurde hier nicht angelegt.

## Konkreter Arbeitsauftrag für die neue Session

1. Arbeitsstand neu abgleichen und die unten verlinkten Einstiege gezielt
   lesen. Bestehende Erkenntnisse wiederverwenden, nicht alle Analysen
   erneut durchführen. Widersprüche am aktuellen Code prüfen.
2. Rollenform und Ablage bestimmen: projektgebundenes Rollenprofil oder
   projektspezifischer Skill sind Möglichkeiten, noch keine Entscheidung.
   Bei einem Skill den passenden Erstellungs-Skill verwenden. Projektfakten
   bleiben im Repository; keine globale Kopie der LED-Dokumentation anlegen.
3. Die Rolle als kompakten Kandidaten mit Auftrag, Eingaben, Ergebnissen,
   Quellenrouter und Entscheidungsgrenzen erstellen. Konkrete Fähigkeiten
   aus den vorhandenen Effekt- und Messarbeiten ableiten.
4. Für den neuen Effekt nur die materiell offenen Produktentscheidungen
   klären: Einsatz/Situation, Eingaben, gewünschtes sichtbares Verhalten und
   Abschluss. Quellen zuerst prüfen; mit `grill-me` höchstens drei Fragen je
   Runde bündeln. Ohne Antworten keine Produktabsicht erfinden.
5. Mit der Rolle einen vollständigen Effektentwurf erstellen und am
   beobachtbaren Nutzerablauf durchgehen. Unklare oder unnütze Rollenregeln
   anschließend korrigieren; den Nutzen und verbleibende Grenzen benennen.
6. Ergebnis mit Quellen, Prüfung und offenen Entscheidungen übergeben.
   Bei dauerhafter Aktivierung neuer Arbeitsregeln den beobachteten Einsatz,
   die Nutzungsbewertung und einen einfachen Rückweg in `PROMOTION.md`
   festhalten. Bestehende Projektregeln nicht stillschweigend ausweiten.

## Erwartete Ergebnisse und erkennbare Abnahme

**Rollenprofil:** Eine Folgesession erkennt daraus, welche fachlichen Fragen
die Rolle beantwortet, welche Eingaben fehlen, welche Quellen sie dafür
gezielt liest und welches prüfbare Ergebnis sie liefert. Keine bloße
Bezeichnung „Architekt“ und kein allgemeiner Ratgeber ohne Projektanschluss.

**Neuer Effektentwurf:** Ein Mensch kann den Ablauf nachvollziehen:
Welche Eingabe erfolgt, welche LED bzw. welcher Bereich zeigt wann welche
Farbe/Helligkeit, und was passiert bei Wiederholung, Loslassen, Ende oder
Unterbrechung? Nur tatsächlich passende Fälle übernehmen. Dazu gehören:

- Zeitbasis, Phasen, räumliche Zuordnung, Farben/Paletten und Parameter;
- Initialisierung, Zustand und Zusammenspiel mit anderen Lichtbeiträgen;
- Berechnung in einer überprüfbaren Form, etwa Pseudocode, Formeln oder
  Zustandstabelle; relevante Grenzen wie Indizes und Ganzzahlarithmetik;
- Annahmen und notwendige Anschlussarbeiten an vorhandene Schnittstellen;
- ein verhältnismäßiger Prüfplan mit Eingaben, erwarteten Beobachtungen,
  Vorbedingungen und vorab begründeten Abnahmekriterien.

**Nachweis der Rollenprobe:** Ein ausgefüllter Entwurf und ein kurzer Bericht
halten fest, welche offenen Punkte die Rolle aufgedeckt hat, welche
Quellenarbeit sie erspart und wo ihr Ablauf nachgebessert wurde. Das ist
eine erste Nutzungsbewertung, kein belegter dauerhafter Produktivitätsgewinn.
Quellprüfung, Rechnerprobe, übertragene LED-Frames, Kamerabild und menschliche
Bewertung des Effekts bleiben getrennte Nachweise. Ein Entwurf allein belegt
keinen gebauten oder optisch abgenommenen Effekt.

## Arbeitsverzeichnis und Veröffentlichungsstand

Am 18.10.2026 ausgelesen:

- Checkout: `C:\Dev\Larp Escape`, Remote `https://github.com/Sven401/Larp-Escape.git`.
- Branch: `codex/led-resonanzkreis`; inhaltliche Ausgangsbasis dieser Übergabe
  `834efe75c9505845b9124fd34a0fdfb49821c32b`.
- [PR #71](https://github.com/Sven401/Larp-Escape/pull/71) enthält Resonanzkreis,
  Kameraerweiterung und Originalanalysen. Ziel ist
  `plan/projektuebergabe-kuratierter-stand`; Integration/Merge hier offen.
- Fremde Änderung: `.vscode/extensions.json`. Unangetastet erhalten.

Der Übergabecommit folgt auf diese Basis. Bei Fortsetzung Branch, HEAD,
Remote, Arbeitsbaum und tatsächlichen PR-Stand frisch prüfen. Für die
eigenständige Rollenerstellung einen eigenen Arbeitsbranch/Worktree nach
Projektregeln verwenden; erforderliche Quellen aus #71 erhalten, auch falls
der Integrationsbranch sie zu diesem Zeitpunkt noch nicht enthält.

## Gezielt zu lesende Quellen

| Zweck | Kanonischer Einstieg |
| --- | --- |
| Mandat, Grenzen, aktuelle Projektbasis | [AGENTS.md](../AGENTS.md), [Maintainer](MAINTAINER.md), [technische Übergabe](README.md) |
| Original-Wake-Ripple und Spielstart | [Analyse](../docs/led/legacy-wake-ripple-analysis.md) |
| Original-Burst, Startkonflikte, Farbübernahme | [Analyse](../docs/led/legacy-burst-analysis.md) |
| Original-Funkeln, Zeitbasis, Dichte, Paletten, Boot | [Analyse](../docs/led/legacy-twinkle-analysis.md) |
| Heutiger Resonanzkreis und gewünschter Nutzerablauf | [Prüfplan](../docs/led/resonance-test-preparation.md) |
| Optischer Teilnachweis und seine Grenzen | [Kameraergebnis](../docs/led/resonance-camera-results-2026-10-18.md), bei Messfragen [Verfahren](../docs/led/resonance-camera-measurement.md) |
| Produktberechnung und Ausgabe | [LedOutput](../firmware/led/src/LedOutput.cpp), [Schnittstelle](../firmware/led/include/LedOutput.h), [RMT](../firmware/led/src/RmtLedBus.cpp) |
| Historische Komposition/Spielaufrufe | `lib/LedFx/`, `lib/GameState/GameState.cpp`, `src/main.cpp`, jeweils am historischen Commit lesen |
| Anschlüsse bei Bedarf | [Firmwareeinstieg](../firmware/led/README.md), [Wire V2](../docs/led/ble-wire-v2.md), [Veranstaltungsarchitektur](../docs/veranstaltungsarchitektur.md) |
| Externe Referenzgrenze | [Owner-Klarstellung](../docs/led/implementation-reference.md), [Korrekturprotokoll](../PROMOTION.md) |

Die drei Originalanalysen beziehen sich ausdrücklich auf
`a40365d715b2055ee23f767fa53b55ac25243437`, den vom Nutzer benannten
Live-Referenzquellstand. Ein damals installiertes Firmwarebinary ist damit
nicht zusätzlich nachgewiesen. Maschinenlesbare Befunde stehen jeweils
verlinkt unter `docs/led/fixtures/`; Details dort statt hier duplizieren.

## Erhaltener Prüfstand und typische Fallen

- **Belegt, Originalanalyse:** Ripple/Burst zählen Zeichenschritte; Funkeln
  berechnet Phasen aus `millis()`. Das ist bei Tempo, Pausen und Wiederholung
  wesentlich. Die ursprüngliche Komposition überschreibt mit Funkeln und
  addiert danach Burst und vier Ripples. Heutige manuelle Namensvettern
  sind keine automatisch originalgetreuen Implementierungen.
- **Belegt, Ripple:** Für Ursprung 2 ergibt der historische Code einen
  negativen Index. Nicht ungeprüft auf Hardware reproduzieren. Gefundene
  Fehler und beabsichtigtes Verhalten getrennt behandeln; keine heimliche
  Korrektur als Original ausgeben.
- **Belegt, Burst/Funkeln:** Ein abgewiesener Burst-Neustart kann die Farbe
  ändern; Funkelziele, Timer und Paletten werden teilweise übernommen.
  Start, Neustart, Reset und Zustandseintritt sind daher eigene Fragen.
- **Gemessen, Resonanzkreis:** Vier gewertete Kameraläufe mit 3232 Messbildern
  bestehen auf allen 23 LEDs bei synthetischen Wake-Eingängen. Reale Sensoren,
  neues BLE-Produktpaar, Spielverständlichkeit und Gesamtspiel bleiben offen.
- **Belegt, Messverfahren:** Indexzuordnung und passende unabhängige
  Farb-/Helligkeitsreferenzen vorab herstellen. Alte Weiß-/Blau-Cyan-Referenzen
  und Grenzwerte nicht pauschal auf beliebige neue Effekte übertragen.
- **Belegt, Original-Funkeln:** Acht begrenzte Rechnerfälle und neun stabile
  Dichten mit 75348 LED-RGB-Zeitpunkten; verwendete Adapter und FastLED-Helfer
  sind im Bericht eingegrenzt. Kein Original-Kameralauf für die drei Effekte.
- **Ausgelesen, letzter Gerätebericht:** Nach Resonanzkreis-Labortests wurde
  der vorherige RSSI-Boxstand vollständig verifiziert wiederhergestellt.
  Dies ist kein frisch ausgelesener Iststand der kommenden Session.
  Vor jedem beauftragten Upload Chip, aktuelle Installation und Flash-Rückweg
  nach Hardware-/Messdokumentation neu abgleichen; COM-Port nicht als Identität.
- **Bestätigte Projektgrenze:** WLED ist nur externe Lernreferenz, keine
  vorgesehene Projektkomponente oder Abhängigkeit.

Die editierbaren einmaligen Proben und Rohbelege existieren lokal unter
`private/legacy-wake-ripple-analysis`, `private/legacy-burst-analysis`,
`private/legacy-twinkle-analysis` und `private/resonance-camera-20261018`.
Sie sind ignoriert und in einem neuen Klon nicht automatisch vorhanden.
Bei Bedarf im genannten Checkout auf Existenz prüfen. Keines dieser
Prüfmittel allein durch diese Übergabe zum dauerhaften Standard machen.

## Offene Punkte und nächster konkreter Schritt

Offen sind die Ablage-/Aktivierungsform der Rolle und der neue Effekt als
fachlicher Anwendungsfall. Die nächste Session soll zunächst den Arbeitsstand
prüfen, den Rollenkandidaten vorbereiten und dann die wenigen fehlenden
Entscheidungen zum neuen Effekt mit dem Nutzer klären. Der erkennbare Abschluss
ist eine erprobte, bewertete Rolle samt nachvollziehbarem Effektentwurf und
Prüfplan; Umsetzung und Hardwareabnahme sind daraus getrennt zu beauftragen.
