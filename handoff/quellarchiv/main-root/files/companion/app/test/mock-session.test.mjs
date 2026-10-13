import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { MockCompanionSession } from "../web/mockSession.mjs";

const fixtureRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../contract/v0");
const fixture = (name) => JSON.parse(fs.readFileSync(path.join(fixtureRoot, name), "utf8"));

const ready = fixture("device-status.ready.json");
const session = new MockCompanionSession(ready, { requestPrefix: "session-test" });
assert.equal(session.read().view.actions.find((action) => action.commandType === "start").enabled, true);

let state = session.submit({ type: "start" });
assert.equal(state.feedback.kind, "pending");
assert.equal(state.feedback.canRepeat, true);
assert.equal(state.feedback.result.status, "accepted");
assert.equal(state.feedback.request.requestId, "session-test-0001");
assert.equal(state.feedback.request.bootId, ready.bootId);
assert.equal(state.feedback.request.expectedRuntimeGeneration, ready.runtime.runtimeGeneration);
assert.equal(state.feedback.request.expectedConfigRevision, ready.runtime.configRevision);
assert.equal(state.status.runtime.phase, "ready");
assert.equal(state.view.operationState, "awaiting-mock-progress");
assert.equal(state.canAdvanceMock, true);
assert(state.view.actions.every((action) => !action.enabled));

const generationAfterStart = state.status.runtime.runtimeGeneration;
state = session.repeatLastRequest();
assert.equal(state.feedback.kind, "duplicate");
assert.equal(state.feedback.canRepeat, true);
assert.equal(state.feedback.request.requestId, "session-test-0001");
assert.equal(state.status.runtime.runtimeGeneration, generationAfterStart);
assert.equal(state.requestNumber, 2);
state = session.submit({ type: "pause" });
assert.equal(state.feedback.kind, "error");
assert.equal(state.requestNumber, 2);
assert.equal(state.status.runtime.phase, "ready");
state = session.advanceMock();
assert.equal(state.feedback.kind, "mock-progress");
assert.equal(state.feedback.canRepeat, true);
assert.equal(state.status.runtime.phase, "running");
assert.equal(state.canAdvanceMock, false);
assert.equal(state.view.operationState, "idle");

state = session.submit({ type: "pause" });
assert.equal(state.status.runtime.phase, "running");
state = session.advanceMock();
assert.equal(state.status.runtime.phase, "paused");
state = session.submit({ type: "resume" });
state = session.advanceMock();
assert.equal(state.status.runtime.phase, "running");
state = session.submit({ type: "select-state", stateId: "finale" });
state = session.advanceMock();
assert.equal(state.status.runtime.stateId, "finale");
assert.equal(state.status.runtime.configRevision, ready.runtime.configRevision + 1);
state = session.submit({ type: "reset" });
state = session.advanceMock();
assert.equal(state.status.runtime.phase, "ready");

const beforeDisconnect = state.status.runtime.runtimeGeneration;
state = session.setConnected(false);
assert.equal(state.connection, "mock-disconnected");
assert(state.view.actions.every((action) => !action.enabled));
assert.equal(state.feedback.request.requestId, "session-test-0005");
const nextRequestNumber = state.requestNumber;
state = session.submit({ type: "start" });
assert.equal(state.feedback.kind, "error");
assert.equal(state.requestNumber, nextRequestNumber);
assert.equal(state.status.runtime.runtimeGeneration, beforeDisconnect);
state = session.setConnected(true);
assert.equal(state.feedback.kind, "error");
assert.equal(state.view.actions.find((action) => action.commandType === "start").enabled, true);

const conflict = new MockCompanionSession(ready);
state = conflict.submit({ type: "start" }, { simulateStaleRuntime: true });
assert.equal(state.feedback.kind, "conflict");
assert.equal(state.feedback.canRepeat, false);
assert.equal(state.feedback.result.reason, "conflict");
assert.equal(state.status.runtime.phase, "ready");
assert.equal(state.requestNumber, 2);
state = conflict.reloadStatus();
assert.equal(state.feedback, null);
state = conflict.submit({ type: "start" });
assert.equal(state.feedback.kind, "pending");
assert.equal(state.feedback.request.requestId, "vite-demo-0002");

const readonly = new MockCompanionSession(fixture("device-status.readonly.json"));
state = readonly.submit({ type: "start" });
assert.equal(state.feedback.kind, "error");
assert.equal(state.requestNumber, 1);
assert.equal(state.status.runtime.phase, "ready");

const invalid = new MockCompanionSession(ready);
state = invalid.submit({ type: "select-state" });
assert.equal(state.feedback.kind, "error");
assert.equal(state.requestNumber, 1);

const fixtureSession = new MockCompanionSession(ready);
fixtureSession.submit({ type: "start" });
state = fixtureSession.loadFixture(fixture("device-status.readonly.json"));
assert.equal(state.feedback.kind, "info");
assert.match(state.feedback.detail, /offene Testanfrage/);
assert.equal(state.canAdvanceMock, false);
assert.equal(state.requestNumber, 2);
assert(state.view.actions.every((action) => !action.enabled));
state = fixtureSession.loadFixture(fixture("device-status.paused.json"));
assert.equal(state.status.runtime.phase, "paused");
assert.equal(state.view.actions.find((action) => action.commandType === "resume").enabled, true);
state = fixtureSession.submit({ type: "reset" });
assert.equal(state.feedback.kind, "pending");
assert.equal(state.status.runtime.phase, "paused");
state = fixtureSession.advanceMock();
assert.equal(state.status.runtime.phase, "ready");
fixtureSession.setConnected(false);
state = fixtureSession.loadFixture(ready);
assert.equal(state.connection, "mock-disconnected");
assert(state.view.actions.every((action) => !action.enabled));

const technical = new MockCompanionSession(ready, {
  device: {
    readStatus: () => structuredClone(ready),
    sendGameMasterCommand: () => { throw new Error("Mock-Ausfall"); },
  },
});
state = technical.submit({ type: "start" });
assert.equal(state.feedback.kind, "error");
assert.equal(state.feedback.title, "Mock-Fehler");
assert.equal(state.feedback.detail, "Mock-Ausfall");
assert.equal(state.feedback.canRepeat, true);
assert.equal(state.feedback.request.requestId, "vite-demo-0001");

console.log("Mock session workflow tests passed");
