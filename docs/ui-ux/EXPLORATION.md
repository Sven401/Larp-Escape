# Visuelle Exploration: Herkunft und Prompts

**Status: historische Bildexploration; durch den [aktuellen Pilotvertrag](BOX-PILOT.md)
und [Figma/Webprototyp](../../design/box-pilot/README.md) ersetzt.**
Die Auswahl-/Fortsetzungsangaben und manuellen Suchregeln unten bewahren den
damaligen Entwurfsstand. Für native Weiterarbeit gilt der
[Stationspilot](../companion/spielleiter-stationspilot.md).
Datum: 18. Oktober 2026. Basis und Designbrief: [Box-Pilot](BOX-PILOT.md).
Werkzeug: eingebaute Bildgenerierung über Product Design / ideate.
Drei unabhängige Bilder, je ein B10-Screen. Zuordnung zur Auswahl folgt der
Anzeigereihenfolge im Chat; Richtungsnamen sind keine festen Optionsnummern.
Auswahl, Figma-Übernahme und visuelle Qualitätsprüfung waren zu diesem Zeitpunkt offen.

## Status im Mittelpunkt

```text
Create realistic, production-quality UI designs with clear hierarchy, strong typography and purposeful spacing. Use case: ui-mockup. Design one focused primary mobile Android app screen for Larp Escape, a game-master companion for a physical BLE box. Target dimensions: 390 x 844, app content only, natural mobile ratio. No device bezel, phone body, notch, OS status bar, clock, signal/battery, home indicator, browser chrome, device mask/shadow. Do not combine multiple screens or ideas.
Design target: interruption and explicit recovery in the flow select box -> connect/authenticate -> control light -> connection loss -> deliberately search and reconnect. This screen is after the connection drops while a light command has no confirmed outcome. German copy. Preserve these constraints: no automatic reconnect or replay, no current/green last-known box status, no claim box or autonomous game has stopped, no fake connectivity, no game-start controls, no fabricated station identities. One primary action: "Erneut suchen". Optional quiet navigation: "Zur Boxauswahl". Readable status text: "Verbindung unterbrochen", "Aktueller Boxzustand unbekannt.", "Letzter Auftrag: Ausgang unklar", "Kein Auftrag wird automatisch wiederholt." Product header "Larp Escape", selected candidate label "LED-Box"; this is a generic candidate name, not verified identity.
Grounding: inspected existing Kotlin Compose source uses Material 3 lightColorScheme, Material typography, 24 dp outer spacing, 16 dp group spacing, full width buttons and radio-based LED choices. No current screenshot or Figma source was available, so this is a proposal grounded in source code, NOT a screenshot recreation. Use a restrained light Material-style interface and muted violet accent (a pilot art-direction choice, not a measured existing color token), neutral text, accessible warm warning accents, Roboto-like readable typography with around 16 sp body and at least 48 dp touch targets. Status must be readable without color. Use whitespace, grouping, alignment and type before dividers/tints; borders and shadows only sparingly. No cards within cards, no feature inventory, no invented metrics, battery, signal or online counts. No decorative imagery, no fantasy art, no large ornamental icons. No unnecessary panels, tabs or badges. Keep all key status and recovery controls comfortably visible, with generous spacing. Use real readable German copy and no clipped content. Do not add dates or times.
Distinct direction: Status im Mittelpunkt. Composition: a stable small product header, selected box context, a prominent but calm interruption heading in the upper third, one clear explanation block and a wide primary recovery button directly below it. Secondary navigation is quiet. Single base surface, no enclosing card. The information hierarchy is status first, cause/uncertainty second, next action third.
```

## Geführter Rückweg

```text
Create realistic, production-quality UI designs with clear hierarchy, strong typography and purposeful spacing. Use case: ui-mockup. Design one focused primary mobile Android app screen for Larp Escape, a game-master companion for a physical BLE box. Target dimensions: 390 x 844, app content only, natural mobile ratio. No device bezel, phone body, notch, OS status bar, clock, signal/battery, home indicator, browser chrome, device mask/shadow. Do not combine multiple screens or ideas.
Design target: interruption and explicit recovery in the flow select box -> connect/authenticate -> control light -> connection loss -> deliberately search and reconnect. This screen is after the connection drops while a light command has no confirmed outcome. German copy. Preserve these constraints: no automatic reconnect or replay, no current/green last-known box status, no claim box or autonomous game has stopped, no fake connectivity, no game-start controls, no fabricated station identities. One primary action: "Erneut suchen". Optional quiet navigation: "Zur Boxauswahl". Readable status text: "Verbindung unterbrochen", "Aktueller Boxzustand unbekannt.", "Letzter Auftrag: Ausgang unklar", "Kein Auftrag wird automatisch wiederholt." Product header "Larp Escape", selected candidate label "LED-Box"; this is a generic candidate name, not verified identity.
Grounding: inspected existing Kotlin Compose source uses Material 3 lightColorScheme, Material typography, 24 dp outer spacing, 16 dp group spacing, full width buttons and radio-based LED choices. No current screenshot or Figma source was available, so this is a proposal grounded in source code, NOT a screenshot recreation. Use a restrained light Material-style interface and muted violet accent (a pilot art-direction choice, not a measured existing color token), neutral text, accessible warm warning accents, Roboto-like readable typography with around 16 sp body and at least 48 dp touch targets. Status must be readable without color. Use whitespace, grouping, alignment and type before dividers/tints; borders and shadows only sparingly. No cards within cards, no feature inventory, no invented metrics, battery, signal or online counts. No decorative imagery, no fantasy art, no large ornamental icons. No unnecessary panels, tabs or badges. Keep all key status and recovery controls comfortably visible, with generous spacing. Use real readable German copy and no clipped content. Do not add dates or times.
Distinct direction: Geführter Rückweg. Composition: lightweight compact three-phase path using plain text labels 'Auswählen', 'Verbinden', 'Bedienen' near the top. Current interruption is centered on the Verbinden step with a strong inline status message and concrete recovery explanation; generous vertical space, wide primary action in a lower action area. Show the path as navigation context, never as a promise of automatic progress or completed future steps. No dashboard, no metrics.
```

