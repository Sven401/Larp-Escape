# Original SDK security evidence (ESP-IDF4.4.6)

Aktuelle Fortsetzung: [Verifizierter Workerauftrag zum Originalaktor](peer-proof-grant-v1.md).
Die nachfolgenden Prüfzahlen und offenen Punkte beschreiben den jeweiligen
historischen Schnitt; dessen Nachweise werden hier nicht neu behauptet.

Status: narrow firmware/tool implementation, host checked; production pairing,
wire authentication attributes and device/RF acceptance remain open. Base9df1d36.
No configuration/proof wire format or Appcode changed in this cut.

## Origin and grant

One process-lifetime selected source is armed before Bluetooth initialization.
Only the isolated SDK target wraps SMP_Register, retaining the existing core
callback. Every event, address/data pointer and result is forwarded unchanged.
The original BTU/source actor captures start events1/2 and the first5 bytes of
terminal event7 **before** BTM/BTA/BTC forwarding. It never signs, reads NVS,
allocates an input buffer or grants rights in the callback. Registration once;
failure/retry/SDK instance replacement stays closed. No SDK patch or private
security-record prefix cast. No late joining of queued GAP events by MAC/handle.

PeerLinkSecurity owns one fixed pairing attempt: device, exact connection token
and packed handle, six original address bytes, registration, interface and
lifecycle. An original start is mandatory. Completion success0/security level4,
no cancel/BR, resolved SC8+MITM4+bond1 (optional keypress16) are required. Requested
SC or the encrypted/authenticated flags alone cannot prove the selected mode.
Original SMP code computes the selected association model; terminal assembly may
also use stored ble.auth_mode. The reader does not independently validate bond
persistence. Missing original start/reencryption-only restoration stays closed.

A repeated start cannot extend the 30000-ms absolute attempt deadline. Re-pairing
invalidates prior mode. Disconnect or expiry while an attempt is pending retains
an abandoned attempt until its original-address terminal callback is drained.
No new attempt can overwrite it, even on a recycled handle/address. If that
terminal never arrives, admission remains closed for this source lifetime;
restarting a live SDK is not a supported recovery. This conservatively handles
the fact that SMP callbacks expose no application attempt token.

The sole source actor calls readOriginalPeerSecurityNow at grant on a frozen
trusted proof binding. It checks actual GATT connection/interface/LE transport
and address, no current security procedure, BTM flags authenticated/encrypted/key
known, LE state encrypted+MITM and 16-byte key, then no security procedure again.
Every port is outside the mux; original lifetime, policy, published authority and
mutable address bytes are rechecked between ports. No publication on false;
output remains unchanged. A failed/re-pair original SMP result rejects the exact
owned IdentityAccess link before forwarding and preserves another publisher.

The reader binds only transport evidence. Cryptographic key possession, captured
boot/proof freshness and current protected credential must be checked separately
before/after this read. The native tool case copies a real verified proof from
the separate crypto model worker; the source actor rechecks original context,
credential and proof deadline, combines evidence and authenticates. A reader
success alone grants no GM role. Stored explicit role assignment is mandatory.
A production asynchronous verified-proof handoff and captured boot context are
still open; the mutable crypto owner is never read across workers in this tool.

## Independent SDK evidence

scripts/check_peer_security_abi.py pins11 official ESP-IDFv4.4.6 sources and
extracts the actual complete scalar types,5 field offsets, callback and5 C API
signatures. An independent compiler executes original smp_get_auth_mode for10
association models (legacy/SC JustWorks/SC MITM/out of range). Completion size5,
alignment1; opaque union pointer only, no whole-union allocation/layout claim.
Resolved-to-terminal dataflow is checked in the pinned original source. Installed
SDK libbt.a and libmbedcrypto.a remain guarded by the existing build pins.

Inputs from official https://github.com/espressif/esp-idf/tree/v4.4.6/components/bt
are separately downloaded, checked against the official recursive Git tree blob
SHA1 and pinned SHA256. They are read-only external inputs, not edited SDK files.
Run:

