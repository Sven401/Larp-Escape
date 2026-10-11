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

## Linked source path check (2026-10-11)

The static contract now includes the actual linked call path. Rebuild the
isolated target and inspect its ELF with:

```powershell
pio run -e esp32-gatts-abi-probe
python scripts/check_f1_hci_link_path.py
```

The rebuild passed with Arduino ESP32 2.0.14 and PlatformIO espressif32 6.6.0;
the probe uses 51,552 bytes RAM and 1,094,797 bytes of its 1,310,720-byte
app slot. The link check passed against pinned `libbt.a` (SHA-256
`1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787`).
It found two original GATT registration calls to `__wrap_GATT_Register`,
one original BTU dispatch call to `__wrap_btu_hcif_process_event`,
two original security-result calls to `__wrap_btm_sec_encrypt_change` and
three original SMP-completion calls to `__wrap_smp_proc_pairing_cmpl` in the
finished ELF. The map ties their original implementations to `libbt.a` and
the HCI/SMP wrappers to `F1EncryptAbi.cpp` and the GATT wrapper/connection
callback to `Esp32GattsSource.cpp`. Each original HCI/SMP function has exactly
one direct ELF caller, inside its observer wrapper; `GATT_Register` has two
direct calls inside its wrapper. No additional direct caller
appears in this linked probe. This proves that the **probe's** existing
birth/event checks are on these linked SDK call paths; it is not a product
build or device execution.

The controller Encryption Change event still exposes only status, HCI handle
and enabled state. It has no selected-key, HCI command ID or GATT-birth token.
The original event wrapper narrows its observation to the current birth and
blocks reused handles, but it cannot establish exclusive command-to-result
provenance or a restored-bond MITM mode. No reader or role gate consumes this
link check as authority.

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
