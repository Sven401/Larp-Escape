# Öffentliches Peer- und Spielleiterregister v1

Status: schmaler Firmware-/Host-/CLIpfad. Kein Produktionszugang und keine Geräteabnahme. Datum: 2026-10-07; Basis ac59465, neuer Registerschnitt.

## Zweck und Vertrauensgrenze

Vier öffentliche P256-Schlüssel werden einem Peer und einer eigenen Schlüsselgeneration zugeordnet. Eine Spielleiterrolle wird ausdrücklich von einem vertrauenswürdigen lokalen Einrichtungsbesitzer vergeben; ihr Principal ist eindeutig und ungleich null. Ein gültiger Schlüssel allein vergibt keine Rolle. Keine privaten Schlüssel, App-Schreibschnittstelle oder Remoteprovisionierung sind enthalten.

`PeerRegistryOwner` prüft echte Kurvenpunkte über `IPeerProofCrypto`, hält den bestätigten Snapshot und liefert nur dessen aktuelle Credentials. `PeerRegistryRuntime` ersetzt die Rollen auf dem ursprünglichen alleinigen `IdentityAccess`-Besitzer. Wiederherstellung und Änderungen schließen den ursprünglichen Zugang; sie öffnen keine Verbindung. Eine bereits verbundene Sitzung bleibt nach Änderung gesperrt und muss sich neu verbinden und nachweisen. Das laufende Spiel wird dadurch nicht gestoppt.

Der direkte Runtimeport ist für serialisierte Startup-/Hostarbeit. Ein Produktions-NVSworker braucht eine kopierte, original gebundene Actor-Anforderung mit Bestätigung auf dem tatsächlichen Identity-/SDKbesitzer. Er darf die Policy nicht direkt von einem zweiten Task aus verändern. Auch Ownergetter und der direkte Host-Proofkontext gehören zum selben serialisierten Prüflauf; `busy_` ist keine Threadsynchronisation. Ein separater Produktions-Kryptoworker braucht eine kopierte synchronisierte Veröffentlichung mit Registrygeneration und Entzugssperre. Ursprüngliche SDKsecurity, gegenseitige Geräteauthentisierung, Einrichtungsablauf und eigene Authentisierungsattribute bleiben offen. Die bestehenden beiden GATT-Schreibattribute setzen bereits GM voraus und eignen sich deshalb nicht als Authentisierungseinstieg.

## Fester Vertrag

Alle Zahlen little endian; kein C++-Structdump und keine Paddingbytes. Exakt **404 Byte**:

| Offset | Länge | Wert |
|---:|---:|---|
| 0 | 4 | ASCII LPRG |
| 4 | 1 | Version 1 |
| 5 | 1 | Anzahl 0 bis 4 |
| 6 | 2 | Null |
| 8 | 4 | Registrygeneration, ungleich null |
| 12 | 16 | Öffentliche Gerätekennung, ungleich null |
| 28 | 4 | Null |
| 32 | 368 | Vier Einträge zu je 92 Byte |
| 400 | 4 | CRC32 IEEE über die ersten 400 Byte |

Ein Eintrag: Peer16, SEC1-uncompressed-P256-Key65, Schlüsselgeneration4, Principal4, GM1, Reserve2. Die GMbyte ist 0/1; ohne GM ist Principal0, mit GM ist Principal ungleich null. Belegte Peers sind lexikografisch aufsteigend und eindeutig; Schlüssel und vergebene Principals sind eindeutig. Unbenutzte 92-Byte-Einträge sind vollständig null. Der Codec prüft Struktur, Prefix und CRC; der Owner prüft zusätzlich den echten Kurvenpunkt. CRC authentisiert weder Rollen noch Flashherkunft.

Die In-Memorygeneration0 bezeichnet ausschließlich ein positiv fehlendes leeres Register. Sie wird nicht gespeichert. Jede bestätigte Änderung erhöht die Registrygeneration genau um eins. Ein unveränderter Schlüssel behält seine Credentialgeneration; Rotation erhöht sie genau um eins, ein neuer Peer beginnt bei1. Kein Zählerumlauf. Stale, strukturell ungültige und inhaltsgleiche Änderungen schreiben nicht und schließen die gültige aktuelle Verbindung nicht.

## Speicherung und Ausfallmodell

`Esp32PeerRegistryStore`: privater NVSnamespace `larp-peers-v1`, einziger Schlüssel `registry-v1`; getrennt von der öffentlichen Gerätekennung und Profilslots. Statischer Mutex mit Wartezeit0 schützt den vollständigen Compare-and-replace: erwartet wird fehlender Schlüssel oder exakt der ganze bisherige404-Byte-Datensatz. `nvs_set_blob`, `nvs_commit` und exakte Rücklesung müssen erfolgreich sein. Danach liest auch der Owner erneut exakt zurück, bevor er den Snapshot oder Rollen veröffentlicht. Automatisches Löschen/Formatieren, Recovery und Rückgriff auf ältere privilegierte Datensätze fehlen bewusst.

Vor jeder angenommenen Änderung schließen Rollen und aktuelle Proofberechtigung. Fehler, Konflikt, ungewisse Speicherung oder fehlerhafte Rücklesung sperren Owner und gemeinsamen Store für den restlichen Boot. Neue Objekte lösen die gemeinsame Sperre nicht. Korruptes aktuelles Register wird nicht durch eine ältere GMrolle ersetzt. Positiv fehlendes Register bleibt leer und geschlossen; ein Lesefehler wird nicht als fehlender Schlüssel behandelt und erzeugt keinen Datensatz.

