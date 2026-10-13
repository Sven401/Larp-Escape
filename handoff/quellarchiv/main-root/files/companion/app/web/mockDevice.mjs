import { deriveGameMasterActions } from "./domain.mjs";

function validateMockRequest(request) {
  const nonNegativeInteger = (value) => Number.isInteger(value) && value >= 0;
  if (request?.protocolVersion !== 0 || request.messageType !== "gm-command-request" ||
      typeof request.bootId !== "string" || !request.bootId ||
      typeof request.requestId !== "string" || !request.requestId ||
      !nonNegativeInteger(request.expectedRuntimeGeneration) ||
      !nonNegativeInteger(request.expectedConfigRevision) ||
      !["start", "pause", "resume", "reset", "select-state"].includes(request.command?.type) ||
      (request.command.type === "select-state" && (typeof request.command.stateId !== "string" || !request.command.stateId))) {
    throw new Error("Ungültiger v0-Mock-Request.");
  }
}

export class BrowserMockDevice {
  constructor(initialStatus) {
    this.status = structuredClone(initialStatus);
    this.acceptedRequests = new Set();
    this.pendingRequests = [];
  }

  readStatus() {
    return structuredClone(this.status);
  }

  sendGameMasterCommand(request) {
    validateMockRequest(request);
    const current = this.status;
    const base = {
      protocolVersion: 0,
      messageType: "gm-command-result",
      bootId: current.bootId,
      requestId: request.requestId,
      runtimeGeneration: current.runtime.runtimeGeneration,
      configRevision: current.runtime.configRevision,
    };
    if (this.acceptedRequests.has(request.requestId)) {
      return { ...base, status: "duplicate", detail: "already accepted" };
    }
    if (request.bootId !== current.bootId ||
        request.expectedRuntimeGeneration !== current.runtime.runtimeGeneration ||
        request.expectedConfigRevision !== current.runtime.configRevision) {
      return { ...base, status: "rejected", reason: "conflict", detail: "expected revision is stale" };
    }
    if (!deriveGameMasterActions(current).some((action) => action.commandType === request.command.type && action.enabled)) {
      return { ...base, status: "rejected", reason: "not_allowed", detail: "command is not available in the current mock status" };
    }
    this.acceptedRequests.add(request.requestId);
    this.pendingRequests.push(structuredClone(request));
    return { ...base, status: "accepted", detail: "queued" };
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
    if (command.type === "select-state") runtime.configRevision += 1;
    if (command.type === "start") Object.assign(runtime, { phase: "running", operation: "live", running: true, stateId: runtime.stateId || "intro", stateName: runtime.stateName || "Intro" });
    if (command.type === "pause") Object.assign(runtime, { phase: "paused", operation: "paused", running: false });
    if (command.type === "resume") Object.assign(runtime, { phase: "running", operation: "live", running: true });
    if (command.type === "reset") Object.assign(runtime, { phase: "ready", operation: "idle", running: false, stateId: null, stateName: null });
    if (command.type === "select-state") Object.assign(runtime, { phase: "running", operation: "live", running: true, stateId: command.stateId, stateName: command.stateId });
  }
}
