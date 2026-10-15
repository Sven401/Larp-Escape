# LED-Funktionalität: externe Beispielimplementierung

## Verbindliche Projektgrenze

**Bestätigt am 15. Oktober 2026:** WLED dient ausschließlich als externe
Beispielimplementierung vieler Funktionen, die das Projekt ebenfalls benötigt.
Quelle ist die ausdrückliche Klarstellung des Projekt-Owners in Codex-Sitzung
`01a13c85-2371-7bd1-8b80-37104cb58c04`.

- WLED darf keine Build- oder Laufzeitabhängigkeit dieses Projekts sein.
- WLED darf in App oder Firmware nicht direkt referenziert werden, auch nicht
  durch Produktnamen, UI-Texte, Ressourcen oder WLED-spezifische Anbindung.
- In künftigen PRs und anderen Veröffentlichungen darf WLED weder als Bestandteil
  noch als benötigte Komponente dieses Projekts geführt werden.
- Die folgenden Quellen bleiben als externe Vergleichs- und Lernbelege erhalten.
  Sie beauftragen weder die Übernahme des WLED-Kerns noch einen Fork, ein Usermod
  oder einen WLED-Client. Funktionen werden im Projekt eigenständig umgesetzt.

Codex hatte den vorherigen Auftrag in Commit
`54933ea6e4b504014af70dc7e7d2803cd7d41dbb` fälschlich als Vorgabe zur Einbindung
von WLED ausgelegt. Diese Auslegung ist zurückgenommen. Die Klarstellung ändert
keine bestehenden fachlichen Ziele und entscheidet weder den Transportweg noch
die übrige Architektur neu. Der Korrekturverlauf steht in
[PROMOTION.md](../../PROMOTION.md).

Die technischen Angaben im folgenden Abschnitt beschreiben ausschließlich das
externe WLED-Projekt. Insbesondere sind dessen APIs, Preset-IDs und
Erweiterungsmechanismen keine Schnittstellenverträge für unsere App/Firmware.

## Belege zur externen Beispielimplementierung

