import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { FirmwareV2StatusStore, mapFirmwareV2Status } from "../src/firmwareV2Status.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../contract/firmware-v2/fixtures");
const fixture = (name) => JSON.parse(fs.readFileSync(path.join(root, name), "utf8"));
const ready = fixture("status-ready.json");
const running = fixture("status-running.json");

const readyView = mapFirmwareV2Status(ready);
assert.equal(readyView.contract, "firmware-v2-fixture");
assert.equal(readyView.boot, "12345678ABCDEF01");
assert.equal(readyView.deviceId, null);
assert.equal(readyView.application.phase, "ready");
assert.equal(readyView.application.configurationRevision, 1);
assert.equal(readyView.application.profileRevision, 3);
assert.equal(readyView.game.phase, "stopped");
assert.equal(readyView.game.stateId, null);
assert.equal(readyView.game.stateRevision, 0);
assert.equal(readyView.control.lease, 1);
assert.equal(readyView.control.writesEnabled, false);
assert.equal(readyView.devices.length, 7);
assert(readyView.devices.every((device) => device.enabled && device.available));

const runningView = mapFirmwareV2Status(running);
assert.equal(runningView.game.phase, "running");
assert.equal(runningView.game.stateId, 1);
assert.equal(runningView.game.entryPending, true);
assert.equal(runningView.game.stateRevision, 1);
assert.equal(runningView.sample, 2);
assert.equal(runningView.control.nextRequest, 2);
assert.equal(runningView.control.writesEnabled, false);

const store = new FirmwareV2StatusStore();
assert.equal(store.read(), null);
assert.equal(store.ingest(ready).accepted, true);
assert.equal(store.ingest(running).accepted, true);
assert.deepEqual(store.ingest(ready), { accepted: false, reason: "stale-sample", status: runningView });
assert.equal(store.read().sample, 2);
const anotherBoot = { ...ready, boot: "12345678ABCDEF02" };
const changed = store.ingest(anotherBoot);
assert.equal(changed.accepted, true);
assert.equal(changed.bootChanged, true);
assert.equal(changed.status.sample, 1);
changed.status.devices[0].available = false;
assert.equal(store.read().devices[0].available, true);

assert.throws(() => mapFirmwareV2Status({ ...ready, schemaVersion: 0 }));
assert.throws(() => mapFirmwareV2Status({ ...ready, capabilities: ["status.configured.read"] }));
assert.throws(() => mapFirmwareV2Status({ ...ready, boot: "bad" }));
assert.throws(() => mapFirmwareV2Status({ ...ready, sample: 0 }));
assert.throws(() => mapFirmwareV2Status({ ...ready, application: { ...ready.application, phase: "paused" } }));
assert.throws(() => mapFirmwareV2Status({ ...ready, devices: [ready.devices[0], ready.devices[0]] }));
assert.throws(() => mapFirmwareV2Status({ ...ready, game: { ...ready.game, running: "no" } }));

console.log("Firmware v2 status projection tests passed");
