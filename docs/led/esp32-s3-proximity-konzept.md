# Konzept für ESP32-S3 Spielstationen mit Proximity Effekten

Stand: **31. Oktober 2026**. **Entwurf zur Produktentscheidung**, keine
Implementierungs- oder Gerätefreigabe. Zwei autonome ESP32-S3-Stationen sollen
auf die Begegnung mit ihrer konfigurierten Partnerstation reagieren und nach
einigen Sekunden grober Funknähe auch Rätsel oder Spielaktionen auslösen.
Der erste Ausbau verbindet diese Begegnungsprüfung mit **Näheglut**:
Annähern verstärkt die warme Glut, Entfernen schwächt sie, fehlende frische
Beobachtungen lassen ihren Beitrag weich erlöschen.

**Bestätigt am 31.10.2026:** Zwei Stationen oder Requisiten werden zueinander
bewegt; bestätigte Nähe darf auch Rätsel oder Spielaktionen auslösen; einige
Sekunden dicht beieinander reichen, grobe Funknähe ist akzeptabel. Präzise
Distanz- oder Andocksensoren sind daher keine Voraussetzung dieses ersten Ausbaus.
Die bisherige Näheglut-Spielbedeutung und ihre Start-/Stopregeln bleiben erhalten.

## Spielerfahrung und erster Umfang

Zwei vorbereitete Requisiten gehören im Spiel zusammen. Nach bewusstem
Spielstart sind ihre Bindungen freigegeben. Erkennt A erstmals die berechtigte,
korrespondierende Station B, beginnt A genau einen durchlaufenden Näheglut-Lauf.
B entscheidet unabhängig über seinen eigenen Lauf. Weitere passende Meldungen
verändern die jeweilige Glut, ohne den Effekt oder seine Rampe neu zu starten.

Zusätzlich prüft jede Station, ob passende Nahproben über die vorgesehene Zeit
anhalten. Erst dieser bestätigte Funkkontakt liefert ein eigenes Spielereignis.
Eine Paketregel kann daraus beispielsweise eine vorbereitete Freigabe ableiten;
der konkrete erste Rätselauslöser ist noch zu entscheiden.

Die Spieler benötigen dafür kein Handy. Die Spielleitung richtet den Aufbau
vorher ein und kann ihn später gezielt bedienen. Router, Internet und Zentrale
sind für das vorbereitete lokale Spiel entbehrlich. Das entspricht
[VA-01](../veranstaltungsarchitektur.md). Beide Geräte können verschiedene
Helligkeiten zeigen: A beobachtet B → A, B beobachtet A → B. Ausrichtung, Körper
und Hindernisse wirken auf beide Richtungen unterschiedlich.

Die Näheanzeige und die einmalige Spielauslösung sind getrennte Bausteine.
Das umfassendere [SP-01-Ziel](../spielpakete-architektur.md) verlangt weiterhin
nachladbare Regeln und echte Spielwechselwirkung zwischen mehreren Boxen;
ein bestandener Lichtversuch schließt dieses Produktziel nicht ab.

## Verbindliche Grundlagen und heutiger Stand

Der [Näheglut-Spielanschluss](naeheglut-spielanschluss.md) bestätigt die Erkennung
einer korrespondierenden Station als Starttrigger. Wiederholte Erkennung ist
kein Neustart. Ein ausdrücklicher Stop bleibt wirksam, bis das Spiel die Bindung
bewusst neu freigibt. Ungültigwerden und Wiederkehr betreffen einen bereits
aktiven Lauf. Diese Festlegungen werden hier übernommen.

Der gemeinsame [NearGlow-Kern](../../firmware/led/LIVE-NEARGLOW.md) und der
[bestehende Entwurf](naeheglut-design.md) sind die fachliche und rechnerische
Basis. Warmes Gold und etwa eine Sekunde weicher Übergang sind bestätigt;
RGB `(24,8,0)`, Gain 1000 und Smoothstep über 1000 ms bleiben Arbeitswerte.
Die historische [Funkprobe](../espnow/naeheglut-result-2026-10-18.md) verwendete
andere ESP32-Geräte und Hoststeuerung. Sie ist keine autonome Zwei-S3-Abnahme.

