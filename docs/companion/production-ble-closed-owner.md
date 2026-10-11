# Closed production BLE owner: measured build gate

Status on 2026-10-11: `fec047e` remains the buildable production state.
`Esp32CompanionIdentityOwner` starts the confirmed public identity and a closed
policy before hardware or radio startup. The product still does not initialize
Bluetooth, register GATT, advertise, authenticate a peer or open diagnosis.

An isolated implementation attempt on branch
`plan/nutzergefuehrte-hardwarediagnose-ble-owner` connected the existing
`Esp32GattsSource` and `Esp32GattsService` to the actual product startup: one
process-lifetime owner, bounded queues, a separate service/ATT worker, the
`GATT_Register` and `SMP_Register` wrappers armed before controller/Bluedroid
initialization, and a selected GATTS callback. It did not start advertising or
install registry, credential, role or diagnosis workers. The build verified
the pinned ESP32 `libbt.a` SHA-256
`1e11ea417c83432360b7309c0d35877a884be8fa3fc8c875cc9d627ff37cf787`
and linked an ELF, then failed the program-size gate. The attempt was removed
from the product files before commit. The ELF is a capacity measurement, not
a flashable firmware or a runtime test.

Measured with `platformio run -e az-delivery-devkit-v4`, PlatformIO
`espressif32@6.6.0`, Arduino ESP32 `2.0.14`, board
`az-delivery-devkit-v4`, 4 MiB flash, on the original product partition map:

| Build | App image | OTA app slot | Static RAM |
| --- | ---: | ---: | ---: |
| Identity-only baseline `fec047e` (fresh counter-build passed) | 1,236,489 B | 1,310,720 B | 56,692 B |
| Closed BT/GATT integration attempt (linked, size check failed) | 1,949,801 B | 1,310,720 B | 73,212 B |

The attempt exceeds the current OTA app slot by **639,081 B**. The SDK's
`tools/partitions/default.csv` assigns `app0` and `app1` 0x140000 bytes each,
with NVS, SPIFFS and coredump partitions. Changing that layout, removing OTA
or reducing other product functions is a separate architecture decision and
requires a rollback and storage assessment. No partition or firmware upload
was changed in this cut.

Even after a size decision, product admission requires evidence that the
selected original SDK actor and service callbacks retain their registration
and connection birth in the actual build. The current isolated ABI probe
shows source mechanics, not a live product registration. F1 full-width
restored bond mode and selected-key provenance, the current registry/P256/
credential and same-birth role ACK, and an original-bound diagnostic transport
remain open. Generic bond/authentication success cannot grant authority.

## Single-app development layout: isolated build check, 2026-10-11

The development candidate is `partitions/development-single-app.csv`, selected
only by `az-delivery-devkit-v4-single-app-capacity`. The standard product build
continues to use its two-slot map. The candidate replaces `app0` and `app1`
with one `factory` app from `0x10000` to `0x290000` (0x280000 = 2,621,440 B).
The generated partition binary was decoded with the pinned framework's
`gen_esp32part.py`: NVS remains at `0x9000`/`0x5000`, otadata at
`0xe000`/`0x2000`, SPIFFS at `0x290000`/`0x160000`, and coredump at
`0x3f0000`/`0x10000`. The parser verified the table without overlap.

Build command: `platformio run -e az-delivery-devkit-v4-single-app-capacity`.
PlatformIO `espressif32@6.6.0`, Arduino ESP32 `2.0.14`, ESP32 4 MiB board;
result **SUCCESS**. The size gate reported 1,236,553 B used and 56,692 B
static RAM. The generated `firmware.bin` is 1,242,848 B, leaving 1,378,592 B
against the 2,621,440 B app partition by binary length. These figures describe
the current identity-only product. The previous closed BLE integration was
removed before commit `d677729`; its exact integration source is not present
on this branch. Its reported 1,949,801 B linked size would be 671,639 B below
the candidate's limit only as an arithmetic comparison across distinct builds,
not a reproduced BLE build or a bound on future F1, role, and diagnosis code.

