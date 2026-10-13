const fs = require("fs");
const path = require("path");
const assert = require("assert");

const root = path.resolve(__dirname, "..", "v0");

function readFixture(name) {
  return JSON.parse(fs.readFileSync(path.join(root, name), "utf8"));
}

function assertString(value, field) {
  assert.strictEqual(typeof value, "string", `${field} must be a string`);
  assert(value.length > 0, `${field} must not be empty`);
}

function assertInteger(value, field) {
  assert(Number.isInteger(value), `${field} must be an integer`);
  assert(value >= 0, `${field} must be non-negative`);
}

function validateEnvelope(message, expectedType) {
  assertInteger(message.protocolVersion, "protocolVersion");
  assert.strictEqual(message.protocolVersion, 0, "protocolVersion");
  assert.strictEqual(message.messageType, expectedType, "messageType");
  assertString(message.bootId, "bootId");
}

function validateStatus(message) {
  validateEnvelope(message, "device-status");
  assertString(message.deviceId, "deviceId");
  assert(message.profile, "profile is required");
  assertString(message.profile.activeProfileId, "profile.activeProfileId");
  assertInteger(message.profile.documentRevision, "profile.documentRevision");
  assertInteger(message.profile.storageGeneration, "profile.storageGeneration");
  assert.strictEqual(typeof message.profile.storedMatchesActive, "boolean", "profile.storedMatchesActive");
  assert(message.runtime, "runtime is required");
  assert(["ready", "fault", "booting", "setup", "running", "paused"].includes(message.runtime.phase), "runtime.phase");
  assert.strictEqual(typeof message.runtime.running, "boolean", "runtime.running");
  assertInteger(message.runtime.runtimeGeneration, "runtime.runtimeGeneration");
  assertInteger(message.runtime.configRevision, "runtime.configRevision");
  assert(message.access, "access is required");
  assert.strictEqual(typeof message.access.observer, "boolean", "access.observer");
  assert.strictEqual(typeof message.access.gameMaster, "boolean", "access.gameMaster");
  assert.strictEqual(typeof message.access.setup, "boolean", "access.setup");
  assert(Array.isArray(message.capabilities.commands), "capabilities.commands");
}

function validateCommandRequest(message) {
  validateEnvelope(message, "gm-command-request");
  assertString(message.requestId, "requestId");
  assertInteger(message.expectedRuntimeGeneration, "expectedRuntimeGeneration");
  assertInteger(message.expectedConfigRevision, "expectedConfigRevision");
  assert(message.command, "command is required");
  assert(["start", "pause", "resume", "reset", "select-state"].includes(message.command.type), "command.type");
  if (message.command.type === "select-state") {
    assertString(message.command.stateId, "command.stateId");
  }
}

function validateCommandResult(message) {
  validateEnvelope(message, "gm-command-result");
  assertString(message.requestId, "requestId");
  assert(["accepted", "rejected", "duplicate", "succeeded", "failed"].includes(message.status), "status");
  assertInteger(message.runtimeGeneration, "runtimeGeneration");
  assertInteger(message.configRevision, "configRevision");
}

function controlsEnabled(status) {
  return status.runtime.phase === "ready" &&
    status.profile.storedMatchesActive &&
    status.access.gameMaster &&
    status.capabilities.commands.includes("start");
}

const status = readFixture("device-status.ready.json");
const runningStatus = readFixture("device-status.running.json");
const pausedStatus = readFixture("device-status.paused.json");
const readOnlyStatus = readFixture("device-status.readonly.json");
const request = readFixture("gm-command-start.request.json");
const pauseRequest = readFixture("gm-command-pause.request.json");
const resumeRequest = readFixture("gm-command-resume.request.json");
const resetRequest = readFixture("gm-command-reset.request.json");
const selectStateRequest = readFixture("gm-command-select-state.request.json");
const accepted = readFixture("gm-command-start.accepted.json");
const duplicate = readFixture("gm-command-start.duplicate.json");
const rejectedConflict = readFixture("gm-command-start.rejected-conflict.json");

validateStatus(status);
validateStatus(runningStatus);
validateStatus(pausedStatus);
validateStatus(readOnlyStatus);
validateCommandRequest(request);
validateCommandRequest(pauseRequest);
validateCommandRequest(resumeRequest);
validateCommandRequest(resetRequest);
validateCommandRequest(selectStateRequest);
validateCommandResult(accepted);
validateCommandResult(duplicate);
validateCommandResult(rejectedConflict);

assert(controlsEnabled(status), "game-master start control should be enabled");
assert.strictEqual(request.bootId, status.bootId, "request must target current boot");
assert.strictEqual(request.expectedRuntimeGeneration, status.runtime.runtimeGeneration, "runtime generation must match");
assert.strictEqual(request.expectedConfigRevision, status.runtime.configRevision, "config revision must match");
assert.strictEqual(accepted.requestId, request.requestId, "accepted result must correlate to request");
assert.strictEqual(duplicate.requestId, request.requestId, "duplicate result must correlate to request");
assert.strictEqual(rejectedConflict.status, "rejected", "conflict fixture must be rejected");
assert.strictEqual(rejectedConflict.reason, "conflict", "conflict fixture must name conflict reason");
assert.strictEqual(pauseRequest.expectedRuntimeGeneration, runningStatus.runtime.runtimeGeneration, "pause request must target running generation");
assert.strictEqual(resumeRequest.expectedRuntimeGeneration, pausedStatus.runtime.runtimeGeneration, "resume request must target paused generation");
assert.strictEqual(readOnlyStatus.access.gameMaster, false, "readonly fixture must withhold game-master access");
assert.throws(
  () => validateStatus({ ...status, runtime: { ...status.runtime, phase: "sleeping" } }),
  /runtime.phase/,
  "unknown runtime phases must be rejected",
);
assert.throws(
  () => validateCommandRequest({ ...selectStateRequest, command: { type: "select-state" } }),
  /command.stateId/,
  "state selection requires a target state",
);
assert.strictEqual(controlsEnabled(readOnlyStatus), false, "readonly status must not expose start");

console.log("Companion contract v0 fixtures passed");
