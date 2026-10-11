# APP_MUTUAL_V1: isolierte ESP32-Kryptografieprobe

Stand: 2026-10-11. Dieser Build prüft ausschließlich, ob die gepinnte ESP32-Toolchain die für den ausgewählten USB-Einschreibungsweg erwarteten primitiven Funktionen kompilieren und linken kann. Er ist kein Authentisierungsprotokoll und wird nicht auf ein Gerät geladen.

## Reproduzierbarer Check

`platformio run -e esp32-app-mutual-crypto-probe`

Die Probe baut nur `src/AppMutualCryptoProbe/main.cpp`. Eine durch `volatile` referenzierte, aber von `setup()` **nicht aufgerufene** Funktion bindet ECDSA-P-256-Schlüsselgenerierung, Signatur und Prüfung, zwei flüchtige P-256-ECDH-Seiten mit beidseitiger Berechnung, AES-128-GCM-Verschlüsselung mit Tag-Prüfung, SHA-256 und `esp_fill_random` ein. Die eingebetteten Nachricht-, AES- und Nonce-Bytes sind öffentliche synthetische Probe-Eingaben; Schlüssel werden weder eingeschrieben noch gespeichert. `setup()` und `loop()` starten keine Funk- oder Geräteaktion. Eine etwaige Laufzeitprüfung wäre ein eigener, expliziter Test mit geeigneter Entropieprüfung und ohne Wiederverwendung der synthetischen GCM-Nonce.

Geprüfte lokale Pins: PlatformIO `espressif32@6.6.0`, Arduino-ESP32-Paket `3.20014.231204` (Arduino 2.0.14), ESP32-SDK-Konfiguration `tools/sdk/esp32/sdkconfig`. Dort sind ECDH, ECDSA und GCM aktiviert; `CONFIG_MBEDTLS_HKDF_C` ist **nicht** gesetzt. Der Build bricht ab, falls HKDF künftig plötzlich aktiviert ist, damit die Schlüsselkonstruktion erneut geprüft wird. Eine eigene HKDF-Implementierung und ein Sitzungsschlüsselplan gehören nicht zu dieser Probe.

Lokales Ergebnis: Build und Link erfolgreich; 282.105 B Flash von 1.310.720 B und 21.132 B RAM von 532.480 B. Diese Größe ist die isolierte Probe und keine Produktgrößenprognose. Kryptografische Operationen, RNG-Qualität im tatsächlichen Betriebszustand, Protokoll, USB-Einschreibung, Widerruf, Android-Gegenstelle, BLE-Transport und physische Boxfunktion wurden hier nicht ausgeführt oder bestätigt.