## Kontext erhalten

```text
Create realistic, production-quality UI designs with clear hierarchy, strong typography and purposeful spacing. Use case: ui-mockup. Design one focused primary mobile Android app screen for Larp Escape, a game-master companion for a physical BLE box. Target dimensions: 390 x 844, app content only, natural mobile ratio. No device bezel, phone body, notch, OS status bar, clock, signal/battery, home indicator, browser chrome, device mask/shadow. Do not combine multiple screens or ideas.
Design target: interruption and explicit recovery in the flow select box -> connect/authenticate -> control light -> connection loss -> deliberately search and reconnect. This screen is after the connection drops while a light command has no confirmed outcome. German copy. Preserve these constraints: no automatic reconnect or replay, no current/green last-known box status, no claim box or autonomous game has stopped, no fake connectivity, no game-start controls, no fabricated station identities. One primary action: "Erneut suchen". Optional quiet navigation: "Zur Boxauswahl". Readable status text: "Verbindung unterbrochen", "Aktueller Boxzustand unbekannt.", "Letzter Auftrag: Ausgang unklar", "Kein Auftrag wird automatisch wiederholt." Product header "Larp Escape", selected candidate label "LED-Box"; this is a generic candidate name, not verified identity.
Grounding: inspected existing Kotlin Compose source uses Material 3 lightColorScheme, Material typography, 24 dp outer spacing, 16 dp group spacing, full width buttons and radio-based LED choices. No current screenshot or Figma source was available, so this is a proposal grounded in source code, NOT a screenshot recreation. Use a restrained light Material-style interface and muted violet accent (a pilot art-direction choice, not a measured existing color token), neutral text, accessible warm warning accents, Roboto-like readable typography with around 16 sp body and at least 48 dp touch targets. Status must be readable without color. Use whitespace, grouping, alignment and type before dividers/tints; borders and shadows only sparingly. No cards within cards, no feature inventory, no invented metrics, battery, signal or online counts. No decorative imagery, no fantasy art, no large ornamental icons. No unnecessary panels, tabs or badges. Keep all key status and recovery controls comfortably visible, with generous spacing. Use real readable German copy and no clipped content. Do not add dates or times.
Distinct direction: Kontext erhalten. Composition: keep a quiet recognizable light-control context behind the interruption area, a small 'Lichtsteuerung' heading and clearly unavailable 'Start' and 'Stop' controls annotated 'Nicht verfügbar'. A prominent attached lower recovery surface contains the interruption heading, unknown outcome copy and wide 'Erneut suchen' action. Flat tonal separation, no dimmed unreadable text, no blocking device mockup. The lower recovery area must not overlap or obscure explanatory content. No displayed active effect or last-known success.
```


## Erzeugte Vorschläge und erster Sichtcheck

Drei Bilder wurden unabhängig erzeugt und im Hauptchat in dieser Reihenfolge
angezeigt: [1](concepts/status-im-mittelpunkt.png),
[2](concepts/gefuehrter-rueckweg.png), [3](concepts/kontext-erhalten.png).
Originale unverändert erhalten; lokale Projektkopien und Hashes im
[Manifest](concepts/manifest.json). Prompts oben sind editierbare Entwurfsquellen,
die PNGs noch keine editierbaren Figma-Komponenten.

Sichtcheck am generierten Bild, keine native UI-Prüfung:
- Alle drei zeigen Unterbrechung, unbekannten Zustand, unklaren Auftrag und
  explizites erneutes Suchen. Kein automatischer Befehlsretry zugesagt.
- Bild 1/2: Warnsymbol größer und alarmierender als im ruhigen Brief gewünscht;
  Hierarchie und Textumfang bei 390 dp und großer Schrift praktisch prüfen.
- Bild 2: Phasenanzeige darf bei Wiederaufnahme keinen linearen Erstablauf oder
  bestätigte Boxidentität suggerieren; Kennzeichnung nach Auswahl präzisieren.
- Bild 3: Generator erfand Aus/Dauerlicht/langsames/schnelles Blinken. Diese Modi
  sind **kein** Produktvorschlag. Bei Weiterarbeit durch die erlaubte Pilotaktion
  Funkeln ersetzen; gegebenenfalls neue korrigierte Bildfassung vor dem Bau zeigen.
- Alle sind Rasterkonzepte in höherer Auflösung im angefragten mobilen Seitenverhältnis.
  Schrift-, Kontrast- und Touchmaße sind daraus nicht als bestanden ableitbar.

Nächster Schritt: Nutzer wählt eine Richtung oder nennt Änderungen. Danach
gegebenenfalls korrigierten Zielentwurf zeigen, dann Figma und Prototyp.
