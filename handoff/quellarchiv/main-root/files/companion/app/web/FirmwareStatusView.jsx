import React from "react";
import { Database, ShieldAlert } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "./components/ui/alert";
import { Badge } from "./components/ui/badge";
import { Button } from "./components/ui/button";
import { mapFirmwareV2Status } from "../src/firmwareV2Status.mjs";
import { classifyFirmwareV2Reply } from "../src/firmwareV2Reply.mjs";
import readyFixture from "../../contract/firmware-v2/fixtures/status-ready.json";
import runningFixture from "../../contract/firmware-v2/fixtures/status-running.json";
import startFixture from "../../contract/firmware-v2/fixtures/command-start.json";
import staleCommandFixture from "../../contract/firmware-v2/fixtures/command-stale-state.json";
import executedReply from "../../contract/firmware-v2/fixtures/reply-executed.json";
import duplicateReply from "../../contract/firmware-v2/fixtures/reply-duplicate.json";
import conflictReply from "../../contract/firmware-v2/fixtures/reply-conflict.json";
import staleReply from "../../contract/firmware-v2/fixtures/reply-stale-state.json";

const fixtures = { ready: readyFixture, running: runningFixture };
const gameLabels = { stopped: "Gestoppt", running: "Läuft", paused: "Pausiert" };
const replyExamples = {
  executed: { label: "Engine", reply: executedReply, command: startFixture },
  duplicate: { label: "Duplikat", reply: duplicateReply, command: startFixture },
  conflict: { label: "Nummernkonflikt", reply: conflictReply, command: null },
  stale: { label: "Alter Zustand", reply: staleReply, command: staleCommandFixture },
};

