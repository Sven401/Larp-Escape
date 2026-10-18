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
  const [helpOpen, setHelpOpen] = useState(false);
  const [size, setSize] = useState("normal");
  const [toolsOpen, setToolsOpen] = useState(false);
  const { setDeviceId } = useMobileDevice();
  useEffect(() => { setDeviceId("pixel-10"); }, [setDeviceId]);
  useEffect(() => {
    let action: Parameters<typeof send>[0] | undefined;
    if (state.phase === "scanning") action = {type:"FOUND", empty:scenario === "empty", matches:scenario === "ambiguous"?2:1};
    if (["lost","stale","background"].includes(state.phase)) action = scenario === "bluetooth" ? {type:"BLUETOOTH"} : {type:"SCAN"};
    if (state.phase === "verifying") action = {type:"VERIFIED", denied:scenario === "access"};
    if (state.phase === "pending") action = scenario.startsWith("loss") ? {type:"LOST", delivered:scenario === "loss"} : {type:"ACK"};
    if (!action) return;
    const timer = window.setTimeout(() => send(action!), state.phase === "pending" ? 1700 : ["lost","stale","background"].includes(state.phase) ? 2400 : 900);
    return () => window.clearTimeout(timer);
  }, [state.phase, scenario]);
  const connected = state.phase === "ready" || state.phase === "pending" || state.phase === "stale";
  const statuses: Partial<Record<Phase, [string, string]>> = {
    idle:["Verbindung pausiert", ""],
    permission:["Gerätezugriff erlauben", "Zum Suchen und Verbinden wird Bluetoothzugriff benötigt."],
    denied:["Gerätezugriff fehlt", "Du kannst die Erlaubnis erneut anfragen."],
    bluetooth:["Bluetooth ist ausgeschaltet", "Schalte Bluetooth ein und starte die Suche erneut."],
    scanning:[state.reconnected ? "Verbindung wird wiederhergestellt …" : "Station wird verbunden …", ""],
    empty:["Station nicht erreichbar", "Gehe näher an die Station und versuche es erneut."],
    verifying:[state.reconnected ? "Verbindung wird wiederhergestellt …" : "Station wird verbunden …", ""],
    access:["Stationszugang fehlt", "Bitte die verantwortliche Person für die Veranstaltung, deinen Stationszugang zu prüfen."],
    assignment:["Stationszuordnung unklar", "Bitte die verantwortliche Person für die Veranstaltung, die Gerätezuordnung dieser Station zu prüfen."],
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
        <option value="normal">Normaler Ablauf</option><option value="loss">Antwort verloren · Auftrag ausgeführt</option><option value="loss-undelivered">Auftrag nicht angekommen</option>
        <option value="empty">Keine Suchergebnisse</option><option value="access">Falscher Zugang</option><option value="bluetooth">Bluetooth ausgeschaltet</option><option value="permission">Gerätezugriff fehlt</option><option value="ambiguous">Zuordnung nicht eindeutig</option>
      </select></label>
      <div className="tool-actions">
        <button onClick={()=>send({type:"LOST"})} disabled={!connected}>Verbindung unterbrechen</button>
        <button onClick={()=>send({type:"STALE"})} disabled={state.phase!=="ready"}>Status veralten lassen</button>
        <button onClick={()=>send({type:"BACKGROUND"})} disabled={state.screen!=="station"}>Hintergrund & Rückkehr</button>
        <button onClick={()=>setScenario("normal")} disabled={!["access","ambiguous"].includes(scenario)}>Einrichtung korrigiert simulieren</button>
        <button onClick={()=>{setHelpOpen(false);send({type:"RESET"});}}>Pilot zurücksetzen</button>
      </div>
      <label>Schriftgröße<select value={size} onChange={e=>{setSize(e.target.value);e.target.blur();}}><option value="normal">100 %</option><option value="large">200 %</option></select></label>
      <p className="tool-note">Vorbereitete Veranstaltung: Stationsverbindung und Wiederaufnahme laufen automatisch. Lichtaufträge werden nicht wiederholt.</p>
      <details><summary>Ablauf und Prüfpunkte</summary><ol><li>Altes Tor auswählen</li><li>Automatische Stationsverbindung abwarten</li><li>Bei Bedarf fehlende Voraussetzung beheben</li><li>Licht auswählen, Start und Antwort abwarten</li><li>Verbindung unterbrechen</li><li>Automatische Wiederverbindung abwarten</li></ol></details></div>
    </aside>, document.body)}
    <div className={`pilot-app ${size === "large" ? "large-type" : ""}`} style={{"--pilot-scale":size==="large"?2:1} as CSSProperties}>
      <header className="pilot-header">
        {state.screen === "station" ? <button className="back" onClick={()=>{setHelpOpen(false);send({type:"BACK"});}}><ArrowLeftIcon/>Stationen</button> : <div className="back context-label">Spielleitung</div>}
        <p className="event-name">Nacht der Siegel</p>
        <h1>{state.screen === "station" ? "Altes Tor" : "Stationen"}</h1>
      </header>
      <MobileScroll className="pilot-scroll"><main className="pilot-content" data-testid="pilot-content">
        {state.screen === "stations" ? <>
          <p className="intro">Wähle die Station, die du vor Ort bedienen möchtest.</p>
          <button className="station-row" onClick={()=>send({type:"OPEN",blocked:scenario==="bluetooth"?"bluetooth":scenario==="permission"?"permission":undefined})}><span><strong>Altes Tor</strong><small>Licht und Stationsbedienung</small></span><ChevronRightIcon/></button>

        </> : helpOpen ? <section className="setup-help" aria-labelledby="setup-title">
          <h2 id="setup-title" tabIndex={-1} ref={node=>node?.focus()}>Einrichtung prüfen lassen</h2>
          <p>Wende dich an die Person, die die Veranstaltung eingerichtet hat. Nenne ihr:</p>
          <p><strong>Nacht der Siegel · Altes Tor</strong><br/>{state.phase === "assignment" ? "Stationszuordnung unklar" : "Stationszugang fehlt"}</p>
          <ol>{state.phase === "assignment" ? <><li>Prüfen, welches Gerät zur Station „Altes Tor“ gehört.</li><li>In der Veranstaltungseinrichtung genau dieses Gerät zuordnen und doppelte Zuordnungen entfernen.</li></> : <><li>Prüfen, ob dein Zugang für diese Veranstaltung und Station gültig ist.</li><li>Den passenden Stationszugang über den vorgesehenen Einrichtungsweg bereitstellen.</li></>}<li>Nach der Korrektur hierher zurückkehren und „Erneut prüfen“ wählen.</li></ol>
          <p>Die Veranstaltungseinrichtung liegt außerhalb dieses Piloten.</p>
          <button className="primary" onClick={()=>setHelpOpen(false)}>Zur Stationsbedienung</button>
        </section> : <>
          <section className={`connection ${isLost ? "warning" : ""}`} aria-label="Verbindungsstatus" aria-live="polite" aria-atomic="true" data-testid="connection-status">
            {isLost ? <>
              <h2><ExclamationTriangleIcon/>Verbindung unterbrochen</h2>
              <p>Aktueller Zustand unbekannt.<br/>{state.uncertain ? `„${state.lastCommand}“: Ergebnis noch offen.` : "Die Verbindung wird wiederhergestellt."}</p>

              <button className="primary" onClick={scan}>Erneut versuchen</button>
            </> : ready ? <>
              <h2 className="success"><CheckCircledIcon/>Bereit</h2>
              <p className="device-report">{state.active ? `Licht: ${state.active}` : "Licht aus"}</p>
            </> : state.phase === "pending" ? <>
              <h2><ReloadIcon className="spin"/>{state.lastCommand === "Licht stoppen" ? "Licht wird gestoppt …" : `${state.selected} wird gestartet …`}</h2>
            </> : <>
              <h2>{["scanning","verifying"].includes(state.phase) && <ReloadIcon className="spin"/>}{status?.[0]}</h2>{status?.[1] && <p>{status[1]}</p>}{state.uncertain && ["scanning","verifying"].includes(state.phase) && <p>Letzter Lichtauftrag noch ungeklärt.</p>}
              {state.phase === "idle" && <button className="primary" onClick={scan}>Erneut verbinden</button>}
              {["permission","denied"].includes(state.phase) && <div className="action-stack"><button className="primary" onClick={()=>send({type:"ALLOW"})}>Zugriff erlauben</button><button className="text-button" onClick={()=>send({type:"DENY"})}>Jetzt nicht</button></div>}
              {state.phase === "bluetooth" && <button className="primary" onClick={()=>{setScenario("normal");send({type:"ENABLE_BT"});}}>Bluetooth einschalten</button>}
              {["empty","background","stale"].includes(state.phase) && <button className="primary" onClick={scan}>Erneut versuchen</button>}
              {["access","assignment"].includes(state.phase) && <div className="action-stack"><button className="primary" onClick={()=>setHelpOpen(true)}>Hilfe zur Einrichtung</button><button className="text-button" onClick={scan}>Erneut prüfen</button></div>}
              {["scanning","verifying"].includes(state.phase) && <button className="text-button" onClick={()=>send({type:"CANCEL"})}>Abbrechen</button>}
            </>}
          </section>
          <section className="light-controls" aria-label="Lichtsteuerung">
            <h2>Licht</h2>

            <fieldset disabled={state.phase==="pending"}><legend>Auswahl</legend>{effects.map(effect=><label className="effect" key={effect}><input type="radio" name="effect" value={effect} checked={state.selected===effect} onChange={()=>send({type:"SELECT",effect:effect as Effect})}/><span>{effect}</span></label>)}</fieldset>
            <div className="command-buttons"><button className="primary" disabled={!canCommand(state)} onClick={e=>{e.currentTarget.blur();send({type:"START"});}}><PlayIcon/>Start</button><button className="secondary" disabled={!canCommand(state)} onClick={e=>{e.currentTarget.blur();send({type:"STOP"});}}><StopIcon/>Stop</button></div>


          </section>
        </>}
      </main></MobileScroll>
    </div>
  </>;
}
