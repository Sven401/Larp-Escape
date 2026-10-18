import type { CSSProperties } from "react";
import { useEffect, useReducer, useState } from "react";
import { createPortal } from "react-dom";
import { ArrowLeftIcon, ChevronRightIcon, ExclamationTriangleIcon, CheckCircledIcon, PlayIcon, StopIcon, ReloadIcon } from "@radix-ui/react-icons";
import { MobileScroll, useMobileDevice } from "./mobile";
import { initial, reducer, canCommand, effects, type Effect, type Phase } from "./pilot";
import "@fontsource/roboto/400.css";
import "@fontsource/roboto/500.css";
import "@fontsource/roboto/700.css";

export default function Prototype() {
  const [state, send] = useReducer(reducer, initial);
  const [scenario, setScenario] = useState("normal");
  const [size, setSize] = useState("normal");
  const [toolsOpen, setToolsOpen] = useState(false);
  const { setDeviceId } = useMobileDevice();
  useEffect(() => { setDeviceId("pixel-10"); }, [setDeviceId]);
  useEffect(() => {
    let action: Parameters<typeof send>[0] | undefined;
    if (state.phase === "scanning") action = {type:"FOUND", empty:scenario === "empty"};
    if (state.phase === "verifying") action = {type:"VERIFIED", denied:scenario === "access"};
    if (state.phase === "pending") action = scenario === "loss" ? {type:"LOST"} : {type:"ACK"};
    if (!action) return;
    const timer = window.setTimeout(() => send(action!), state.phase === "pending" ? 1700 : 1100);
    return () => window.clearTimeout(timer);
  }, [state.phase, scenario]);
  const connected = state.phase === "ready" || state.phase === "pending" || state.phase === "stale";
  const statuses: Partial<Record<Phase, [string, string]>> = {
    idle:["Station verbinden", "Suche nach dem vorbereiteten Gerät dieser Station."],
    permission:["Gerätezugriff erlauben", "Zum Suchen und Verbinden wird Bluetoothzugriff benötigt."],
    denied:["Gerätezugriff fehlt", "Du kannst die Erlaubnis erneut anfragen."],
    bluetooth:["Bluetooth ist ausgeschaltet", "Schalte Bluetooth ein und starte die Suche erneut."],
    scanning:["Geräte werden gesucht …", "Die Station bleibt ausgewählt. Du kannst die Suche abbrechen."],
    found:["Gerät in der Nähe gefunden", "Der Zugang und die Zuordnung zur Station sind noch ungeprüft."],
    empty:["Kein Gerät gefunden", "Gehe näher an die Station und suche erneut."],
    verifying:["Zugang wird geprüft …", "Erst nach der Prüfung und einem frischen Gerätestand ist die Bedienung möglich."],
    access:["Zugang nicht bestätigt", "Prüfe den vorbereiteten Stationszugang. Es wurden keine Befehle gesendet."],
    stale:["Status nicht aktuell", "Die Verbindung besteht, aber der Gerätestand ist nicht mehr bestätigt."],
    background:["Verbindung beendet", "Nach der Rückkehr werden Zugang und Gerätestand erneut geprüft."],
  };
  const status = statuses[state.phase];
  const isLost = state.phase === "lost";
  const ready = state.phase === "ready";
  function scan() {
    if (scenario === "bluetooth") { send({type:"BLUETOOTH"}); return; }
    send({type:"SCAN"});
  }
  return <>
    {createPortal(<aside className={`pilot-tools ${toolsOpen ? "is-open" : ""}`} aria-label="Prototyp-Steuerung">
      <button className="tool-toggle" aria-expanded={toolsOpen} onClick={()=>setToolsOpen(!toolsOpen)}>{toolsOpen ? "Testfälle schließen" : "Testfälle"}</button><div className="tool-body">
      <p className="tool-eyebrow">INTERAKTIVER ENTWURF</p>
      <h2>Den Ablauf ausprobieren</h2>
      <p>Beispielveranstaltung · simulierte Geräte.<br/>Es werden keine echten Befehle gesendet.</p>
      <label>Testfall<select value={scenario} onChange={e=>{setScenario(e.target.value);e.target.blur();}}>
        <option value="normal">Normaler Ablauf</option><option value="loss">Abbruch vor Befehlsantwort</option>
        <option value="empty">Keine Suchergebnisse</option><option value="access">Falscher Zugang</option><option value="bluetooth">Bluetooth ausgeschaltet</option>
      </select></label>
      <div className="tool-actions">
        <button onClick={()=>send({type:"LOST"})} disabled={!connected}>Verbindung unterbrechen</button>
        <button onClick={()=>send({type:"STALE"})} disabled={state.phase!=="ready"}>Status veralten lassen</button>
        <button onClick={()=>send({type:"BACKGROUND"})} disabled={state.screen!=="station"}>Hintergrund & Rückkehr</button>
        <button onClick={()=>send({type:"RESET"})}>Pilot zurücksetzen</button>
      </div>
      <label>Schriftgröße<select value={size} onChange={e=>{setSize(e.target.value);e.target.blur();}}><option value="normal">100 %</option><option value="large">200 %</option></select></label>
      <p className="tool-note">Wiederaufnahme: erst suchen, dann Zugang und frischen Stand prüfen. Kein Auftrag wird wiederholt.</p>
      <details><summary>Ablauf und Prüfpunkte</summary><ol><li>Altes Tor auswählen</li><li>Geräte suchen und Zugriff erlauben</li><li>Kandidaten prüfen und verbinden</li><li>Licht auswählen, Start und Antwort abwarten</li><li>Verbindung unterbrechen</li><li>Erneut suchen und bewusst verbinden</li></ol></details></div>
    </aside>, document.body)}
    <div className={`pilot-app ${size === "large" ? "large-type" : ""}`} style={{"--pilot-scale":size==="large"?2:1} as CSSProperties}>
      <header className="pilot-header">
        {state.screen === "station" ? <button className="back" onClick={()=>send({type:"BACK"})}><ArrowLeftIcon/>Stationen</button> : <div className="back context-label">Spielleitung</div>}
        <p className="event-name">Nacht der Siegel</p>
        <h1>{state.screen === "station" ? "Altes Tor" : "Stationen"}</h1>
      </header>
      <MobileScroll className="pilot-scroll"><main className="pilot-content" data-testid="pilot-content">
        {state.screen === "stations" ? <>
          <p className="intro">Wähle die Station, die du vor Ort bedienen möchtest.</p>
          <button className="station-row" onClick={()=>send({type:"OPEN"})}><span><strong>Altes Tor</strong><small>Licht und Stationsbedienung</small></span><ChevronRightIcon/></button>
          <p className="quiet">Die Verbindung und der Zugang werden erst beim Verbinden geprüft.</p>
        </> : <>
          <section className={`connection ${isLost ? "warning" : ""}`} aria-label="Verbindungsstatus" aria-live="polite" aria-atomic="true" data-testid="connection-status">
            {isLost ? <>
              <h2><ExclamationTriangleIcon/>Verbindung unterbrochen</h2>
              <p>Aktueller Zustand unbekannt.<br/>{state.uncertain ? `„${state.lastCommand}“ wurde nicht bestätigt.` : "Für die Bedienung erneut verbinden."}</p>
              {state.uncertain && <p className="quiet compact">Keine automatische Wiederholung.</p>}
              <button className="primary" onClick={scan}>Erneut suchen</button>
            </> : ready ? <>
              <h2 className="success"><CheckCircledIcon/>Zugang bestätigt</h2><p>{state.reconnected ? "Neu verbunden · Gerätestand frisch gelesen." : "Zugang für Altes Tor geprüft · bereit zur Bedienung."}</p>
              <p className="device-report">Box meldet: {state.active || "LEDs aus"}</p>
              <button className="text-button" onClick={()=>send({type:"LOST"})}>Verbindung trennen</button>
            </> : state.phase === "pending" ? <>
              <h2><ReloadIcon className="spin"/>Warte auf Rückmeldung …</h2><p>{state.lastCommand} angefragt.</p><p className="quiet compact">Ein gesendeter Auftrag ist noch keine Bestätigung.</p>
            </> : <>
              <h2>{["scanning","verifying"].includes(state.phase) && <ReloadIcon className="spin"/>}{status?.[0]}</h2><p>{status?.[1]}</p>
              {state.phase === "idle" && <button className="primary" onClick={scan}>Geräte suchen</button>}
              {["permission","denied"].includes(state.phase) && <div className="action-stack"><button className="primary" onClick={()=>send({type:"ALLOW"})}>Zugriff erlauben</button><button className="text-button" onClick={()=>send({type:"DENY"})}>Jetzt nicht</button></div>}
              {state.phase === "bluetooth" && <button className="primary" onClick={()=>{setScenario("normal");send({type:"ENABLE_BT"});}}>Bluetooth einschalten</button>}
              {state.phase === "found" && <div className="candidate"><p><strong>Kandidat in der Nähe</strong><br/><span className="quiet">Identität noch ungeprüft</span></p><button className="primary" onClick={()=>send({type:"CONNECT"})}>Prüfen und verbinden</button></div>}
              {["empty","background","stale"].includes(state.phase) && <button className="primary" onClick={scan}>Erneut suchen</button>}
              {state.phase === "access" && <button className="primary" onClick={()=>{setScenario("normal");send({type:"ACCESS_FIXED"});}}>Vorbereiteten Zugang verwenden</button>}
              {["scanning","verifying"].includes(state.phase) && <button className="text-button" onClick={()=>send({type:"CANCEL"})}>Abbrechen</button>}
            </>}
          </section>
          <section className="light-controls" aria-label="Lichtsteuerung">
            <h2>Licht</h2>
            <p className="intro">Wähle einen Lichteffekt und sende ihn an die Station.</p>
            <fieldset disabled={state.phase==="pending"}><legend>Auswahl</legend>{effects.map(effect=><label className="effect" key={effect}><input type="radio" name="effect" value={effect} checked={state.selected===effect} onChange={()=>send({type:"SELECT",effect:effect as Effect})}/><span>{effect}</span></label>)}</fieldset>
            <div className="command-buttons"><button className="primary" disabled={!canCommand(state)} onClick={e=>{e.currentTarget.blur();send({type:"START"});}}><PlayIcon/>Start</button><button className="secondary" disabled={!canCommand(state)} onClick={e=>{e.currentTarget.blur();send({type:"STOP"});}}><StopIcon/>Stop</button></div>
            <p className="quiet control-hint">{!canCommand(state) ? "Bedienung nach erneuter Verbindung." : "Die Auswahl sendet nichts. Start löst den Effekt aus."}</p>
            {ready && <p className="quiet">Die Geräteantwort bestätigt die Ausführung. Prüfe das sichtbare Licht vor Ort.</p>}
          </section>
        </>}
      </main></MobileScroll>
    </div>
  </>;
}
