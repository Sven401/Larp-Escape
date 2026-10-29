# BLE-Softwaretore: Arbeitsstand 30.10.2026

Status: **Teilumsetzung, keine Fortsetzungsfreigabe für den vollständigen Produktweg.**
Basis: `2f0b03f925cb5d54fc72ce0059d97815632652a4`, PR #103.
Am 30.10. per GitHub-Checkdiagnose gelesen: alle 14 gemeldeten Checks erfolgreich,
einschließlich nativer Qualifikation (Run 40970381064). Das gilt für die Basis.

## Neue Software

`PeerTlsBridge` verbindet den Host-Eingang mit einem einzigen Kryptografiearbeiter.
Je Richtung 16 kopierte Fragmente, maximal 128 Eingangsbytes und 20 Notifybytes.
Kein Heap, keine Kryptografie und kein SDK-Aufruf unter dem Mailboxlock.
Volle Eingangsqueue liefert false an den bereits geschlossen ablehnenden Quelladapter.
Busy behält identische Ausgangsbytes; veraltete Ausgänge werden verworfen.
Die ursprüngliche Epoche bleibt erhalten. Der Worker prüft alte TLS-Lifetime vor
neuem Open. Prozesslebenslange Credentials und genau ein Worker sind Voraussetzung.
Kein Start im Firmwareeinstieg, keine Rolle, kein physischer Pairingbestätiger.
Der Anschluss ist Vorbereitung; eine echte Gegenstelle durch diese Bridge fehlt noch.

Unabhängiger Quellenreview durch `/root/review`: Reconnect-Befund korrigiert;
keine weiteren konkreten P1/P2 im begrenzten Umfang. Keine eigenen Testläufe des
Gegenprüfers. Busy-Retention und aktiver TLS-Reconnect brauchen Bridge-Regressionen.

## S5: Quellvergleich und inkompatibler Datenpfad

| Daten | Tatsächlicher Quellstand | Sichere Behandlung / offener Anschluss |
| --- | --- | --- |
| Partitionen | Native Konfiguration verwendet unverändert `firmware/led/partitions.csv`: NVS 0x9000/0x5000, app0 0x10000/0x300000, larpkeys 0x310000/0x6000, LittleFS-Label spiffs 0x316000/0xda000 | Ein Appslot; kein OTA-Rollback. Gleiche Offsets beweisen keine Formatkompatibilität. |
| Geräteidentität | `Esp32DeviceIdentityStore`: Default-NVS, larp-companion/device-id-v1 | nvs_flash_init-Fehler liefert false; kein Autoerase. Nativer Produktstart bindet diesen Store noch nicht an. |
| Rollenregister | `Esp32PeerRegistryStore`: larp-peers-v1/registry-v1, feste Datensatzlänge, Fehler sperren | Registerformat und NVS-Backend müssen im nativen Ziel gemeinsam geprüft werden. TLS verleiht keine Rolle. |
| Baugruppenschlüssel | Legacy ProductMain: separate Partition larpkeys, Namespace larp-led | Nicht als TLS-Schlüssel oder Spielrolle übernehmen. Native Provisionierung fehlt. |
| Bonds | Neuer NimBLE-Adapter verlangt NVS-Persistenz aus; RAM-Store | Keine automatische Übernahme alter Bluedroid-Bonds. Neuer Pairingpfad samt physischer Bestätigung erforderlich. |
| Profile | Esp32ProfileStore: /larp-profiles/profile-a.larp und profile-b.larp, Label spiffs, format_if_mount_failed=false, grow_on_mount=false | LittleFS-Komponente ist im nativen Produkt noch nicht aufgelöst. Mount-/Schema-/Journalfehler müssen sperren; kein Formatieren als Reparatur. |

Damit liegt ein Quellvergleich vor, **kein bestandener S5-Kompatibilitätslauf**.
Der native Produktpfad darf inkompatible/missing Daten nicht als erfolgreichen
Import melden. Vor Upload Chipidentität, Vollbackuphash und Rückweg nach vorhandener
Hardwaredokumentation erneut abgleichen. Kein Upload in dieser Sitzung.

## Tore und nächster konkreter Schritt

| Tor | Ergebnis dieses Arbeitsstands | Rest |
| --- | --- | --- |
| S1 | SDK-Quellpin frisch geprüft; sieben Ablehnungsfälle erfolgreich; grüne native Basis-CI ausgelesen | Advisory-/Variantenmatrix vollständig abschließen; neue Firmware-CI auf Änderungscommit |
| S2 | Kopierte Mailboxen ergänzt | echte SMP/HCI/ACL-Ordnung, physisches Pairing, Fristen und Geräteprobe |
| S3 | BLE-/TLS-Bridge ergänzt | echte Bridge-Gegenstelle, Busy/Reconnect, Android, finale Verbraucherautorisierung |
| S4 | keine neue Geräte-/Profilruntime | reale Adapter, Profileigentümer und Fehler-/Budgetnachweis implementieren |
| S5 | obiger Quellvergleich | native Storeintegration, inkompatible Datenfälle, Rückwegprüfung vor Upload |
| S6 | gemeinsame grüne Basis, unabhängiger begrenzter Quellenreview | aktueller Commit/CI und Integrationsreview des vollständigen Ziels |

Nächster Schritt: Bridge durch eine echte TLS-Gegenstelle betreiben und
Reconnect/Busy-Ausgang prüfen; danach Worker im nativen Integrationsziel mit
expliziter Provisionierung, physischer Pairing-UI und autorisiertem Verbraucher
verdrahten. Erst dieses Ziel erlaubt eine konkrete installierbare Geräteprobe.

## Frische lokale Prüfungen

Auf dem beschriebenen Änderungsbaum: CMake/GCC16.2 mit exakt geprüftem SDK-Pin,
Build erfolgreich; 36/36 CTest-Einträge erfolgreich (10,72s), einschließlich
Bridge-Grenzen, 18 bestehender TLS-Gegenstellen-Unterfälle und PSA-Kryptografie.
Sieben SDK-Guardfälle erfolgreich (24,96s). Der erste CTest-Aufruf scheiterte
am fehlenden Python-Kryptografiepaket; Wiederholung mit vorhandenem
cryptography45.0.7-Paket erfolgreich. Keine neue ESP32-Imageprüfung auf diesem
Änderungsbaum; die ausgelesene grüne Firmware-CI gehört ausschließlich zur Basis.
