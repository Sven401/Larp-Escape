# Challenge-issue fixture provenance

`observation.json` is exported by the actual C++ native test
`test_issue_actual_crypto_original_sdk_write_to_read_and_existing_proof_grant`
in `test/test_configured_control/PeerChallengeIssueCases.h`. `issueHex` comes from
the recording ingress reached by the actual source callback before the static
worker. `challengeHex` and read metadata come from the actual adapter's
`GATTS_SendRsp` intercepted by the SDK model. Grant principal comes from its ACK.

Regenerate once with `LARP_EXPORT_PEER_CHALLENGE_ISSUE_FIXTURE=1` and the required
audited `LARP_PEER_MBEDTLS_DIR` / `LARP_PEER_MBEDTLS_ARCHIVE`, running
`pio test -e host-peer-proof`. A normal run, without export, compares exact LF
bytes. Existing public challenge/key/proof vectors are reused unchanged; no
private key is introduced. Native verification uses real MbedTLS2.28.4, while
source tasks, SDK/SMP/NVS, captured scope and nonce are deterministic models.

The independent Python checker uses its own 18-byte client encoder and
cryptography45.0.7 to verify byte/scope correlation and the P256 signature. It
rejects altered metadata/types and malformed issue commands. It does not prove
RF delivery, Android execution, SDK scheduling or production resource reserve.