export function FirmwareStatusView() {
  const [selected, setSelected] = React.useState("ready");
  const [selectedReply, setSelectedReply] = React.useState("executed");
  const status = mapFirmwareV2Status(fixtures[selected]);
  const replyExample = replyExamples[selectedReply];
  const reply = classifyFirmwareV2Reply(replyExample.reply, replyExample.command);
  return (
    <section className="firmware-view" aria-label="Firmware-v2-Statusfixture">
      <header className="mode-heading"><div><p className="mode-kicker">Vertragsabgleich</p><h1>Firmware v2</h1></div><Badge variant="secondary"><Database className="h-4 w-4" /> Fixture · lesend</Badge></header>
      <nav className="firmware-fixtures" aria-label="Statusfixture">
        <Button variant={selected === "ready" ? "secondary" : "ghost"} aria-current={selected === "ready" ? "page" : undefined} onClick={() => setSelected("ready")}>Bereit</Button>
        <Button variant={selected === "running" ? "secondary" : "ghost"} aria-current={selected === "running" ? "page" : undefined} onClick={() => setSelected("running")}>Laufend</Button>
      </nav>
      <Alert><ShieldAlert className="h-4 w-4" /><AlertTitle>Kein Gerätezugang</AlertTitle><AlertDescription>Diese Werte stammen aus Firmware-Fixtures. Die öffentliche Lease und die Fähigkeit game.control.core erteilen diesem App-Prototyp keine Schreibberechtigung. Die Firmware wählt Antworten für verifizierte Verbindungen aus; ein App-Transport existiert noch nicht.</AlertDescription></Alert>
      <div className="firmware-facts">
        <section><h2>Statusquelle</h2><dl className="technical-list">
          <div><dt>Bootkennung</dt><dd>{status.boot}</dd></div>
          <div><dt>Sample</dt><dd>{status.sample}</dd></div>
          <div><dt>Geräte-Millis</dt><dd>{status.sampledMilliseconds}</dd></div>
          <div><dt>Geräteidentität</dt><dd>Nicht im Status</dd></div>
        </dl></section>
        <section><h2>Konfiguration</h2><dl className="technical-list">
          <div><dt>Phase</dt><dd>{status.application.phase}</dd></div>
          <div><dt>Konfiguration</dt><dd>Revision {status.application.configurationRevision}</dd></div>
          <div><dt>Profil</dt><dd>Revision {status.application.profileRevision}; Name nicht im Status</dd></div>
          <div><dt>Speicher</dt><dd>{status.application.storedMatchesActive ? "Entspricht aktivem Stand" : "Weicht vom aktiven Stand ab"}</dd></div>
          <div><dt>Hardwarefreigabe</dt><dd>{status.application.hardwareReady ? "Gemeldet" : "Nicht gemeldet"}</dd></div>
        </dl></section>
        <section><h2>Spiel</h2><dl className="technical-list">
          <div><dt>Phase</dt><dd>{gameLabels[status.game.phase]}</dd></div>
          <div><dt>Zustands-ID</dt><dd>{status.game.stateId ?? "Keine"}</dd></div>
          <div><dt>Zustandsrevision</dt><dd>{status.game.stateRevision}</dd></div>
          <div><dt>Eintritt ausstehend</dt><dd>{status.game.entryPending ? "Ja" : "Nein"}</dd></div>
          <div><dt>Rollen-ID</dt><dd>{status.game.roleId ?? "Keine"}</dd></div>
        </dl></section>
        <section><h2>Steuerkontext</h2><dl className="technical-list">
          <div><dt>Lease</dt><dd>{status.control.lease || "Keine"}</dd></div>
          <div><dt>Nächste Nummer</dt><dd>{status.control.nextRequest || "Keine"}</dd></div>
          <div><dt>App-Schreibzugang</dt><dd>Unbestätigt</dd></div>
        </dl></section>
      </div>
      <section className="firmware-devices"><div className="section-heading"><h2>Komponenten</h2><span>{status.devices.length} im Status</span></div>
        <div className="firmware-table-scroll"><table><thead><tr><th scope="col">ID</th><th scope="col">Aktiviert</th><th scope="col">Verfügbar</th><th scope="col">Letzter Ausgang</th></tr></thead><tbody>
          {status.devices.map((device) => <tr key={device.id}><th scope="row">{device.id}</th><td>{device.enabled ? "Ja" : "Nein"}</td><td>{device.available ? "Ja" : "Nein"}</td><td>{device.lastOutcome}</td></tr>)}
        </tbody></table></div>
        <p className="mode-kicker">„Verfügbar“ ist kein physischer Heartbeat-Nachweis.</p>
      </section>
      <section className="firmware-replies" aria-label="Firmware-v2-Antwortfixtures">
        <div className="section-heading"><h2>Antwortbeispiele</h2><span>Nur Fixtures</span></div>
        <nav className="firmware-fixtures" aria-label="Antwortfixture">
          {Object.entries(replyExamples).map(([key, example]) => <Button key={key} variant={selectedReply === key ? "secondary" : "ghost"} aria-current={selectedReply === key ? "page" : undefined} onClick={() => setSelectedReply(key)}>{example.label}</Button>)}
        </nav>
        <Alert><AlertTitle>{reply.title}</AlertTitle><AlertDescription>{reply.detail}</AlertDescription></Alert>
        <h3>Erwarteter Request aus dem Fixture</h3>
        {replyExample.command ? <dl className="technical-list">
          <div><dt>Befehl / Ziel</dt><dd>{replyExample.command.kind} / {replyExample.command.target}</dd></div>
          <div><dt>Boot / Lease / Anfrage</dt><dd>{replyExample.command.boot} / {replyExample.command.lease} / {replyExample.command.request}</dd></div>
          <div><dt>Erwartete Revisionen</dt><dd>Konfiguration {replyExample.command.expectedConfigurationRevision}, Zustand {replyExample.command.expectedStateRevision}</dd></div>
        </dl> : <p>Der ursprüngliche Request ist in diesem Fixture-Snapshot nicht enthalten.</p>}
        <h3>Antwort aus dem Fixture</h3>
        <dl className="technical-list">
          <div><dt>Ergebnis</dt><dd>{reply.outcome}</dd></div>
          <div><dt>Boot / Lease / Anfrage</dt><dd>{reply.correlation.boot} / {reply.correlation.lease} / {reply.correlation.request}</dd></div>
          <div><dt>Duplikat / verbraucht</dt><dd>{reply.duplicate ? "Ja" : "Nein"} / {reply.consumed ? "Ja" : "Nein"}</dd></div>
          <div><dt>Nächste Nummer</dt><dd>{reply.nextRequest}</dd></div>
          <div><dt>Revisionen im Ergebnis</dt><dd>Konfiguration {reply.configurationRevision}, Zustand {reply.stateRevision}</dd></div>
        </dl>
        {selectedReply === "conflict" && <p className="mode-kicker">Der geänderte Request zum Nummernkonflikt ist ebenfalls nicht Teil des Fixture-Snapshots.</p>}
      </section>
    </section>
  );
}
