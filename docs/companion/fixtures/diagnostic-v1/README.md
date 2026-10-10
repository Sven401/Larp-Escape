# Diagnose v1 Golden-Fixtures

Alle Kennungen, Firmwarestände, Messwerte und Beobachtungen sind synthetisch.
Kein Bericht stammt von der Reichstagsbox. `evidence=synthetic` bleibt auch bei
`verdict=passed` erhalten. Canonical JSON-Umschläge enthalten `report` und SHA-256.

| Datei | unveränderter Nachweis |
| --- | --- |
| completed.json | Wake 1 aktiv/inaktiv und LED 0 rot/aus, getrennte simulierte Nutzerbeobachtungen |
| failed.json | passende Geräteantwort, abweichende Nutzerbeobachtung |
| not-tested.json | LED-Schritt übersprungen, kein Gerätebeleg |
| timeout.json | lokaler Timeout, spätere Antwort nur als Rohbeleg |
| cancelled.json | Nutzerabbruch, Ausgang bleibt unklar |
| disconnected.json | Verbindungsverlust, keine automatische Wiederholung |
| app_restarted.json | Journalwiederaufnahme, keine automatische Wiederholung |
| device_restarted.json | Bootwechsel, alte Antwort ist kein neuer Nachweis |

Prüfung: `python scripts/check_diagnostic_contract.py`. Goldenbytes werden dabei
nicht regeneriert. Das Schema prüft Struktur; zusätzliche Semantikprüfungen prüfen
Kennungen, Zeitfolge, Schrittfolge und Urteile. Es gibt noch keinen C++-/Kotlin-
Produzenten dieser Dokumente. Medien-/Dateisystem- und Gerätepfade bleiben offen.
