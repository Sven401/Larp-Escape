# Original-bound challenge issue v1

Status: hardware-free opt-in SDK/worker slice, 8 October 2026. Production startup,
RF and Android execution remain unimplemented/unmeasured. This adds the client
challenge request preceding the existing `peer-challenge-read-v1.md` flow.

## Client contract

Optional characteristic `407e6a1d-1cc2-43fd-856b-4aff85a20006`, little-endian
SDK UUID from `GattsServiceDefinition::issue()`. Property WRITE_NR, permission
WRITE_ENC_MITM, response BY_APP, maximum value 18 bytes. Use an ordinary ATT
Write Command (no response), offset 0; prepared writes/execute, Write Requests,
other lengths and attributes do not enter the issue worker. There is no ATT
success/acceptance reply for a Write Command and no automatic retry.

| Bytes | Meaning |
|---|---|
| 0 | Version 1 |
| 1 | Kind 11, challenge issue |
| 2..17 | Nonzero opaque 16-byte public peer identity in stored byte order |

The selector carries no public key, generation, role, connection or security
claims. The worker resolves the key/generation from the protected persisted
registry publication. Unknown/unavailable peers produce no challenge. Read the
existing challenge characteristic (UUID suffix 5) on the same connection to
observe success. Its 108-byte transcript must match the requested peer and
the pinned server/connection; subsequent blob reads preserve its nonce/deadline.
Read and worker scheduling may race; a missing challenge is not evidence of
successful issue. No new delivery guarantee or mutual server authentication.

## Original SDK and worker ownership

The selected earliest SDK type-2 callback freezes scalars and exactly 18 owned
bytes, then the original boot/device/registration/interface/connection birth
token/lifecycle/address before its first clock port. Foreign, recycled, closed,
wrong-task or changed-SDK scope cannot adopt a fresh binding. Callbacks only copy;
they do not decode credentials, make entropy, validate keys or assign GM rights.

Four static copied requests, zero queue wait. Same already-bound crypto/grant
worker; its first failed binding stays failed. Each service call discards at most
four stale entries and processes at most one current request. Local admission
age is strictly below 1000 ms from the source capture and is rechecked after
registry lookup, with unsigned rollover arithmetic. This is policy, not measured
radio throughput or an ATT timeout. Work already accepted into the proof owner
uses that owner's independent absolute challenge deadline and post-crypto checks.

The existing owner permits one issue attempt per original connection, including
entropy/key failure or expiry. Repeated commands never regenerate a nonce or
restart the 30000-ms challenge deadline. A fresh original connection is needed
for another attempt. Registry closure/credential change and connection loss
invalidate the old scope. Issue alone never grants a role; positive read output
still requires the existing original SMP evidence and current SDK security.

## Optional service installation

Legacy five-handle and challenge-only seven-handle setups remain available.
`PeerChallengeReadSetup::issue` opts into nine handles. Creation order is command,
profile, challenge, issue; actual returned attributes must be strictly increasing
inside the reserved range. Freeze the bundle and require source installation ACK
before starting. Partial issue/read/context/boot bundles, wrong handles, late
configuration and SDK failures fail closed without rebinding or retry. Existing
service phase values retain their numbers; Issue is appended.

## Reproduce and boundaries

`pio test -e host-configured-control` compiles the real SDK source/worker against
deterministic SDK/RTOS models. Tests cover copying before borrow mutation/clock
closure, queue bounds, old connections, bad lengths/modes, wrong workers,
registry closure, entropy failure, duplicate/expired issue and optional startup.
`pio test -e host-peer-proof` additionally uses audited official MbedTLS 2.28.4
sources: original SDK Write Command -> stored key -> issue -> original SDK read
-> real P256 proof -> existing original-actor grant ACK. The proof submission in
this slice is the existing worker API, not a client SDK proof-write endpoint.

See `fixtures/peer-challenge-issue/README.md` and independent Python client/tool
`scripts/check_peer_challenge_issue.py`. The ESP32 ABI probe links the actual
closed source/worker/service paths; it starts no radio or new task. SDK allocation,
full task stacks/heap, real entropy, RF/SMP timing and delivery require separate
device acceptance. Production controller stays at its existing implementation.

The proof-write/result extension is documented in `peer-auth-result-v1.md`.
Next: bond reconnect/mutual server proof/trusted setup and production integration.
