# Nutzergeführte Hardwarediagnose v1 — gemeinsamer Vertrag

Stand: 10. Oktober 2026. Status: **ausführbarer Vertragsentwurf mit Fake-Gerät**.
Produktbasis: Firmware F0 `e973818`; Android A0–A2 `3c9992a` besitzt eine getrennte
Git-Abstammung. Dieses Paket ergänzt keinen GATT-Endpunkt und keine reale
Diagnoseausführung. Es enthält keine Freigabe für Upload oder Hardwareabnahme.

## Umfang und Daten

[Schema](diagnostic-v1.schema.json), [Golden-Fixtures](fixtures/diagnostic-v1/README.md)
und [ausführbares Referenzmodell](../../scripts/diagnostic_contract_model.py) teilen
zwei Operationen. Unbekannte Versionen, Operationen und zusätzliche Felder werden
abgewiesen. JSON ist ein logischer Dokumentvertrag, noch keine BLE-Rahmenbelegung.

| Operation | Anforderung | Gerätebeleg | unabhängige Nutzerbeobachtung |
| --- | --- | --- | --- |
| `wake_capture` | logischer Kanal 1, bis 10 s Erfassungsfrist | alle erfassten Flanken, auch fremde Kanäle 1–4; `rawHigh`, nach deklarierter `activeLevel` abgeleitetes `active`, Bootzeit und Überlauf | einmal richtige Taste drücken/loslassen; richtige Zuordnung und keine Nebenauslösung |
| `led_single` | ausschließlich logischer Index 0, RGB `[16,0,0]`, 500 ms | Anzahl der Renderaufrufe, bestätigte Owner-Rückkehr zu aus, verfügbare Versorgungsspannung oder `null` | ausschließlich richtige LED rot und danach aus; Abweichung oder fehlende Beobachtung separat |

Die RGB-Werte sind ein begrenzter synthetischer Vertragsreiz, **kein geprüftes
elektrisches Sicherheitsbudget**. Pins, LED-Chiptyp und reale Kanalpolarität werden
nicht erfunden. `capabilities.hardwareProfileRevision` bindet später das geprüfte
Geräteprofil, `source` dessen Herkunft. `polaritySource=configured_unverified`
kennzeichnet konfigurierte, physisch unbestätigte Polarität. `active` ist keine
Behauptung eines realen Tastendrucks. Die Golden-Fixtures verwenden `synthetic`.
Nur Kanal 1 wird getestet; fremde Flanken werden zur Fehlererkennung erhalten.

Jede Anforderung/Antwort trägt `schemaVersion=1`, `runId` (neue UUID v4 pro Lauf),
`stepId` (streng steigend, 1..UINT32_MAX, nie wiederverwenden), öffentliche
`deviceId` (32 Upperhex wie vorhandener Identitätsvertrag), `bootId` (16 Upperhex),
`grantId` (UUID v4 der Gerätefreigabe) und `operation`. Gerätekennung und Boot sind
keine Authentisierungsbeweise. Integer werden ohne Exponent oder Bruch übertragen;
Millisekunden bleiben bis 2^53−1 exakt darstellbar. Gerätezeiten sind pro Boot
monoton erweiterte Millisekunden, kein rohes überlaufendes `millis()`.

`timeoutMs` beträgt 1000..10000. Der Geräteowner zählt ab Aufnahme; die App setzt
vor dem Senden ihre eigene absolute Frist. Warteschlangenzeit darf deren Frist
nicht verlängern. Ein Schritt muss vollständig in die verbleibende Freigabe passen.
`startedDeviceMs` und `endedDeviceMs` sind Gerätezeit; App-UTC wird separat erfasst.
Es gibt ohne gemessenen Offset keine behauptete Synchronität beider Uhren.

Für jede übertragbare Endantwort gilt `0 <= endedDeviceMs - startedDeviceMs <
timeoutMs`. Gleichheit mit dem Timeout ist bereits zu spät. Eine Endantwort
`led_single/completed` braucht zusätzlich mindestens 500 ms Dauer, zwei
Renderaufrufe und bestätigtes `restoredOff`. Abgebrochene oder fehlgeschlagene
LED-Schritte dürfen kürzer sein. Diese Grenzen entsprechen dem bestehenden
JSON-Semantikprüfer; ein intern erzeugtes Timeout-Ergebnis ist keine noch
fristgerechte Geräteantwort.

## Rechte, Freigabe und Lebenszyklus

Diagnosezugang braucht den aktuellen vertrauenswürdigen `IdentityAccess.route`-
Beleg einschließlich ursprünglicher Verbindung, Authority und ausdrücklicher
Spielleitungsrolle. Diese Metadaten stammen ausschließlich aus dem Adapter und
stehen niemals in Client-JSON. Eine Game-Lease oder ein App-Cache ist keine
Diagnosefreigabe. Der Firmwareadapter muss Zugangsprüfung und Geräteausführung
am serialisierten Owner gegen Entzug schützen.