```text
python scripts/check_peer_security_abi.py --source-dir <official-security-source>
  --gatt-source-dir <official-lifecycle-source> --compiler <independent-g++>
python scripts/check_peer_security.py
pio test -e host-configured-control
pio test -e host-peer-proof
pio run -e esp32-gatts-abi-probe
```

Native environment: audited official MbedTLS2.28.4 include/library sources and
pinned archive via LARP_PEER_MBEDTLS_DIR/LARP_PEER_MBEDTLS_ARCHIVE. Independent
record/P-256 counterpart uses cryptography45.0.7. No fake crypto fallback.

## Public vertical observation

fixtures/peer-security/observation.json is emitted by exactly
`test_security_actual_crypto_original_smp_metadata_frames_game_and_revoke` with
LARP_EXPORT_PEER_SECURITY_FIXTURE=1; normal native tests reread it byte-for-byte.
One original registration, three SMP callbacks, five same-actor SDK metadata
reads, six actual MbedTLS proof frames, thirteen original command/ATT frames,
stored GM restore and confirmed revoke. Actual game runs and remains running
after old proof/route rejection. SDK callbacks, tasks and NVS are explicit host
models. Independent Python counterpart correlates the observation with three
public registry records/proof metadata and rejects53 malformed correlations.

Public generator key is a test fixture, never commission it on a real device.
RF/SMP timing, actual pairing UI, entropy, physical bond/NVS durability, mutually
authenticated server identity and production startup remain open. Existing bond
reencryption, pre-GM challenge/proof/result attributes, original output/CCC,
live security loss on all authenticated traffic and application adapters must
be added in small vertical paths. No board upload or COM access in this cut.

## Resource scope

The kernel and callback use fixed byte copies. No own heap or NVS in SMP/source
reads; SDK internals are separate costs. Source actor exclusively owns the kernel.
Mux sections only copy the fixed policy snapshot; SDK calls occur outside.
30000 ms is a cancellation policy, not measured pairing latency. Final compiler
sizes/frames/builds are recorded separately in the project Werkbank. The closed
SDK probe links the reader and wrappers but starts no SDK, pairing, radio or NVS.
Separate target build sizes do not prove a combined production reserve.

## Minimal App–ESP pairing observation contract (pending product wiring)

An LE connection and an Android OS bond may be established while every GM and
diagnosis route remains closed. The pairing report must distinguish these facts:

1. `connected`: the original selected GATT connection callback opens one fresh
   `PeerTransportBinding` on the BTU/source actor. Its packed GATT handle,
   connection token, registration, interface and lifecycle form an opaque local
   birth identifier. Disconnect closes that birth. Do not join a later event by
   address or recycled HCI/GATT handle alone.
2. `os_bonded`: Android may report its own bond state, with its own timestamp and
   attempt identifier. This is an App observation only; it is not firmware bond
   persistence, encryption, MITM, GM role or diagnosis permission.
3. `fresh_smp_complete`: for this exact open birth, the original `SMP_Register`
   callback has delivered start event 1 or 2 followed by terminal event 7. Copy
   the five result bytes before forwarding: reason, security level, cancelled,
   over-BR and the SDK's **8-bit** mode. Success requires reason 0, level 4,
   cancelled 0, over-BR 0 and an allowed fresh-pairing mode. The callback does
   not carry an App attempt token or a full restored-bond mode.
4. `live_link_secure`: on the same original actor and birth, the SDK reader
   verifies the current LE connection, no active security procedure, BTM
   authenticated/encrypted/key-known flags and LE encrypted+MITM state with a
   16-byte key. This is volatile link evidence after fresh pairing; it does not
   prove selected stored-key provenance or durable NVS bond storage.

