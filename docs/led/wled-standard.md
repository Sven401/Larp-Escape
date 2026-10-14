# WLED-Standard für die LED-Implementierung

## Entscheidung und Geltung

**Bestätigt am 15. Oktober 2026:** Der Projekt-Owner legt WLED als verbindliche
Grundlage für die LED-Implementierung fest. Quelle: ausdrücklicher Auftrag in
Codex-Sitzung `01a13c85-2371-7bd1-8b80-37104cb58c04`, nach dem Abgleich von WLED
mit dem LED-Referenzfall. Diese Datei ist die kanonische Projektfestlegung.

Für neue oder geänderte LED-Funktionalität gilt:

1. WLED-Funktionen, Effekte, Segmente, Presets und Playlists zuerst verwenden.
   Eine parallele eigene LED-Engine ist nicht der Projektstandard.
2. Vorhandene WLED-Bedienung und dokumentierte Schnittstellen nutzen. Eine
   eigene Android-App oder das bisherige BLE-/GATT-Protokoll sind keine
   Voraussetzung für den ersten LED-Ablauf. Eine spätere Spielleitungsoberfläche
   kann auf der WLED-Schnittstelle aufbauen.
3. Fehlende projektspezifische Funktionen gezielt über einen Adapter oder ein
   Usermod ergänzen. Eine Erweiterungsmöglichkeit ist noch kein Beleg, dass eine
   bestimmte Erweiterung existiert oder mit unserer Box funktioniert.
4. Für übernommene Firmware und Erweiterungen Version/Commit, Buildvariante,
   Herkunft sowie editierbare Konfiguration und Presets festhalten. Die unten
   geprüften Upstreamstände sind Quellenbelege, keine gewählte Produktversion.
5. Geräteantwort und sichtbare LED-Wirkung getrennt nachweisen. Ein abgesendeter
   Befehl oder HTTP-Erfolg allein bestätigt weder den gewünschten Zustand noch
   die sichtbare Wirkung. Nach Preset-/Playlist-Anforderungen den erwarteten
   Zustand durch nachfolgende Abfragen mit begrenzter Wartezeit abgleichen;
   bei ausbleibender Bestätigung bleibt die Ausführung unbestätigt.
6. Abweichungen von WLED als Grundlage mit konkreter Funktionslücke und Belegen
   dem Projekt-Owner zur Entscheidung vorlegen. Vorhandener Eigenbau allein ist
   keine Begründung für eine Abweichung.

Die Festlegung betrifft LEDs. Die Anbindung von NFC, Kristall-Eingabematrix,
Audio und Spielregeln bleibt offen; deren vollständige Abdeckung durch WLED
ist hier nicht behauptet. Die Eingabematrix des Spiels ist keine LED-Matrix.

## WLED-Belege

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
wurden an diesen Commits gelesen. Verfügbare Effekte und API-Verhalten sind bei
Auswahl einer konkreten Firmwareversion erneut gegen deren Stand zu prüfen.

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
nicht pauschal den Neustart jeder Animation. Der gewünschte Wiederholungsablauf
muss für den ausgewählten Effekt festgelegt und geprüft werden. Eine
Zustandsantwort ist außerdem keine Messung der realen LEDs.

## Verhältnis zum vorhandenen Code

**Ausgelesen am 15. Oktober 2026:** Projektbasis
`42610abfef07c971d9902ea6ca5a350ed68981bf`; der vorherige Abgleich erfolgte auf
`0a1e6be68bf8e70f5a5184b3f6b3de1f94744b23`, dessen zusätzliche Änderungen nur
Maintainer-Dokumentation betreffen. [Produktstart](../../src/main.cpp),
[Licht-Port](../../include/GamePorts/ILightEffects.h) und
[Laufzeitvertrag](../runtime.md) beschreiben die bestehende eigene LED-Lösung.

WLED ist mit dieser Dokumentationsänderung noch nicht eingebaut. Die bisherigen
FastLED-Task-, Queue- und Framevorgaben gelten für den vorhandenen Code; sie
werden nicht als Vorgaben auf WLED übertragen. Die konkrete bisherige Effekt-API
verpflichtet nicht zu einer optisch identischen Nachbildung in WLED.

Die [technische Übergabe](../../handoff/README.md) und der
[Hardwarevertrag](../../Hardware/HARDWAREVERTRAG-V0.1-2026-10-11.md) bleiben
Belegorte für Gerät und Verdrahtung. Deren historische Diagnose-/GATT-Abläufe
sind keine Voraussetzungen der WLED-LED-Bedienung. GPIO15 ist dort als
nutzerbestätigte LED-Datenleitung festgehalten; LED-Typ und tatsächliche Funktion
sind weiterhin offen. WLED-Unterstützung für einen Chiptyp ersetzt diesen
Hardwareabgleich nicht. Vor einem später beauftragten Upload gilt der in der
Hardwareübergabe bezeichnete chipgebundene Flash-Rückweg.

## Abnahme der späteren Implementierung

Der bestehende Referenzfall bleibt: am Handy LEDs starten, stoppen und bewusst
erneut starten, mit Geräteantwort und getrennter Nutzerbeobachtung.

- Das richtige Gerät ist über den dokumentierten Zugangsweg am Handy erreichbar.
- Start erzeugt den ausgewählten Lichtzustand oder Ablauf; Stop beendet die
  Ausgabe und der sichtbare Auszustand wird beobachtet.
- Derselbe gewünschte Ablauf lässt sich nach Stop bewusst erneut auslösen,
  ohne Neustart von Handy-App oder ESP. Falls ein Animationsstart vom Anfang
  verlangt ist, wird genau dieses Verhalten am gewählten Effekt geprüft.
- Die Bedienung zeigt eine zum Vorgang gehörende Geräteantwort oder einen frisch
  gelesenen Zustand. Fehlende Antworten bleiben erkennbar; sie gelten nicht als
  erfolgreiche Ausführung. Bei Presets/Playlists wird der nachfolgend gemeldete
  Zustand gegen den erwarteten Ablauf geprüft.
- Der Nachweis nennt Firmwarestand, Konfiguration, Befehlsfolge, Antworten und
  separate Beobachtung der LEDs. Bei einer Einbindung ins gesamte Spiel wird
  zusätzlich das Zusammenspiel mit dessen übrigen Funktionen geprüft.

**Prüfstand dieser Festlegung:** Quellen- und Dokumentationsabgleich;
WLED-Build, Integration, Upload und Geräteabnahme sind offen. Die Entscheidung
samt Nutzungsbewertung und Rückweg steht in [PROMOTION.md](../../PROMOTION.md).