Zusätzlich fordert der Nutzer in der App sichtbar eine Testfreigabe an; das Gerät
bestätigt eine neue `grantId` für genau Lauf, Boot und ursprüngliche Verbindung.
Maximal 60000 ms, keine automatische Verlängerung. Freigabe nur im ruhenden Spiel
und bei exklusivem Diagnosebesitz der beteiligten Komponenten. Ein laufendes Spiel
wird nicht automatisch gestoppt. Solange der Owner Testeingaben verarbeitet,
werden sie nicht an die Spiel-Engine weitergereicht; Startversuche sind gesperrt.
Die reale atomare Besitzübergabe und sichtbare Geräte-/App-Testanzeige fehlen noch.

Disconnect, neue Verbindungsgeneration, Bootwechsel, Rechteentzug, Ablauf oder
ausdrücklicher Abbruch beenden die Freigabe. Der Geräteowner schaltet seinen
LED-Testreiz spätestens nach 500 ms unabhängig von App und Antworttransport aus;
bei Ende der Freigabe sofort. Der Fake modelliert dies mit `tick`/`retire`; die
Produktintegration muss garantierte Timer-/Ownerbedienung und Fehlerbehandlung
belegen. Fehlgeschlagene Aus-Bestätigung ist `failed/owner_error`; verlorene
Bestätigung lässt den physischen Zustand offen. LED-Zustand wird nie aus einem
erfolgreichen Sendeaufruf abgeleitet. Die Freigabe ist flüchtig und überlebt keinen
Neustart. Abschluss widerruft sie; auf dem Gerät verbleibende Frist ist begrenzt.

Nur ein Schritt gleichzeitig. Annahme verbraucht seine `stepId`, auch bei verlorenem
Ergebnis. Doppelte Anforderungen führen niemals zu erneuter Ausgabe. Ein höherer
Schritt wird nur nach ausdrücklicher neuer Nutzeraktion gestartet. Es gibt in v1
keine implizite Wiederholung und keinen Resume-Endpunkt. Neue Freigabe verlangt
einen neuen Lauf. Ein bewusst späterer Retry hat neue Lauf-/Schrittkennung und
ersetzt den alten unklaren Befund nicht. Queued/Write-success sind keine
`diagnostic_reply`; nur der tatsächliche Owner erzeugt die korrelierte Endantwort.

## Bewertung, Unterbrechung und Speicherung

Die App persistiert den Auftrag mit `pending` **vor** dem Senden atomar. Beim
Wiederöffnen wird ein noch offener Auftrag `app_restarted`; nichts wird gesendet.
Ein verlorener Link ergibt `disconnected`, Gerätebootwechsel `device_restarted`,
lokaler Fristablauf `timeout`, Nutzerabbruch `cancelled`. Diese Zustände behalten
`deviceReply=null` und Urteil `unclear`: Ausführung kann bereits erfolgt sein.
Ein sicher nie gesendeter übersprungener Schritt ist `not_tested`.

Antworten werden vor Verwendung gegen Schema, ursprüngliche Transportbindung,
alle fünf Korrelationsfelder, Operation und Zeiten geprüft. Fremde, doppelte oder
nach terminalem Zustand eingetroffene Antworten dürfen weder den nächsten Schritt
noch das Urteil ändern. Sie bleiben als begrenzte Rohbelege in `ignoredReplies`
(höchstens 16 pro Schritt); bei größerem Anfall muss der spätere Adapter den Lauf
beenden und seine vollständige Transportdiagnostik gesondert exportieren. Eine
mutmaßlich passende verspätete Antwort wird nicht automatisch zur Abnahme.

| Voraussetzung | Urteil |
| --- | --- |
| nie getestet | `not_tested` / nicht geprüft |
| Unterbrechung, keine Endantwort, abgewiesen/abgebrochen oder fehlende Beobachtung | `unclear` / unklar |
| Ownerfehler, falsche/fehlende/zusätzliche Wake-Flanken, Überlauf oder Nutzerabweichung | `failed` / fehlgeschlagen |
| korrelierte Endantwort `completed`, passender Gerätebeleg **und** passende Nutzerbeobachtung | `passed` / bestanden |

Ein Wake-Erfolg verlangt genau aktiv/inaktiv auf Kanal 1 in richtiger Zeitfolge
innerhalb des Schritts. Rohpegel und deklarierte Polarität müssen dazu passen.
Weitere Flanken führen zum Fehler. LED-Erfolg im Vertrag verlangt zwei
Renderaufrufe und `restoredOff=true`; dies beweist allein kein sichtbares Licht.
Nutzerbeobachtung enthält Quelle `user`, Soll, Ist, Zeitpunkt und Medien-IDs.
Jedes synthetische Urteil bleibt durch `evidence=synthetic` als Simulation erkennbar.