**Belegt durch Quellenlektüre am 15. Oktober 2026.** Einstieg ist die offizielle
[WLED-Organisation](https://github.com/wled). Die festen Links halten die
geprüfte Fassung fest; die Dokumentationslinks dienen dem späteren Abgleich.

| Gegenstand | Beleg und konkrete Aussage |
| --- | --- |
| ESP32 und LED-Unterstützung | [WLED-README, fester Stand](https://github.com/wled/WLED/blob/2b116e1de395ec77d88fd3b066e40469114cd782/readme.md): ESP32, unter anderem WS2811, Effekte und getrennt steuerbare Segmente sind beschrieben. [Projekt](https://github.com/wled/WLED). |
| Handy und direkter Zugang | [Quick Start, fester Stand](https://github.com/wled/WLED-Docs/blob/42f69c184790455c2b59f1792f734c038357a439/docs/basics/getting-started.md): Weboberfläche und Android-App; direkter Browserzugang über den WLED-Zugangspunkt ist ohne vorhandenen WLAN-Router möglich. [Dokumentation](https://kno.wled.ge/basics/getting-started/). |
| Gespeicherte Lichtzustände | [Presets, fester Stand](https://github.com/wled/WLED-Docs/blob/42f69c184790455c2b59f1792f734c038357a439/docs/features/presets.md): Presets speichern Licht-/Segmentzustände, können API-Befehle enthalten und zu Playlists verbunden werden. [Dokumentation](https://kno.wled.ge/features/presets/). |
| Steuerung und Zustand | [JSON API, fester Stand](https://github.com/wled/WLED-Docs/blob/42f69c184790455c2b59f1792f734c038357a439/docs/interfaces/json-api.md): GET liest den Zustand, POST auf `/json/state` ändert ihn; `v:true` fordert die Zustandsantwort an. Ein/Aus, Helligkeit, Farben, Effekte, Segmente und Playlists sind dokumentiert. [Dokumentation](https://kno.wled.ge/interfaces/json-api/). |
| Eigene Erweiterungen | [Custom Features, fester Stand](https://github.com/wled/WLED-Docs/blob/42f69c184790455c2b59f1792f734c038357a439/docs/advanced/custom-features.md): Usermods erweitern WLED. Die Aktivierung über `custom_usermods` und `REGISTER_USERMOD()` ist ausdrücklich ab WLED 16 beschrieben. [Dokumentation](https://kno.wled.ge/advanced/custom-features/) und [offizielle Usermod-Vorlage](https://github.com/wled/wled-usermod-example). |
| Erweiterungen aus der Community | [Community Usermods, fester Stand](https://github.com/wled/WLED-Docs/blob/42f69c184790455c2b59f1792f734c038357a439/docs/advanced/community-usermods.md): Verzeichnis mit eigener Pflege durch Autoren; ein Eintrag bedeutet keine Prüfung oder Freigabe durch das WLED-Projekt. [Verzeichnis](https://kno.wled.ge/advanced/community-usermods/). |

Die öffentlichen HEADs wurden zusätzlich mit `git ls-remote` abgeglichen:
WLED `2b116e1de395ec77d88fd3b066e40469114cd782`, WLED-Docs
`42f69c184790455c2b59f1792f734c038357a439`. Alle sechs fest verlinkten Dateien
wurden an diesen Commits gelesen. Diese Referenzstände dienen ausschließlich
der Nachprüfbarkeit der Quellen; sie pinnen keine Projektabhängigkeit.

**Zusätzlich am WLED-Code belegt:** Der reine Preset-Aufruf über `ps` lädt
asynchron. [JSON-Verarbeitung](https://github.com/wled/WLED/blob/2b116e1de395ec77d88fd3b066e40469114cd782/wled00/json.cpp#L519)
und [Preset-Verarbeitung](https://github.com/wled/WLED/blob/2b116e1de395ec77d88fd3b066e40469114cd782/wled00/presets.cpp#L127)
merken die Anforderung zunächst vor; die
[HTTP-Antwort](https://github.com/wled/WLED/blob/2b116e1de395ec77d88fd3b066e40469114cd782/wled00/wled_server.cpp#L450)
wird direkt nach der JSON-Verarbeitung erzeugt. `v:true` ist daher ein
Gerätezustand zum Antwortzeitpunkt, keine Abschlussbestätigung eines
Preset-Wechsels. Ein reiner `ps`-Aufruf mit bereits aktiver Preset-ID wird an
diesem Stand zudem nicht erneut angewandt. Diese drei Codefundstellen wurden
am selben WLED-Commit gelesen; ein Gerätelauf fand nicht statt.

**Grenze des Belegs:** `on:true` bedeutet Einschalten und kann bei bereits
aktivem Licht ohne Änderung bleiben. Ein erneuter Presetaufruf garantiert
nicht pauschal den Neustart jeder Animation. Das externe Beispiel zeigt, warum
Einschalten und Animationsneustart getrennt zu betrachten sind. Eine
Zustandsantwort ist außerdem keine Messung der realen LEDs.

## Nutzen für unsere eigenständige Implementierung

Als fachliche Beispiele sind getrennt steuerbare LED-Bereiche, auswählbare
Effekte, gespeicherte Lichtzustände, bewusst wiederholbare Abläufe und eine
verständliche Rückmeldung interessant. Welche davon umgesetzt werden, folgt
aus den Projektanforderungen; der Funktionsumfang von WLED ist kein Backlog.

Aus dem asynchronen Verhalten der Referenz lässt sich eine allgemeine Prüffrage
ableiten: Bestätigt eine Antwort nur die Annahme oder bereits den gewünschten
Zustand? Für unsere Umsetzung wird diese Bedeutung ausdrücklich festgelegt.
WLED-Felder wie `ps` oder `v:true` werden dadurch nicht zum Projektprotokoll.

Der vorhandene Referenzfall aus der [technischen Übergabe](../../handoff/README.md)
bleibt: LEDs am Handy starten, stoppen und bewusst erneut starten, mit
zuordenbarer Geräteantwort und getrennter Nutzerbeobachtung. Ein Wiederholungs-
oder Animationsneustart wird an unserer eigenen Implementierung geprüft.

[Produktstart](../../src/main.cpp), [Licht-Port](../../include/GamePorts/ILightEffects.h)
und [Laufzeitvertrag](../runtime.md) bleiben die Belegorte für bestehenden Code;
der [Hardwarevertrag](../../Hardware/HARDWAREVERTRAG-V0.1-2026-10-11.md) für
Verdrahtungsbefunde. Aus der externen Referenz folgt kein Auftrag, diese
Verträge durch WLED zu ersetzen oder deren Fertigstellung zu umgehen.

## Prüfstand und Reichweite der Korrektur

**Ausgelesen am 15. Oktober 2026:** Der Dokumentationsbranch beruhte auf
`42610abfef07c971d9902ea6ca5a350ed68981bf` und erhielt in `54933ea` ausschließlich
Dokumentationsänderungen. Dieser Korrektureintrag ersetzt jene Festlegung;
er entfernt selbst keine Software von einem Gerät und korrigiert keine
abweichenden Implementierungsbranches automatisch.

Die Upstream-Quellenlektüre bleibt ein Beleg über die externe Referenz. Sie ist
weder ein Nachweis über unsere App/Firmware noch ein Geräte- oder Funktionstest
dieses Projekts. Für diese Klarstellung wurde kein Geräteupload durchgeführt.
