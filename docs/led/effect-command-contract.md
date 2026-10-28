# Fachlicher Vertrag: Spielaufträge und LED-Beiträge

Stand: **18. Oktober 2026**. **Entwurf des Produktvertrags**, keine implementierte Produkt-API
oder neue Produkt-Wire-Version. Für die [LED-Effekt-Architektenrolle](rollen/led-effekt-architekt.md).
Nutzerauftrag: Effekte parallel beauftragen, ändern, stoppen und lesbar mischen.
Produktbasis geprüft: `dead12040c196ff74ab4be17ba71451a9351a676`.
Historische Quellen: Rollenrouter, Original `a40365d`, Analysen bei `e7ec0f9`.

Eine begrenzte Teilmenge ist inzwischen im [isolierten LIVE-Labor](../../firmware/led/LIVE-NEARGLOW.md)
ausführbar: zwei Instanzen, RGB-Festkomma, ADD32 und gezielter Stop.
Dessen Session-Token ersetzt keine produktive Epochen-/Profilintegration.
[Praktischer Nachweis](naeheglut-live-results-2026-10-18.md).

## Iststand und Anschluss

| Vorhandener Pfad | Belegt im Code | Fehlender Anschluss / Folgearbeit |
| --- | --- | --- |
| Festes Spiel | `ReichstagGame → ILightEffects → GameLightEffects`; Wertqueue 32, LED-Besitzer, Funkeln → Burst → 4 Ripples → FastLED | Allgemeine Instanzidentität, gezielter Stop, Einzelfehlerrückmeldung fehlen. Port/Adapter vor Produktintegration ergänzen. |
| Profilspiel | `Engine/Session → IProfileDevices → Broker::applyEffect → Owner::Led`; kopierte Effektwerte und Tickets | Reale Adapter und Instanz-/Mixvertrag offen. Kurzen Auftragsabschluss von lang lebendem Effekt trennen; Profilbarrieren müssen beide berücksichtigen. |
| Manuelle Baugruppe | `ProductMain → LedOutput::start/stop → RMT`; ein angeforderter Effekt, Start ersetzt ihn, Stop fordert Schwarz | Kein Mixer; V2 meldet eine Effekt-ID. Neue Semantik benötigt eigenen API-/Wire-/App-Auftrag, keine Umdeutung vorhandener Nachrichten. |

Belegorte: [Lichtport](../../include/GamePorts/ILightEffects.h),
[GameLightEffects](../../lib/LedFx/GameLightEffects.cpp),
[Laufzeit](../runtime.md), [Profilruntime](../configuration/device-runtime.md),
[Broker](../../src/DeviceRuntime/Broker.cpp),
[Channel](../../include/DeviceRuntime/Channel.h),
[LedOutput](../../firmware/led/include/LedOutput.h),
[RMT](../../firmware/led/src/RmtLedBus.cpp), [V2](ble-wire-v2.md).

Das Original beweist eine konkrete Komposition, keinen universellen Mixer:
Funkeln überschreibt den vollständigen Puffer, Burst und vier Ripples addieren
saturierend in RGB8. Danach wirken globale Helligkeit, Farbkorrektur und
Leistungsbegrenzung. Diese Reihenfolge und ihre Zustandseffekte sind im
Rollenrouter nachlesbar; historische Fehler werden nicht zum neuen Soll.

## Zuständigkeiten

```mermaid
flowchart LR
  G[Lokales Spiel: Bedeutung und Gültigkeit] --> A[Serialisierte Auftragsübernahme]
  A --> E[Unabhängige Effektinstanzen]
  E --> M[Komposition: ein frischer Frame]
  M --> O[Ausgabegrenzen und einziger LED-Treiber]
  A --> R[Annahme oder Ablehnung]
  E --> R
  O --> R
```

Das Spiel liefert bewertete Werte und Ereignisse. Eine Effektinstanz liefert
für einen Zeitstempel einen Lichtbeitrag mit Maske, kein Hardwarekommando.
Der LED-Besitzer verwaltet Zustand und Komposition; genau ein Adapter gibt
den Frame aus. Gleichzeitige fachliche Aufträge bedeuten unabhängig laufende
Beiträge, nicht mehrere schreibende LED-Tasks.

BLE-, Sensor- und Funkcallbacks reichen Wertkopien an bestehende Besitzer
weiter. Das lokale Spiel läuft nach [VA-01](../veranstaltungsarchitektur.md)
autonom. Ein BLE-Abbruch beendet manuell besessene Ausgaben nach deren
Vertrag, nicht pauschal alle Spielbeiträge.

## Aufträge, Identität und Lebensdauer

Semantische Felder, **kein festgelegtes C++-/Wireformat**:

- Geltungsbereich: lokale Box/Komponente, Spiel- oder manueller Besitzer,
  Durchlauf/Profilgeneration bzw. Sitzung.
- Instanzschlüssel: Geltungsbereich + Instanz-ID + Startgeneration.
- Auftrag: monotone Reihenfolge innerhalb der Instanz, Operation,
  vollständige Parameter, Gültigkeitsinformation.
- Übergabe: ein lokaler Zeitstempel und vollständiger Eingangsschnappschuss
  an einer Rendergrenze; gleiche Übernahmezeit für dasselbe Eingangspaket.

