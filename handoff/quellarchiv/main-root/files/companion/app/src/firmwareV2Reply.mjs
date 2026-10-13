const MAX_U32 = 0xffffffff;
const OUTCOMES = new Set([
  "executed", "execution_rejected", "invalid_request", "wrong_boot",
  "unauthorized", "stale_configuration", "stale_state", "out_of_order",
  "receipt_expired", "request_conflict", "not_ready",
]);

function uint(value, name) {
  if (!Number.isInteger(value) || value < 0 || value > MAX_U32) throw new Error(`${name} ist keine gültige v2-Zahl.`);
  return value;
}

function boot(value) {
  if (typeof value !== "string" || !/^(?!0000000000000000$)[0-9A-F]{16}$/.test(value)) {
    throw new Error("Ungültige v2-Bootkennung.");
  }
  return value;
}

export function classifyFirmwareV2Reply(reply, expectedCommand = null) {
  if (!reply || reply.schemaVersion !== 2 || !OUTCOMES.has(reply.outcome) ||
      typeof reply.duplicate !== "boolean" || typeof reply.consumed !== "boolean" ||
      typeof reply.executionOutcome !== "string" || reply.executionOutcome.length > 32) {
    throw new Error("Ungültige v2-Antwort.");
  }
  const correlation = {
    boot: boot(reply.boot),
    lease: uint(reply.lease, "lease"),
    request: uint(reply.request, "request"),
  };
  const nextRequest = uint(reply.nextRequest, "nextRequest");
  const configurationRevision = uint(reply.configurationRevision, "configurationRevision");
  const stateRevision = uint(reply.stateRevision, "stateRevision");
  if (expectedCommand) {
    if (expectedCommand.schemaVersion !== 2 || boot(expectedCommand.boot) !== correlation.boot ||
        uint(expectedCommand.lease, "expected.lease") !== correlation.lease ||
        uint(expectedCommand.request, "expected.request") !== correlation.request) {
      return {
        kind: reply.outcome === "wrong_boot" ? "boot-changed" : "foreign-result",
        title: reply.outcome === "wrong_boot" ? "Neuer Gerätestart" : "Antwort gehört zu anderer Anfrage",
        detail: "Status und Zuordnung neu prüfen; diese Antwort nicht auf die offene Anfrage anwenden.",
        correlation,
        physicalEffectConfirmed: false,
      };
    }
  }

  const messages = {
    executed: ["engine-executed", "Engineoperation ausgeführt", "Die Firmware meldet eine lokale Engineoperation; Gerätewirkungen können noch ausstehen."],
    execution_rejected: ["rejected", "Engine hat abgelehnt", "Aktuellen Status lesen und den nächsten Schritt neu prüfen."],
    invalid_request: ["rejected", "Anfrage ungültig", "Anfrageformat und Ziel gegen den v2-Vertrag prüfen."],
    wrong_boot: ["boot-changed", "Neuer Gerätestart", "Boot und authentisierte Lease neu beziehen."],
    unauthorized: ["unauthorized", "Keine Spielleiterberechtigung", "Zugang nicht aus der sichtbaren Lease ableiten."],
    stale_configuration: ["conflict", "Konfiguration geändert", "Aktuellen Status lesen und mit neuer Nummer erneut entscheiden."],
    stale_state: ["conflict", "Spielzustand geändert", "Aktuellen Status lesen und mit neuer Nummer erneut entscheiden."],
    out_of_order: ["sequence-error", "Anfragereihenfolge unpassend", "Offene Anfragen und aktuelle Lease prüfen."],
    receipt_expired: ["receipt-unknown", "Alte Antwort nicht mehr verfügbar", "Der ursprüngliche Befehl kann ausgeführt worden sein; Status lesen."],
    request_conflict: ["request-conflict", "Anfragenummer anders verwendet", "Der ursprüngliche Befehl kann ausgeführt worden sein; dieselbe Nummer nicht umdeuten."],
    not_ready: ["not-ready", "Gerät nicht bereit", "Aktuellen Status lesen und Freigaben prüfen."],
  };
  const [baseKind, baseTitle, baseDetail] = messages[reply.outcome];
  const duplicate = reply.duplicate;
  return {
    kind: duplicate ? "duplicate" : baseKind,
    title: duplicate ? "Historische Antwort erneut" : baseTitle,
    detail: duplicate ? `${baseDetail} Die Antwort enthält historische Revisionen; vor weiterer Bedienung Status neu lesen.` : baseDetail,
    outcome: reply.outcome,
    duplicate,
    consumed: reply.consumed,
    correlation,
    nextRequest,
    configurationRevision,
    stateRevision,
    executionOutcome: reply.executionOutcome,
    requiresStatusReload: true,
    physicalEffectConfirmed: false,
    originalMayHaveRun: ["receipt_expired", "request_conflict"].includes(reply.outcome),
  };
}