**Unbestätigte Änderung bei echtem Stromausfall:** Der nach Neustart lesbare Datensatz kann alt oder neu sein. Die Sperre ist RAMzustand; dieses Modell garantiert keinen dauerhaften Entzug nach fehlgeschlagener unbestätigter Änderung. Erst Saved bedeutet im geprüften Modell bestätigte Rücklesung. Physische NVSdurabilität, Stromausfälle, Flashrollback und ein vertrauenswürdiger Recoveryablauf sind offen. Keine Abnahme solcher Eigenschaften aus Hosttests ableiten.

## Vertikaler Prüfpfad und Tool

Die tatsächlichen NVS-/FreeRTOSadapter laufen gegen deterministische Hostmodelle. Der separate echte Mbed-TLS2.28.4-Pfad speichert Key-only, vergibt GM ausdrücklich, stellt die geschlossene Rolle mit einem neuen Owner wieder her, prüft die unabhängige P256-Signatur durch sechs20-Byte-Proofframes, führt13 tatsächliche Befehlsframes bis zum laufenden Spiel aus und sperrt den alten Proof/Zugang nach bestätigtem Entzug. SDKsecurity bleibt modelliert; die neue SDKprobe linkt den tatsächlichen Registeradapter ausschließlich im ungeöffneten Zustand. Kein Bluetoothstart oder NVSinit/-write in dieser Probe.

Öffentliche C++-Fixtures: `fixtures/peer-registry/{key-only,game-master,revoked}.hex` und `observation.json`. Export nur im tatsächlichen Nativefall bei `LARP_EXPORT_PEER_REGISTRY_FIXTURES=1`; normale Prüfläufe vergleichen die vorhandenen Dateien. Der zweite öffentliche Testpunkt entsteht mit `make_peer_registry_fixture.py` (cryptography45.0.7, öffentlich bekannter Testskalar2; niemals einsetzen).

`profile-tools --inspect-peer-registry-record <808 uppercase hex characters>` nutzt den tatsächlichen festen Codec und meldet Gerät, Generation, Anzahl und GManzahl. Die Ausgabe sagt ausdrücklich `cryptoValidated:false` und `stored:false`. Das Tool prüft keine Kurve, Herkunft oder Rolle und provisioniert nichts. `check_peer_registry.py --tool <profile-tools.exe>` prüft dieselben C++-Bytes unabhängig mit struct/zlib und cryptography45.0.7 einschließlich Negativfällen und Toolabgleich.

## Ressourcen und offene Abnahmen

Vier Plätze, ein404-Byte-Record, keine dynamischen Container im eigenen Registrykern. Mehrere unveränderte lokale Snapshots sind zusätzliche Stackkopien. NVS und MbedTLS dürfen intern allokieren/blockieren. Beides gehört auf einen getrennten Worker; Wartezeit0 am Namespace-Mutex begrenzt nicht die NVS-/Kryptografiedauer. Workerstack, Heapspitzen, vollständige Aufrufketten und Worst-case-Laufzeit müssen auf dem Ziel gemessen werden. Es gibt keine übernommene2000-ms-Befehlsfrist und keine behauptete Flash-/Funkzeitgarantie.

### Tatsächlicher Prüf- und Compilerstand

406/406 Basis-Hostfunktionen, 410/410 im echten Kryptoziel; gegenüber ac59465 fünfzehn neue Basis- und zwei neue Kryptofunktionen. Fünf Builds erfolgreich, nach Stackoptimierung betroffener SDKbuild erneut erfolgreich. Architektur134 Dateien/284 direkte Includes, 80 unabhängige C++11-Header und279 Formatdateien erfolgreich. Drei tatsächliche C++-Datensätze unabhängig geprüft;432 beschädigte Datensätze, ein ungültiger Kurvenpunkt und15 falsche Beobachtungen zurückgewiesen. Zwölf gemeinsame App-v2-Dateien frisch byte-/SHAgleich; keine Appprüfung/-änderung.

ESP32-Compilergrößen: Owner832, Snapshot408, Entry96, Record404, Runtime8, Store8 Byte; IdentityAccess144 unverändert. Eigene einzelne Compilerframes aus den tatsächlichen SDKobjekten: Ownerbegin864, Ownerreplace1696, Encode864, Decode848, Valid160, Runtimeclose112/replace544, Policyreplace176/proofCurrent64, Storebegin/read48, Readhelper448, Storereplace1712. Zwei nur nacheinander benötigte Arbeitskopien werden wiederverwendet; Storereplace vorher2480, jetzt1712 Byte (**768 Byte weniger**), ohne Verlust der eingefrorenen vollständigen CASrecords oder exakten Rücklesung. Der bekannte eigene Aufrufpfad Ownerreplace→Storereplace→Decode→Valid hat schon4416 Byte Compilerframes vor zusätzlichen Bibliotheks-/NVS-/Aufruferkosten. Frühere3072-/4096-Byte-Probestacks sind daher keine passende Abnahme für diesen Worker. Das ist statische Codeauswertung, keine gemessene Stackspitze.

Finale SDKprobe44.552 RAM/1.063.661 Flash (81,2% Appslot), netto864 RAM/3.608 Flash gegenüber ac59465. Produktion55.148/1.229.077 (93,8%), USB21.480/291.881, Profilprobe107.560/406.849 plus57.272 Startup-Profilheap und Allocator/Dateisystem. Getrennte Proben sind keine kombinierte Produktionsreserve. Produktionsmain verwendet weiterhin den bisherigen Controller.

Nächster kleiner vertikaler Schnitt: synchronisierte kopierte Registryveröffentlichung und original gebundener geschlossener Rollenauftrag mit Bestätigung auf dem tatsächlichen Identityactor. Danach ursprüngliche SDKsecurity und eigene Vor-GM-Authendpunkte. Keine Gerätedurabilität, RF, SMP, Android, Energie, Laufzeit oder komplette Heap-/Stackmessung aus diesen Belegen ableiten.
