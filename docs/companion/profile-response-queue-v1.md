# Copied FreeRTOS profile responses, version1

This small P0 path joins the [control owner](profile-controls-v1.md) to a transport owner without shared mutable Application state. Kind7/8 bytes and their semantics are unchanged. Real SDK lifecycle/security/bearer and embedded temporary source are still separate.

## Producer and fixed port

IProfileResponseQueue is a synchronized copy-only port. ProfileTransferControl.offerResponse constructs one immutable RoutedProfileResponse from its original connection, original producedAt and cached20-byte response. It observes rights/deadline and calls the port once. Only Copied releases pending owner output; QueueFull/Uninitialized/invalid port result preserves it without a new timestamp, so ProfileFramePump keeps further input blocked. Existing source service and separate Application/owners continue; offer adds no game tick/wait. Revocation/age closes pending response under normal policy. This is local buffer admission, not peer receipt.

FreeRtosProfileResponseQueue begins once before workers, idempotently without resetting pending values. Four static slots store whole36-byte routed values; producer/consumer operations use zero wait. copyResponse freezes all scalars before external clock/authority observation, validates the response and requires original current GM/principal/epoch and less than10000ms since original production. It never derives authority from response bytes. QueueFull inserts nothing. A later closure may still leave an old copied item; no atomic authentication-to-send promise is made.

Sole transport takeCurrent drains at most four stale/invalid/expired/unauthorized values per call, returning at most one original routed value; false preserves destination. It never attaches the current epoch to an old response or changes producedAt. Clock and authority are trusted synchronized ports; uint32 timing requires observation within a counter cycle. Waiting in owner/outputqueue/sender shares the same10000ms response lifetime. Sender load and real bearer recheck again; take success can still be followed by revocation and is not delivery.

## Transport owner

ProfileResponseReader owns queue+sender selection with a reentrancy fence. Pending immutable sender or nested select consumes no queue item. Otherwise one bounded takeCurrent/load, no bearer call/Application service/tick. Sender load failure is visible, selected old output is discarded; exact last control retry can replay the ownercache as documented. Caller normally invokes reader.select then serviceProfileConfiguredFrames in its serialized transport task. Producer uses offerResponse from the sole Application task. There is no cross-task access to ProfileTransferControl/ProfileResponseSender internals. Actual FreeRTOS supplies synchronization for the copied port; Host model does not prove SMP behavior.

Game replyâ†’profile replyâ†’status priority remains captured before service; a completing higher priority reply owns the whole step. The actual bearer must preserve original SDK generation/attribute/peer binding before copy. No queue method writes Profile, journal, keys or filesystem.

## Vertical evidence and remaining acceptance

Native executes actual copied inputâ†’Host temporary sourceâ†’private Application import/receiptâ†’Apply/synthetic owners/journalmodelâ†’offerResponseâ†’actual copied outputâ†’original-bound sender/real encoder. QueueFull retains exact owner time/snapshot and next input; FIFO/idempotent begin/false preservation/invalid/future/age/wrap/producer mutation/revocation before/after observations and reentrant owner/reader are checked. Independent struct counterpart correlates Saved with original producer23/sender25/modelcfg2/generation2 and rejects changed budget/time/scope observations.

SDK ABI probe creates the actual static response queue and copies/takes one **synthetic** DraftReady observation under Fake GM. It initializes no Bluetooth, temporary source or Application and performs no save. Production/Profile probe do not instantiate this output queue; successful compilation does not connect production entry.

Still open: real verified identity/explicit durable GM mapping/provisioning/key storage, original SDK lifecycle/service/attribute/peer binding and bearer, embedded temporary store/layout, production reserve/Application integration, real owners and physical RF/Android/SMP/time/stack/heap/energy/flash failure acceptance. No upload/formatting/push/PR/merge.

## Final local evidence,7 October2026

229/229 targeted Native functions/exit0,12 new here. Actual owner→static copied output→sender/real wire path reaches Saved cfg2/profile4/generation2 with producer23/sender25; QueueFull, retained owner snapshot/time/input, stale/future/wrap/reconnect/reentrant owner/reader and loss after selection/bearer recheck passed. Independent counterpart validates actual Saved/model observation and rejects7 wrong observations; prior controls/frames/schema checks retained. No new device/RF/SMP/storage durability evidence.

Five builds SUCCESS/exit0: production55,148RAM/1,228,849flash93.8%, oldUSB21,480/291,881, profile107,328/395,321 plus57,272startupProfileheap/allocator/filesystem unchanged; SDK26,088/278,225, net+240RAM/+1,120flash versuscbf866a. Actual ELF outputqueue244bytes (0xf4), separateclock4; net build delta is not the sum of named objects. Native Hoststub queue180/control20 differs from actual SDK layout; routed item36/owner136/sender48/reader12. Pure/actualSDK own offer80/reader80; actualSDK own queue begin32/copy80/take80 plus authority/clock/RTOS/caller chains. These do not prove complete task stack or realtime reserve. SDK source emits synthetic=true/network=false/saved=false and copies/takes only a Fake observation. Actual production/Profile entrypoints still do not instantiate this queue/new transport.

Architecture118files/257includes,72 independent C++11headers,204format files passed. Twelve App-v2 pairs byte/SHA equal; Appcode/tests remain with App session. Evidence:%TEMP%/larp-profile-response-queue-host-final-20261007.log, larp-profile-response-queue-builds-20261007.log, larp-profile-response-queue-budget-20261007 and actual SDK object/ELF. No new upload/format/remote operation.

Next small P0 path: profile metadata/data/control admission through original raw SDK write binding and exact ATT copy responses, with frozen event/binding/value before external observation, actual queue/source/control/output/sender counterpart. This closes neither real security verification nor SDK lifecycle/service registration; those remain open.
