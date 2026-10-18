# Überarbeiteter Entwurf: Veranstaltung und Station

**Status: historischer Bildentwurf; durch [aktuellen Pilotvertrag](BOX-PILOT.md) ersetzt.**
Manuelle Such-/Wiederaufnahmeregeln und der Prompt unten sind ausschließlich Herkunft.
Datum: 18. Oktober 2026. Sitzung: `01a1505d-dfd4-7ad2-89cf-8d7be1b02240`.
Basis: Rollen-/Pilotcommit `74ab53085da289bb8988362c6feda9eee90c1434`.

## Bestätigte Produktvorgabe

Quelle: Nutzerauftrag nach dem apple-design-Review: verdichtete Variante 1 mit
räumlicher Kontinuität von Variante 3; neuen Entwurf vor Figma/Prototyp zeigen.
Im Veranstaltungseinsatz bleibt die Spielleitung im konkreten Spielkontext.
Veranstaltung und Station geben Orientierung. Appmarke und technische
Boxbezeichnung sind keine primären Bedienüberschriften. Veranstaltungsübergreifende
Inhalte brauchen einen eigenen Bereich. Dessen konkrete Navigation bleibt offen.
Das ergänzt die Identitätstrennung aus VA-01; eine technische Box ist nicht die
logische Station. Der Entwurf behauptet keine implementierte Stationszuordnung.

## Entwurf und Abgrenzung

[Überarbeitetes Bild](concepts/veranstaltung-station-v2.png).
„Nacht der Siegel“ und „Altes Tor“ sind frei gewählte Beispieldaten, keine realen
Veranstaltungsdaten oder geprüfte Geräteidentität.

- Veranstaltung/Station ersetzen Produkt-/Hardwareüberschriften.
- Kleine Unterbrechungsanzeige mit einer Wiederaufnahmeaktion; kein großer Alarm.
- Lichtauswahl bleibt als lokale Auswahl sichtbar; Start/Stop sind gesperrt.
- Erlaubte Modi Funkeln, Lichtimpuls und Ripple ersetzen die erfundenen Modi.
- Keine automatische Befehlswiederholung; Ausgang des offenen Auftrags bleibt unklar.

Sichtcheck am Bild: diese Inhalte vorhanden, keine abgeschnittenen Hauptaktionen,
keine globale Verwaltung und kein behaupteter aktueller Spielzustand. Native
Schrift-/Touchmaße, Kontrast, Screenreader und tatsächliche räumliche Stabilität
über Zustandswechsel bleiben ungeprüft. Die generierte Erklärung „Wähle einen
Lichteffekt und sende ihn an die Station“ kann in der Endfassung entfallen, da
die Sperrerklärung darunter bereits den nächsten Schritt erklärt.

Die bisherige generische Pilotbenennung LED-Box beschreibt nur den technischen
Bestandsweg; die neue Produktoberfläche folgt dem Veranstaltungskontext.
Nach Nutzerfeedback den Zielentwurf festlegen; erst danach Figma/Prototyp.

## Herkunft

Werkzeug: eingebaute Bildgenerierung, ein überarbeiteter Entwurf.
Tatsächlich angehängte Bildreferenzen: `status-im-mittelpunkt.png` und
`kontext-erhalten.png`. Originalgeneration:
`exec-1aa6d3e3-d6df-43c3-930b-93649248950c.png`.

Editierbarer Generierungsprompt:

```text
Use case: ui-mockup. Create ONE revised realistic production-quality mobile Android/Material 3 UI design, target 390 x 844 aspect ratio, app content only. Combine the attached references: first reference contributes clear hierarchy and one recovery action; second contributes retaining the light-control workspace during disconnection. They are sources to revise, NOT designs to copy literally.
Critical user correction: the operator is a game master working WITHIN a specific event and station. The screen's identity must be event + station, not app branding or physical hardware type. REMOVE ALL instances of "Larp Escape", "LED-Box", "Box auswählen", global settings, technical product headings and app logos. Cross-event administration belongs on its own separate surface outside this screen. Illustrative fictional event and station names are sample data only, not a verified device identity or existing event.
Header: quiet back navigation labeled "Stationen"; a small event-context label "Nacht der Siegel"; strong but compact station title "Altes Tor". Keep these stable at top. No heroic product heading. No OS chrome/device bezel/status bar/home indicator/notch/browser/device shadow.
Layout: clean flat light Material-like base surface, 24 dp side padding, restrained muted violet accent, Roboto-like system font, normal 16 sp body, 48 dp or greater action targets, all text left aligned, large enough contrast. Calm confident tone. Use grouping/whitespace before borders, no nested cards, no decorative background, no large exclamation symbol, no giant empty gaps. One viewport, no clipping, no multiple screens. Do not fake iOS glass or add blur.
Near top, directly below station context, a COMPACT persistent connection-status region. This replaces the ordinary status IN PLACE; it is NOT a modal, sheet or full-screen alarm. Very small warm amber status icon allowed. Heading exactly "Verbindung unterbrochen". Body exactly "Aktueller Zustand unbekannt." followed by "„Funkeln starten“ wurde nicht bestätigt." and a short quiet line "Keine automatische Wiederholung." State uncertainty once, avoid redundant repeated warnings. One clear wide violet primary button "Erneut suchen", close to this status text. This starts only discovery after explicit user action; no automatic reconnect, no replay, no claim success or box stopped or game stopped.
Below, maintain the light-control workspace readable and at its normal location, with heading "Licht". Three existing choices ONLY: "Funkeln", "Lichtimpuls", "Ripple". Show Funkeln as a preserved LOCAL SELECTION ONLY, not live effect. Use small label "Auswahl" if needed. Compact radio rows, not big cards. Buttons "Start" and "Stop" visibly unavailable (greyed buttons but readable labels), with brief explanatory text "Bedienung nach erneuter Verbindung." No fake unavailable modes like Dauerlicht, Blinken or Aus. No stale green live status. No game-state badges, no made-up current effect or telemetry. In this concept Start means the light effect, not the game.
Priorities from apple-design: purpose, calm recovery, user agency, spatial continuity, restrained visual hierarchy, typography by weight plus size, next action near its reason. Keep event/station context and light selection visible together with the compact recovery region. Do NOT add a wizard or progress stepper. Do NOT add a second competing recovery button. No extra nav or decorative illustration. This is a single revised image for review before Figma/prototype.
```
