# Durable public device label, version1

This slice supplies a stable public display label across startup and plugs it into the existing [identity metadata](device-identity-v1.schema.json). It does not implement verified credential identity, encrypted/MITM/SC/bond attribution, peer pinning, SDK connection origin or GameMaster provisioning. Neither a stored label nor its CRC grants any role. Existing peer evidence in the synthetic probes remains explicitly Fake.

## Record and immutable startup owner

DeviceIdentityRecord is28 bytes: ASCII LDID at0..3, version1 at4, zero reserved5..7, nonzero opaque identity16 at8..23, little-endian ISO-HDLC CRC32 of the first24 bytes at24..27. Same CRC byte routine as profile source integrity, no authentication/signature/key claim. Codec failures preserve the destination; decode refuses other versions, reserved bytes, corruption and zero identity.

DeviceIdentityOwner performs one trusted serialized startup attempt. Present means load/decode without entropy/write. Only positively observed Missing permits one entropy fill and conditional creation. Confirmed creation requires a fresh exact readback of the selected record before exposing the label. AlreadyPresent means reload the existing record without replacing it. Invalid/read/entropy/write/uncertain/readback failures expose no label and stay failed for this owner; no implicit reset/rotate/delete/retry. Repeat successful begin returns the original result with no new IO; returned identity is a copy and remains immutable. Nested begin is Busy, nested identity observation exposes no pending label. A new startup owner may load an actually present valid value after restart; an uncertain first attempt never published a candidate.

## Actual pinned ESP adapters

Esp32DeviceIdentityStore uses namespace larp-companion and key device-id-v1 on the default NVS partition. begin is a trusted pre-worker initialization and uses nvs_flash_init with no explicit erase/deinit/reinitialization fallback. Read opens NVS_READONLY, queries size, requires exactly28 and checks the second returned length before publishing. Missing namespace/key is separate from type/size/IO failure. Conditional creation freezes the record before an external observation, validates it, opens READWRITE, rechecks key absence and never replaces an existing value. A write attempt or commit error is Uncertain; Confirmed requires both set_blob and commit success. The startup owner then separately verifies readback.

Every runtime read/create across these adapter instances shares one static zero-wait FreeRTOS mutex for this fixed namespace. Busy performs no NVS call. No dynamic mutex, callback/ISR use, spinlock held around file/flash IO, worker tick or whole-document buffer. All users of this namespace must use this owner; unrelated direct NVS writes/deinit are outside its lifetime contract. begin must finish before workers; byte/mutex bounds do not measure NVS heap/flash latency or SMP fairness. Fixed stored content is public, no credentials/private keys/GM assignments.

Esp32StartupIdentityEntropy brackets one16-byte esp_fill_random with bootloader_random_enable/disable; it is permitted only before RF/ADC/I2S/device owners use that resource. Zero output fails without changing the target; no automatic generation loop. [Espressif RNG documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/system/random.html) requires releasing that entropy resource before ADC/I2S/RF use. SDK call order is checked under a Fake source and compiled against pinned real APIs; physical entropy quality/uniqueness remains an acceptance item.

Pinned Arduino2.0.14/IDF4.4.6 headers were read. [Espressif NVS documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.6/esp32/api-reference/storage/nvs_flash.html) describes fixed namespaces, length queries and required commit. Those API guarantees are the adapter's model, not physical power-loss acceptance. CRC does not protect against deliberate NVS tampering; this public label is never used as an authentication proof. Earlier Arduino framework startup is separate from this adapter's no-explicit-erase policy.

## Vertical tool/firmware proof

Native executes the actual NVS and entropy adapter bodies under deterministic NVS/static-mutex/entropy models. Tests distinguish Missing, damaged/type/size/IO failures, partial/uncertain writes with and without persisted bytes, second-read changes, readback mismatch, immutable duplicate/restart, an existing value racing creation, caller mutation and cross-instance nested access. Actual policy initialized with the stored public label remains closed without separately verified peer evidence and explicit GM assignment. No SMP/radio/crypto/physical durability claim.

The built tool --inspect-device-identity-record accepts an exact56-character uppercase record hex, runs the actual codec and emits the existing display_identity JSON. It performs no NVS IO, random generation, assignment or verification. Independent struct/zlib counterpart checks actual record/metadata/observations and rejects malformed/false variants. This tool accepts public untrusted bytes for inspection, never as a connection proof.

The isolated profile probe loads/creates this public label before filesystem/Application/device-owner startup; peer/security evidence remains Fake, network=false. The SDK ABIprobe executes the actual unopened-store read branch and checks failure/target preservation to link the real adapter/API signatures: initialized=false, no NVS IO/write/erase/RF. Both are compile-only here, no device upload/execution. Production entry remains legacy Controller.

Open: credential verifier/local provisioning/explicit durable GM, original SDK lifecycle/service/bearer, production integration/layout/reserve/owners and physical NVS/full-partition/power-loss/entropy/SMP/RF/Android/time/stack/heap/energy. No upload/format/remote operation.

## Measured local acceptance, 2026-10-07

271/271 final Native cases passed without fixture-export mode,16 new. Initial test inclusion/generator and a get-counter name collision were corrected before final verification; no older-commit failure claim. All five final targets built successfully: production55148 RAM/1228849 Flash (93.8% appslot), legacy21480/291881, profile107520/405313 plus57272 startup Profileheap/allocator/FS, SDK26128/286349. Net from95d88d6: profile+144RAM/+8216Flash, SDK+40RAM/+7052Flash. Production/legacy unchanged. The first sizeof-only ABIprobe let linking strip the adapter; the final actual unopened-read branch retains read/create and preserves the target without NVS IO. No board/RF/crypto/storage measurement.

Xtensa pure sizes: owner28, record28, ESP store8, entropy4 bytes. Profile ELF named namespaceMutexStorage84, mutexhandle4, publicIdentity28, publicIdentityStore8, identityEntropy4. SDK ELF retains unopenedStore8/mutexhandle4/read/create and drops the unused initialization/storage; net RAM deltas are measured independently of named-object sums. Pure own entry frames encoder80/decoder80/ownerbegin160/identitycopy32 plus virtual/CRC/NVS/RTOS/entropy/caller chains. These are not a complete startup-stack or heap/time reserve.

Architecture122files/263includes,74 independent C++11 headers and228 format files passed. Independent counterpart validates actual record/CRC/metadata and rejects39 malformed records plus10 false model observations. Actual built CLI emits exact metadata bytes (apart from its outer Windows CRLF) and rejects12 malformed arguments with no output. Twelve App-v2 snapshot files are freshly byte/SHA256 identical; no App code execution.

Local-only evidence: Windows user temp larp-device-identity-host-final-20261007.log, larp-device-identity-builds-final-20261007.log, larp-device-identity-counterpart-20261007.log and larp-device-identity-budget-20261007 (pure size/stack objects and actual profile ELF dump). No new upload/erase/format/push/PR/merge.

Actual profile ELF own entries match pure codec80/80/owner160/copy32; NVSbegin48/read80/create96 and startup entropy64, plus full SDK/RTOS/flash/caller chains. No complete stack/latency reserve inferred.
