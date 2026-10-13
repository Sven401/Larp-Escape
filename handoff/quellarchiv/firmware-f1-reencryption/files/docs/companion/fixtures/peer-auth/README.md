# Native peer authentication observation

`observation.json` is generated from the actual SDK callbacks/owned worker copies
and SDK response model in
`test_auth_actual_crypto_sdk_issue_reads_proof_writes_ack_and_result_blobs`.
It captures issue bytes, challenge, six proof frames, grant-confirmed result and
a pinned blob suffix, plus original SDK read scope. It uses the existing public
test generator vector from `../peer-proof`; that key must never be deployed.

The native test executes audited official MbedTLS 2.28.4. SDK/task/security/nonce
are deterministic models; RF is not initialized and no device is measured.
No queued operation is labeled as delivered. Normal runs compare all observation
bytes to this LF fixture. Only explicit `LARP_EXPORT_PEER_AUTH_FIXTURE=1` regenerates
it; export once, inspect, then run again without that variable.

With the existing audited `LARP_PEER_MBEDTLS_DIR` and archive configured:

```text
pio test -e host-peer-proof
python scripts/check_peer_auth.py
```

The independent Python checker uses cryptography 45.0.7, reconstructs actual
SDK-copied proof frames, verifies P256 and validates strict result states,
message/peer/sequence/principal and pinned suffix correlation. Mutation checks
cover every observation byte and each proof frame byte. Neither test proves
radio security, scheduling latency, heap capacity or Android behavior.
