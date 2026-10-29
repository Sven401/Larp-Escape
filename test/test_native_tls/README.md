# Native TLS worker qualification

Source: the immutable feasibility archive in
`handoff/quellarchiv/ble-tls-feasibility-2026-10-28/`. This directory is a new
port; the original archive and its byte hashes remain unchanged.

The executable links exactly `lib/PinnedPeerTls`, also compiled as the native
ESP-IDF `larp_tls` component. The qualified SDK supplies Mbed TLS 4.1.1 and its
TF-PSA backend; CMake verifies its full SDK/submodule pin first. The host uses
that source configuration, while the SDK uses its resolved sdkconfig. These
remain separate evidence. Cryptography 45.0.7 only generates synthetic test
certificates; Python/OpenSSL is the independent actual TLS peer. Scalars 1/2/3
are public TEST VALUES, never provisioning input for a real box.

CMake/CTest registers `native-tls-peer` when `LARP_PSA_IDF_PATH` is supplied.
Install `cryptography==45.0.7` for the configured Python interpreter or expose
an explicitly selected local installation through PYTHONPATH. CI installs this
version. The probe has a 15-second case budget, a 2-second response budget,
child cleanup and a 120-second CTest limit. It writes private generated PEMs
only into a temporary directory, removed after the test process ends.

Eighteen cases cover 1/7/20-byte ciphertext fragments; wrong client/server key;
missing client certificate; tamper; plaintext injection; record replay; expired
handshake across uint32 wrap; revoked original lifetime; stale feed/start
origins; blocked output with caller buffer replacement; full input queue;
revocation of queued ciphertext; partial-record abort; and ciphertext replay
into a new TLS session. Peer authentication grants no GM role. TLS1.2 with
ECDHE-ECDSA/AES128-GCM and P256 is pinned for this cut; tickets, session cache
and renegotiation are disabled. Exact expected leaf SPKI is trusted, with no
intermediate/hostname fallback. Other X509 error flags remain effective.

The worker owns one serialized TLS context and bounded application/BIO buffers.
Its `ITlsLifetime` must be a trusted thread-safe observation that remains false
for revoked origins. A live session cannot be implicitly replaced by begin;
replacement requires explicit close. Ciphertext and plaintext users must carry
the same origin through their final host/role-owner check. TLS reads never
assert original SMP, a bond or a role. Native BLE coupling, clock/provisioning,
credential storage, actual resource budgets and device behavior remain open.
