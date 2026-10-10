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

Only `esp32-gatts-abi-probe` links the two observation wrappers. The outer wrapper
enters at `btu_hcif_process_event`, called externally by the original BTU task.
It requires controller zero, event group `0x1000`, offset zero, six payload bytes,
event code `0x08`, four parameters, status zero and enabled value one. Unsupported
packet shapes remain unobserved and are forwarded unchanged to the original SDK.

The source copies the event fields and current GATT birth before SDK dispatch.
Exactly one matching `btm_sec_encrypt_change` invocation may consume that copy
within the same synchronous dispatch. The source rechecks the current birth and
ACL mapping. Scope exit, nesting, argument mismatch and connection changes prevent
reuse. The wrapper discards the result and forwards every invocation unchanged;
it does not retain a security result after dispatch.

The standalone security hook has key-refresh and command-error callers. The new
outer scope distinguishes these from the original Encryption Change event.
`check_f1_hci_origin.py` verifies original header layout, event constants, dispatch
selection, BTU ownership and function ABI against pinned ESP-IDF 4.4.6 sources.

## Authentication boundary

The selected, restored bond mode is not read by this path. Neither a matching
handle, public lease, stored bond nor SDK security flags authorize a peer.
`PeerLinkSecurity` still requires its original pairing start and completion.
Current challenge, P-256 proof, credential and registry ACK remain necessary for
roles on the current connection. A completion without a start stays rejected.

The original `smp_proc_pairing_cmpl` selects `auth_mode` from either the current
pairing control block or the device record and resets the control block before
calling the registered callback. The pinned-source test executes that unchanged
function with explicit model records: successful modes 13 and 29 from fresh
pairing and stored mode produce byte-identical callback data. The callback alone
therefore does not prove that the used mode came from the restored bond. Its
address or strong-looking flags cannot close that provenance gap.

Before enabling reconnection, the implementation must prove restore provenance
and selected SC+MITM+Bond mode on the same original event and connection, then
connect a separately bounded re-encryption attempt to current proof/role checks.
Re-pair, security loss, bond deletion and registry revocation require explicit
invalidation tests. Host/ABI tests and a successful link do not prove radio or
persistent-storage behavior.
