# Öffentliche Challenge-Leseprüfung

Status: hardwarefreier Prüfwert aus tatsächlichem nativen C++lauf, 7. Oktober2026.
Basis vor Schnitt a3d1667; keine Produktionsschlüssel oder Geräte/Funkmessung.

Quelle: `test/test_configured_control/PeerChallengeReadCases.h`, Fall
`test_challenge_read_actual_crypto_from_original_sdk_blob_snapshot_to_grant_ack`.
Er erzeugt fünf tatsächliche `GATTS_SendRsp`-Parameterwerte im unabhängigen
SDKmodell aus Originalreads und einem einzigen tatsächlichen Proofownersnapshot.
Die öffentlichen Challenge-/Schlüssel-/Signaturinputs in `../peer-proof/` bleiben
unverändert. Tatsächliches MbedTLS verifiziert den Proof; Originalgrant und
SDK-/SMPprüfungen ergeben gespeicherte GMzuordnung/ACK. Tasks/SMP/NVS/SDK sind
Modelle; `peerDelivered` und `rfMeasured` sind ausdrücklich false.

Regeneration: beide geprüften `LARP_PEER_MBEDTLS_DIR` und
`LARP_PEER_MBEDTLS_ARCHIVE` setzen; temporär
`LARP_EXPORT_PEER_CHALLENGE_READ_FIXTURE=1`, dann
`platformio test -e host-peer-proof`. Normaler Lauf ohne Exportvariable vergleicht
exakte LFbytes der Fixture. `.gitattributes` hält den Wert reproduzierbar.

Unabhängige Gegenstelle: Python `scripts/check_peer_challenge_read.py` mit
cryptography45.0.7. Sie prüft Originaltransaktion/Handle/Offsets, sämtliche
verbleibenden SDKbytes, stellt mit dem dokumentierten MTU23-Größenmodell108Bytes
zusammen und prüft echte P256/SHA256-Signatur.83 geänderte Werte/Typen/
Korrelations-/Scopebehauptungen werden verworfen. Keine Größenrechnung als
beobachtete Luftübertragung ausgeben. Source-/Read-/Responseabi unabhängig gegen
zwölf SHAgeprüfte offizielle IDF4.4.6-Dateien kompiliert.
