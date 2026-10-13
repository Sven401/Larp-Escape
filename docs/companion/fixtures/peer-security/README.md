# Original SDK security observation

Public host artifact; actual adapter/P-256 execution with modeled SDK/task/NVS.
No measured SMP/RF/bond durability. Base9df1d36, 2026-10-07.
Editable producer: test/test_configured_control/PeerRegistryActorCases.h,
registryActorCryptoVertical(true), new native test only. Regenerate with
LARP_EXPORT_PEER_SECURITY_FIXTURE=1 and audited host-peer-proof environment.
Normal native tests require exact bytes; .gitattributes pins LF.
Independent consumer scripts/check_peer_security.py rejects53 wrong values,
types and record/proof correlations. SDK ABI/mode checks are independent in
scripts/check_peer_security_abi.py. See docs/companion/peer-security-sdk-v1.md.
No private key, device secret or measured-throughput claim is present.
