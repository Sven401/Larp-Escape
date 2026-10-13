const MAX_U32 = 0xffffffff;
const MAX_U16 = 0xffff;
const PHASES = new Set(["empty", "pending_activation", "applying", "saving", "ready", "fault"]);

function uint(value, name, maximum = MAX_U32, minimum = 0) {
  if (!Number.isInteger(value) || value < minimum || value > maximum) {
    throw new Error(`${name} ist keine gueltige Ganzzahl.`);
  }
  return value;
}

function bool(value, name) {
  if (typeof value !== "boolean") throw new Error(`${name} muss boolesch sein.`);
  return value;
}

export function mapFirmwareV2Status(status) {
  if (!status || status.schemaVersion !== 2) throw new Error("Firmware-Status v2 erwartet.");
  if (typeof status.boot !== "string" || !/^(?!0000000000000000$)[0-9A-F]{16}$/.test(status.boot)) {
    throw new Error("Ungültige Firmware-Bootkennung.");
  }
  if (!Array.isArray(status.capabilities) || status.capabilities.length !== 2 ||
      status.capabilities[0] !== "status.configured.read" || status.capabilities[1] !== "game.control.core") {
    throw new Error("Unbekannte Firmware-Fähigkeiten.");
  }
  if (!status.application || !PHASES.has(status.application.phase) || !status.control || !status.game || !Array.isArray(status.devices) || status.devices.length > 16) {
    throw new Error("Unvollständiger Firmware-Status.");
  }
  const app = status.application;
  const game = status.game;
  const ids = new Set();
  const devices = status.devices.map((device) => {
    if (!device) throw new Error("Ungültiger Geräteeintrag.");
    const id = uint(device.id, "devices.id", MAX_U16, 1);
    if (ids.has(id)) throw new Error("Doppelte Geräte-ID.");
    ids.add(id);
    if (typeof device.lastOutcome !== "string") throw new Error("Ungültiger Geräteausgang.");
    return {
      id,
      enabled: bool(device.enabled, "devices.enabled"),
      available: bool(device.available, "devices.available"),
      lastOutcome: device.lastOutcome,
    };
  });
  const stateId = game.state === null ? null : uint(game.state, "game.state", MAX_U16, 1);
  const roleId = game.role === null ? null : uint(game.role, "game.role", MAX_U16, 1);
  const running = bool(game.running, "game.running");
  const paused = bool(game.paused, "game.paused");
  return {
    contract: "firmware-v2-fixture",
    boot: status.boot,
    deviceId: null,
    sample: uint(status.sample, "sample", MAX_U32, 1),
    sampledMilliseconds: uint(status.sampledMilliseconds, "sampledMilliseconds"),
    application: {
      phase: app.phase,
      hasActive: bool(app.hasActive, "application.hasActive"),
      hardwareReady: bool(app.hardwareReady, "application.hardwareReady"),
      storedMatchesActive: bool(app.storedMatchesActive, "application.storedMatchesActive"),
      configurationRevision: uint(app.configurationRevision, "application.configurationRevision"),
      profileRevision: uint(app.profileRevision, "application.profileRevision"),
      storedGeneration: uint(app.storedGeneration, "application.storedGeneration"),
      storedProfileRevision: uint(app.storedProfileRevision, "application.storedProfileRevision"),
    },
    game: {
      phase: paused ? "paused" : running ? "running" : "stopped",
      stateId,
      roleId,
      stateRevision: uint(game.stateRevision, "game.stateRevision"),
      entryPending: bool(game.entryPending, "game.entryPending"),
    },
    control: {
      lease: uint(status.control.lease, "control.lease"),
      nextRequest: uint(status.control.nextRequest, "control.nextRequest"),
      writesEnabled: false,
    },
    devices,
  };
}

export class FirmwareV2StatusStore {
  constructor() {
    this.current = null;
  }

  ingest(status) {
    const next = mapFirmwareV2Status(status);
    if (this.current?.boot === next.boot && next.sample <= this.current.sample) {
      return { accepted: false, reason: "stale-sample", status: this.read() };
    }
    const bootChanged = this.current !== null && this.current.boot !== next.boot;
    this.current = next;
    return { accepted: true, bootChanged, status: this.read() };
  }

  read() {
    return this.current ? structuredClone(this.current) : null;
  }
}
