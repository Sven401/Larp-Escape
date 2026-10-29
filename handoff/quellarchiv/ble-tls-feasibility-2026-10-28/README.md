# Erhaltener TLS-Machbarkeitsversuch

Stand: 28.10.2026. Status: **neun Hostfälle ausgeführt, keine Produktfreigabe**.
Quelle: Sitzung `01a183bd-0df7-72e3-a67c-ddde84798a0b`, Arbeitsbasis `4ee93f0`.
Dateiherkunft und SHA-256 stehen in `manifest.json`; die drei Dateien wurden
unverändert aus `private/tls-feasibility/` übernommen. Kein dauerhaft installierter
Prüfauftrag, kein Hook, keine Migration und kein Gerätelauf.

Der tatsächliche Mbed-TLS-2.28.4-Server wurde gegen Python/OpenSSL-MemoryBIO
mit TLS 1.2 und ECDHE-ECDSA-AES128-GCM-SHA256 ausgeführt. Beide Seiten verlangen
den vorbereiteten Zertifikatsschlüssel. Ein normaler 40-Byte-Transport gelingt
mit 1-, 7- und 20-Byte-Eingangsstücken; falscher Client, falscher Serverpin,
fehlendes Clientzertifikat, manipuliertes Record, Klartexteinschleusung und
Recordwiederholung werden abgewiesen. Kein zusätzlicher Klartext bei den
Recordfehlern. Ergebnisse: `results.json`.

**Diese Bibliotheksversion ist nicht für den neuen Produktzugang freigegeben.**
Die Fälle prüfen keine Speicher-/Hardwarefehler aus CVE-2025-27810. Sie belegen
weder eine Rolle, ursprüngliches SMP/Bond, Profilimport noch Speicherung oder
Geräteverhalten. Der erste Versuch ohne expliziten CA-Anker endete mit
`MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED`; nach dessen Konfiguration bestanden die
neun Fälle. Der Pinvergleich bleibt zusätzlich erforderlich.

## Bearbeitbare Quellen und Wiederholung

`server.cpp` enthält einen Host-BIO mit begrenzten Puffern, partiellen Zugriffen,
WANT_READ/WRITE, exaktem Blatt-Schlüsselvergleich, gesperrter Renegotiation und
gesperrten Tickets/Sessioncache. `run.py` erzeugt ausschließlich synthetische
Testzertifikate und startet die unabhängige Gegenstelle. Private Skalare 1/2/3
sind öffentliche Testwerte und dürfen **niemals auf Geräten eingerichtet** werden.
Reale Schlüssel, Zertifikatdateien, Flashabbilder und Programme sind nicht im Archiv.

Der Lauf erwartete den Repository-Stamm als Arbeitsverzeichnis und seine Dateien
unter `private/tls-feasibility/`. Der Python-Runner enthält den damaligen lokalen
MinGW-Pfad; auf anderen Rechnern explizit anpassen. Das Archiv wird nicht als
automatisch ausführbare neue CI- oder Maintainerregel übernommen.

Der tatsächlich ausgeführte Build verwendete die bereits archiv-/quellengeprüfte
Mbed-TLS-Bibliothek des `host-peer-proof`-Prüfwegs, MinGW und:

```text
g++ -std=c++11 -Wall -Wextra -Wpedantic -Werror
  -I private/peer-mbedtls/source/include
  private/tls-feasibility/server.cpp
  .pio/build/host-peer-proof/libpeer_mbedtls.a
  -lws2_32 -ladvapi32 -o private/tls-feasibility/server.exe
python private/tls-feasibility/run.py
```

Die Argumente des Compileraufrufs wurden als ein Aufruf ausgeführt. Python
benötigte `cryptography==45.0.7` unter `private/quality-tools`; dieses Modul wurde
nur zum Erzeugen der Testzertifikate verwendet. Der TLS-Client ist Python `ssl`.

Nutzungsbewertung: Der Versuch bestätigte echte Interoperabilität und die
Teilzugriffsregeln, fand die CA-Anker-Pflicht und grenzt den geplanten Kanal
gegen bloße Server-Signaturprüfung ab. Er ersetzt die Backendaktualisierung
nicht. Bei Wiederaufnahme auf der gewählten Migrationsbasis neu ausführen;
alte grüne Ergebnisse nicht als Nachweis des neuen TLS-Backends übernehmen.
