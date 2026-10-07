# Öffentliche Registerprüfdaten

Quelle: tatsächlicher C++-Nativepfad mit Mbed-TLS2.28.4, NVS-/FreeRTOS-/SDKsecurity-Hostmodelle. Datum: 2026-10-07. Export durch `LARP_EXPORT_PEER_REGISTRY_FIXTURES=1`; normale Nativeprüfungen vergleichen vorhandene Bytes. Die 404-Byte-Hexdateien enthalten nur öffentlich bekannte Testschlüssel und Kennungen; niemals als Einrichtung verwenden.

- key-only.hex: Generation1, ein Schlüssel, keine GMrolle.
- game-master.hex: Generation2, gleicher Schlüssel, ausdrücklich GM/Principal7.
- revoked.hex: Generation3, gleicher Schlüssel, bestätigter Rollenentzug im Modell.
- observation.json: Tatsächlicher erfolgreicher Speichern-/Restore-/Proof-/Spielstart-/Entzugsfall. Kryptografie echt; NVS, Sicherheitsereignisse und RTOSausführung modelliert. Kein RF-/Stromausfall-/SMP-/Gerätenachweis.

Vertrag: [peer-registry-v1.md](../../peer-registry-v1.md). Unabhängige Prüfung: `scripts/check_peer_registry.py`, struct/zlib und cryptography45.0.7; optional tatsächlicher CLIcodec. CRC beweist keine Herkunft. Die Modellsperre bei unbestätigter Speicherung gilt für den Boot; ein realer Neustart kann den alten oder neuen gültigen Datensatz lesen.
