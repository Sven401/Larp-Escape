const VALID_COMMANDS = ["start", "pause", "resume", "reset", "select-state"];
const VALID_RESULT_STATUS = ["accepted", "rejected", "duplicate", "succeeded", "failed"];

function requireString(value, field) {
  if (typeof value !== "string" || value.length === 0) {
    throw new Error(`${field} must be a non-empty string`);
  }
}

function requireInteger(value, field) {
  if (!Number.isInteger(value) || value < 0) {
    throw new Error(`${field} must be a non-negative integer`);
  }
}

function validateEnvelope(message, expectedType) {
  requireInteger(message.protocolVersion, "protocolVersion");
  if (message.protocolVersion !== 0) {
    throw new Error("unsupported protocolVersion");
  }
  if (message.messageType !== expectedType) {
    throw new Error(`messageType must be ${expectedType}`);
  }
  requireString(message.bootId, "bootId");
}

function validateDeviceStatus(message) {
  validateEnvelope(message, "device-status");
  requireString(message.deviceId, "deviceId");
  if (!message.profile) throw new Error("profile is required");
  requireString(message.profile.activeProfileId, "profile.activeProfileId");
  requireInteger(message.profile.documentRevision, "profile.documentRevision");
  requireInteger(message.profile.storageGeneration, "profile.storageGeneration");
  if (typeof message.profile.storedMatchesActive !== "boolean") {
    throw new Error("profile.storedMatchesActive must be boolean");
  }
  if (!message.runtime) throw new Error("runtime is required");
  requireString(message.runtime.phase, "runtime.phase");
  if (!["ready", "fault", "booting", "setup", "running", "paused"].includes(message.runtime.phase)) {
    throw new Error("runtime.phase is not supported");
  }
  if (typeof message.runtime.running !== "boolean") {
    throw new Error("runtime.running must be boolean");
  }
  requireInteger(message.runtime.runtimeGeneration, "runtime.runtimeGeneration");
  requireInteger(message.runtime.configRevision, "runtime.configRevision");
  if (!message.access) throw new Error("access is required");
  for (const key of ["observer", "gameMaster", "setup"]) {
    if (typeof message.access[key] !== "boolean") {
      throw new Error(`access.${key} must be boolean`);
    }
  }
  if (!message.capabilities || !Array.isArray(message.capabilities.commands)) {
    throw new Error("capabilities.commands must be an array");
  }
  return message;
}

function validateCommandRequest(message) {
  validateEnvelope(message, "gm-command-request");
  requireString(message.requestId, "requestId");
  requireInteger(message.expectedRuntimeGeneration, "expectedRuntimeGeneration");
  requireInteger(message.expectedConfigRevision, "expectedConfigRevision");
  if (!message.command || !VALID_COMMANDS.includes(message.command.type)) {
    throw new Error("command.type is not supported");
  }
  if (message.command.type === "select-state") {
    requireString(message.command.stateId, "command.stateId");
  }
  return message;
}

function validateCommandResult(message) {
  validateEnvelope(message, "gm-command-result");
  requireString(message.requestId, "requestId");
  if (!VALID_RESULT_STATUS.includes(message.status)) {
    throw new Error("status is not supported");
  }
  if (message.status === "rejected") {
    requireString(message.reason, "reason");
  }
  requireInteger(message.runtimeGeneration, "runtimeGeneration");
  requireInteger(message.configRevision, "configRevision");
  return message;
}

module.exports = {
  validateDeviceStatus,
  validateCommandRequest,
  validateCommandResult,
};
