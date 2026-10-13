import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";
import { fileURLToPath } from "node:url";
import { BrowserMockDevice } from "../web/mockDevice.mjs";
import { classifyCommandResult, createGameMasterRequest, deriveGameMasterActions } from "../web/domain.mjs";
import { resultToViewState, statusToViewState } from "../web/presentation.mjs";

const require = createRequire(import.meta.url);
const { MockCompanionDevice } = require("../src/mockDevice.js");
const hostPresentation = require("../src/presentation.js");
const fixtureRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../contract/v0");

function pair(fixture = "device-status.ready.json") {
  const initial = JSON.parse(fs.readFileSync(path.join(fixtureRoot, fixture), "utf8"));
  return [new BrowserMockDevice(initial), new MockCompanionDevice({ fixtureRoot, statusFixture: fixture })];
}

function pairFromStatus(initial) {
  return [new BrowserMockDevice(initial), new MockCompanionDevice({ fixtureRoot, initialStatus: initial })];
}

function directRequest(status, requestId, command) {
  return {
    protocolVersion: 0,
    messageType: "gm-command-request",
    bootId: status.bootId,
    requestId,
    expectedRuntimeGeneration: status.runtime.runtimeGeneration,
    expectedConfigRevision: status.runtime.configRevision,
    command,
  };
}

function checkCommand(browser, host, id, command) {
  const before = browser.readStatus();
  assert.deepEqual(before, host.readStatus());
  const request = createGameMasterRequest(before, id, command);
  const browserResult = browser.sendGameMasterCommand(request);
  const hostResult = host.sendGameMasterCommand(request);
  assert.deepEqual(browserResult, hostResult);
  assert.deepEqual(browser.readStatus(), before);
  assert.deepEqual(browser.readStatus(), host.readStatus());
  assert.equal(classifyCommandResult(request, browserResult).kind, "pending");
  assert.deepEqual(browser.advanceMock(), host.advanceMock());
  assert.deepEqual(browser.readStatus(), host.readStatus());
  return request;
}

for (const fixture of ["device-status.ready.json", "device-status.running.json", "device-status.paused.json", "device-status.readonly.json"]) {
  const [browser, host] = pair(fixture);
  const expected = require("../src/domain.js").deriveGameMasterActions(host.readStatus());
  assert.deepEqual(deriveGameMasterActions(browser.readStatus()).map(({ commandType, enabled }) => ({ commandType, enabled })), expected.map(({ commandType, enabled }) => ({ commandType, enabled })));
  for (const connection of ["mock-connected", "mock-disconnected"]) {
    const webView = statusToViewState(browser.readStatus(), { connection });
    const hostView = hostPresentation.statusToViewState(host.readStatus(), { connection });
    for (const key of ["connection", "deviceId", "title", "mode", "operation", "profileName", "profileRevision", "storageGeneration", "storedMatchesActive", "warningCount", "lastError"]) {
      assert.deepEqual(webView[key], hostView[key], `${fixture}: ${connection}: ${key}`);
    }
    assert.deepEqual(webView.actions.map(({ commandType, enabled }) => ({ commandType, enabled })), hostView.actions.map(({ commandType, enabled }) => ({ commandType, enabled })));
    assert.deepEqual(webView.technical, hostView.technical);
    if (connection === "mock-disconnected") assert(webView.actions.every((action) => !action.enabled));
  }
  const pendingView = statusToViewState(browser.readStatus(), { operationState: "awaiting-mock-progress" });
  assert(pendingView.actions.every((action) => !action.enabled));
}

const [browser, host] = pair();
const start = checkCommand(browser, host, "browser-mock-1", { type: "start" });
const duplicate = browser.sendGameMasterCommand(start);
assert.deepEqual(duplicate, host.sendGameMasterCommand(start));
assert.equal(classifyCommandResult(start, duplicate).kind, "duplicate");
assert.equal(resultToViewState(start, duplicate).requestId, start.requestId);
assert.equal(resultToViewState(start, duplicate).expectedRuntimeGeneration, start.expectedRuntimeGeneration);
checkCommand(browser, host, "browser-mock-2", { type: "select-state", stateId: "finale" });
checkCommand(browser, host, "browser-mock-3", { type: "pause" });
checkCommand(browser, host, "browser-mock-4", { type: "resume" });
checkCommand(browser, host, "browser-mock-5", { type: "reset" });
assert.equal(browser.advanceMock(), null);
assert.equal(host.advanceMock(), null);

