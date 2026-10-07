# Raw SDK profile admission, version1

This narrow path connects kind5/6/7 input to the actual profile queue and the existing [control/output owner](profile-response-queue-v1.md). It shares the original raw SDK gate with game commands, without using game command assembly for large profile documents. Real verified identity/SDK lifecycle/service/attributes and bearer remain open; current SDKprobe is synthetic and initializes no RF.

## Frozen callback observation

admitGattsProfileWrite and admitGattsWrite now share the same bounded gate. They copy the complete event scalars and original binding before any external authority observation, validate original context and freeze at most128 value bytes before the first authority call. The first authority callback cannot change selected bytes, transaction, response desire, attribute, connection token or authority. They retain no SDK pointer beyond the call. Caller still must acquire the original lifecycle binding under its own synchronization; a copy cannot establish origin from a recycled handle or late raw SDK event. Borrowed pointer must denote the stated live memory range.

Original nonzero generation/peer address/attribute/interface and exact SDK interface/handle/peer address are required; unknown context receives no response. Original GM/principal/epoch is rechecked, then selected attribute, prepared/execute/cancel and outer offset/length. One actual copied queue insertion, one optional bounded ATTresponse, no parser/Profile/file IO/Apply/save/tick/retry/wait inside callback. Profile queue independently rechecks original authority and validates exact metadata20/data9..128/control12. Explicit enum mapping keeps queue outcomes independent of C++ ordinal coincidence.

ATT Ok0 occurs only after Copied; Full0x11, Unauthorized0x08, Unsupported0x06, malformed allowed-type0x04, outer length0x0d, wrong attribute0x01, outer offset0x07, uninitialized0x0e. Prepared/execute/cancel rejected0x06, no assembled prepared transaction. NeedResponse=false performs no ATT copy. Response copy failure does not undo input admission. Error response must still use the selected original SDK context and real bearer recheck; no recycled handle lookup. ATTresponse never means completed source, accepted draft, selected Apply, confirmed save or peer delivery.

## Owner path and exact stages

On the actual Native path, callback copies metadata+169 data values+Finish+Apply =172 values, then the actual one-item owner handles source/Finish/private import/receipt/Apply. Three independent20-byte semantic responses are DraftReady, ApplyQueued and Saved. They travel through actual static output queue/reader/sender and represent host journalmodel cfg2/profile4/generation2, never a radio measurement. Empty input/FIFO remains no finish. Existing request cache, original collection/control/output deadlines, stopped game/revision/rights checks and selected-transaction policy are unchanged. Profile ATTadmission does not create an Application proof or activate anything in callback.

SDK ABIprobe converts one synthetic esp_ble_gatts_cb_param_t.write containing an exact12-byte control through the actual inline converter and actual profile gate/queue. It confirms Copied/Ok and takes the copied value under Fake assigned GM, emitting synthetic=true/network=false/applied=false. No BLE initialization, real callback registration, endpoint/service, verifier, filesystem or Application execution is performed.

## Acceptance

Original context/prepare/offset/type/count, Full/exact retry, NeedResponsefalse, lost ATTcopy, reconnect/original epoch, mid-admission revoke, uninitialized/no binding and mutable caller event/binding/value are checked. The shared game command gate has an added snapshot test and all prior command checks remain. Independent counterpart verifies actual Saved/172ATTvs3semantic observations and rejects incorrect counts/stages/measurement claims.

Still open: real crypto verifier/stable credential identity/explicit durable GM/local provisioning/key storage, original SDK connection-generation and callback ordering/peer attribution, real service/attribute/bearer binding, embedded temporary store/layout, production reserve/Application hookup, real owners and physical RF/Android/SMP/stack/time/heap/energy/flash failures. No new upload/format/push/PR/merge.

### Measured local verification, 2026-10-07

238/238 Native cases passed without fixture-export mode; nine new cases. All five final targets built successfully (production, legacy USBprobe, profile tool, profile runtime probe, SDK ABIprobe). Production RAM55148/Flash1228849 (93.8% appslot); legacy21480/291881; profile107328/395321 plus57272 startup Profileheap/allocator/FS; SDK26088/279297, net+0 RAM/+1072 Flash from8242a56. Architecture118files/257includes,72 independent C++11 headers and205 format files passed. Twelve App-v2 snapshot files are freshly byte/SHA256 identical; no App execution. Independent fixture/counterpart checks include eight rejected incorrect GATTSprofile observations. No peer/RF/SMP/physical time/heap/stack/energy/flash claim. Final logs are local-only `larp-gatts-profile-host-final-20261007.log` and `larp-gatts-profile-builds-20261007.log` in the Windows user temp directory.

Actual SDK ELF own entry frames: admitGattsWrite336 bytes and admitGattsProfileWrite336 bytes (`entry a1,0x150`), plus virtual authority, queue and response call chains. Increased freezing uses bounded callback stack; these own entries are not a complete callback-stack reserve measurement.
