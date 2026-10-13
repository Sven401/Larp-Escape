import { classifyCommandResult, deriveGameMasterActions } from "./domain.mjs";

export function statusToViewState(status, options = {}) {
  const connection = options.connection || "mock-connected";
  const actions = deriveGameMasterActions(status).map((action) => ({
    ...action,
    enabled: connection === "mock-connected" && (options.operationState || "idle") === "idle" && action.enabled,
  }));
  return {
    connection,
    deviceId: status.deviceId,
    title: status.runtime.stateName || "Bereit",
    mode: status.runtime.running ? "running" : status.runtime.phase,
    operation: status.runtime.operation,
    profileName: status.profile.activeProfileId,
    profileRevision: status.profile.documentRevision,
    storageGeneration: status.profile.storageGeneration,
    storedMatchesActive: status.profile.storedMatchesActive,
    actions,
    operationState: options.operationState || "idle",
    lastRequest: options.lastRequest || null,
    warningCount: status.health.warnings.length,
    lastError: status.health.lastError,
    technical: {
      bootId: status.bootId,
      runtimeGeneration: status.runtime.runtimeGeneration,
      configRevision: status.runtime.configRevision,
      activeStateId: status.runtime.stateId,
    },
  };
}

export function resultToViewState(request, result) {
  const outcome = classifyCommandResult(request, result);
  return {
    ...outcome,
    request,
    result,
    requestId: request.requestId,
    expectedRuntimeGeneration: request.expectedRuntimeGeneration,
    expectedConfigRevision: request.expectedConfigRevision,
    canRepeat: outcome.kind === "pending" || outcome.kind === "duplicate",
  };
}
