# BLE-Fortsetzung: gemeinsame Basis und offene Softwaretore

Fortsetzung 30.10.2026: [Bridge, Datenvergleich und offene Tore](BLE-SOFTWARETORE-2026-10-30.md). Keine Produktfreigabe.

Stand: 28.10.2026. **In Umsetzung; noch keine Freigabe der vollständigen BLE-Produktintegration.**
Auftrag dieser Sitzung: Voraussetzungen aus dem
[Fortsetzungsplan](BLE-FORTSETZUNGSPLAN-2026-10-28.md) umsetzen;
notwendige Geräteuploads konkret vorbereiten und getrennt beauftragen.

## Aktualisierung zur Mergevorbereitung · 30.10.2026

Aktuelle Integrationsbasis: `eb3f4507369b12ea11cd4097195325b73b1d8f0b`,
mit #100/#101/#102/#104. #101-Merge-CI: Quality 3/3 und native Qualifikation
1/1 erfolgreich. SDK-Wartungspin und ECC-Regression sind damit bereits integriert.
[Arbeitsbericht dieser Vorbereitung](PR103-MERGEVORBEREITUNG-2026-10-30.md).
#103 ergänzt ausschließlich die noch inaktiven TLS-/NimBLE-Bausteine und
beide isolierten Qualifikationsziele; keine Freigabe der vollständigen Produktstrecke.

Die ursprüngliche #103-PR-CI am Head `b1410a0` bestand bereits einschließlich
Hosttests und beider SDK-Images. Die unten beschriebenen früheren Buildprobleme
sind historische Fehlerbelege. Die neue CI am aktualisierten Head bleibt ein
eigener Nachweis. Kein Übertragen alter grüner Läufe auf neue Commits.

## Historische gemeinsame Herkunft

Branch `codex/ble-fortsetzungsbasis`, Ausgangsbasis
`191d08164614bd6472db241336d263da77691f49`.
Getrennte lokale Zusammenführung des Migrationsentwurfs #101 bei `f26e87e`
und des jüngsten #100-Planstands `0c8ced2`; gemeinsamer Herkunftscommit
`ba3551e`. #100 und #101 bleiben offene Entwürfe, kein Merge in den
Integrationsbranch durch dieses Paket. Der Dokumentations-PR #102 bleibt
separat; seine Aktualisierung wird hier nicht als integriert behauptet.

Federführung Plattform/Integration und Umsetzung: Hauptagent `/root`.
Anderer Security-/Qualitätsgegenprüfer: `/root/review`, eigenständige
Quellenprüfung. Der Gegenprüfer schrieb die öffentliche ECC-Parserregression;
der Hauptagent prüfte diesen eigenen Testbeitrag getrennt. Modelle, echte
Kryptografie, SDK-Build und Geräteverhalten werden getrennt bewertet.

## S1: SDK-Sicherheitsstand