For reconnect, report `restored_bond_unverified` until a separate immutable
selected-key/full-16-bit-mode witness is tied to that GATT birth and the
controller result. The existing callback low byte, address, HCI enqueue and
Android `BOND_BONDED` cannot supply this witness. Keep the role and diagnosis
routes closed even if steps 1–4 succeed: a current protected credential,
verified P-256 proof, explicit GM assignment and registry install ACK are
separate prerequisites. The report should use opaque per-boot birth/attempt IDs
and outcome codes, never key bytes or private SDK records. No product report
characteristic/UUID or physical pairing result is implemented by this F1 cut.

## F1 pre-reset observation (closed probe)

The isolated ABI probe additionally wraps original `smp_proc_pairing_cmpl`.
`check_f1_smp_prereset_abi.py` extracts the complete unmodified 720-byte
`tSMP_CB`, its dependencies and the original function signature from six pinned
IDF4.4.6 headers. Xtensa compilation verifies seven metadata offsets; the pinned
`libbt.a` instructions independently confirm those loads and reset-before-callback
ordering. A 32-bit host run checks the actual narrow reader against that original
type. No private keys, nonce or OOB fields are read by the adapter.

Only our registered SMP callback, successful uncancelled LE completion and zero
fresh-pairing mode can enter the observation. It requires the original BTU actor,
the same GATT birth and a matching successful Encryption Change dispatch. A copy
of pre-reset metadata survives the SDK reset only until that synchronous call
returns. The callback byte must match the copied address/security metadata and
be 13 or 29. Wrong task, nested dispatch/completion, repeated hook/completion,
changed ACL handle or changed birth prevents reuse. Arguments still pass to the
original SDK unchanged. No private SDK pointer is retained.

This proves only the source branch of the selected **callback byte**. The SDK
loads a 16-bit device-record mode and truncates it to 8 bits. The unmodified
original completion function maps both 0x000d and 0x010d to 13 (and 0x001d and
0x011d to 29). Thus neither the complete stored mode, NVS restoration, nor the
actually selected bonded key is proven. The observation is discarded; it never
sets link security, reconnect readiness, a role or diagnosis authority.

The F1 negative regression `test_security_restored_mode_low_byte_alias_never_starts_pairing_or_grants`
passes all four full-mode values through the callback-byte narrowing. Even with
synthetically favorable link flags, key length and proof binding, a completion
without an original pairing start cannot select transport security. This is a
closed host check, not a positive bond or role test. The 16-bit mode and the
PENC/LENC key selected by the original actor still need a birth-bound source
before restored-bond admission can be implemented.

### Restored record and key path audit (2026-10-11)

The official ESP-IDF v4.4.6 source narrows the remaining question but does not
close it. `btm_int.h` declares `tBTM_SEC_BLE.auth_mode` as `UINT16`, alongside a
key-type mask and separate peer/local LTK fields. `btc_ble_storage.c` writes the
persisted mode through a `uint8_t` argument into an integer setting. On restore,
it reads that integer when it first finds a key, posts `BTA_DmAddBleDevice`,
then posts each available key. `bta_dm_add_ble_device` passes the mode to
`BTM_SecAddBleDevice`, whose `UINT32` parameter is assigned to the record's
`UINT16` field; `BTM_SecAddBleKey` later populates key fields on the record.
This establishes the source-level restoration route, not a measured NVS value
or the complete private record layout in the linked binary.

The original source has two different encryption-key paths. As master,
`btm_ble_start_encrypt` requires `BTM_LE_KEY_PENC` and passes the record's
`rand`, `ediv` and `pltk` to the controller. As slave, an LTK request resolves
the record by handle; absent an active SMP request, its reply requires
`BTM_LE_KEY_LENC` and passes `lltk`. The SMP completion fallback separately
finds a record by pairing address and copies only its low mode byte into the
callback. None of these source-level observations proves that this callback
used the record and key selected for the current GATT birth. A same-address
record, a stored key and an encryption-success event cannot substitute for
that binding.

