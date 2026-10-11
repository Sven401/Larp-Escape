# F1 bond provenance: critical precheck (2026-10-11)

The requested `grilling` skill tool is unavailable in this session. This
precheck was recorded before extending the F1 implementation.

**Goal.** Determine whether the original ESP-IDF v4.4.6 SDK can tie its full
stored LE authentication mode and selected bond key to the exact current
encryption event and GATT connection birth. A useful result may be a narrower
negative or conditional provenance result; it does not need to open access.

**Assumptions to challenge.** The official v4.4.6 source corresponds to the
pinned Arduino ESP32 `libbt.a`; the private `tBTM_SEC_DEV_REC` layout for the
actual configuration may be compiled independently; the record selected by
the LTK/encryption path may differ from the record found later by the SMP
completion address; and source-order callbacks may not be birth-bound once
queued. None of these assumptions is an authorization fact.

**Known counterevidence.** The actual `smp_proc_pairing_cmpl` binary loads
`ble.auth_mode` as 16 bits but passes an 8-bit callback field. `0x000d` and
`0x010d` both yield callback 13. The source restores device metadata and bond
keys through separate BTA messages. Master encryption uses PENC/pltk; slave
LTK reply uses LENC/lltk. The existing wrapper copies SMP control metadata,
not the full device record or the selected key. The production BLE owner also
exceeds the current app partition, so a probe cannot imply a shippable path.

**Security boundary.** No raw key bytes leave the SDK, enter fixtures or logs,
or become an application claim. No private offset is read in firmware without
an exact original-header, target-configuration and linked-binary check. A
source model, matching address, key mask, encryption-success callback or
stored role cannot grant reconnect, game-master or diagnosis authority.

**Acceptance gate.** A narrow host test must distinguish a full 16-bit mode
from the truncated callback and distinguish the candidate key record from an
address-matched but different record. It must reject missing, stale and
ambiguous original bindings. ABI assertions must be derived from pinned
original declarations and relevant target binary instructions. Positive F1
admission remains closed until actual selected-key use, same-birth HCI/GATT
correlation, current proof/credential and registry ACK are all established.
