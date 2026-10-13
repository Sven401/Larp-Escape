const { validateDeviceStatus, validateCommandResult } = require("./contract");

const COMMAND_LABELS = {
  start: "Spiel starten",
  pause: "Pausieren",
  resume: "Fortsetzen",
  reset: "Zuruecksetzen",
  "select-state": "Zustand wechseln",
};

function hasGameMasterAccess(status) {
  return status.access.gameMaster === true;
}

function canStartGame(status) {
  validateDeviceStatus(status);
  return status.runtime.phase === "ready" &&
    status.runtime.running === false &&
    status.profile.storedMatchesActive === true &&
    status.access.gameMaster === true &&
    status.capabilities.commands.includes("start");
}

function canPauseGame(status) {
  validateDeviceStatus(status);
  return status.runtime.phase === "running" &&
    status.runtime.running === true &&
    hasGameMasterAccess(status) &&
    status.capabilities.commands.includes("pause");
}

function canResumeGame(status) {
  validateDeviceStatus(status);
  return status.runtime.phase === "paused" &&
    status.runtime.running === false &&
    hasGameMasterAccess(status) &&
    status.capabilities.commands.includes("resume");
}

function canResetGame(status) {
  validateDeviceStatus(status);
  return ["running", "paused", "fault"].includes(status.runtime.phase) &&
    hasGameMasterAccess(status) &&
    status.capabilities.commands.includes("reset");
}

function canSelectState(status) {
  validateDeviceStatus(status);
  return ["running", "paused"].includes(status.runtime.phase) &&
    hasGameMasterAccess(status) &&
    status.capabilities.commands.includes("select-state");
}

function isCommandAvailable(status, commandType) {
  const checks = {
    start: canStartGame,
    pause: canPauseGame,
    resume: canResumeGame,
    reset: canResetGame,
    "select-state": canSelectState,
  };
  if (!checks[commandType]) {
    throw new Error(`unsupported command: ${commandType}`);
  }
  return checks[commandType](status);
}

function deriveGameMasterActions(status) {
  validateDeviceStatus(status);
  return Object.keys(COMMAND_LABELS).map((commandType) => {
    const enabled = isCommandAvailable(status, commandType);
    return {
      commandType,
      label: enabled ? COMMAND_LABELS[commandType] : `${COMMAND_LABELS[commandType]} nicht verfuegbar`,
      enabled,
    };
  });
}

function createGameMasterRequest(status, requestId, command) {
  validateDeviceStatus(status);
  if (!command || typeof command.type !== "string") {
    throw new Error("command.type is required");
  }
  if (!isCommandAvailable(status, command.type)) {
    throw new Error(`${command.type} command is not currently available`);
  }
  if (typeof requestId !== "string" || requestId.length === 0) {
    throw new Error("requestId must be a non-empty string");
  }
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

function createStartGameRequest(status, requestId) {
  return createGameMasterRequest(status, requestId, { type: "start" });
}

function createPauseGameRequest(status, requestId) {
  return createGameMasterRequest(status, requestId, { type: "pause" });
}

function createResumeGameRequest(status, requestId) {
  return createGameMasterRequest(status, requestId, { type: "resume" });
}

function createResetGameRequest(status, requestId) {
  return createGameMasterRequest(status, requestId, { type: "reset" });
}

function createSelectStateRequest(status, requestId, stateId) {
  if (typeof stateId !== "string" || stateId.length === 0) {
    throw new Error("stateId must be a non-empty string");
  }
  return createGameMasterRequest(status, requestId, {
    type: "select-state",
    stateId,
  });
}

function classifyCommandResult(request, result) {
  validateCommandResult(result);
  if (result.requestId !== request.requestId) {
    return {
      kind: "foreign-result",
      label: "Different request",
      detail: "The result belongs to another command.",
    };
  }
  if (result.status === "rejected" && result.reason === "conflict") {
    return {
      kind: "conflict",
      label: "Gerätezustand geändert",
      detail: "Status neu laden und den Befehl anschließend erneut prüfen.",
    };
  }
  if (result.bootId !== request.bootId) {
    return {
      kind: "foreign-result",
      label: "Wrong device session",
      detail: "The result belongs to a different boot.",
    };
  }
  if (result.status === "accepted") {
    return {
      kind: "pending",
      label: "Angenommen",
      detail: "Der Befehl wurde angenommen und zur Ausführung vorgemerkt.",
    };
  }
  if (result.status === "duplicate") {
    return {
      kind: "duplicate",
      label: "Bereits angenommen",
      detail: "Die Request-ID wurde wiedererkannt; der Befehl wurde nicht erneut ausgeführt.",
    };
  }
  if (result.status === "rejected") {
    return {
      kind: "rejected",
      label: "Abgelehnt",
      detail: result.detail || result.reason,
    };
  }
  if (result.status === "succeeded") {
    return {
      kind: "succeeded",
      label: "Ausgeführt",
      detail: "Das Gerät hat die Ausführung bestätigt.",
    };
  }
  return {
    kind: "failed",
    label: "Ausführung fehlgeschlagen",
    detail: result.detail || "Das Gerät konnte den Befehl nicht ausführen.",
  };
}

module.exports = {
  canPauseGame,
  canResetGame,
  canResumeGame,
  canSelectState,
  canStartGame,
  createGameMasterRequest,
  createPauseGameRequest,
  createResetGameRequest,
  createResumeGameRequest,
  createSelectStateRequest,
  createStartGameRequest,
  deriveGameMasterActions,
  classifyCommandResult,
};