Source pins, from the official v4.4.6 Git tree: `btm_int.h`
`57854e0a915dc3b08187d1f1b63a9704aa49bf0f`, `btm_ble.c`
`c236d5d8aaf72f894d8977b9e900512e6d236c75`, `btc_ble_storage.c`
`c4b67c4f1ab551dd13a5f1261cf74039de044932`, `bta_dm_act.c`
`41a5da662cf94a09af8ecc2820997f29bd461817`, and `smp_act.c`
`f77085870a25a0928fcd9603a9093e3d65b0da0b358a9ba55c4ed06b424dec12`.
The local ESP32 `libbt.a` still hashes to
`1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787`.
At this audit stage the full `tBTM_SEC_DEV_REC` configuration, offsets and
selected-key path had not been matched to its actual instructions. The later
static archive check below narrows this limit. No private record reader,
positive F1 admission or diagnosis bypass follows from either check.

### Pinned key-selection source model (2026-10-11)

`python scripts/check_f1_bond_key_origin.py` verifies the pinned upstream
`btm_int.h` and `btm_ble.c` bytes and the installed ESP32 `libbt.a` hash, then
compiles and executes the unchanged upstream `btm_ble_start_encrypt`,
`btm_ble_ltk_request` and `btm_ble_ltk_request_reply` functions in a private
host record model. It checks that master encryption selects `pltk`, `rand`
and `ediv` from the address-resolved record only with PENC; a slave request
resolves by handle and, if SMP does not own it, replies with that record's
`lltk` only with LENC. Missing keys and unknown handles do not yield a key
reply. Different modeled records and full `auth_mode` values are kept distinct.

This is a source-behavior check, not an ABI check: the private model deliberately
does not declare the complete `tBTM_SEC_DEV_REC`. The original header confirms
the field types and conditional layout. The separate static archive audit
below verifies relevant compiled offsets and branches. No observed HCI command,
controller key acceptance, GATT birth,
restored NVS record or live bond is part of the test. The test must not be used
to read private record offsets or grant access. A genuine selected-key-to-birth
binding still requires proof at the original actor and a user-driven device
report, alongside the current proof and registry acknowledgement.

### Static ESP32 archive ABI and branch audit (2026-10-11)

`python scripts/check_f1_record_binary_path.py` hashes the installed ESP32
`libbt.a` and the pinned original header, extracts only `btm_ble.c.obj`, pins
that object's SHA-256, and checks the Xtensa disassembly plus linked ABI-probe
map. The inspected build uses PlatformIO `espressif32@6.6.0`, Arduino ESP32
`2.0.14`, `az-delivery-devkit-v4` and Xtensa GCC 8.4.0
(`esp-2021r2-patch5`). The probe environment uses `-std=gnu++11`,
`-I include`, `-Wall`, `-Wextra`, five explicit `--wrap` flags and source
`-Werror` from `platformio.ini`; no SDK archive or source is patched.

The pinned binary's `BTM_SecAddBleDevice` allots and zeros `0x148` (328) bytes
for `tBTM_SEC_DEV_REC`. Its BLE subrecord begins at byte 176; it stores the
restored mode with a 16-bit instruction at byte `0x130` (304). The inspected
instructions also use these record offsets:

| Field or action | Record offset | Static binary evidence |
| --- | ---: | --- |
| `sec_state` | 150 | 8-bit state read in master encryption |
| LE HCI handle | 166 | 16-bit read in both key paths |
| BLE key-type mask | 199 | PENC bit 0 / LENC bit 4 branches |
| `pltk` | 216 | pointer passed to master start-encryption HCI call |
| `lltk` | 248 | pointer passed to slave positive LTK reply |
| `rand` | 280 | pointer passed to master start-encryption HCI call |
| `ediv` | 288 | 16-bit value passed with `rand` and `pltk` |
| key size | 293 | byte read for slave LTK reply state |
| `auth_mode` | 304 | 16-bit restore store |

