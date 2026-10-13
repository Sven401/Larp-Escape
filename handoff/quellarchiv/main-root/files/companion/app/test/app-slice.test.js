const assert = require("assert");
const path = require("path");

const { MockCompanionDevice } = require("../src/mockDevice");
const {
  createPauseGameRequest,
  createResetGameRequest,
  createResumeGameRequest,
  createSelectStateRequest,
  createStartGameRequest,
  deriveGameMasterActions,
} = require("../src/domain");
const { statusToViewState, resultToViewState } = require("../src/presentation");
const { buildGameMasterScreen, renderGameMasterScreenText } = require("../src/screenModel");

const fixtureRoot = path.resolve(__dirname, "..", "..", "contract", "v0");

function createDevice() {
  return new MockCompanionDevice({ fixtureRoot });
}

function enabledActions(status) {
  return deriveGameMasterActions(status)
    .filter((action) => action.enabled)
    .map((action) => action.commandType);
}

function testStatusMapsToPresentationState() {
  const device = createDevice();
  const status = device.readStatus();
  const view = statusToViewState(status);

  assert.strictEqual(view.connection, "mock-connected");
  assert.strictEqual(view.mode, "ready");
  assert.strictEqual(view.profileName, "standard");
  assert.strictEqual(view.startAction.enabled, true);
  assert.strictEqual(view.startAction.label, "Spiel starten");
  assert.deepStrictEqual(view.actions.map((action) => action.commandType), [
    "start",
    "pause",
    "resume",
    "reset",
    "select-state",
  ]);
  assert.strictEqual(view.technical.bootId, status.bootId);
  assert.strictEqual(view.technical.runtimeGeneration, status.runtime.runtimeGeneration);
  assert.strictEqual(view.technical.configRevision, status.runtime.configRevision);
}

function testActionsFollowStatusAndAccess() {
  const running = createDeviceWithStatus("device-status.running.json").readStatus();
  const paused = createDeviceWithStatus("device-status.paused.json").readStatus();
  const readonly = createDeviceWithStatus("device-status.readonly.json").readStatus();

  assert.deepStrictEqual(enabledActions(running), ["pause", "reset", "select-state"]);
  assert.deepStrictEqual(enabledActions(paused), ["resume", "reset", "select-state"]);
  assert.deepStrictEqual(enabledActions(readonly), []);
}

function createDeviceWithStatus(statusFixture) {
  return new MockCompanionDevice({ fixtureRoot, statusFixture });
}

function testStartRequestUsesCurrentBootAndRevisions() {
  const device = createDevice();
  const status = device.readStatus();
  const request = createStartGameRequest(status, "app-session-b-test-0001");

  assert.strictEqual(request.bootId, status.bootId);
  assert.strictEqual(request.requestId, "app-session-b-test-0001");
  assert.strictEqual(request.expectedRuntimeGeneration, status.runtime.runtimeGeneration);
  assert.strictEqual(request.expectedConfigRevision, status.runtime.configRevision);
  assert.strictEqual(request.command.type, "start");
}

function testAcceptedAndDuplicateResultsAreDistinct() {
  const device = createDevice();
  const status = device.readStatus();
  const request = createStartGameRequest(status, "app-session-b-test-0002");

  const accepted = device.sendGameMasterCommand(request);
  const duplicate = device.sendGameMasterCommand(request);
  const acceptedView = resultToViewState(request, accepted);
  const duplicateView = resultToViewState(request, duplicate);

  assert.strictEqual(accepted.status, "accepted");
  assert.strictEqual(acceptedView.kind, "pending");
  assert.strictEqual(acceptedView.severity, "info");
  assert.strictEqual(acceptedView.runtimeGeneration, accepted.runtimeGeneration);
  assert.strictEqual(acceptedView.configRevision, accepted.configRevision);
  assert.strictEqual(duplicate.status, "duplicate");
  assert.strictEqual(duplicateView.kind, "duplicate");
  assert.strictEqual(duplicateView.severity, "info");
}

function testBootAndRevisionConflictsAreRejected() {
  const device = createDevice();
  const status = device.readStatus();

  const wrongBoot = createStartGameRequest(status, "app-session-b-boot-conflict");
  wrongBoot.bootId = "another-boot";
  const bootResult = device.sendGameMasterCommand(wrongBoot);
  assert.strictEqual(bootResult.status, "rejected");
  assert.strictEqual(bootResult.reason, "conflict");
  assert.strictEqual(resultToViewState(wrongBoot, bootResult).kind, "conflict");

  const staleRevision = createStartGameRequest(status, "app-session-b-revision-conflict");
  staleRevision.expectedRuntimeGeneration -= 1;
  const revisionResult = device.sendGameMasterCommand(staleRevision);
  assert.strictEqual(revisionResult.status, "rejected");
  assert.strictEqual(revisionResult.reason, "conflict");
}

