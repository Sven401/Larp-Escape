# Companion: offene Abhängigkeiten

Stand: 2026-10-10. Android ist die Projekt-App; Vite bleibt befristete Referenz.
Produktbasis: main / 29da937c4309beddfd45c5dd0fbf84fdf853d8e4 plus lokale
companion-/experiments-Quellen. Firmwarebasis: codex/firmware-peer-ingress /
7f2a3e44223cc045aea46f78b3c124ede615573c plus lokaler Peer-/SDK-Diff.
HEAD allein beschreibt die jeweiligen unversionierten Quellen nicht.

## Vorhandener, begrenzter Nachweis

Android 0.1 implementiert echten BLE-Scan, GATT-Erkennung, lesenden
v2-Laborstatus, Trennen und Reconnect. Historische Handyabnahme: Moto G14,
Android14; synthetische Geräte, eingespeiste Autorität und RAM-Journal.
Sieben Kotlin/JVM-Fälle prüfen Framing, nicht das vollständige Statusschema.
Der Debug-Build erlaubt das Vertragslabor; Release verwendet es nicht.
Android benötigt weder JS-screenModel noch Vite als Laufzeitabhängigkeit.

Das separate v2-Vertragslabor hat auch Befehls-/Antwortmessungen mit dem
.NET-Android-Diagnoseclient. Dieser Nachweis gilt für diesen Client und das
Labor, nicht für Kotlin-Anmeldung oder Produktionssteuerung.

## Kanonischer Firmwarezugang und Spielleitung

- Firmware hat optionale Issue-/Challenge-/Proof-/Auth-Ergebnisvertikalen
  einschließlich ursprünglicher Verbindungsbindung und Rechte-ACK.
  Produktionsstart, Bond-Wiederverbindung, vertrauenswürdige Einrichtung,
  gegenseitiger Gerätenachweis und Funkabnahme bleiben eigene Abhängigkeiten.
  Maßgeblich sind im aktuellen Firmware-Arbeitsbaum
  `docs/companion/peer-challenge-issue-v1.md`, `peer-auth-result-v1.md`,
  `peer-proof-v1.md` und die zugehörigen SDK-/Hostprüfungen.
- Android bestätigt am kanonischen GATT-Dienst bislang nur dessen Existenz.
  Identität/Schlüsselhaltung, Anmeldung, aktueller Status, bestätigte Rechte,
  Befehlssenden und Antwortkorrelation fehlen dort noch.
- `game.control.core` und eine öffentliche Lease verleihen keine Rechte.
  Firmware muss den aktuellen Principal und die konkrete Verbindung bestätigen;
  vor physischem Senden muss die ursprüngliche Bindung weiterhin gültig sein.
- Vor echter Steuerung v2 `boot`, `lease`, positive `request` und beide
  erwarteten Konfigurations-/Zustandsrevisionen verwenden. Eine lokale UI-ID
  darf nur auf den vollständigen v2-Request abbilden, nicht als Wirefeld dienen.
- Antworten mit vollständigem boot/lease/request korrelieren. Duplikate
  enthalten historische Revisionen/nextRequest. request_conflict oder
  receipt_expired beweisen keine Nichtausführung. Bei unklarem Ausgang keine
  automatische neue ID oder veränderte Payload senden; aktuellen Status prüfen.
- v2 hat `stop`, keinen pauschalen `reset`. change_state/take_transition
  benötigen gültige konfigurierte numerische Ziele und ein passendes geprüftes
  Profil. V0-Reset/Finale und dessen access-Flags bleiben Mockreferenzen.
- Engine-Ergebnis `executed` bestätigt nicht pauschal alle physischen Effekte.
  Spätere Statusbeobachtung und Geräteabnahme bleiben getrennte Nachweise.

## Android-Verbindung und Status

- Semantischen Kotlin-Statusdecoder anhand des abgeglichenen Schemas aufbauen.
  Alte/gleiche Samples, Bootwechsel, fehlende oder ungültige Felder und
  ausbleibende Aktualisierungen behandeln. Vollständig zusammengesetzte Bytes
  sind noch keine vollständige Statusschema-Abnahme.
- Tatsächliche Berechtigungsdialoge, Verweigerung/Entzug, Bluetooth aus,
  ESP-Ausfall, Rotation, Appwechsel und Prozessneustart prüfen.
  Installation mit automatisch vergebenen Rechten ersetzt diese Fälle nicht.
- Eine Verbindungsgeneration besitzt die BLE-Sitzung; alte Rückrufe dürfen
  keine neue Sitzung aktualisieren. Beim Trennen wird die Anzeige gelöscht.
  Hintergrundbetrieb erst bei bestätigtem Bedarf separat entwickeln.
- Laborergebnisse beweisen keinen kanonischen Peerzugang und keine reale
  Spielhardware. Weitere Handymodelle, Dauerbetrieb, Board/NFC/MCP/Batterie
  und reale Komponenten bleiben ungeprüft.

## Lokale Dokumente und spätere Editoren

- Vite hat lokale Entwürfe mit JSON `version: 1`, Draft-/Szenen-IDs, Titeln,
  Zielen und Hinweisen sowie höchstens 24 Szenen. Android-Editor,
  atomare Speicherung, Leseprobe und Dateiauswahl stehen noch aus.
- Browserentwürfe vor Stilllegung exportieren; Import/Export/Neustart auf
  Android müssen IDs, Reihenfolge und Inhalte erhalten. Browser-localStorage
  ist kein Git- oder Androidbestandteil.
- Leseprobe ist redaktionell und berücksichtigt ungespeicherte Änderungen.
  Sie aktiviert kein Firmwareprofil und simuliert keinen physischen Spieltest.
- Szenen auf Firmwarezustände/Transitionen/Bedingungen/Effekte abzubilden
  benötigt einen eigenen bestätigten Vertrag mit Grenzen und Validierung.
  Profil-/Hardwareeditoren warten auf schmal ausführbare Firmwarepfade.
- JS-Domäne, screenModel, Projektionen und Fachtests bleiben
  Erwartungsreferenzen. Android implementiert eigene Repository-/ViewModel-
  Zustände; v0-Mockabläufe werden nicht als v2-Geräteaktionen portiert.

## Reproduzierbarkeit und Ablösung

Der v2-App-Snapshot stammt aus c9654c3; seine Integritätsprüfung ersetzt
keinen Abgleich mit dem jüngeren Firmwarestand. Lokale App-/Firmwarediffs
vor einer versionierten Übernahme dateigenau prüfen und sichern.
Android-Build/JVM/Lint ist der normale App-Prüfweg. Vite-Fachtests bleiben
während der Übernahme ausdrücklich aufrufbare Referenzprüfungen.
Release-Signierung/Verteilung und vollständige Produktfreigabe sind offen.