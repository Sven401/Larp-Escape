# APP_MUTUAL_V1 GATT migration matrix

**Status 2026-10-11:** code-backed review matrix for the optional reversible
[USB-enrolled mutual-key development design](app-mutual-v1.md). The later user
instruction targets an NFC exchange for future productive Game Master
authentication. This matrix may inform that migration but does not authorize
one. It changes no attribute, permission, key, role or device. Current UUIDs
are canonical text order;
numeric ATT handles are assigned by SDK callbacks and are **not** fixed UUID
suffixes. `E+M` below means today's SDK encrypted **and BLE-MITM** permission,
not an APP_MUTUAL_V1 proof. `E` means SDK encrypted permission. The planned
column is an admission requirement, not a claim that the current firmware can
enforce it.

Every `…NNNN` entry expands to UUID
`407e6a1d-1cc2-43fd-856b-4aff85a2NNNN`; each CCCD is the standard
`00002902-0000-1000-8000-00805f9b34fb`. The table spells out each
characteristic's actual current permission rather than relying on `E+M`.

Code anchors: [`GattsServiceDefinition.h`](../../lib/GattsWriteAdmission/GattsServiceDefinition.h),
[`SdkGattsService.cpp`](../../lib/GattsWriteAdmission/SdkGattsService.cpp),
[`DiagnosticGattsDefinition.h`](../../lib/ClosedBleStackOwner/DiagnosticGattsDefinition.h),
[`DiagnosticGattsService.cpp`](../../lib/ClosedBleStackOwner/DiagnosticGattsService.cpp),
[`Esp32GattsSource.cpp`](../../lib/GattsWriteAdmission/Esp32GattsSource.cpp),
[`GattsWriteAdmission.cpp`](../../lib/GattsWriteAdmission/GattsWriteAdmission.cpp),
[`AndroidDiagnosticGattShellV1.kt`](../../companion/android/app/src/main/java/de/larpescape/companion/platform/ble/AndroidDiagnosticGattShellV1.kt), and
[`DiagnosticGattPreflightV1.kt`](../../companion/android/core/protocol/src/main/kotlin/de/larpescape/protocol/DiagnosticGattPreflightV1.kt).

## Present attributes and proposed gate

| Service / value UUID suffix | Current SDK property, ATT permission, limit | Current operation and admission | APP_MUTUAL_V1 migration gate and required protection |
| --- | --- | --- | --- |
| Product `…0001` | Primary service; 13 reserved handles with all six values | Discoverable service; no grant | Discovery may remain public. Exact service registration/birth must precede every protected value. |
| Command `…0002` | WRITE with response, `WRITE_ENC_MITM`, max 128 B | Game command frames, header 8 + payload up to 120 B; original source routes to `admitOriginalGattsWrite`, which checks current GM authority. No CCCD/read. | **Only after mutual Finished + current GM ACK.** Versioned authenticated envelope and AEAD before frame decoder/queue; recheck birth/session, key and role generations at execution and any response. Pre-Finished/role writes fail closed; ATT ACK alone is no game result. |
| Profile `…0003` | WRITE with response, `WRITE_ENC_MITM`, max 128 B | Profile frames on original source through `admitOriginalGattsProfileWrite`; current GM authority required. No CCCD/read. | Same post-Finished/current-GM gate, independent profile permission and AEAD envelope. No profile update from a stale session, partial frame or unknown outcome. |
| Challenge `…0005` | READ, `READ_ENC_MITM`, max 128 B | F0 108 B raw challenge with Read/Read Blob offset handling; only original bound peer proof context. | **After mutual Finished, before GM** only if F0 is deliberately migrated as a role-request channel. Response must bind verified session, current enrolled key, original birth and challenge generation; plain legacy challenge is not a new mutual handshake. Otherwise retire it from the new protocol. No pre-Finished F0 data. |
| Issue `…0006` | WRITE_NR, `WRITE_ENC_MITM`, max 18 B | Exact 18 B peer selector, offset zero, no response; current original source copies to challenge-issue ingress. | After Finished, before GM only as a versioned F0 role selector, authenticated under current session. A selector cannot choose an unknown key or assign GM. A no-response ATT write gives no delivery proof. |
| Proof `…0007` | WRITE_NR, `WRITE_ENC_MITM`, max 128 B | F0 72 B P-256 proof fragmented as 8 B header + up to 120 B payload; no ATT response. | After Finished, before GM only if retained as an additional versioned role proof; authenticate each fragment and complete ordered assembly under one birth/session. Do not treat a valid signature as GM. Otherwise replace/retire. |
| Result `…0008` | READ, `READ_ENC_MITM`, max 32 B | F0 raw 32 B result via Read/Read Blob, including `Authenticated` versus actor-ACK `Granted`. | After Finished, during the matching F0 role exchange or its actor ACK; bind every fragment/read to the same pinned result, original birth and enrollment generation. A locally read `Granted` without current actor authority is not a grant. |
| Diagnostic `…0010` | Primary service; 11 reserved handles | Discoverable alongside product service; neither service registration nor advertising grants diagnosis. | Discovery may remain public; registration and selected birth are checked for all six below. |
| Control request `…0011` | WRITE with response, `WRITE_ENC_MITM`, max 128 B | `DG` v1 request exactly 69 B; source presently returns closed authorization error, copies no payload to diagnostic owner. | Only after Finished + current GM ACK + deliberate diagnostic start and real park/off gates. Authenticate full request with session, run/grant ID, sequence and origin before decoding; recheck at action and reply. No accepted write merely because an old grant is cached. |
| Control reply `…0012` | NOTIFY, no read/write permission, max 128 B | `DG` v1 reply 88 B, but live source currently emits only a five-byte transport probe after CCCD ACK. Notify is unconfirmed delivery. | Only a current, authenticated control owner may emit an AEAD-bound, correlated reply on same birth/session/role/grant. A probe must remain visibly transport-only. Missing notification leaves action outcome unknown; no implicit retry. |
| Control CCCD `0x2902` after `…0012` | WRITE, `WRITE_ENCRYPTED`, exactly 2 B; no read | `0x0001`/`0x0000`, offset zero, normal write with response; current source ACKs preparation and may emit one probe. | May be written **before Finished** solely to prepare transport, with no role or diagnosis implied. Track subscription on exact birth, require successful SDK result and actual delivery evidence separately; stale/ambiguous descriptor response closes route. |
| Step request `…0013` | WRITE with response, `WRITE_ENC_MITM`, max 128 B | `DG` v1 request exactly 80 B; source currently refuses before copying. | After Finished + current GM + live diagnostic grant only. AEAD binds sequence, step/run/grant IDs, operation, exact birth, length and expiry. Reject after cancel/timeout or changed hardware gate. |
| Step reply `…0014` | NOTIFY, no read/write permission, max 128 B | `DG` v1 fragment up to 128 B; source currently emits only five-byte transport probe after CCCD ACK. | Authenticated, sequenced notification fragments from the same owner; reassembly must reject overlap, gaps, wrong grant/birth/session and stale authority. Reply/callback alone does not prove visible hardware effect. |
| Step CCCD `0x2902` after `…0014` | WRITE, `WRITE_ENCRYPTED`, exactly 2 B; no read | Same narrow subscription/ACK/probe rule as Control CCCD. | Same pre-Finished transport-only treatment, independently scoped to Step Reply. One CCCD ACK never enables the other channel or a privileged request. |

