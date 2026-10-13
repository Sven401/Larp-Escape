# Original-bound proof writes and authentication result v1

Status: opt-in hardware-free SDK/worker contract, 8 October 2026. Continues
`peer-challenge-issue-v1.md` and `peer-challenge-read-v1.md`. The production
controller does not yet start this transport. SDK tasks/security are modeled;
the native vertical test executes actual MbedTLS 2.28.4 P256 verification.

## Client sequence

On the same protected connection, issue the stored peer selector (UUID suffix 6),
read the 108-byte challenge (suffix 5), and sign that exact transcript. Check its
device, original binding and peer against the intended pinned server. Submit the
existing 72-byte canonical low-S P256 proof using suffix 7, then read suffix 8.
No command has a delivery guarantee; a queued write or a valid signature alone
does not prove that a role was granted. This slice adds no mutual server proof.

Proof characteristic: `407e6a1d-1cc2-43fd-856b-4aff85a20007`, WRITE_NR,
WRITE_ENC_MITM, BY_APP, maximum 128 bytes. Ordinary ATT Write Commands only,
offset 0, no prepare/execute or Write Request. Existing frame header: version 1,
kind 10, LE16 nonzero message, LE16 total 72, LE16 byte offset, followed by
1..120 proof bytes. Sequential nonoverlapping fragments must use one message
and total. At modeled MTU 23, six 20-byte frames carry 12 bytes each. The server
uses the existing bounded receiver, whose duplicate and failure policy remains
authoritative. See `peer-proof-v1.md` for signature/transcript encoding.

Result characteristic: `407e6a1d-1cc2-43fd-856b-4aff85a20008`, READ,
READ_ENC_MITM, BY_APP, maximum 32 bytes. The result is a raw value, not a frame.

| Bytes | Meaning |
|---|---|
| 0, 1 | Version 1, kind 12 |
| 2, 3 | State and reason below |
| 4..5 | LE16 proof message |
| 6..7 | LE16 proof bytes received, at most 72 |
| 8..11 | LE32 nonzero challenge sequence |
| 12..15 | LE32 principal; nonzero only for Granted |
| 16..31 | Nonzero 16-byte public peer identity |

| State | Meaning and invariants |
|---|---|
| 1 Receiving | 1..71 bytes, reason 0, principal 0 |
| 2 Rejected | Reason 1 frame, 2 proof, or 3 grant rejection; principal 0 |
| 3 GrantPending | All 72 bytes verified, awaiting original actor ACK; reason/principal 0 |
| 4 Authenticated | Original actor ACK confirms authentication without GM role; reason/principal 0, 72 bytes |
| 5 Granted | Exact original actor ACK confirms GM principal; reason 0, 72 bytes |

Require message, peer and challenge sequence to match the submitted proof.
States 4/5 require a current credential, exact original grant job/ACK and actual
current channel authority. A result cannot grant rights by itself. Terminal
success ignores further proof writes on that original connection, preserving
the accepted message and avoiding a second grant.

## Reads, lifetimes and failure

An ordinary Read at offset 0 pins the latest result for subsequent ReadBlob
requests. Blob fragments continue that same value even if proof/grant progress
changes; another ordinary Read gets the latest value. The pin has an absolute
1000 ms lifetime. Offset 32 is a valid empty suffix; other offsets above 32, or
a nonzero ordinary Read offset, return ATT status 7. Missing/expired/unavailable
results return status 8 when the original scope remains valid. Security loss,
registry fencing, reconnect or credential change drops the old reply; clients
must not interpret a timeout as authentication success or automatic retry.

The challenge's original 30000 ms deadline remains absolute. Neither fragment
arrival nor Read/ReadBlob refreshes it. Result expiry does not revoke an already
confirmed role; the established authority lifecycle remains authoritative. A
fresh original connection is needed for a new challenge/proof attempt.

## Server ownership and installation

Earliest SDK callbacks copy bounded bytes and original device/boot/registration/
interface/connection birth/lifecycle/address before external clock observation.
Four static proof requests and four static reads use zero wait on the existing
crypto/grant worker; no task or timer is added. A service call drops at most four
stale entries per queue and processes at most one current proof, read or grant
observation. Each request must remain younger than 1000 ms from source capture.

A single owned reply slot posts at most one original BTU callback; nonwrapping
opaque tokens prevent an old callback from consuming a new reply. Before sending,
the source rechecks scope, deadlines, credential, actual SDK security and, for
successful states, exact grant ACK/current authority. No pointer into the SDK
request or response escapes. Posting and SDK SendRsp do not claim peer delivery.

`PeerChallengeReadSetup::auth` opts into the full thirteen-handle service together
with context/read/issue/boot. Creation order is command, profile, challenge,
issue, proof, result. Original source installation ACK must precede service start;
partial bundles, overlapping/out-of-range handles and late configuration fail
closed. Existing five/seven/nine-handle services remain available.

## Verification and remaining acceptance

`pio test -e host-configured-control` uses SDK/RTOS models against the actual
source and worker code. `pio test -e host-peer-proof` additionally audits official
MbedTLS 2.28.4 and executes the whole SDK issue/read/proof/ACK/result/blob path.
`scripts/check_peer_auth.py` independently encodes client fragments, verifies the
captured proof with cryptography 45.0.7 and checks result/blob correlation and
malformed inputs. See `fixtures/peer-auth/README.md` for provenance and commands.

Real ESP32 entropy, RF/SMP timing, Android reads/writes, task stacks/heap and
delivery remain unmeasured. Next: original-bound bond reconnect evidence, mutual
server proof and trusted setup before production/app integration.