Reconstruction audit on 2026-10-11: `git log --all` found no committed
`src/main.cpp` change containing `esp_bt_controller_init` or
`Esp32GattsService`. `git fsck --full --no-reflogs --unreachable` found six
unreachable commits, none carrying that production startup, and a scan of
5,441 unreachable blobs found no `esp_bt_controller_init` or the attempted
`Esp32GattsService` construction. The removed working-tree experiment was
therefore not recoverable as a defined Git revision. Recreating a new BLE
startup from the ABI probe would be new integration work, not reproduction of
the 1,949,801 B build; it could silently change callback, worker, or authority
semantics. The next gate is to implement and review a fresh, narrowly closed
production owner from the pinned SDK and current contracts, then build that
exact source against this layout and report its actual image length and RAM.
It must keep advertising, registry/credential/role and diagnostic admission
disabled until their separate proofs exist.

This check did not execute the ESP32 bootloader. A future device procedure must
verify how the actual bootloader chooses the sole `factory` app when existing
otadata still contains a prior OTA selection, and must specify how the old
otadata is handled during a USB layout migration. The app's runtime, GATT
registration, security/role admission, and reconnect behavior are unverified.
No upload, restore, flash write, or hardware test occurred. The accepted
development fallback is another USB flash; two-slot OTA remains a later
product-level feasibility and rollback target.

## Fresh closed BLE stack capacity cut, 2026-10-11

The new `ClosedBleStackOwner` is compiled and called only by the isolated
`az-delivery-devkit-v4-single-app-capacity` environment. After the production
identity owner reports `ReadyClosed`, it initializes and enables the ESP32 BLE
controller and Bluedroid exactly once. A failed stage stays failed; the game
startup continues, but no BLE access is admitted. The class exposes no GAP
advertising, GATT registration/service, connection, pairing, or role API.
There is no SDK callback or GATT attribute installed. A successful return
means only that local stack initialization calls returned `ESP_OK` in source;
the build itself never executed them.

Fresh build with the pinned PlatformIO `espressif32@6.6.0` and Arduino ESP32
`2.0.14` toolchain: `platformio run -e
az-delivery-devkit-v4-single-app-capacity` **SUCCESS**. The size gate reports
1,924,613 B flash and 69,348 B static RAM. The generated `firmware.bin` is
1,930,912 B, leaving **690,528 B** in the 2,621,440 B factory partition.
The generated partition binary was decoded and verified again with the
framework's `gen_esp32part.py`. This is a new, narrower build, not a
reproduction of the removed 1,949,801 B GATT experiment. Its remaining flash
is not a budget guarantee for the future GATT, F1, role, and diagnosis paths.
The unchanged standard `az-delivery-devkit-v4` environment also built
successfully: 1,236,489 B flash against its original 1,310,720 B OTA slot and
56,692 B static RAM. Its startup does not call `ClosedBleStackOwner`.

This capacity cut intentionally does not install a product GATT service:
the original connection, restored-bond key provenance, role ACK, and
diagnostic admission are not yet assembled into one production owner. It
cannot serve the Android app. Before any device run, confirm the actual
bootloader's behavior with old otadata and test BLE/Wi-Fi coexistence,
startup heap, stack and game timing on the connected box in a user-led flow.
No upload, RF run, authentication, or device-function claim is made here.

## Closed product GATT registration, 2026-10-11

The single-app development build now uses `ProductGattsOwner` at product startup
after the existing identity owner reaches `ReadyClosed`. It reuses that exact
owner's initialized `IdentityAccess` and control channel. A process-lifetime
source and response queue are installed, the pinned `GATT_Register` and
`SMP_Register` wrappers are armed before controller/Bluedroid startup, and a
separate worker begins before GATTS callback/application registration. The
callback copies selected setup events to the bounded service queue. The
service has command and profile attributes with SDK encrypted/MITM write
permissions; the actual `IdentityAccess` remains closed. No peer registry,
proof worker, role grant, command/profile consumer, diagnosis admission, or GAP
advertising is started. Repeated `begin()` calls retain the first failure or
registration-pending state; there is no automatic retry after a partial start.

The pinned `espressif32@6.6.0` / Arduino ESP32 2.0.14 build succeeded for this
exact source. The SHA-256 checks of `libbt.a` and `libmbedcrypto.a` passed. The
ELF size gate reports **1,949,649 B flash**, **73,212 B static RAM** against a
2,621,440 B factory app slot. The generated `firmware.bin` is **1,955,952 B**,
leaving **665,488 B** by image length in that slot. The previous closed-stack
image was 1,930,912 B; the GATT source/service link adds 25,040 B of image in
this build. These are build figures, not a measured heap/stack/RF budget.

