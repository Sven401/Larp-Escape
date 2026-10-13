import React from "react";
import { createRoot } from "react-dom/client";
import { Activity, Cable, Database, Pause, Play, RotateCcw, SkipForward, StepForward, RefreshCw, Unplug, FastForward } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "./components/ui/alert";
import { Badge } from "./components/ui/badge";
import { Button } from "./components/ui/button";
import { MockCompanionSession } from "./mockSession.mjs";
import { EditorView } from "./EditorView.jsx";
import { ReadingPreviewView } from "./ReadingPreviewView.jsx";
import { FirmwareStatusView } from "./FirmwareStatusView.jsx";
import initialStatus from "../../contract/v0/device-status.ready.json";
import runningStatus from "../../contract/v0/device-status.running.json";
import pausedStatus from "../../contract/v0/device-status.paused.json";
import readonlyStatus from "../../contract/v0/device-status.readonly.json";
import "./styles.css";

const mockFixtures = { ready: initialStatus, running: runningStatus, paused: pausedStatus, readonly: readonlyStatus };

const actionMeta = {
  start: { label: "Spiel starten", icon: Play },
  pause: { label: "Pausieren", icon: Pause },
  resume: { label: "Fortsetzen", icon: StepForward },
  reset: { label: "Zurücksetzen", icon: RotateCcw },
  "select-state": { label: "Zum Finale (Mock)", icon: SkipForward },
};

const phaseLabels = { ready: "Bereit", running: "Läuft", paused: "Pausiert", fault: "Fehler", booting: "Startet", setup: "Einrichtung" };

function TechnicalView({ status, connection, fixtureKey, onFixtureChange }) {
  return (
    <section className="technical-view" aria-label="Technische Ansicht">
      <header className="mode-heading"><div><p className="mode-kicker">Entwicklung</p><h1>Technik</h1></div><Badge variant="secondary">{connection === "mock-connected" ? "Mock verbunden" : "Mock getrennt"}</Badge></header>
      <div className="technical-grid">
        <section><h2>Gerät</h2><dl className="technical-list">
          <div><dt>Geräte-ID</dt><dd>{status.deviceId}</dd></div>
          <div><dt>Boot-ID</dt><dd>{status.bootId}</dd></div>
          <div><dt>Transport</dt><dd>{status.capabilities.transports.join(", ")}</dd></div>
        </dl></section>
        <section><h2>Profil und Laufzeit</h2><dl className="technical-list">
          <div><dt>Aktives Profil</dt><dd>{status.profile.activeProfileId}</dd></div>
          <div><dt>Dokumentrevision</dt><dd>{status.profile.documentRevision}</dd></div>
          <div><dt>Speichergeneration</dt><dd>{status.profile.storageGeneration}</dd></div>
          <div><dt>Gespeicherter Stand</dt><dd>{status.profile.storedMatchesActive ? "Entspricht aktivem Profil" : "Weicht vom aktiven Profil ab"}</dd></div>
          <div><dt>Laufzeitgeneration</dt><dd>{status.runtime.runtimeGeneration}</dd></div>
          <div><dt>Konfigurationsrevision</dt><dd>{status.runtime.configRevision}</dd></div>
        </dl></section>
      </div>
      <div className="technical-fixture">
        <label className="editor-field">Mock-Statusfixture
          <select value={fixtureKey} onChange={(event) => onFixtureChange(event.target.value)}>
            <option value="ready">Bereit</option>
            <option value="running">Laufend</option>
            <option value="paused">Pausiert</option>
            <option value="readonly">Nur Lesen</option>
          </select>
        </label>
        <Button variant="outline" size="sm" onClick={() => onFixtureChange(fixtureKey)}><RotateCcw className="h-4 w-4" /> Fixture neu laden</Button>
      </div>
      <Alert><AlertTitle>Geräteeinrichtung offen</AlertTitle><AlertDescription>Für Pins, Busse, LEDs und Audio liegt in diesem App-Schnitt noch kein bestätigter Schreibvertrag vor.</AlertDescription></Alert>
    </section>
  );
}

