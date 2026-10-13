const { classifyCommandResult, deriveGameMasterActions } = require("./domain");
const { validateDeviceStatus } = require("./contract");

function statusToViewState(status, options = {}) {
  validateDeviceStatus(status);
  const connection = options.connection || "mock-connected";
  const actions = deriveGameMasterActions(status).map((action) => ({
    ...action,
    enabled: connection === "mock-connected" && (options.operationState || "idle") === "idle" && action.enabled,
  }));
  const mode = status.runtime.running ? "running" : status.runtime.phase;
  return {
    connection,
    deviceId: status.deviceId,
    title: status.runtime.stateName || "Bereit",
    mode,
    operation: status.runtime.operation,
    profileName: status.profile.activeProfileId,
    profileRevision: status.profile.documentRevision,
    storageGeneration: status.profile.storageGeneration,
    storedMatchesActive: status.profile.storedMatchesActive,
    actions,
    operationState: options.operationState || "idle",
    lastRequest: options.lastRequest || null,
    startAction: actions.find((action) => action.commandType === "start"),
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

function resultToViewState(request, result) {
  const outcome = classifyCommandResult(request, result);
  const severityByKind = {
    pending: "info",
    duplicate: "info",
    conflict: "warning",
    rejected: "warning",
    succeeded: "success",
    failed: "error",
    "foreign-result": "ignore",
  };
  return {
    requestId: request.requestId,
    kind: outcome.kind,
    severity: severityByKind[outcome.kind],
    title: outcome.label,
    message: outcome.detail,
    runtimeGeneration: result.runtimeGeneration,
    configRevision: result.configRevision,
    canRepeat: outcome.kind === "pending" || outcome.kind === "duplicate",
  };
}

module.exports = {
  statusToViewState,
  resultToViewState,
};