## Export v1

Ein abgeschlossener Export ist ein JSON-Umschlag `{report,sha256}`. Er enthält alle
Schritte mit Rohanforderungen/-antworten, Unterbrechungen, Urteilen, Beobachtungen,
App-Version, Box- und Empfänger-Firmwarestände, Fähigkeiten samt Herkunft,
Freigabe, Zeitbezug, Testmittel und Medienmanifest. Fehlende Empfängerversion ist
`null/source=unavailable`, kein erfundener gleicher Firmwarestand. Fehlende
Spannungsmessung bleibt `null` und ein fehlendes Messgerät sichtbar.

`sha256` ist SHA-256 über die kanonischen Bytes von **report**, nicht des Umschlags:
rekursiv lexikographisch sortierte ASCII-Schlüssel; kein Leerraum; Dezimalinteger;
`true/false/null`; Strings mit JSON-Escapes wie Python `ensure_ascii=True` (Unicode
als kleine `\uXXXX`, Nicht-BMP als Surrogatpaar; keine Unicode-Normalisierung).
Der Umschlag verwendet dasselbe Profil und abschließendes LF. Keine NaN/Infinity,
Floats, doppelten Schlüssel oder unpaarigen Surrogate. Das ist ein enges v1-Profil,
keine Behauptung, allgemeines JCS zu implementieren. Plattformimplementierungen
müssen dieselben Goldenbytes erzeugen. Prüfsummen dienen Integrität, nicht Signatur
oder Geräteauthentisierung. Nach Prüfung folgt Schema- **und** Semantikprüfung;
eine neue Prüfsumme legitimiert keine gefälschte Bewertung.

Medien liegen relativ unter `media/` und besitzen jeweils SHA-256; Import muss
ihre Bytes prüfen und fehlende Dateien melden. Dieser erste Schnitt prüft leere
Medienmanifeste, keinen Kamera-/Dateidialog. Export darf kein `pending` enthalten;
ein offener Schritt wird zuerst mit begründetem Unterbrechungszustand abgeschlossen.
Schema begrenzt 128 Schritte, 64 Flanken, 16 Medien und 16 Testmittel. Adapter müssen
zusätzlich Parser-/Nachrichten-/Speicherbudgets vor Allokation begrenzen. Keine
Schlüssel oder Sicherheitsbelege werden exportiert.

## Anschluss und verbleibende Tore

- Firmware: eigener Diagnoseowner neben `CompanionIntegration/GameControl`,
  bestätigte Besitzübergabe an I2C-/LED-Owner aus `DeviceRuntime/Plan`, echte
  originalgebundene Aufnahme/Antwort und Lebenszyklus-Fence. `GameCommand v2`
  akzeptiert diese Operationen weiterhin nicht. Kein neues Opcode, kein Umdeuten
  bestehender Frames. Framing, Capability-Abfrage, Freigabe-/Abbruchantwort und
  explizite Bytebudgets sind vor GATT-Anschluss zu implementieren und zu prüfen.
- Android: Vertragsparser/Reducer im `companion/android/core/protocol`, persistenter
  Diagnoseablauf über `data/device/DeviceRepository` und bestehende BLE-Plattform-
  Schicht. A0–A2 und A3-Hostkern belegen noch keine Produkt-GATT-/Schlüsselverwaltung.
  Kein Editor wurde erweitert. Atomare Journalpersistenz, Berechtigungen,
  Hintergrund-/Prozessende, Nutzerführung und Dateiexport sind noch zu implementieren.
- Hardware: GPIO15/WS2811 im Firmwarestand gegenüber G4/DATA in Entwurfsquellen ist
  ungeklärt; Wake-Kanal-/Polaritätszuordnung physisch offen. Ein echter Adapter
  benötigt geklärtes Profil und Hardwarezuständigkeit. Dieser Vertrag ändert keine
  Pins und übernimmt keine serielle Sonderdiagnose als Produktpfad.
- Abnahme: Fakefälle sind kein C++-Build, Androidtest, Flash-/Timingnachweis oder
  physischer Befund. Der nächste reale Lauf erfolgt erst nach geprüfter eigenständig
  bedienbarer App-/Firmwarediagnose durch den Nutzer; spätere Sitzung liest nur den
  abgeschlossenen Export. Die Originalbox-Abnahme bleibt unterbrochen.

## Reproduzierbarer Prüflauf

Python 3.11+ und `jsonschema==4.23.0` (bereits bestehende Schema-Prüfversion des
Projekts), dann `python scripts/check_diagnostic_contract.py`.
Die Tests prüfen das Schema, acht unveränderte Golden-Umschläge, beide simulierten
Operationen, Rechte-/Verbindungsgrenzen, Zeit-/Abbruchfälle, Doppel-/Spätantworten,
Rohpegel-/Fremdflanken, Nutzerurteile und verlustfreien Export/Import.
