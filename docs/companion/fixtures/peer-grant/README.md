# Original proof-worker grant observation

Public host artifact, 2026-10-07, base a161539. Actual Source/worker/MbedTLS
execution; SDK, task switching and NVS are deterministic models. No measured
RF/SMP/Flash/bond durability, CPU budget or task scheduling guarantee.

Editable producer: `test/test_configured_control/PeerRegistryActorCases.h`,
`registryActorCryptoVertical(true, true)`, new native test only. Set
`LARP_PEER_MBEDTLS_DIR` to the original MbedTLS2.28.4 source root and
`LARP_PEER_MBEDTLS_ARCHIVE` to its pinned official archive; audited inputs are
required. Regenerate with `LARP_EXPORT_PEER_GRANT_FIXTURE=1` and
`pio test -e host-peer-proof`. Normal tests require byte-identical fixture
content without this export variable. LF is pinned by `.gitattributes`.

Consumer: `python scripts/check_peer_grant.py`; use pinned cryptography45.0.7
as in the existing proof counterpart. It independently validates signature,
original boot/registration/handle/token/sequence bytes, typed observation,
three public404Byte records and explicit GM/ACK correlation. Negative inputs
must be rejected. See `../../peer-proof-grant-v1.md` for policy and limitations.
Only public deterministic test credentials are used; never deploy these keys.
