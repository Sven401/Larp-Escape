# Companion-Status v1

**Status:** Protokollkern implementiert und hostprüfbar. Ein Transport und die Android-App folgen später. **Stand:** 5. Oktober 2026.

## Zuständigkeit

`GameSnapshot` ist eine Wertkopie ohne UID, Gerätezeiger oder Zugangsdaten. Der Anwendungslauf erzeugt sie nach jedem Spieltick. `CompanionStatusStore` überträgt die letzte Kopie synchronisiert an einen Leser. Ein späterer Netzwerk-Callback liest die Mailbox und serialisiert die Kopie; er ruft weder Controller noch Effekte direkt auf.

`encodeStatus(snapshot, buffer, capacity, written)` braucht keinen Heap und keine JSON-Bibliothek. Die v1-Ausgabe passt in **512 Bytes inklusive Nullterminator**. `written` zählt Bytes ohne Terminator. Bei ungültigem Snapshot oder zu kleinem Puffer: `written=0`; ein vorhandener, nicht leerer Ausgabepuffer beginnt mit `\0`. Ein fehlender Puffer ist zulässig und liefert einen Fehler, keine Größenabfrage.

## Dokument

```json
{"schemaVersion":1,"capabilities":["status.read"],"game":{"phase":"waiting_for_crystals","role":"krieger","crystalCount":0,"roundCount":0,"seenRoleCount":1,"incorrectCrystal":false,"keystoneError":false,"audioBusy":false}}
```

| Feld | Bedeutung / Bereich |
| --- | --- |
| `schemaVersion` | Genau 1 |
| `capabilities` | Aktuell nur `status.read` |
| `game.phase` | Ausgewählter Zielzustand; dessen Eintrittsaktion kann erst im nächsten Tick laufen |
| `game.role` | Beibehaltenes bekanntes Rollenprofil oder `null`; kein Nachweis physischer Kartenanwesenheit |
| `game.crystalCount` | 0 bis 3 akzeptierte Kristalle der Runde |
| `game.roundCount` | Anzahl abgeschlossener Runden; keine aktuelle Rundennummer |
| `game.seenRoleCount` | 0 bis 6 einzigartige bekannte Rollen im Spiel |
| `game.incorrectCrystal` | Noch anstehender Kristallfehler |
| `game.keystoneError` | Noch anstehender Kartenfehler |
| `game.audioBusy` | Audioport meldet Busy; umfasst laufende Wake-Sequenz |

Phasen: `standby`, `idle`, `waiting_for_crystals`, `first_crystal_placed`, `second_crystal_placed`, `third_crystal_placed`, `game_completed`, `bonus`, `error`.

Rollen: `puppenspielerin`, `krieger`, `narr`, `abenteurerin`, `ueberlebende`, `wissende`. Wire-Namen sind stabile ASCII-Bezeichner; C++-Enumordnungen werden nicht übertragen. Die App übersetzt sichtbare Namen selbst.

Ein Fehlerflag kann vom Übergang verbraucht werden und anschließend `false` sein, obwohl `phase="error"` ist. Fortschritt und Phase sind Momentaufnahmen, kein Ereignisjournal. Unbekannte Schemas muss die App sichtbar als nicht unterstützt behandeln; neue optionale Felder darf ein v1-Leser ignorieren.

## Spätere WLAN-Anbindung

Für eine erste lokale Bedienoberfläche ist `GET /api/v1/status` im selben WLAN vorgesehen. Der Pfad ist heute **nicht erreichbar**. Polling reicht für den Einstieg; WebSocket-Ereignisse, Gerätediscovery und Schreibbefehle brauchen jeweils einen eigenen Vertrag. Die v1-Capability verspricht keine Reset-/Start-/Aktivierungsbefehle.

Die Android-App wird eine gewöhnliche WLAN/IP-Anwendung. ESP-NOW nutzt spezielle WiFi-Frames; aus den verfügbaren Android-WLAN-APIs folgt die Architekturentscheidung, ESP32-zu-ESP32-Kommunikation beizubehalten und die App über IP anzubinden. Quellen: [Espressif ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html), [Android WLAN](https://developer.android.com/develop/connectivity/wifi).

Ein gemeinsames LAN braucht keinen Internetzugang. Eine SoftAP-Verbindung kann später für Offlinebetrieb oder Einrichtung dienen; Androids [WiFi Network Request API](https://developer.android.com/develop/connectivity/wifi/wifi-bootstrap) führt dabei einen Systemdialog aus. AP+STA erlaubt beim ESP32 keine unabhängigen Funkkanäle; der STA-Kanal bestimmt den SoftAP-Kanal. ESP-NOW-Peers müssen dazu passen: [WiFi-Treiber](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi-driver/overview.html), [ESP-NOW FAQ](https://github.com/espressif/esp-faq/blob/master/docs/en/application-solution/esp-now.rst).

WLED dient als Vorbild für Status-/Info-API und klar getrennte Steuerwege: [WLED JSON API](https://github.com/wled/WLED-Docs/blob/main/docs/interfaces/json-api.md). Sein voller Webumfang wird nicht als Ressourcenbedarf vorausgesetzt.

## Android-Verträge vor der Umsetzung

- Android 17 / Ziel-SDK 37: lokalen Netzwerkzugriff und `ACCESS_LOCAL_NETWORK` beziehungsweise unterstützte Discovery-Auswahl berücksichtigen. [Offizielle Vorgaben](https://developer.android.com/privacy-and-security/local-network-permission), geprüft 5. Oktober 2026.
- Lokales HTTP erfordert eine bewusste Netzwerksicherheitskonfiguration; Cleartext ist für Apps mit Ziel-API 28+ standardmäßig deaktiviert. HTTPS oder gezielt freigegebenes lokales HTTP vor Implementierung festlegen. [Android Network Security Configuration](https://developer.android.com/privacy-and-security/security-config).
- Keine Rollenanzeige für Spieler voraussetzen: Die Statusdaten können Rätselinhalte verraten. Als erste Annahme dient eine Bedienoberfläche für Spielleitung.
- Gerätesuche, Mehrgeräteübersicht, Authentisierung, WLAN-Provisionierung und Befehle sind eigene spätere Schritte. Optionale BLE-Provisionierung hängt von der ausgewählten ESP32-Frameworkversion ab.

Offene Produktentscheidungen: Anzahl der ESP32-Geräte/Gateway; Routerverfügbarkeit; Spielleitung oder Spieler als Zielgruppe; gewünschte Fernbefehle. Diese Entscheidungen blockieren den getesteten Protokollkern nicht.
