# Firmware-Fixtures v2 für die Companion-App

Aus tatsächlichem Firmwarecodec/Adapter/GameControl/ProfileApplication im vertikalen Hostschnitt erzeugt. Synthetische Geräteeigentümer und modellierte Speicherbytes; keine Funk-/Anmeldungs- oder Boardbelege.

| Datei | Bedeutung |
| --- | --- |
| status-ready | Einrichtung/Speicherung im Modell bestätigt, Spiel gestoppt |
| command-start | acht Pflichtfelder, Boot/Lease/Request und zwei erwartete Revisionen |
| reply-executed | Engine hat Start ausgeführt, kein bloßes Queue-Accepted |
| status-running | kopierter Zustand nach Start, entryPending kann noch true sein |
| reply-duplicate | ursprüngliche Antwort erneut, duplicate=true; historische Revisionen/nextRequest |
| reply-conflict | gleiche Nummer mit anderem Inhalt; ursprünglicher Start bleibt ausgeführt |
| command-stale-state / reply-stale-state | aktuelle Nummer mit falscher Zustandsrevision, consumed=true |

`pio test -e host-configured-control` aus dem Projektverzeichnis prüft Fixturebytes gegen den tatsächlichen Ablauf. Für absichtliche Vertragsänderungen LARP_EXPORT_COMPANION_FIXTURES=1 ausschließlich für einen Testaufruf setzen, danach ohne Export erneut prüfen. Quelle: test/test_configured_control/test_main.cpp, Fall test_app_fixtures_match_actual_firmware_vertical_flow. Kein Export bei normaler Prüfung.

Clientadapter verwendet boot (16 uppercase Hexzeichen), positive numerische lease/request, expectedConfigurationRevision und expectedStateRevision. expectedRuntimeGeneration ersetzt stateRevision nicht. UUID/requestId und accepted/duplicate als eigene Ergebnisarten gehören nicht zu diesem Vertrag. principal/connectionEpoch/GM-Verifikation sind Transportmetadaten, niemals Client-JSON-Felder; eine öffentliche Lease ist keine Berechtigung. Profildokumente haben weiterhin Schema1; sichtbare Namen kommen aus dem passenden validierten Profil. Transport und echte Anmeldung sind noch offen.

Schema-Prüfung: `python scripts/check_companion_fixtures.py` mit jsonschema==4.23.0 in einer separaten Pythonumgebung. Prüft genau acht Dokumente und alle drei kanonischen Schemata. Die Native-Prüfung ergänzt Semantik/Bytevergleich; Schema allein belegt keine Berechtigung oder Ausführung.