`btm_ble_ltk_request` calls `btm_find_dev_by_handle`; the master path calls
`btm_find_dev` by address. The archive contains both positive and negative
slave LTK replies. These checks establish the archived binary's relevant
record size, offsets and branches, and that its object is linked in the probe
ELF. A fresh regular product build with `env:az-delivery-devkit-v4`
(`-std=gnu++11 -I include`) succeeded at 1,229,077 / 1,310,720 flash bytes
and 55,148 / 532,480 RAM bytes. Its map contains the controller's `bt.c.obj`
from `libbt.a` but **does not link `btm_ble.c.obj` or these F1 host-key
functions**. Reproduce the negative check with:

```text
python scripts/check_f1_record_binary_path.py --map .pio/build/az-delivery-devkit-v4/firmware.map --expect-no-host-ble
```

Consequently no productive F1 host-key branch is selected in this build.

The audit does not compile the *complete* upstream record type under proven
private Arduino feature macros: the original declaration has conditional
fields and the installed Arduino SDK omits its internal `btm_int.h`. The
archive instructions are stronger for these particular offsets than a host
model, but they do not prove every field of the structure or that the upstream
source matches the archive in all details. Most importantly, a static branch
does not reveal which restored record/key a live controller used for a given
GATT birth in a later BLE-enabled build. No private-offset reader, F1
authorization or diagnosis privilege is enabled by this audit.

Before positive F1 admission: independently audit the complete device-record
ABI and full-width selected mode/key provenance at the original actor, connect
the bounded reencryption state machine including loss/re-pair/bond deletion,
and require current challenge/P256/credential/registry acknowledgement on the
same birth. F2 must provide the production BTU owner and protected worker-to-
actor handoff. Existing bond records, cached GM assignment, lease and strong
flags alone never open diagnosis. The wrappers remain exclusive to the probe;
no radio, production pairing or hardware acceptance is claimed.

### F1 selected-key provenance gate (follow-up audit, 2026-10-11)

The pinned `btm_int.h` declares both `btm_find_dev(BD_ADDR)` and
`btm_find_dev_by_handle(UINT16)`, and the installed pinned `libbt.a` exports
both symbols. In the original `btm_ble.c`, master `btm_ble_start_encrypt`
resolves the record by address and passes its PENC `pltk`/`rand`/`ediv` to
`btsnd_hcic_ble_start_enc`; slave `btm_ble_ltk_request` first resolves by
handle and, when SMP does not own the request, `btm_ble_ltk_request_reply`
resolves by address and passes its LENC `lltk` to the HCI reply. The static
archive audit confirms the relevant record offsets and branches. It does not
observe which record or key the original actor used at runtime.

A trustworthy immutable witness would need to freeze the current GATT birth
on that actor, compare its address and HCI handle against one unchanged SDK
record at the actual HCI send, check the full 16-bit mode and the selected
PENC/LENC branch, then correlate the controller result and encryption change
back to that same birth. Key bytes must stay in the original synchronous SDK
call; a witness, test fixture, log or report must never contain them. The
current probe pins neither the complete private record ABI and HCI-send
wrapper signatures nor that whole call/result chain. Comparing only an
address, callback byte, stored mode, key pointer or later encryption event
would leave an attribution gap. No provenance port or positive restored-bond
admission is installed from this audit.

The regular product map check above still reports no linked BLE host key path.
The next bounded investigation is an isolated ABI probe for the original
record getter and both HCI-send signatures, with same-address/recycled-handle,
PENC/LENC, short-key, wrong-mode and failed-controller-command negatives.
Only after that probe and a real source-actor binding can a metadata-only
witness be considered; role and diagnosis authority remain separately gated.

### Isolated getter/HCI result probe (2026-10-11)