export function App({ sessionFactory = () => new MockCompanionSession(initialStatus), scenario } = {}) {
  const [session] = React.useState(sessionFactory);
  const [, refresh] = React.useReducer((value) => value + 1, 0);
  const [mode, setMode] = React.useState("game-master");
  const [developerView, setDeveloperView] = React.useState("scenario");
  const [fixtureKey, setFixtureKey] = React.useState("ready");
  const [previewSource, setPreviewSource] = React.useState({ draft: null, saved: false });
  const onDraftChange = React.useCallback((next) => setPreviewSource(next), []);
  const { status, connection, requestNumber, feedback, view, canAdvanceMock } = session.read();
  const demoScenario = scenario ?? new URLSearchParams(window.location.search).get("scenario");
  const actions = view.actions.map((action) => ({ ...action, ...actionMeta[action.commandType] }));
  const availableActions = actions.filter((action) => action.enabled);

  function submit(commandType) {
    if (commandType === "reset" && !window.confirm("Das Mock-Spiel zurücksetzen?")) return;
    const command = commandType === "select-state" ? { type: commandType, stateId: "finale" } : { type: commandType };
    session.submit(command, { simulateStaleRuntime: demoScenario === "conflict" && requestNumber === 1 });
    refresh();
  }

  function toggleMockConnection() {
    session.setConnected(connection !== "mock-connected");
    refresh();
  }

  function loadMockFixture(key) {
    session.loadFixture(mockFixtures[key]);
    setFixtureKey(key);
    refresh();
  }

  return (
    <main className="app-shell min-h-screen bg-background text-foreground" data-mode={mode}>
      <div className="app-frame">
        <nav className="mode-nav" aria-label="Modus">
          <Button variant={mode === "game-master" ? "default" : "ghost"} aria-current={mode === "game-master" ? "page" : undefined} onClick={() => setMode("game-master")}>Spielleitung</Button>
          <Button variant={mode === "developer" ? "default" : "ghost"} aria-current={mode === "developer" ? "page" : undefined} onClick={() => setMode("developer")}>Entwickler</Button>
        </nav>

        <div hidden={mode !== "developer"} className="developer-shell">
          <nav className="developer-nav" aria-label="Entwicklung">
            <Button variant={developerView === "scenario" ? "secondary" : "ghost"} aria-current={developerView === "scenario" ? "page" : undefined} onClick={() => setDeveloperView("scenario")}>Szenario</Button>
            <Button variant={developerView === "reading" ? "secondary" : "ghost"} aria-current={developerView === "reading" ? "page" : undefined} onClick={() => setDeveloperView("reading")}>Leseprobe</Button>
            <Button variant={developerView === "technical" ? "secondary" : "ghost"} aria-current={developerView === "technical" ? "page" : undefined} onClick={() => setDeveloperView("technical")}>Technik</Button>
            <Button variant={developerView === "firmware" ? "secondary" : "ghost"} aria-current={developerView === "firmware" ? "page" : undefined} onClick={() => setDeveloperView("firmware")}>Firmware v2</Button>
          </nav>
          <div hidden={developerView !== "scenario"}><EditorView onDraftChange={onDraftChange} /></div>
          <div hidden={developerView !== "reading"}><ReadingPreviewView {...previewSource} onEdit={() => setDeveloperView("scenario")} /></div>
          <div hidden={developerView !== "technical"}><TechnicalView status={status} connection={view.connection} fixtureKey={fixtureKey} onFixtureChange={loadMockFixture} /></div>
          <div hidden={developerView !== "firmware"}><FirmwareStatusView /></div>
        </div>

        <div hidden={mode !== "game-master"} className="game-master-view">
          <header className="mode-heading">
            <div><p className="mode-kicker">Regiekonsole · v0-Mock</p><h1>Spielleitung</h1></div>
            <div className="connection-controls"><Badge variant="secondary" className="connection-badge">{connection === "mock-connected" ? <Cable className="h-4 w-4" /> : <Unplug className="h-4 w-4" />}{connection === "mock-connected" ? "Mock verbunden" : "Mock getrennt"}</Badge><Button variant="ghost" size="sm" onClick={toggleMockConnection}>{connection === "mock-connected" ? <Unplug className="h-4 w-4" /> : <Cable className="h-4 w-4" />}{connection === "mock-connected" ? "Trennen" : "Verbinden"}</Button></div>
          </header>
          <section className="live-state" aria-label="Aktueller Spielzustand">
            <div><p className="mode-kicker">{connection === "mock-connected" ? "Aktueller Zustand" : "Zuletzt gelesener Zustand"}</p><h2>{status.runtime.stateName || phaseLabels[status.runtime.phase] || status.runtime.phase}</h2><p className="live-phase"><Activity className="h-4 w-4" /> {phaseLabels[status.runtime.phase] || status.runtime.phase}</p></div>
            <dl className="status-facts">
              <div><dt>Gerät</dt><dd>{view.deviceId}</dd></div>
              <div><dt>Boot-ID</dt><dd>{view.technical.bootId}</dd></div>
              <div><dt>Aktives Profil</dt><dd>{view.profileName}</dd></div>
              <div><dt>Profilstand</dt><dd>Revision {view.profileRevision}</dd></div>
              <div><dt>Speicherstand</dt><dd>{view.storedMatchesActive ? "Aktiv = gespeichert" : "Aktiver Stand weicht ab"}</dd></div>
              <div><dt>Aktuelle Revisionen</dt><dd>Laufzeit {view.technical.runtimeGeneration} · Konfiguration {view.technical.configRevision}</dd></div>
            </dl>
          </section>
          {status.health.lastError && <Alert variant="destructive"><AlertTitle>Gerätefehler</AlertTitle><AlertDescription>{status.health.lastError}</AlertDescription></Alert>}
          <section className="action-section" aria-label="Spielleiteraktionen">
            <div className="section-heading"><h2>Steuerung</h2><span>Verfügbare Aktionen</span></div>
            <div className="action-buttons">
              {actions.map((action) => { const Icon = action.icon; return <Button key={action.commandType} disabled={!action.enabled} variant={action.commandType === "reset" ? "destructive" : "default"} onClick={() => submit(action.commandType)}><Icon className="h-4 w-4" /> {action.label}</Button>; })}
              {availableActions.length === 0 && <p>{connection !== "mock-connected" ? "Mock getrennt. Zum Steuern erneut verbinden." : canAdvanceMock ? "Mock-Annahme offen. Zuerst den Mockstatus fortschalten." : "Im aktuellen Zustand sind keine Spielleiteraktionen verfügbar."}</p>}
            </div>
          </section>
          <Alert variant={feedback?.kind === "error" ? "destructive" : "default"}><Database className="h-4 w-4" /><AlertTitle>{feedback?.title || "Bereit"}</AlertTitle><AlertDescription>
            {feedback?.detail || "Noch keine Anfrage gesendet."}
            {feedback?.request && <div className="request-details">Anfrage {feedback.request.requestId} · Boot {feedback.request.bootId} · Laufzeit {feedback.request.expectedRuntimeGeneration} · Konfiguration {feedback.request.expectedConfigRevision}</div>}
            {feedback?.result && <div className="request-details">Ergebnis: {feedback.result.status}{feedback.result.reason ? ` (${feedback.result.reason})` : ""}</div>}
            {canAdvanceMock && <Button className="mt-3" variant="secondary" size="sm" onClick={() => { session.advanceMock(); refresh(); }}><FastForward className="h-4 w-4" /> Mockstatus fortschalten</Button>}
            {feedback?.kind === "conflict" && <Button className="mt-3" variant="secondary" size="sm" onClick={() => { session.reloadStatus(); refresh(); }}><RefreshCw className="h-4 w-4" /> Status neu laden</Button>}
            {feedback?.canRepeat && connection === "mock-connected" && <Button className="mt-3" variant="outline" size="sm" onClick={() => { session.repeatLastRequest(); refresh(); }}><RefreshCw className="h-4 w-4" /> Anfrage wiederholen</Button>}
          </AlertDescription></Alert>
        </div>
      </div>
    </main>
  );
}

if (typeof document !== "undefined") createRoot(document.getElementById("app")).render(<App />);
