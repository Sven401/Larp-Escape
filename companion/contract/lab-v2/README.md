# Debug-Laborrahmen v2

Quelle: `experiments/v2-contract-probe/README.md`, unversionierter Produktstand
auf `main` / `29da937c4309beddfd45c5dd0fbf84fdf853d8e4`, gelesen am 2026-10-10.
Hier werden nur die für den Androidlesepfad erforderlichen Vertragswerte
festgehalten. Laborquellen, Uploadabläufe und alte Flashrückwege sind nicht
Teil dieses Pakets.

Labservice `86ddc5b0-3878-4ff7-9f55-17c62aeb1001`: Endung `1002` READ
Labmarker, `1003` WRITE Befehlsframes, `1004` NOTIFY Antwort-/Statusframes.
Android Debug liest den Marker und abonniert Status; Android Release lehnt
Laborgeräte ab. Der kanonische Produktservice bleibt ein eigener Zugang.

Rahmenversion 1, Typ 1 Command / 2 Reply / 4 Status, 8-Byte-Header mit
message/total/offset als LE16. Im Labor höchstens 20 Byte pro ATT-Wert,
also 12 Nutzbytes. Vollständige JSON-Nachrichten verwenden Schema 2.
Status bis 6143 Byte, Reply bis 511 Byte, Command bis 1024 Byte.
Absolute Fristen: Command/Reply 2 Sekunden, Status 30 Sekunden.
Die App verarbeitet erst vollständig zusammengesetzte, geprüfte Nachrichten.

Synthetische Geräte, RAM-Profiljournal und direkt eingespeiste GM-Autorität
sind Laborbedingungen. Laborstatus belegt weder Peer-Anmeldung noch physische
Spielgerätefunktion. Kein Laborupload ist durch diesen Vertrag beauftragt.
