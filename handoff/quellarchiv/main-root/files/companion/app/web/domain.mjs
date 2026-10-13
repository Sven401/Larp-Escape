const COMMANDS = ["start", "pause", "resume", "reset", "select-state"];

function isAvailable(status, commandType) {
  if (!status.access.gameMaster || !status.capabilities.commands.includes(commandType)) return false;
  if (commandType === "start") return status.runtime.phase === "ready" && !status.runtime.running && status.profile.storedMatchesActive;
  if (commandType === "pause") return status.runtime.phase === "running" && status.runtime.running;
  if (commandType === "resume") return status.runtime.phase === "paused" && !status.runtime.running;
  if (commandType === "reset") return ["running", "paused", "fault"].includes(status.runtime.phase);
  return commandType === "select-state" && ["running", "paused"].includes(status.runtime.phase);
}

export function deriveGameMasterActions(status) {
  return COMMANDS.map((commandType) => ({ commandType, enabled: isAvailable(status, commandType) }));
}

export function createGameMasterRequest(status, requestId, command) {
  if (!COMMANDS.includes(command?.type) || !isAvailable(status, command.type)) {
    throw new Error("Aktion im aktuellen Geraetezustand nicht verfuegbar.");
  }
  if (!requestId || typeof requestId !== "string") throw new Error("Request-ID fehlt.");
  if (command.type === "select-state" && !command.stateId) throw new Error("Zielzustand fehlt.");
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

export function classifyCommandResult(request, result) {
  if (result.requestId !== request.requestId) {
    return { kind: "foreign-result", title: "Unpassende Antwort", detail: "Die Antwort gehört nicht zu dieser Anfrage." };
  }
  if (result.status === "rejected" && result.reason === "conflict") {
    return { kind: "conflict", title: "Gerätezustand geändert", detail: "Gerätezustand hat sich geändert; Status neu laden und erneut prüfen." };
  }
  if (result.bootId !== request.bootId) {
    return { kind: "foreign-result", title: "Unpassende Antwort", detail: "Die Antwort gehört zu einem anderen Gerätestart." };
  }
  if (result.status === "accepted") {
    return { kind: "pending", title: "Angenommen", detail: "Der Mock hat die Anfrage angenommen. Eine Firmwareausführung ist nicht bestätigt." };
  }
  if (result.status === "duplicate") {
    return { kind: "duplicate", title: "Bereits angenommen", detail: "Diese Request-ID ist bekannt; der Befehl wurde nicht erneut angewendet." };
  }
  return { kind: "error", title: "Anfrage fehlgeschlagen", detail: result.detail || "Der Mock konnte die Anfrage nicht verarbeiten." };
}
