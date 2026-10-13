import { createGameMasterRequest } from "./domain.mjs";
import { BrowserMockDevice } from "./mockDevice.mjs";
import { resultToViewState, statusToViewState } from "./presentation.mjs";

export class MockCompanionSession {
  constructor(initialStatus, options = {}) {
    this.device = options.device || new BrowserMockDevice(initialStatus);
    this.requestPrefix = options.requestPrefix || "vite-demo";
    this.connection = "mock-connected";
    this.status = this.device.readStatus();
    this.feedback = null;
    this.pending = null;
    this.requestNumber = 1;
  }

  read() {
    const status = structuredClone(this.status);
    const feedback = this.feedback ? structuredClone(this.feedback) : null;
    return {
      status,
      feedback,
      connection: this.connection,
      requestNumber: this.requestNumber,
      canAdvanceMock: this.connection === "mock-connected" && this.pending !== null,
      view: statusToViewState(status, {
        connection: this.connection,
        lastRequest: feedback?.request || null,
        operationState: this.pending ? "awaiting-mock-progress" : "idle",
      }),
    };
  }

  setConnected(connected) {
    this.connection = connected ? "mock-connected" : "mock-disconnected";
    if (connected) this.status = this.device.readStatus();
    return this.read();
  }

  loadFixture(initialStatus) {
    const hadPending = this.pending !== null;
    this.device = new BrowserMockDevice(initialStatus);
    this.status = this.device.readStatus();
    this.pending = null;
    this.feedback = {
      kind: "info",
      title: "Mock-Fixture geladen",
      detail: hadPending
        ? "Die lokale Mock-Sitzung wurde ersetzt; die offene Testanfrage wurde verworfen."
        : "Die lokale Mock-Sitzung wurde ersetzt. Kein Gerätestatus wurde empfangen.",
    };
    return this.read();
  }

  reloadStatus() {
    if (this.connection !== "mock-connected") return this.fail("Mock getrennt", "Zum Neuladen zuerst wieder verbinden.");
    this.status = this.device.readStatus();
    this.feedback = null;
    return this.read();
  }

  submit(command, options = {}) {
    if (this.connection !== "mock-connected") return this.fail("Mock getrennt", "Zum Steuern zuerst wieder verbinden.");
    if (this.pending) return this.fail("Anfrage noch offen", "Zuerst den Mockstatus fortschalten.");
    let request;
    try {
      request = createGameMasterRequest(this.status, `${this.requestPrefix}-${String(this.requestNumber).padStart(4, "0")}`, command);
    } catch (error) {
      return this.fail("Anfrage nicht gesendet", error.message);
    }
    if (options.simulateStaleRuntime) request.expectedRuntimeGeneration += 1;
    this.requestNumber += 1;
    return this.apply(request);
  }

  repeatLastRequest() {
    if (this.connection !== "mock-connected") return this.fail("Mock getrennt", "Zum Wiederholen zuerst wieder verbinden.");
    if (!this.feedback?.request) return this.fail("Keine Anfrage", "Es gibt noch keine Anfrage zum Wiederholen.");
    return this.apply(this.feedback.request);
  }

  apply(request) {
    try {
      const result = this.device.sendGameMasterCommand(request);
      this.status = this.device.readStatus();
      this.feedback = resultToViewState(request, result);
      if (result.status === "accepted") this.pending = { request: structuredClone(request), result: structuredClone(result) };
    } catch (error) {
      this.feedback = { kind: "error", title: "Mock-Fehler", detail: error.message, request, canRepeat: true };
    }
    return this.read();
  }

  advanceMock() {
    if (this.connection !== "mock-connected") return this.fail("Mock getrennt", "Zum Fortsetzen zuerst wieder verbinden.");
    if (!this.pending) return this.fail("Keine offene Anfrage", "Der Mock hat keinen angenommenen Befehl.");
    try {
      const advanced = this.device.advanceMock();
      if (!advanced || advanced.requestId !== this.pending.request.requestId) {
        throw new Error("Mock-Fortschritt passt nicht zur offenen Anfrage.");
      }
      this.status = this.device.readStatus();
      this.feedback = {
        kind: "mock-progress",
        title: "Mockstatus fortgeschaltet",
        detail: "Der Mock hat den Status geändert. Eine Firmwareausführung ist damit nicht bestätigt.",
        request: this.pending.request,
        result: this.pending.result,
        canRepeat: true,
      };
      this.pending = null;
    } catch (error) {
      this.feedback = { kind: "error", title: "Mock-Fehler", detail: error.message, request: this.pending.request };
    }
    return this.read();
  }

  fail(title, detail) {
    this.feedback = { kind: "error", title, detail };
    return this.read();
  }
}
