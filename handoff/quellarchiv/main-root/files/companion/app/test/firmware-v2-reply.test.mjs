import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { classifyFirmwareV2Reply } from "../src/firmwareV2Reply.mjs";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../contract/firmware-v2/fixtures");
const fixture = (name) => JSON.parse(fs.readFileSync(path.join(root, name), "utf8"));
const start = fixture("command-start.json");

const executed = classifyFirmwareV2Reply(fixture("reply-executed.json"), start);
assert.equal(executed.kind, "engine-executed");
assert.equal(executed.outcome, "executed");
assert.equal(executed.consumed, true);
assert.equal(executed.duplicate, false);
assert.equal(executed.correlation.request, 1);
assert.equal(executed.nextRequest, 2);
assert.equal(executed.stateRevision, 1);
assert.equal(executed.physicalEffectConfirmed, false);
assert.equal(executed.requiresStatusReload, true);

const duplicate = classifyFirmwareV2Reply(fixture("reply-duplicate.json"), start);
assert.equal(duplicate.kind, "duplicate");
assert.equal(duplicate.outcome, "executed");
assert.equal(duplicate.duplicate, true);
assert.equal(duplicate.nextRequest, 2);
assert.match(duplicate.detail, /historische Revisionen/);

const stale = classifyFirmwareV2Reply(fixture("reply-stale-state.json"), fixture("command-stale-state.json"));
assert.equal(stale.kind, "conflict");
assert.equal(stale.outcome, "stale_state");
assert.equal(stale.consumed, true);
assert.equal(stale.nextRequest, 3);
assert.equal(stale.correlation.request, 2);

const conflict = classifyFirmwareV2Reply(fixture("reply-conflict.json"));
assert.equal(conflict.kind, "request-conflict");
assert.equal(conflict.consumed, false);
assert.equal(conflict.originalMayHaveRun, true);
assert.equal(conflict.physicalEffectConfirmed, false);

const foreign = classifyFirmwareV2Reply(fixture("reply-executed.json"), { ...start, request: 9 });
assert.equal(foreign.kind, "foreign-result");
assert.equal(foreign.physicalEffectConfirmed, false);
const wrongBoot = classifyFirmwareV2Reply({ ...fixture("reply-executed.json"), boot: "12345678ABCDEF02", outcome: "wrong_boot" }, start);
assert.equal(wrongBoot.kind, "boot-changed");

assert.throws(() => classifyFirmwareV2Reply({ ...fixture("reply-executed.json"), schemaVersion: 0 }));
assert.throws(() => classifyFirmwareV2Reply({ ...fixture("reply-executed.json"), duplicate: "true" }));
assert.throws(() => classifyFirmwareV2Reply({ ...fixture("reply-executed.json"), request: -1 }));

console.log("Firmware v2 reply classification tests passed");