function testPresentationKeepsRequestAndOperationState() {
  const device = createDevice();
  const status = device.readStatus();
  const request = createStartGameRequest(status, "app-session-b-presentation-0001");
  const view = statusToViewState(status, { connection: "mock-disconnected", operationState: "sending", lastRequest: request });

  assert.strictEqual(view.connection, "mock-disconnected");
  assert.strictEqual(view.operationState, "sending");
  assert.strictEqual(view.lastRequest.requestId, request.requestId);
  assert.strictEqual(view.actions.find((action) => action.commandType === "start").enabled, false);
  assert.strictEqual(view.startAction.enabled, false);
}

function testMockDeviceAdvancesRuntimeForNarrowCommands() {
  const device = createDevice();
  const ready = device.readStatus();
  const start = device.sendGameMasterCommand(createStartGameRequest(ready, "app-session-b-flow-0001"));
  assert.strictEqual(start.status, "accepted");
  assert.deepStrictEqual(device.readStatus(), ready);
  assert.strictEqual(statusToViewState(ready, { operationState: "awaiting-mock-progress" }).startAction.enabled, false);
  assert.strictEqual(device.advanceMock().requestId, "app-session-b-flow-0001");

  const running = device.readStatus();
  assert.strictEqual(running.runtime.phase, "running");
  assert.deepStrictEqual(enabledActions(running), ["pause", "reset", "select-state"]);

  const select = createSelectStateRequest(running, "app-session-b-flow-0002", "finale");
  const selected = device.sendGameMasterCommand(select);
  assert.strictEqual(selected.status, "accepted");
  assert.strictEqual(device.readStatus().runtime.stateId, "intro");
  device.advanceMock();
  assert.strictEqual(device.readStatus().runtime.stateId, "finale");

  const pause = createPauseGameRequest(device.readStatus(), "app-session-b-flow-0003");
  const paused = device.sendGameMasterCommand(pause);
  assert.strictEqual(paused.status, "accepted");
  device.advanceMock();
  assert.strictEqual(device.readStatus().runtime.phase, "paused");

  const resume = createResumeGameRequest(device.readStatus(), "app-session-b-flow-0004");
  const resumed = device.sendGameMasterCommand(resume);
  assert.strictEqual(resumed.status, "accepted");
  device.advanceMock();
  assert.strictEqual(device.readStatus().runtime.phase, "running");

  const reset = createResetGameRequest(device.readStatus(), "app-session-b-flow-0005");
  const resetResult = device.sendGameMasterCommand(reset);
  assert.strictEqual(resetResult.status, "accepted");
  device.advanceMock();
  assert.strictEqual(device.readStatus().runtime.phase, "ready");
  assert.strictEqual(device.advanceMock(), null);
}

function testQueuedMockCommandIsCopied() {
  const device = createDevice();
  const request = createStartGameRequest(device.readStatus(), "app-session-b-copy-0001");
  device.sendGameMasterCommand(request);
  request.command.type = "reset";
  device.advanceMock();
  assert.strictEqual(device.readStatus().runtime.phase, "running");
}

function testRejectedConflictIsVisible() {
  const device = createDevice();
  const status = device.readStatus();
  const request = createStartGameRequest(status, "app-session-b-test-0003");
  request.expectedConfigRevision -= 1;

  const rejected = device.sendGameMasterCommand(request);
  const view = resultToViewState(request, rejected);

  assert.strictEqual(rejected.status, "rejected");
  assert.strictEqual(rejected.reason, "conflict");
  assert.strictEqual(view.kind, "conflict");
  assert.strictEqual(view.severity, "warning");
  assert.strictEqual(view.title, "Gerätezustand geändert");
}

function testGameMasterScreenModelShowsStatusAndNarrowControls() {
  const device = createDeviceWithStatus("device-status.running.json");
  const status = device.readStatus();
  const screen = buildGameMasterScreen(status);
  const text = renderGameMasterScreenText(screen);

  assert.strictEqual(screen.screenTitle, "Spielleitung");
  assert.strictEqual(screen.connection.primary, "Mock-Geraet verbunden");
  assert.deepStrictEqual(screen.actions.map((action) => action.commandType), [
    "pause",
    "reset",
    "select-state",
  ]);
  assert(text.includes("Profil: standard"));
  assert(text.includes("Spiel: Intro"));
  assert(!text.includes("Spiel starten"));
}

function testDisconnectedScreenKeepsLastStatusWithoutControls() {
  const status = createDevice().readStatus();
  const screen = buildGameMasterScreen(status, null, null, { connection: "mock-disconnected" });

  assert.strictEqual(screen.connection.state, "mock-disconnected");
  assert.strictEqual(screen.connection.primary, "Geraet nicht verbunden");
  assert.strictEqual(screen.profile.primary, status.profile.activeProfileId);
  assert.deepStrictEqual(screen.actions, []);
}

testStatusMapsToPresentationState();
testActionsFollowStatusAndAccess();
testStartRequestUsesCurrentBootAndRevisions();
testAcceptedAndDuplicateResultsAreDistinct();
testBootAndRevisionConflictsAreRejected();
testPresentationKeepsRequestAndOperationState();
testMockDeviceAdvancesRuntimeForNarrowCommands();
testQueuedMockCommandIsCopied();
testRejectedConflictIsVisible();
testGameMasterScreenModelShowsStatusAndNarrowControls();
testDisconnectedScreenKeepsLastStatusWithoutControls();

console.log("Companion app slice tests passed");