The existing configured-control host suite passed **513/513** cases. The source
and service counterpart scripts passed 17 and 58 negative model checks. The
GATT lifecycle script could not run in the current Python environment because
`jsonschema` is absent. None of these tests executes the ESP32 startup or
proves original SDK registration, callback order, service readiness or a live
connection. F1 restored-bond provenance, same-birth registry/P256/credential
and role ACK, Android bearer, user-led full-box report, bootloader selection
with old otadata, and an independently verified rollback remain open. No
upload, restore or RF test occurred.

## Challenge/Proof/Result production gate, 2026-10-11

The product owner currently supplies the empty `PeerChallengeReadSetup` to
`Esp32GattsService`. This is deliberate. The smallest real authenticated GATT
path requires the complete original-bound chain, not merely three more
attributes:

1. A valid immutable `BootIdentity` from the production run, plus the actual
   selected source registration and service handles before the first link.
   Commit `c9f21bd` makes `Esp32CompanionIdentityOwner` generate an ephemeral
   boot identity after its closed policy starts and expose it only on a
   successful startup. It is not yet passed to peer proof or GATT; a boot
   identity alone establishes no peer authority.
2. A process-lifetime `IPeerProofContext` backed by the same selected source
   and a started `FreeRtosPeerRegistryRuntime` whose credential snapshot is
   installed with an original SDK actor ACK. The product owner has no registry
   worker, loaded peer registry or credential publisher. A caller-supplied
   public key, test credential or device identity would not establish this.
3. The separate crypto worker with real nonce/verification, grant handoff,
   challenge-read and issue/proof/result ingress all started against that same
   boot and original source. `Esp32GattsService::begin` rejects an incomplete
   setup; `Esp32GattsSource::installEndpoints` also rejects missing context or
   ingress before binding a challenge. Result success additionally rechecks
   current credential and same-birth grant ACK. A proof alone gives no GM role.
4. The original selected-key and restored-bond provenance from F1 must be
   established before a positive peer result can be trusted on reconnect.
   The present SDK security reader documents fresh pairing only. The product
   does not advertise, so no app connection or RF behavior is being claimed.

Adding Challenge/Issue/Proof/Result UUIDs with a dummy boot/context would
create an apparent authentication surface without a valid challenge source or
role result. This package therefore leaves the product source and GATT layout
unchanged. The next implementation should bind the real boot identity to a
registry/worker contract, require its startup ACK before the first connection, and then wire
the already tested complete service bundle. On any missing or stale original
registration, credential, SDK security observation or role ACK it must remain
closed. A real device run and user-led diagnostic report are still separate
gates.

## Stored-registry startup slice, 2026-10-11

The next isolated owner now copies the fresh production `BootIdentity` and
device identity before starting Bluetooth. Failure leaves the GATT stack
unstarted. After the selected original SDK source has registered and the
closed service setup reports ready, a separate IO worker starts the real
`Esp32PeerRegistryStore`, `FreeRtosPeerRegistryRuntime` and
`PeerRegistryOwner`. It waits for original source-actor close/install ACKs,
reads and validates the public registry in NVS, and compares the owner's loaded
snapshot with the installed actor snapshot. A missing registry is a confirmed
empty snapshot; a read/validation/post/ACK failure is fenced for this boot.

`StoredRegistryInstalledClosed` says only that this stored public snapshot was
installed on the original actor while the channel had no current authority.
Each status read rechecks the installed source snapshot and absence of current
authority; stale observations return `RegistryRejected`.
The worker publishes no key from an ATT packet. It does **not** prove a
connected peer's selected key, F1 restored-bond mode, P256 possession, GM
grant, challenge/result readiness or diagnostic admission. Those remain gated;
the service still has only command/profile attributes, and GAP advertising
remains off. The copied boot identity is retained for a future complete
challenge setup but is not used to mint an authentication result here.

The exact isolated single-app source built with pinned `espressif32@6.6.0` and
Arduino ESP32 2.0.14. The ELF size gate reports **1,963,841 B flash** and
**75,876 B static RAM**. The generated `firmware.bin` is **1,970,144 B**, leaving
**651,296 B** of the 2,621,440 B factory app partition by image length. The
new 8 KiB registry worker stack and runtime heap remain unmeasured on hardware.
The first fresh build attempt stopped before compilation at a Windows path
limit while installing FastLED; the successful build used an ignored shorter
`PLATFORMIO_LIBDEPS_DIR` under the workspace `.pio` directory. The configured
control host suite passed **522/522**, including a negative case showing that
the stored GM key and original role-install ACK leave channel authority absent.
No ESP32 boot, NVS read, RF run, upload, restore or full-box test occurred.