Der ursprüngliche stabile SDK-Pin `fff9895c82d744c7237be8847347bdd1b07c6643`
aus #101 enthält Mbed TLS 4.1.0 / TF-PSA 1.1.0. Die Quellenprüfung bestätigt
[CVE-2026-50583](https://mbed-tls.readthedocs.io/en/latest/security-advisories/mbedtls-security-advisory-2026-07-zero-length-ecc-public-key-oob-read/):
`pk_ecc.c` liest bei leerem ECPoint vor dem Schlüsselpin-Callback ein Byte
jenseits des Eingangs. Exakter Leaf-Pin/TLS1.2 verhindert diesen Parserpfad nicht.
Dieser alte Stand wird für den neuen Zugang nicht freigegeben.

Separater Hersteller-Wartungskandidat:
`9a97f6c54ec638111ce55cd36581b3c192f15207` auf `release/v6.1`,
Mbed TLS `ce3f3485a121c100f58f36d700cb35b060f6e866` (4.1.1, TF-PSA1.1.1),
NimBLE `b3c38720cecb56d6a36adde7be95fa0c70caa4e0`.
Das ist ein exakt gepinnter Wartungscommit, kein erfundener stabiler 6.1.1-Tag.
SDK, Untermodulstand und lokale Quelländerungen bleiben durch den SDK-Wächter
geprüft; alte Versions-/ABI-Guards werden nicht abgeschaltet.

Die lokale Quellenprüfung bestätigt im Kandidaten zusätzlich die EMS- und
basicConstraints-Korrekturen zu
[CVE-2026-50581](https://mbed-tls.readthedocs.io/en/latest/security-advisories/mbedtls-security-advisory-2026-07-extended-master-secret-calculation-failure-ignored/)
und [CVE-2026-49300](https://mbed-tls.readthedocs.io/en/latest/security-advisories/mbedtls-security-advisory-2026-07-x509-ca-bit-forgery-invalid-basicconstraints/).
Stream-TLS, vollständiges Free/Init, keine Sessionserialisierung, keine Tickets,
kein DTLS und keine sig_algs-alleinige Zertifikatspolicy begrenzen die betrachteten
weiteren Advisorypfade. Das ist keine pauschale Security- oder Constant-Time-Freigabe.
SDK-/CI-Qualifikation und übrige produktbezogene Bewertung bleiben bis zu ihren
jeweiligen neuen Nachweisen offen.

## Stand der sechs Tore

| Tor | Status und vorhandener Teil | Noch nötig |
| --- | --- | --- |
| S1 | offen; neuer exakter Herstellerpin, bestätigte Parser-/EMS-/X509-Fixes, echte öffentliche ECC-Regression mit Guardpage | vollständiger Abgleich der tatsächlich genutzten Varianten/Advisories; finale CI gesondert auswerten |
| S2 | offen; nativer öffentlicher NimBLE-Quelladapter und Hostmodell-Gegenfälle | tatsächliche SMP-/HCI-/ACL-Ordnung, Nebenläufigkeit/Fristen und begrenzter Gerätelauf |
| S3 | offen; wiederverwendbarer `PinnedPeerTls`-Worker und echte OpenSSL-Gegenstelle am neuen Backend | tatsächliche BLE-/Androidkopplung, finale Rollen-/Verbraucherautorisierung und Geräte-/Ressourcennachweise |
| S4 | offen; nativer Kern/RTOS und getrennte Prüfeinstiege vorhanden | benötigte reale Geräteadapter/Profileigentümer, tatsächliches Integrationsziel und Fehler-/Budgetnachweis |
| S5 | offen; unveränderte Partition-CSV, geschlossener Start, kein Autoformat/Erase, keine ungeprüfte alte Bondübernahme | dokumentierter Vergleich NVS/LittleFS/Bond/Schlüssel/Profile und expliziter inkompatibler Datenpfad; chipgebundener Abgleich vor Upload |
| S6 | offen; gemeinsame konfliktfreie Herkunft und neue Regressionen vorhanden | gesicherter Codecommit, vollständige aktuelle CI und geschlossene bzw explizit sperrende unabhängige Befunde |

## Implementierte Software und Prüfgrenzen

`PinnedPeerTls` besitzt einen serialisierten Kontext, feste BIO-/Anwendungspuffer,
ursprüngliche Registrierung/Verbindungsepoche, feste Handshakefrist und erneute
Lebensdauerprüfung vor Klartext-/Ciphertextfreigabe. TLS-Erfolg verleiht weder
SMP/Bond noch eine Spielrolle. Die verpflichtende finale Verbraucherprüfung
trägt dieselbe ursprüngliche Epoche weiter. Ein veralteter Begin-Auftrag kann
eine aktive neuere Sitzung nicht schließen. [Prüfweg und Herkunft](../test/test_native_tls/README.md).

`NimblePeerSource` hält GAP/GATT/Sicherheit und Ausgabe auf einem Hosttask,
übernimmt Eingänge als feste Kopien und lehnt volle Mailboxen geschlossen ab.
Numeric Comparison verlangt eine ausdrücklich bestätigte ursprüngliche Epoche
und Zahl über eine vertrauenswürdige physische Bedienung; kein automatisches
Pairing-Accept. SC-only, Legacy/Debugkeys aus, 16-Byte-Schlüssel und
verschlüsselt/authentifiziert/gebondet werden verlangt. Bondstore bleibt in
RAM; keine ungeprüften Altdaten werden importiert. Notifyfragmente sind auch
bei MTU23 auf20Bytes begrenzt; der Sender muss Restbytes behalten.

Der current()-Locksnapshot im Hostnotify ist Auswahlpunkt einer bereits
kopierten Ciphertextsubmission. Danach eintreffender Workerentzug sperrt
weitere Auswahl, zieht aber die bereits ausgewählte Übergabe nicht zurück.
Das ist keine Rollenautorisierung und kein Hardware-/Speichererfolgsbeleg.

Der neue Hersteller-NPL-Port kann bei voller Eventqueue Ereignisse ohne
Rückgabefehler verwerfen. Ein ausgefallener Pollpfad wird über das100ms-
Hostlebenszeichen permanent geschlossen; keine SDK-/Queuearbeit im Spinlock.
Ein Hostmodell belegt weder tatsächliche globale HCI-/ACL-Eingangsordnung noch
SMP am Chip. Diese Grenzen bleiben S2-Tore. `firmware/idf-ble` ist zunächst
Compilequalifikation ohne Radio-/Enrollmentstart, keine installierbare Produktfreigabe.

## Tatsächlich ausgeführter Zwischenprüfstand

Am neuen Herstellerkandidaten: 35 CTest-Prüfungen erfolgreich, darunter
29 native Quellmodellfälle, 18 echte TLS-Gegenstellenfälle, öffentliche
ECC-Guardpage-Regression sowie erhaltene Journal-, Steuerungs-, Wire- und
PSA-Suiten. Die 18 TLS-Fälle sind Unterfälle eines CTest-Eintrags.
Der andere Gegenprüfer führte die neue ECC-Regression zusätzlich selbst aus;
keine alten Binärdateien wurden als Negativkontrolle verwendet.
Sieben SDK-Guardfälle und Architekturprüfung bestanden vor dem Pinwechsel;
nach dem Wechsel erneut prüfen und dem finalen Codecommit zuordnen.

Der erste native TLS-SDK-Build am alten Pin war erfolgreich, ersetzt aber
keine Qualifikation des neuen Sicherheitskandidaten. Der neue BLE-Build fand
zunächst Windows-Pfadgrenzen im SDK-Linkerskript und anschließend die im
Herstellerbeispiel gesondert deklarierte Store-Initialisierung. Diese Korrektur
ist vorbereitet; keine erfolgreiche vollständige neue BLE-Imagequalifikation
behaupten, solange der entsprechende Build nicht bestanden hat.

## Weiterarbeit in diesem Paket

1. Finale Host-/ESP32-Builds und SDK-Ablehnungsfälle am aktualisierten Head auswerten;
   Quell-/Format-/Reviewstand sichern und CI an genau diesem Head auswerten.
2. BLE-Quelladapter und TLS-Worker mit begrenzten kopierten Mailboxen zusammenführen;
   Runtime-/Datenkompatibilität für den tatsächlichen Profilpfad qualifizieren.
3. Konkrete isolierte Geräteprobe mit App-/Firmwarezuordnung und Rückweg vorbereiten;
   erst nach eigenem Uploadauftrag durchführen. Fehlende Geräteevidenz offen lassen.
4. Reale Adapter/Profileigentümer und Daten-/Rückwegvoraussetzungen vervollständigen.
   Erst wenn S1–S6 tatsächlich erfüllt sind, Fortsetzungsfreigabe an die BLE-Sitzung
   übergeben; keine Erfüllung aus einem Minimalbuild oder Echo ableiten.