const [pausedBrowser, pausedHost] = pair("device-status.paused.json");
checkCommand(pausedBrowser, pausedHost, "browser-paused-reset", { type: "reset" });
assert.equal(pausedBrowser.readStatus().runtime.phase, "ready");

const [pendingBrowser, pendingHost] = pair();
const pendingRequest = createGameMasterRequest(pendingBrowser.readStatus(), "browser-pending", { type: "start" });
assert.deepEqual(pendingBrowser.sendGameMasterCommand(pendingRequest), pendingHost.sendGameMasterCommand(pendingRequest));
assert.deepEqual(pendingBrowser.sendGameMasterCommand(pendingRequest), pendingHost.sendGameMasterCommand(pendingRequest));
assert.equal(pendingBrowser.readStatus().runtime.phase, "ready");
assert.deepEqual(pendingBrowser.advanceMock(), pendingHost.advanceMock());
assert.equal(pendingBrowser.readStatus().runtime.phase, "running");

const [copiedBrowser, copiedHost] = pair();
const copiedRequest = createGameMasterRequest(copiedBrowser.readStatus(), "browser-copy", { type: "start" });
copiedBrowser.sendGameMasterCommand(copiedRequest);
copiedHost.sendGameMasterCommand(copiedRequest);
copiedRequest.command.type = "reset";
assert.deepEqual(copiedBrowser.advanceMock(), copiedHost.advanceMock());
assert.equal(copiedBrowser.readStatus().runtime.phase, "running");

const [conflictBrowser, conflictHost] = pair();
for (const field of ["bootId", "expectedRuntimeGeneration", "expectedConfigRevision"]) {
  const request = createGameMasterRequest(conflictBrowser.readStatus(), `browser-conflict-${field}`, { type: "start" });
  if (field === "bootId") request.bootId = "other-boot";
  else request[field] += 1;
  const result = conflictBrowser.sendGameMasterCommand(request);
  assert.deepEqual(result, conflictHost.sendGameMasterCommand(request));
  assert.equal(classifyCommandResult(request, result).kind, "conflict");
  assert.equal(result.reason, "conflict");
}

const [readonly] = pair("device-status.readonly.json");
assert.throws(() => createGameMasterRequest(readonly.readStatus(), "not-allowed", { type: "start" }));

const readyStatus = pair()[0].readStatus();
const withoutStart = structuredClone(readyStatus);
withoutStart.capabilities.commands = withoutStart.capabilities.commands.filter((command) => command !== "start");
for (const [scenario, status] of [
  ["readonly", readonly.readStatus()],
  ["capability", withoutStart],
  ["phase", pair("device-status.running.json")[0].readStatus()],
]) {
  const [browserDevice, hostDevice] = pairFromStatus(status);
  const request = directRequest(status, `direct-${scenario}`, { type: "start" });
  const browserResult = browserDevice.sendGameMasterCommand(request);
  assert.deepEqual(browserResult, hostDevice.sendGameMasterCommand(request));
  assert.equal(browserResult.status, "rejected");
  assert.equal(browserResult.reason, "not_allowed");
  assert.deepEqual(browserDevice.readStatus(), status);
  assert.equal(browserDevice.advanceMock(), null);
}

const [readonlyBrowser, readonlyHost] = pairFromStatus(readonly.readStatus());
const staleReadonly = directRequest(readonly.readStatus(), "readonly-stale", { type: "start" });
staleReadonly.expectedConfigRevision += 1;
const readonlyConflict = readonlyBrowser.sendGameMasterCommand(staleReadonly);
assert.deepEqual(readonlyConflict, readonlyHost.sendGameMasterCommand(staleReadonly));
assert.equal(readonlyConflict.reason, "conflict");

const [invalidBrowser, invalidHost] = pair();
for (const request of [
  { ...directRequest(readyStatus, "", { type: "start" }) },
  directRequest(readyStatus, "missing-target", { type: "select-state" }),
]) {
  assert.throws(() => invalidBrowser.sendGameMasterCommand(request));
  assert.throws(() => invalidHost.sendGameMasterCommand(request));
}

console.log("Browser mock parity tests passed");