[PR #115](https://github.com/Sven401/Larp-Escape/pull/115), noch nicht integriert, bereitet das Waveshare
ESP32-S3-Zero vor. Sein
[Gerätenachweis bei c4cbe28](https://github.com/Sven401/Larp-Escape/blob/c4cbe28fcf95057841f8f61d5abead2d7019d2c3/docs/led/s3-installation-2026-10-31.md)
belegt **einen** realen S3 mit manueller Handy-Bedienung von Funkeln,
Lichtimpuls und Ripple. Die einzelne RGB-LED an GPIO21 ist eine Vorschau;
Streifenwirkung und Näheglut sind damit nicht abgenommen. Das später an COM5
identifizierte ATmega32U4-Gerät zählt nicht als zweite S3-Station.

## Stationen und Zuständigkeiten

Jeder S3 bleibt Eigentümer seines lokalen Spielzustands. Eine Station sendet
Beobachtungen oder Ereignisse; sie greift nicht unmittelbar in den LED-Treiber
oder die Spiellogik einer anderen Station ein.

| Bestandteil | Verantwortung |
| --- | --- |
| Quellenadapter | Peerbindung, geprüfte Herkunft, Reihenfolge, Empfangsalter und gültiger Nähewert |
| Lokaler Spielbesitzer | Korrespondenz erkennen, freigegebene Instanz starten, Spielregeln und Stop entscheiden |
| Effektadapter | Kopierte Start-/Update-/Stopaufträge, Instanzgeneration, begrenzte Slots und bestätigte Ergebnisse |
| Ein LED-Besitzer | NearGlow-Beiträge berechnen und über genau einen Ausgabetreiber ausgeben |
| Spielleiter-Handy | Berechtigte Einrichtung, Vorbereitung, Status und bewusste Bedienung je Station |

Der Datenweg lautet:

```text
Funkbeobachtung → Quellenprüfung und Frische → lokaler Spielbesitzer
               → gebundene Effektinstanz → ein LED-Besitzer → Ausgabe
```

Die App ist keine dauernde Laufzeitsteuerung des Spieleffekts. Ein BLE-Abbruch
stoppt den autonomen Spielbeitrag nicht pauschal. Der heutige manuelle Testpfad
hat dagegen einen Stop bei BLE-Abbruch; er bleibt ein eigener Besitzer. Ein
neues Spiel darf nicht einfach diesen Schutz abschalten und damit Autonomie
behaupten. Besitzer-, Spiel- und Instanzgeneration müssen getrennt sein.

## Näheglut und mögliche weitere Effekte

| Effekt | Sichtbare Aussage und Umfang |
| --- | --- |
| Näheglut als erster Ausbau | Alle zugeordneten LEDs glühen gemeinsam warm; mehr bewertete Nähe verstärkt den Beitrag, weniger Nähe schwächt ihn. Kein selbst erfundenes Atmen oder Pulsieren. |
| Bestätigter Funkkontakt als Regelbaustein | Eine freigegebene Paketregel nutzt die länger bestätigte Begegnung für Rätsel oder Spielaktion. Die konkrete Regel und ihre Wiederbewaffnung werden konfiguriert. |
| Begegnungsimpuls als Gestaltungsvorschlag | Ein kurzer Akzent kann die angenommene Begegnung anzeigen. Diese zusätzliche Optik ist noch keine bestätigte Produktentscheidung; nicht bei jeder Funkprobe auslösen. |

Neue Effekte sollen keine bereits ausgeschlossenen Informationen preisgeben.
Die [bestehenden Gestaltungsgrenzen](spielinformation-2026-10-18.md) für
atmosphärischen Kristallfortschritt und allgemeine Störung bleiben erhalten.

| Ereignis | Verhalten des Näheglut-Beitrags |
| --- | --- |
| Boot oder Spielreset | Alte Instanzen und alte Messwerte nicht als aktiv oder frisch wiederherstellen; bewusster Spielstart gibt Bindungen frei. |
| Passende Station bei freigegebener Bindung erkannt | Genau einen neuen Lauf beginnen; kein Start allein durch eine starke fremde Funkquelle. |
| Neuer gültiger Wert | Den aktuellen Lauf weich zum neuen Ziel führen; identische Ziele starten die Rampe nicht erneut. |
| Quelle veraltet oder fällt aus | `valid=false`; aktiver Beitrag blendet nach null, Bindung und Lauf bleiben unterscheidbar erhalten. |
| Frische passende Quelle kehrt zurück | Derselbe aktive Lauf erholt sich weich; ein ausdrücklich gestoppter Lauf bleibt gestoppt. |
| Gezielter Stop | Weitere Updates dieser Generation sperren, Beitrag ausblenden, Abschluss und Ausgabe bestätigen. |
| Erkennung nach Stop | Keine neue Wirkung bis zur bewussten Freigabe und einem neuen Start mit neuer Generation. |
| Pause oder Fortsetzen | Eigene noch offene Produktentscheidung; kein impliziter Stop durch beliebigen Phasenwechsel. |

Ein übergeordnetes Gesamt-Aus entfernt alle Beiträge und verlangt eine frische
bestätigte Schwarzausgabe. Ein berechneter Nullwert, eine leere Queue oder eine
angenommene Stopmeldung beweist noch kein physisches Aus.

## Funknähe und Frische

RSSI beschreibt Empfangsstärke und liefert einen qualitativen Näheeindruck.
Es ist keine zugesagte Entfernung in Metern und erkennt keine Person ohne
mitgeführtes Funkgerät. Der vorhandene
[Gehversuch](../espnow/distance-camera-result-2026-10-18.md) begründet den
Messversuch, ersetzt aber keine Kalibrierung der S3-Geräte im späteren Gehäuse.

Für den ersten Zwei-S3-Versuch werden vorhandene Arbeitswerte wiederverwendet:
eine Probe etwa alle 500 ms, Q8-Filter mit 25 % neuem Anteil und
`p = clamp(20 × (gefilterter RSSI + 90), 0, 1000)`. Nur neue, gültige und der
gebundenen Quelle zugeordnete Antworten erneuern das lokale Empfangsalter.
Bei Alter **ab 3000 ms** wird der Eingang ungültig. Diese Werte sind
Messkampagnen-Ausgangspunkte, keine S3-Reichweiten- oder Reaktionszusage.

`p=0` und `valid=false` bleiben fachlich verschieden. Beide können einen dunklen
Beitrag ergeben; bei unbekannter Quelle darf das Spiel keine bestätigte Ferne
oder Rätsellösung ableiten. Der Effekt kennt nur den bewerteten Wert und seine
Gültigkeit. Frische wird auch ohne neue Funkmeldung lokal geprüft.

Der bestehende RSSI-Funkeltest fällt bei Ausfall auf 20 % Intensität zurück.
Näheglut blendet seinen Beitrag dagegen nach null aus. Die unterschiedlichen
Verträge dürfen nicht durch einen gemeinsamen pauschalen Fallback ersetzt werden.
Eine Schaltschwelle für Begegnungen benötigt zusätzlich Hysterese und eine
Bestätigungsdauer oder Probenzahl; konkrete Werte folgen erst aus Messungen.

## Bestätigter Funkkontakt und Spielauslösung

Der erste Ausbau bindet genau zwei korrespondierende Stationen vorab. Eine
neue Nahprobe braucht eine zeitlich passende authentisierte Antwort auf eine
lokale Einmal-Challenge im richtigen Veranstaltungs-, Durchlauf-, Paket- und
Bindungskontext. Alte, doppelte oder fremde Antworten bestätigen kein Verweilen.
Eine starke einzelne Probe und ein anschließend ablaufender Timer reichen nicht.

**Vorschlag für den ersten Messversuch:** drei Sekunden Verweilen mit mindestens
vier neuen Nahproben und höchstens einer Sekunde Lücke zwischen diesen Proben.
Der letzte Nachweis muss eine neue gültige Nahprobe sein. Eintritts- und
Austrittsschwelle werden getrennt im späteren Gehäuse ermittelt; die Fernschwelle
liegt unter der Nahschwelle. Das sind Kalibrierungswerte, keine Metergrenzen.
Das Verhalten im Zwischenbereich der Hysterese ist vor der Umsetzung als
Mess-/Zustandsregel festzulegen; die letzte Nahprobe darf dort nicht ungeprüft weiterzählen.

| Kontaktzustand | Spielseitige Folge |
| --- | --- |
| Freigegeben, passende Nahprobe | Verweilprüfung beginnen. |
| Frische zusammenhängende Nahproben | Dauer und Probenzahl prüfen; bei erfüllten Bedingungen einmaliges Encounterereignis an die lokale Paketregel liefern. |
| Kontakt verbraucht, weitere Nahproben | Keine weitere gleiche Spielaktion; laufende Glut darf weiter modulieren. |
| Ungültig oder Lücke während Verweilprüfung | Prüfung verwerfen; keinen Kontakt aus alten Werten nachträglich bestätigen. |
| Frisch bestätigte Fernprobe während Verweilprüfung | Prüfung verwerfen; unterbrochene Nähe nicht über mehrere Annäherungen aufsummieren. |
| Funkverlust nach ausgelöstem Kontakt | Verbrauchten Kontakt gesperrt halten; unbekannt ist nicht bestätigt fern. |
| Frisch und ausreichend lange bestätigt fern | Wiederbewaffnung nur nach der konfigurierten Spielregel; alternativ bewusste lokale Spielleitungsfreigabe. |

Wiederbewaffnung des Kontakts hebt den ausdrücklichen Näheglut-Stop nicht auf.
Funkwiederkehr, Peer-Neustart oder neue BLE-Verbindung bewaffnen weder Kontakt
noch Effekt still erneut. A kann früher als B entscheiden; eine gemeinsame
gleichzeitige oder atomare Zweigeräteaktion wird nicht zugesagt.

Der Produktadapter muss Kontaktepisode, verbrauchte Spielaktion, Wiederbewaffnung
und stabile Vorgangskennung im bestehenden D1/P0-Speichervertrag korrelieren.
Die lokale Annahme und der vollständige Folgeplan gehören vor einer Hardwarefolge
in den dauerhaften Beleg. Nach Crash keine alten Messwerte als frisch werten oder
Spielaktion blind wiederholen. Unklarer Commit oder Hardwareausgang bleibt ein
Wiederherstellungsfall; logische Deduplizierung ist keine physische Exactly-once-Zusage.

## Peerbindung und mehrere Stationen

Eine MAC-Adresse ist eine Funkroute, keine logische Station und kein
Berechtigungsnachweis. Die Vorbereitung bindet physische Box, logische Station,
Veranstaltung, Spieldurchlauf und passende Quellen aneinander. Ein Ersatzgerät
übernimmt diese Bindung nicht automatisch. Unterschiedliche Rollen bleiben
Konfiguration derselben Stationsplattform, keine getrennten Sender-/Empfängerprodukte.

Die heutige LENT-Probe mit Tokenkorrelation ist kein produktives
Authentisierungsprotokoll. Empfang, berechtigter Kontakt und angenommene
Spielaktion sind drei getrennte Nachweise. Der Produktanschluss muss die
bestehenden [D0-Verträge](../veranstaltung/d0-vertrag-v1.md) für Identität,
Herkunft und Wiederholungssperre nutzen. Der dortige Beobachtungstyp enthält
noch keinen skalierten Proximitywert; dessen Fähigkeit, Version und Adaptergrenze
müssen ausdrücklich festgelegt werden. Keine still ergänzten Felder oder
pauschalen neuen Veranstaltungsschlüssel.

Eine frische authentisierte Antwort beweist Kontakt mit einem berechtigten
Endpunkt, keine Entfernung oder Personenidentität. Auch Linkverschlüsselung
macht RSSI nicht zu einer gegen Weiterleitung abgesicherten Distanzmessung.
Spielentscheidungen mit präzisen Ortsgrenzen brauchen zusätzlich qualifizierte
Sensorik oder eine bewusste bestätigende Handlung.

Für weitere Stationen wird ein vorbereiteter Nachbarschaftsgraph vorgeschlagen:
Nur konfigurierte Beziehungen werden beobachtet. Kein unbeschränktes
Alle-gegen-alle-Polling, kein stiller Wechsel zum stärksten Sender. Kanal,
Sendezeiten, Peerzahl, Queues und Ausgaben benötigen messbare Budgets.
A und B behalten eigene Frische und eigene Ergebnisse; gegenseitig bestätigter
Kontakt wäre eine eigene Regel mit zwei zeitlich passenden Beobachtungen.

Der Prototyp besitzt zwei NearGlow-Slots. Das ist keine Zusage von zwei Effekten
für beliebig viele Nachbarn und keine Ableitung aus dem P0-Stationsbudget.
Nicht darstellbare Bindungen beim Vorbereiten ablehnen; spätere Aggregation als
eigene Spielregel spezifizieren. Bei Überlast Beobachtung sichtbar ungültig oder
zurückgestellt behandeln, statt alte Werte als frisch zu verwenden.

## Hardware und Umsetzung

Der erste Aufbau benötigt **zwei tatsächlich identifizierte und gesicherte
ESP32-S3-Zero** mit eigener Versorgung und festgelegter Peerbindung. Ihre
Board-LEDs genügen für die Funktionsprobe. Ein späterer LED-Streifen braucht
einen eigenen Pin-, Pegel-, Versorgungs- und Helligkeitsvertrag sowie optische
Abnahme. Das tatsächliche Gehäuse und die Antennenlage gehören in die Funkprüfung.

NearGlow.cpp bleibt der gemeinsame Rechenkern. Der Produktadapter verwendet
Einzelbeiträge und mischt sie genau einmal; keine zweite Effektkopie und kein
zweiter RMT-/FastLED-Besitzer. Der Arduino-Testpfad bleibt ein begrenzter
Hardwarebeleg. Der Produktanschluss folgt der vorhandenen
[nativen ESP-IDF-Migration](../migration/esp-idf-qualification-2026-10-28.md),
ohne eine konkurrierende dauerhafte Plattform festzulegen.

| Paket | Kleiner überprüfbarer Abschluss |
| --- | --- |
| P1 Zwei S3 bereitstellen | Beide Chips, Sicherungen und Firmwarezuordnung belegen; lokale Start-/Stop-Bedienung je Board prüfen. |
| P2 Quelle und Funkkontakt | In beiden Richtungen passende, fremde, alte, doppelte und ausgefallene Antworten unterscheiden; Frische-, Verweil- und Lückengrenzen sowie Hysterese prüfen. |
| P3 Autonomer Näheglut-Lauf | Korrespondierende Station startet genau einmal; Nah–Fern–Nah moduliert, Quellenverlust blendet aus, Wiederkehr erholt denselben Lauf, Stop sperrt Wiedererkennung. Handy darf danach fehlen. |
| P4 Paket- und Besitzeranschluss | Konkrete Begegnungsregel, neue Fähigkeit/Version, dauerhafte Kontaktannahme und Folgeplan; Deduplizierung, Funkverlust ohne Re-Arm, bestätigtes Fern, Stop-Latch und Crash vor/nach Annahme prüfen. Ein LED-Besitzer und bestätigte Schwarzausgabe bleiben Pflicht. |
| P5 Reales Begegnungsspiel | Beide Geräte im Gehäuse, wechselnde Ausrichtung/Körperabschattung, gleichzeitige Bedienung und Funklast; sichtbare Wirkung, Fehlstarts, Frische und Laufzeit getrennt protokollieren. |

P2 und P3 können mit synthetischen Quellen vorbereitet werden. Die reale
Zwei-S3-Abnahme verlangt anschließend genau dieses Geräte- und Firmwarepaar.
Der beauftragte Spielaktionsausbau benötigt konkrete Paketregeln und SP-01-Mehrbox-/Drittspielnachweise.

## Noch zu entscheidende Produktfragen

1. Welche konkrete erste Paketregel verarbeitet den bestätigten Kontakt:
   beispielsweise eine Freigabe oder ein bestätigter gemeinsamer Rätselschritt?
2. Was bedeutet Pause für die laufende Glut, und wie wird bewusst fortgesetzt?

Nähebereiche, genaue Verweilparameter und Erscheinungsbild werden im Geräteversuch
qualifiziert. Die bestätigte grobe Funknähe, die Erlaubnis für Spielaktionen und
die bestehende Start-/Stop-Semantik stehen nicht erneut zur Entscheidung.

## Prüfstand dieses Entwurfs

Grundlage: Integrationscommit `ec1c52084e286892ada6540159e4c8036a907d04`,
Nutzerauftrag vom 31.10.2026 und die oben getrennt verlinkten S3-Belege aus
PR #115. Konzeptautor: Game Design mit LED-Effekt-Mandat; Plattform und
Geräteintegration prüfen die technischen Grenzen, Qualität die Quellen und
Abnahmebehauptungen. Prüfweg: Quellenabgleich und Dokumentprüfung.
Keine Implementierung, Uploads oder neuen Funk-/Lichtversuche in diesem Auftrag.
