# Näheglut live in Larp Dev

Status: Android-Korrekturkandidat 0.3.2/code5, 31.10.2026; Basis `deb9ab9`.
Gerätevertrag: [PROX-BLE/1](../led/prox-ble-v1.md). Native Build-, Screenshot-
und Gerätebelege werden separat geführt; dieser Brief behauptet keine Abnahme.

## Ziel und Zuständigkeit

Larp Dev 0.3.2 beobachtet zwei autonome S3-Stationen gleichzeitig und ändert ihre
Nähe-Helligkeitskennlinie bewusst während eines laufenden Versuchs. Die Station
bleibt Eigentümer ihrer Messung, Glutausgabe und dauerhaft gespeicherten Parameter.
Der Handyabbruch beendet ihre autonome Funktion nicht.

Android/UI: Entwickler dieses Pakets; Firmware/Protokoll: Plattformentwickler;
unabhängiger Review und echte Build-/Screenshot-/Geräteprüfung: separat durch
Projektmanager und Reviewer. [UI-Rollenvertrag](ROLLE.md) gilt.

## Oberfläche und Bedienweg

Ein zusätzlicher Bereich „Näheglut“ nutzt die vorhandenen Compose-/Material-
Komponenten. Zwei getrennte USB-exportierte Stationszugänge ordnen unterschiedliche
Stationen A/B zu. Die App verwahrt beide Zugänge in eigenen Keystore-geschützten
Slots; der vorhandene LED-Zugang und das ESP-NOW-Paargeheimnis bleiben getrennt.
„Verbinden“ sucht anhand öffentlicher Device-ID und prüft die tatsächliche Station
über deren eigene HMAC-Anmeldung. Bluetoothadresse und WiFi-STA-MAC sind verschieden.
Die bestehende ESP-Bedienung bleibt ein anderer Bereich. Kein Web-/Figma-
Neubau und keine neue Spielaktion gehören zu dieser Erweiterung.

Jede Stationskarte zeigt Stationsidentität und Partner, Handyverbindung,
Partnerfunkzustand, gefilterte Signalstärke in dBm und Glut-Ziel in Prozent.
Das Ziel, die gespeicherte Konfigurationsrevision und die zuletzt vom RMT
bestätigte RGB-Ausgabe samt Ausgaberevision sind getrennt. Die Konfigurationsrevision
ist kein Beleg, dass diese Kennlinie bereits im laufenden Ziel angewandt wurde.
Bei Funkverlust wird das Ziel null, während RGB noch
ausblendet. RGB-Vorschau und RMT-Bestätigung sind keine optische Helligkeitsmessung.
Farbe ergänzt lesbaren Text; sie ist kein alleiniger Beleg.
Beide Karten bleiben standardmäßig kompakt: Station, Handy-/Partnerzustand,
RSSI, Glut-Ziel und RGB-Vorschau stehen oben. „Einrichten“ beziehungsweise
„Details“ klappt MACs, Zugang, Revisionen und gelesene Kennlinie auf. Grundlage
dieser Korrektur ist der echte native Screen `nearglow-empty.png` vom 31.10.;
der erste Stand ließ die zweite Karte unnötig weit nach unten rutschen.
Signalstärke bezeichnet grobe Funknähe, keine Meterentfernung.

| Zustand | Sichtbare Rückmeldung | Erlaubte Bedienung |
| --- | --- | --- |
| Nicht zugeordnet | „Stationszugang importieren“ | Je eigenen Zugang importieren |
| Handy verbindet | „Handyverbindung wird aufgebaut …“ | Abbrechen |
| Gültige Telemetrie, Partner frisch | Aktuelle dBm, Ziel, RMT-RGB und Revisionen | Kennlinie bearbeiten/senden |
| Handy verbunden, Partner ungültig | „Partnerfunk fehlt“; keine aktuelle RSSI-Zahl | Kennlinie bearbeiten; getrennte Gerätebestätigung |
| Telemetrie überholt | „Keine aktuellen Daten“; keine aktuellen Messzahlen | Entwurf bearbeiten, erneut verbinden |
| Handy getrennt | „Handy getrennt · Station arbeitet autonom weiter“ | Erneut verbinden; kein automatisches Senden |
| Falsche Identität/Version | Datenprüfung fehlgeschlagen; Werte unbekannt | Zugang prüfen, neu verbinden; Schreiben gesperrt |

