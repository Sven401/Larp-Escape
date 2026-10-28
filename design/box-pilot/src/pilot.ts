export const effects = ["Funkeln", "Lichtimpuls", "Ripple"] as const;
export type Effect = typeof effects[number];
export type Phase = "idle"|"permission"|"denied"|"bluetooth"|"scanning"|"empty"|"verifying"|"access"|"ready"|"pending"|"lost"|"stale"|"background"|"assignment";
export type State = {screen:"stations"|"station"; phase:Phase; permission:boolean; selected:Effect; active:Effect|null; world:Effect|null; pendingEffect:Effect|null|undefined; uncertain:boolean; lastCommand:string; reconnected:boolean; commandCount:number};
export const initial:State={screen:"stations",phase:"idle",permission:true,selected:"Funkeln",active:null,world:null,pendingEffect:undefined,uncertain:false,lastCommand:"",reconnected:false,commandCount:0};
export type Action = {type:"OPEN";blocked?:"bluetooth"|"permission"}|{type:"RESET"|"BACK"|"SCAN"|"ALLOW"|"DENY"|"BLUETOOTH"|"ENABLE_BT"|"START"|"STOP"|"ACK"|"STALE"|"BACKGROUND"|"CANCEL"}|{type:"LOST";delivered?:boolean}|{type:"FOUND";empty:boolean;matches:number}|{type:"VERIFIED";denied:boolean}|{type:"SELECT";effect:Effect};
export const canCommand=(s:State)=>s.screen==="station"&&s.phase==="ready";
export function reducer(s:State,a:Action):State {
  switch(a.type){
    case "RESET": return {...initial};
    case "OPEN": return {...s,screen:"station",phase:a.blocked==="bluetooth"?"bluetooth":a.blocked==="permission"||!s.permission?"permission":"scanning",permission:a.blocked==="permission"?false:s.permission,active:null};
    case "BACK": return {...s,screen:"stations",phase:"idle",active:null,uncertain:s.uncertain||s.phase==="pending"};
    case "SCAN": return {...s,phase:s.permission?"scanning":"permission",active:null};
    case "ALLOW": return {...s,permission:true,phase:"scanning"};
    case "DENY": return {...s,permission:false,phase:"denied"};
    case "BLUETOOTH": return {...s,phase:"bluetooth",active:null};
    case "ENABLE_BT": return {...s,phase:s.permission?"scanning":"permission"};
    case "FOUND": return s.phase==="scanning"?{...s,phase:a.empty?"empty":a.matches!==1?"assignment":"verifying"}:s;
    case "VERIFIED": return s.phase==="verifying"?{...s,phase:a.denied?"access":"ready",active:a.denied?null:s.world,reconnected:s.uncertain||s.reconnected,uncertain:a.denied?s.uncertain:false}:s;
    case "SELECT": return {...s,selected:a.effect};
    case "START": return canCommand(s)?{...s,phase:"pending",lastCommand:`${s.selected} starten`,pendingEffect:s.selected,uncertain:false,commandCount:s.commandCount+1}:s;
    case "STOP": return canCommand(s)?{...s,phase:"pending",lastCommand:"Licht stoppen",pendingEffect:null,uncertain:false,commandCount:s.commandCount+1}:s;
    case "ACK": return s.phase==="pending"?{...s,phase:"ready",active:s.pendingEffect ?? null,world:s.pendingEffect ?? null,pendingEffect:undefined,uncertain:false}:s;
    case "LOST": return ["ready","pending","stale"].includes(s.phase)?{...s,phase:"lost",active:null,world:s.phase==="pending"&&a.delivered?s.pendingEffect ?? null:s.world,pendingEffect:undefined,uncertain:s.phase==="pending"||s.uncertain,reconnected:true}:s;
    case "STALE": return s.phase==="ready"?{...s,phase:"stale",active:null}:s;
    case "BACKGROUND": return {...s,phase:"background",active:null,uncertain:s.uncertain||s.phase==="pending",reconnected:true};
    case "CANCEL": return {...s,phase:"idle",active:null};
  }
}