The suffixes `…0004` and `…0009` are not product values in these definitions.
The new mutual authentication exchange has **no assigned GATT characteristic
or wire encoding yet**. It needs a separate reviewed, versioned pre-Finished
channel reachable on encrypted Just Works, bounded to the original connection.
None of the existing F0/diagnostic request values is silently repurposed as
that channel. Its pre-Finished parser must be isolated from game, profile and
diagnostic command queues and cannot publish a role before both Finished
messages and an explicit original-owner ACK.

## Operation-by-operation refusal rules

The current product service uses application responses and current diagnostic
request/CCCD values also use `ESP_GATT_RSP_BY_APP`. The source captures original
callback scalars and exact connection birth; for diagnostic protected writes it
currently refuses before copying request bytes. This is the safe migration
baseline. The following must be specified/tested for every affected handle
*before* any `ENC_MITM` permission is relaxed:

| ATT operation | Required behavior on the new class |
| --- | --- |
| Discover service/characteristics | May expose UUID/properties only. Unknown/duplicate service or ambiguous characteristic closes Android preflight. Discovery is no identity proof. |
| Ordinary Read and Read Blob | Only an explicitly allowed current F0/auth value; pin one authenticated result and validate offset, session, birth and revision on every fragment. No game/diagnostic reads; no stale result after revoke. |
| Write Request | Auth/GM/diagnostic gate before payload copy, parser, queue or owner; response only to original live transaction. `OK` means admitted locally, not executed. |
| Write Command / WRITE_NR | Only explicitly versioned F0/auth paths; silently drop invalid/stale input because there is no ATT reply. Never route a Command as a privileged Write Request. |
| Prepare/Execute Write | Unsupported on present values. Reject before accumulation; an EXEC with no retained attribute must never select a new session or replay an old buffer. |
| CCCD write | Accept only exact two-byte enable/disable on encrypted current birth; per-channel state and ACK are transport preparation, no session/role/grant. No offset, prepare or write command. |
| Notify | Recheck exact source, birth, encrypted session, role/revocation generation, subscription, MTU and deadline at SDK posting. Never rebind an old reply to a recycled handle. Delivery and physical outcome need separate evidence. |

On a current original link, a forbidden response-required operation should
return the reviewed authorization/length/offset/unsupported ATT status. On a
stale or ambiguous link, consume it without a response that could target a
new birth. Any failed recheck closes the privileged route and records a
specific diagnostic reason. A malformed, duplicate or unauthenticated frame
must not reach a game/profile/diagnostic decoder. The exact ATT error matrix
for each future auth channel is open; do not promise a uniform SDK error where
the controller's own permission check runs before the application callback.

## Byte budget and unresolved migration decisions

The existing `CommandFrameLimits::Frame`, product attribute maxima and
diagnostic attribute maxima are each 128 B. Android currently requests MTU
131 and admits only effective ATT value >=128 B (`MTU - 3`). At that bound,
the 128-byte Command/Proof/Step Reply frames fill the value entirely; AEAD
tag, sequence, key ID and session binding cannot simply be appended. The
69-byte Control and 80-byte Step requests have some unused capacity, but a
different envelope there alone would leave game/profile, F0 and replies
unprotected. F0 Challenge 108 B can use Read Blob at smaller MTU; an
authenticated replacement needs its own fragment rules and one pinned
snapshot. The current Android shell writes `WRITE_TYPE_DEFAULT` only for
Control/Step and distinguishes write ACK, probe, final reply and authority;
its `originalAuthority` supplier is still null.

Decide and review together: dedicated auth UUID(s), canonical envelope and
suite, whether F0 role exchange is retained or replaced, fragmentation/MTU
policy including Android API callbacks and ESP32 attribute maxima, per-handle
ATT error behavior, and version negotiation without fallback. Golden fixtures
must span C++/Android with exact lengths and negative cases. Only after that
may tests, builds and a chip-bound user run support any rights change. This
matrix itself proves no BLE pairing security, GM, hardware-off state or app
diagnosis.
