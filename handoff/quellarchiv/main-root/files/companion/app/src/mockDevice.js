const fs = require("fs");
const path = require("path");
const { validateDeviceStatus, validateCommandRequest } = require("./contract");
const { deriveGameMasterActions } = require("./domain");

class MockCompanionDevice {
  constructor(options) {
    this.fixtureRoot = options.fixtureRoot;
    this.statusFixture = options.statusFixture || "device-status.ready.json";
    this.requests = new Set();
    this.pendingRequests = [];
    this.status = options.initialStatus ? JSON.parse(JSON.stringify(options.initialStatus)) : this.readJson(this.statusFixture);
  }

  readJson(name) {
    return JSON.parse(fs.readFileSync(path.join(this.fixtureRoot, name), "utf8"));
  }

  readStatus() {
    return validateDeviceStatus(JSON.parse(JSON.stringify(this.status)));
  }

  sendGameMasterCommand(request) {
    validateCommandRequest(request);
    const status = this.readStatus();
    const stale = request.bootId !== status.bootId ||
      request.expectedRuntimeGeneration !== status.runtime.runtimeGeneration ||
      request.expectedConfigRevision !== status.runtime.configRevision;

    if (this.requests.has(request.requestId)) {
      return {
        protocolVersion: 0,
        messageType: "gm-command-result",
        bootId: status.bootId,
        requestId: request.requestId,
        status: "duplicate",
        runtimeGeneration: status.runtime.runtimeGeneration,
        configRevision: status.runtime.configRevision,
        detail: "already accepted",
      };
    }

    if (stale) {
      return {
        protocolVersion: 0,
        messageType: "gm-command-result",
        bootId: status.bootId,
        requestId: request.requestId,
        status: "rejected",
        reason: "conflict",
        runtimeGeneration: status.runtime.runtimeGeneration,
        configRevision: status.runtime.configRevision,
        detail: "expected revision is stale",
      };
    }

    if (!deriveGameMasterActions(status).some((action) => action.commandType === request.command.type && action.enabled)) {
      return {
        protocolVersion: 0,
        messageType: "gm-command-result",
        bootId: status.bootId,
        requestId: request.requestId,
        status: "rejected",
        reason: "not_allowed",
        runtimeGeneration: status.runtime.runtimeGeneration,
        configRevision: status.runtime.configRevision,
        detail: "command is not available in the current mock status",
      };
    }

    this.requests.add(request.requestId);
    this.pendingRequests.push(JSON.parse(JSON.stringify(request)));
    return {
      protocolVersion: 0,
      messageType: "gm-command-result",
      bootId: status.bootId,
      requestId: request.requestId,
      status: "accepted",
      runtimeGeneration: status.runtime.runtimeGeneration,
      configRevision: status.runtime.configRevision,
      detail: "queued",
    };
  }

  advanceMock() {
    const request = this.pendingRequests.shift();
    if (!request) return null;
    this.applyAcceptedCommand(request.command);
    return { requestId: request.requestId, status: this.readStatus() };
  }

  applyAcceptedCommand(command) {
    const runtime = this.status.runtime;
    runtime.runtimeGeneration += 1;
    runtime.configRevision += command.type === "select-state" ? 1 : 0;

    if (command.type === "start") {
      runtime.phase = "running";
      runtime.operation = "live";
      runtime.running = true;
      runtime.stateId = runtime.stateId || "intro";
      runtime.stateName = runtime.stateName || "Intro";
    } else if (command.type === "pause") {
      runtime.phase = "paused";
      runtime.operation = "paused";
      runtime.running = false;
    } else if (command.type === "resume") {
      runtime.phase = "running";
      runtime.operation = "live";
      runtime.running = true;
    } else if (command.type === "reset") {
      runtime.phase = "ready";
      runtime.operation = "idle";
      runtime.running = false;
      runtime.stateId = null;
      runtime.stateName = null;
    } else if (command.type === "select-state") {
      runtime.phase = "running";
      runtime.operation = "live";
      runtime.running = true;
      runtime.stateId = command.stateId;
      runtime.stateName = command.stateId;
    }
  }
}

module.exports = {
  MockCompanionDevice,
};