Das Empfangsalter läuft auf einer lokalen monotonen Uhr. Ein wiederholtes altes
Paket frischt Werte nicht auf. Der erste empfangene Nachrichtenfragment-Zeitpunkt
zählt bereits mit, sodass bekannte Assemblierungs- und App-Wartezeit keine alte
Quelle frisch erscheinen lässt. Das beweist keine synchronisierte Uhr oder
genaue Einweglatenz. Geräte-Neustart und Handyverbindungsgeneration
entwerten vorherige Messwerte. Gerätefrische des Partnerfunks und Alter der
Handytelemetrie werden getrennt bewertet. Die Anzeige gilt unter 2500 ms als
aktuell; Partnerfrische gilt zusätzlich nur bei fortgeschriebenem Quellenalter
unter 3000 ms. Ein Verwaltungsauftrag benötigt Telemetrie höchstens 1000 ms alt
und erhält 2000 ms gerätelokale Annahmefrist. Diese Fristen sind keine Metergrenzen.

## Kennlinie und Bestätigungen

Vier editierbare Stützpunkte ordnen aufsteigende RSSI-Werte von −127 bis −1 dBm
monoton nicht fallenden Helligkeiten von 0 bis 100 Prozent zu. Default ist
(−90,0), (−75,30), (−60,60), (−40,100). Ungültige Reihenfolge, doppelte
dBm-Werte oder sinkende Helligkeit sperren das Senden mit konkretem Feldhinweis.
Eine einfache Kurvenvorschau ergänzt die numerischen Felder.

Der Entwurf ist vom bestätigten Gerätestand getrennt. „Auf beide Stationen
übernehmen“ startet zwei getrennte Änderungsaufträge gegen den jeweils frisch
bestätigten Stand. Die App zeigt je Station
„Entwurf“, „Gesendet · Bestätigung ausstehend“, „Dauerhaft bestätigt · Revision …“
oder „Ausgang unklar“. Eine Bestätigung enthält die tatsächlich gespeicherte
Revision und Kennlinie; eine bloße erfolgreiche Bluetooth-Schreiboperation
genügt nicht. Bei unklarem Ausgang liest die App den Gerätestand erneut, ohne
automatisch den Änderungsauftrag zu wiederholen. Unterschiedliche Revisionen
bleiben sichtbar; es gibt keine atomare Änderung beider Stationen.
Speicherfehler bleibt „Ausgang unklar“ und sperrt weitere Übernahme bis zum
erneut gelesenen Stand. „Gespeichert, Anwendung fehlgeschlagen“ ist ein eigener
Fall: Persistenz und Runtime-Anwendung werden nicht zu einer Erfolgsbehauptung
zusammengezogen. Eine verspätete Bestätigung für einen vorherigen Entwurf
bestätigt niemals einen inzwischen bearbeiteten neuen Entwurf.

## Abnahme

1. Zwei reale Stationen getrennt identifizieren; Richtungsasymmetrie sichtbar
   lassen, keine Messwerte zwischen Karten spiegeln.
2. Stationen annähern/entfernen: frische dBm, Glut/RGB und Partnerverlust je
   Station beobachten; Handyunterbrechung davon unterscheiden.
3. „Auf beide übernehmen“ auslösen und beide dauerhaften Bestätigungen getrennt
   prüfen; bei Teilfehler die erfolgreiche Station nicht zurücksetzen.
4. Während einer Änderung Handyverbindung unterbrechen: Ausgang unklar anzeigen,
   nach Rückkehr gespeicherten Stand lesen, keinen Schreibauftrag wiederholen.
5. Station neu starten: persistente Revision/Kennlinie wiederfinden, alte
   Messung und ausstehende Bestätigung nicht als aktuell darstellen.
6. Screenshotprüfung mit beiden Karten, fehlendem Partnerfunk, überholten Daten,
   ungültigem Entwurf und Teilbestätigung; große Schrift und beschriftete
   Touchflächen separat prüfen. Screenshots/Build/physische Wirkung getrennt belegen.
