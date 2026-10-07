# Copied FreeRTOS profile input, version 1

A narrow callback/owner connection for [temporary profile sources](profile-staging-v1.md). Actual static FreeRTOS queue code is compiled against both the pinned SDK and a deterministic Host copy model. Live SDK profile attributes/callback lifecycle/response bearer and profile controls are documented separately in [profile-controls-v1.md](profile-controls-v1.md); real endpoint remains open.

## Callback admission

FreeRtosProfileFrameQueue owns four static slots of CopiedProfileFrame. Each item holds a fixed128-byte value, original trusted principal/epoch, original uint32 monotonic receipt time and exact count. begin is process-lifetime/idempotent and does not reset pending items. Take/send use zero wait; false take preserves the destination.

copyWrite is task-context callback work, never ISR. It rejects uninitialized/prepared/outer-offset/null/<=8/>128 input. It freezes the whole bounded value before external authority observation, requires the exact current verified GM principal/epoch, and validates either exact20-byte kind5 source metadata or kind6 bounded data or exact12-byte kind7 controls. Other types/versions are UnsupportedFrame; invalid allowed-type headers are InvalidWrite. Full queue yields QueueFull without inserting that value; an exact retry after capacity is available makes one copied item. Copied means only queue admission, never complete source/import/apply/save/peer receipt.

The trusted upstream adapter must prove original connection generation, service/attribute/peer binding and supply captured route/time. Those never come from profile JSON/metadata. Revocation immediately after the authority observation may still leave an old copied item: consumer/source checks it again before any successful temporary source admission/write. No lossy lifecycle FIFO is needed to close rights. Existing game command queue continues rejecting profile kinds; no large source enters its1024-byte command assembler.

## Sole owner service

ProfileFramePump services source expiry/revocation even with an empty FIFO, then takes at most ONE copied value. A reentrant service or busy source consumes no pending item. It returns only scalar route/input/outcome fields, never a borrowed payload into the local item. Untrusted port counts remain bounded by the actual decoders; a malformed port item cannot trigger an oversized memory read.

Copied metadata reaches ProfileStaging.start with its **original callback timestamp**, including queue waiting. Data reaches ProfileStaging.data with the original queued route; later data timestamps never replace the first deadline. Each item is checked for future/expired age before dispatch using the same120000ms source policy. Original source service/append/seal/read rights/time checks remain in force. An expired individual item is rejected, not relabeled with the current time/connection.

Empty FIFO does not finish a source. Even after all payload bytes, phase remains Receiving until an explicit trusted finish operation. An optional bounded control sink explicitly performs Finish/private import/internal receipt/authorized apply and later journal observation; see profile-controls-v1.md. Without a sink, kind7 is rejected by the pump. This pump adds no retries, wait, game tick, automatic start, App JSON role or hardware work in callback. Filesystem/backend latency is still a backend acceptance requirement; one queue step is a fixed count of operations, not a measured realtime promise.

## Actual vertical evidence

Native path uses the actual four-place queue, actual one-item pump and actual Host temporary file Ã¢â€ â€™ private Application import Ã¢â€ â€™ private receipt Ã¢â€ â€™ controlled apply/synthetic owners/journal model Ã¢â€ â€™ current original-bound mailbox/real v2 statusencoder. QueueFull/FIFO/exact retry, immutable callback bytes during first authority observation, prepare/offset/type/count/role rejection, one-item service, empty queue, waiting/deadline/wrap/future timestamp, original epoch/reconnect, reentrant backend and corrupted port count are checked separately. Original metadata timestamp17 remains17 while later frames/services advance the fake clock. Model confirms configurationRevision2/storedGeneration2/stopped game; this is not device durability/RF.

Independent counterpart validates an actual scalar observation and current v2 status and rejects seven incorrect observations, including a changed timestamp or treating copied queue input as a complete source. Existing kind5/6 tool frames and receive tool remain the independent input source; no Appcode is changed.

The isolated SDK-ABIprobe creates the actual profile queue and copies/takes one SYNTHETIC20-byte metadata value using its explicitly Fake verified GM. It prints synthetic=true/network=false/imported=false. It does not open an embedded temporary store, initialize radio or invoke a real profile GATTS callback. The existing synthetic write/execute command ABI checks remain intact.

## Next acceptance

Profile controls and responses are implemented in the linked narrow owner path; next is actual profile SDK service/attributes, original lifecycle/security attribution, verifier/provisioning/role-key persistence, bearer and embedded temporary storage/layout. Production Application hookup/flash reserve, real owners and RF/Android/SMP/time/stack/heap/energy/storage failures remain open. No new upload/formatting/push/PR/merge.


## Historical queue-only evidence (45c12c2), 7 October 2026

192/192 focused native functions passed/exit0, eleven added here. Actual queue/Host file/private import/apply/confirmed journal model/current status path, queue and owner boundary failures passed. Independent counterpart rejects seven changed observations and validates actual current v2 status. Previously committed source/frame/tool contracts remain unchanged; no new physical upload/radio/SMP/Android/storage durability evidence.

All five final builds SUCCESS/exit0. Production55,148RAM/1,228,729flash(93.7%), oldUSB21,480/291,881 and profile107,328/395,201(+57,272profile startup heap/allocator/filesystem) unchanged versus4cc055e. Isolated SDK now25,848/276,573: +672staticRAM/+1,384flash for the synthetic profile queue copy/take probe. Actual SDK ELF profileQueue symbol size0x2a0=672bytes. It is an additional allocation in this probe, not a replacement for the command queue. Production entry remains the prior controller. Architecture110files/240includes,68headers,194format files passed; prior schema/generator/frame checks passed.

Native32bit sizes: copied item144/pump16/scalar result24bytes. Temporary pure Xtensa service frame224; actual pinned SDK service entry240bytes, plus source/backend/clock/authority/caller chains. Different compiler/SDK flags explain distinct measured entries; neither is a complete stack or realtime reserve. Owner handles at most one queue item, while source backend has its own bounded-call requirement. Evidence: %TEMP%/larp-profile-queue-host-final-20261007.log, larp-profile-queue-builds-final-20261007.log, larp-profile-queue-budget-20261007 and actual SDK objects/ELF.