| Operation | Vertragsanforderung |
| --- | --- |
| Start | Nur vollständig validiert und innerhalb deklarierter Kapazität annehmen. Keine andere Instanz still ersetzen. Initialisierung und Startkonflikt je Effekt festlegen. |
| Update | Nur passende aktive Generation; Wertkopie atomar. Ungültige Beobachtung ist ein definierter fachlicher Eingang, fehlerhafte Parameter sind Ablehnung ohne Teilwirkung. |
| Stop(instanz) | Ausschließlich diesen Beitrag beenden, sofort oder über definierte Ausblendung. Andere Beiträge laufen unverändert weiter. |
| Stop(scope) | Nur Instanzen dieses Besitzers/Geltungsbereichs beenden; Umfang nennen. |
| Gesamt-Aus/Reset | Alle Beiträge entfernen und Schwarz ausgeben; alte Generationen sperren. Erst nach erfolgreicher Schwarzübertragung als ausgegeben bestätigen. |
| Natürlicher Abschluss | Beitrag entfernen, Hintergrund neu zusammensetzen; kein implizites Gesamt-Schwarz. |
| Neustart | Bewusste neue Generation; keine aus einem Update, Verbindung oder Zustandseintritt abgeleitete Wiederbelebung. |

Wiederholung eines noch im Wiederholungsspeicher enthaltenen Auftrags liefert
dessen Resultat ohne neue Wirkung. Außerhalb dieses deklarierten Fensters
ältere Reihenfolgen sichtbar als veraltet abweisen, niemals neu ausführen.
Ältere Generationen dürfen keinen neuen Lauf treffen.
Gleiche Sequenz mit anderem Inhalt ist ein Konflikt. Abgewiesener Start
ändert insbesondere weder Farbe noch Phase (Gegenbeispiel: Original-Burst).

Start/Update/Stop werden in lokaler Reihenfolge vor Berechnung übernommen.
Der Folgeauftrag im selben Paket sieht den zuvor vollständig übernommenen
Zustand. Konkrete Queuegröße, Speicherung der Wiederholungsantworten,
Überlaststrategie und Zahl paralleler Instanzen sind Implementierungsentscheidungen.
Die Schnittstelle muss volle Kapazität sichtbar ablehnen. Stop darf nicht
unbemerkt verloren gehen: reservierter Pfad oder sichtbare Ablehnung samt
zuständigem Wiederholer sind vor Implementierung festzulegen.

Zustandseintritt, Pause, Reset und Profilwechsel definieren ausdrücklich,
welche Instanzen weiterlaufen. Ein Profilwechsel darf keine Instanz mit alten
Profilverweisen zurücklassen. Kein stilles Übernehmen von Palette, Zielwert,
Timer oder Phase; bewusste Übernahme als Teil des Entwurfs benennen.

## Rückmeldungen ohne falsche Wirkungsbehauptung

1. **Angenommen:** validiert/eingereiht; noch kein Framebeleg.
2. **Angewandt:** LED-Besitzer hat die Parameter/Stop-Anforderung übernommen.
   Das kurze Verarbeitungsticket kann abgeschlossen werden. Lang lebende
   Instanzen besitzen getrennte Zustände und bleiben für Profilbarrieren sichtbar.
3. **Aktiv / ausblendend / beendet:** berechneter Instanzlebenszyklus.
4. **Frame übertragen:** Ausgaberevision und enthaltene Instanzgenerationen
   zuordnen. Ein verspäteter alter Transfer bestätigt keine neue Revision.
5. **Abgewiesen / Ausgabefehler:** Grund und betroffene Identität erhalten.
   Bei Fehler keinen erfolgreichen Schwarz-/Stopnachweis erfinden.
6. **Optisch beobachtet:** gesonderter Gerätebeleg, niemals aus 1–4 ableiten.

Ein Beitrag kann trotz erfolgreicher Übertragung durch Sättigung/Überdeckung
unsichtbar sein. Auch eine saubere Rückmeldung beweist seine Erkennbarkeit nicht.

## Mischvertrag je Entwurf

Jeder Effekt nennt Maske, Beitrag und Mischoperator. Außerhalb der Maske
trägt er null bei. Vor jedem Frame wird die Basis neu hergestellt; kein
Restpuffer und kein Löschen fremder Beiträge durch einen ausgelaufenen Effekt.

Mindestens zu dokumentieren:

- Kanal-/Farbraum und Wertebereich **vor und nach** globaler Skalierung;
- Grundschicht und feste Reihenfolge, insbesondere für Ersetzen oder Alpha;
- ADD: breite Addition, dann deklarierte Sättigung; MAX: Kanalmaximum;
  Alpha: definierte Deckung, Reihenfolge und Rundung, kein impliziter Standard;
- Helligkeits-/Leistungsgrenze zuletzt, nur ein Ausgabebesitzer;
- Maskenüberlappung, Übersteuerung, Stop eines Beitrags und Wiederfreigabe
  zuvor verdeckter Beiträge an konkreten Zahlen.

Die [Näheglut-Probe](naeheglut-design.md) verwendet ausschließlich additive
Beiträge im begrenzten Kanalcoderaum 0–32. Das ist ein ausdrücklicher
Entwurfsvorschlag für die Probe, keine originalgetreue FastLED-Ausgabe und
keine beschlossene universelle Produktfarbpipeline.

## Vor einer Produktumsetzung offen

Firmware-Maintainer: Zieladapter, Instanzkapazität, Queue-/Stopzustellung,
Profilbarrieren, Rückmeldespeicher und Ressourcenbudget bestimmen.
Produktverantwortlicher: Verhalten bei Spielpause/Phasenwechsel sowie Priorität
konkurrierender Lichtaussagen für die konkrete Verwendung bestätigen.
Protokoll-/App-Verantwortlicher: nur bei Bedarf separate V2-Nachfolgeänderung.
Kein Teil dieses Dokuments ersetzt diese Arbeiten oder beauftragt Geräteuploads.