`python scripts/check_f1_hci_key_send_abi.py` pins four untouched official
ESP-IDF v4.4.6 files and the installed `libbt.a`, checks that the four getter
and HCI-send symbols are exported, and compiles their original declarations
for Xtensa with 32-bit pointers and one-byte `BOOLEAN`. It checks the source
branch of both HCI sends: allocation failure returns false; true follows
`btu_hcif_send_cmd`. Thus true means a command was enqueued, **not** that the
controller accepted the key or encrypted the link. The original slave
`btm_ble_ltk_request_reply` ignores the HCI-send return entirely. The handle
getter may match either a classic or BLE handle; the address getter may match
a direct, pseudo or resolvable address. Neither getter alone binds the record
to our current GATT birth.

The expanded pinned-source model in `check_f1_bond_key_origin.py` exercises
both key masks, the `0x000d`/`0x010d` low-byte alias, a same-address recycled
HCI handle, caller-supplied STK, and refused master/slave HCI enqueues. The
slave refusal cannot be observed by its original caller. These are source
control-flow negatives under a deliberately private host record model; no
device key is read, printed or exported.

The complete `tBTM_SEC_DEV_REC` still has conditional fields whose compiled
configuration and full type dependencies have not been pinned for this
Arduino archive. The previous static offset audit covers only selected
instructions. This probe therefore does **not** establish a complete record
ABI or a synchronous controller-result-to-GATT-birth chain. No immutable
provenance witness was installed and restored-bond, role and diagnosis gates
remain closed. The next probe must establish that configuration and binary
layout, then bind the original HCI command, its later controller result and
the encryption-change event to one birth without retaining key bytes.

### Controller result and complete-record gate (2026-10-11)

The next pinned-source check, `scripts/check_f1_controller_result_gap.py`,
uses unmodified ESP-IDF v4.4.6 `btu_hcif.c` (SHA-256
`e4485a0f4a035119827050596c0ac707158ebafc3946afecd22725efc8e1d81f`)
and `btm_sec.c` (SHA-256
`d3207f4b4a369cf534f128c124fca8192979769a01986ed46bcb15ace4c8b394`).
Supply those files from the official v4.4.6 paths
`components/bt/host/bluedroid/stack/btu/btu_hcif.c` and
`components/bt/host/bluedroid/stack/btm/btm_sec.c` as `--hci-source` and
`--security-source`.
It verifies that Encryption Change reads exactly status, HCI handle and
enabled, then forwards those three values to ACL and security handlers.
`btm_sec_encrypt_change` looks up a record by handle and branches on the
current ACL transport. The event contains no command identifier, key
identifier or GATT-birth token. An old result after handle reuse and a result
for a second key selection on the same handle have identical event bytes.
The existing HCI-birth witness rejects reused handles, but neither that
witness nor a successful event can identify which key was selected.

The full private `tBTM_SEC_DEV_REC` declaration depends on feature guards
including `L2CAP_UCD_INCLUDED`, `BLE_PRIVACY_SPT`,
`SMP_SLAVE_CON_PARAMS_UPD_ENABLE` and `BLE_50_FEATURE_SUPPORT`, and on
transitive private types absent from the installed Arduino SDK headers. The
verified 328-byte allocation and selected offsets remain a partial binary
check; they are not a complete structure ABI. Consequently no positive
metadata witness is justified by this package. A next isolated test must
compile the complete unmodified record against the exact archived build
configuration and compare all relevant offsets/size to `libbt.a`. A separate
source-actor test must show one exclusive HCI command per frozen birth, reject
re-keying and recycling, and correlate command status plus Encryption Change
before any restored-bond security evidence is considered. No role or
diagnosis grant follows from this source-level work.

```text
python scripts/check_f1_smp_prereset_abi.py
  --gatt-source-dir <lifecycle-source-with-original-bt_defs.h-and-bt_target.h>
  --security-source-dir <security-source>
  --compiler <xtensa-esp32-elf-g++.exe> --host-compiler <32-bit-g++.exe>
  --objdump <xtensa-esp32-elf-objdump.exe> --sdk-library <pinned-libbt.a>
```
