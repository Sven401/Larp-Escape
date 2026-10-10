# HCI birth observation

This internal F1 prerequisite compares an SDK security-hook invocation with the
current GATT connection. It does not enable bond reconnection authentication.

On the original BTU connection callback, `Esp32GattsSource` captures the current
transport binding after `GATT_GetConnectionInfor` and `BTM_GetHCIConnHandle` agree
on peer, interface and LE transport. It checks the binding again after each SDK
lookup. HCI and GATT handles remain separate values.

`PeerHciBirthWitness` accepts a 12-bit HCI handle only once per source lifetime.
The process owns one source instance. Closing a connection invalidates its active
witness without clearing the used-handle bitmap. A recycled handle stays closed
until restart; this deliberately limits availability while old HCI events lack a
separate generation identifier. The bitmap consumes 512 bytes per source.

The observation repeats the ACL lookup on the original BTU actor and requires the
same transport birth, status zero and enabled value one. It returns a transient
comparison result and retains no security state. Failures, duplicates and later
comparisons therefore cannot act as a stored encryption or role grant.

Only `esp32-gatts-abi-probe` links the security-hook wrapper. The wrapper discards
the observation and forwards every invocation unchanged to the pinned SDK.
`btm_sec_encrypt_change` also has key-refresh and command-error callers: its
arguments alone do not identify the originating Encryption Change event.

## Authentication boundary

The selected, restored bond mode is not read by this path. Neither a matching
handle, public lease, stored bond nor SDK security flags authorize a peer.
`PeerLinkSecurity` still requires its original pairing start and completion.
Current challenge, P-256 proof, credential and registry ACK remain necessary for
roles on the current connection. A completion without a start stays rejected.

Before enabling reconnection, the implementation must prove original event kind,
restore provenance and selected SC+MITM+Bond mode on the same connection, then
connect a separately bounded re-encryption attempt to current proof/role checks.
Re-pair, security loss, bond deletion and registry revocation require explicit
invalidation tests. Host/ABI tests and a successful link do not prove radio or
persistent-storage behavior.
