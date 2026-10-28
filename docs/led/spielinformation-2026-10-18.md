# Spielinformation durch LED-Effekte

**Status: Nutzerentscheidung vom 18.10.2026, am 20.10.2026 aus dem Chat
erneut ausgelesen.** Quelle: „Schlage drei LED-Effekte vor“, Sitzung
`01a14f1d-43b9-7ca1-955d-9ee7efb590fa`, Nutzerantworten auf die beiden
Fragen zur Kristallfortschrittsanzeige und Unterscheidung von Fehlerursachen.
Der damalige Entwurfsabgleich bezog sich auf den Spielcode bei `fcb9121`.
Dies ist ein Gestaltungsbeschluss, kein Implementierungs- oder Gerätenachweis.

## Bestätigte Grenzen

- **Kristallfortschritt: „Nur atmosphärisch andeuten“.** Die Frage bezog sich
  ausdrücklich darauf, ob neue Effekte verraten dürfen, wie viele der drei
  Kristalle korrekt akzeptiert wurden. Der daraufhin formulierte Vorschlag
  beschreibt zunehmende Verdichtung ohne zählbare Bänder oder Stufen.
- **Fehlerursache: „Nur allgemeine Störung“.** Falscher Kristall und fehlender
  Schlüsselstein sollen durch den neuen Fehlereffekt nicht unterscheidbar
  werden. Der Lichtverlauf zeigt eine allgemeine Störung.

Der Resonanzkreis wurde danach als erster Effekt zur Umsetzung beauftragt.
Diese Reihenfolge erteilt keine Freigabe für eine spätere zählbare
Kristallanzeige. Sie ändert auch keine bereits vorhandenen Spielregeln.

## Konsequenz für neue Entwürfe

Neue Kristall-/Fehlereffekte gegen diese Grenzen prüfen. Im Spieltest bleibt
offen, ob eine atmosphärische Verdichtung trotzdem als versteckte Zählanzeige
verstanden wird. Technische Zustandskorrektheit und wahrgenommene Spielinformation
sind getrennte Abnahmen.

**Abweichung festgestellt · 20.10.2026:** Der noch offene
[Kristallsiegelentwurf #89](https://github.com/Sven401/Larp-Escape/pull/89)
bei `5da6b40` stellt einen Abschnitt je bestätigtem Kristall und dauerhafte
Ablesbarkeit von 1/3, 2/3 und 3/3 vor. Das weicht von der obigen Entscheidung ab.
Der PR bezeichnet seine Spielbedeutung selbst als noch offen. Vor Übernahme
den Entwurf anpassen oder eine ausdrücklich geänderte Nutzerentscheidung
einholen; eine technische Prüfung oder grüne CI entscheidet diese Frage nicht.
Der Audit hat den Entwurf weder geändert noch verworfen.
