import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import React from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { createServer } from "vite";
import { MockCompanionSession } from "../web/mockSession.mjs";

const appRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const ready = JSON.parse(readFileSync(path.join(appRoot, "../contract/v0/device-status.ready.json"), "utf8"));
const previousWindow = globalThis.window;
globalThis.window = { location: { search: "" }, localStorage: { getItem: () => null } };
const vite = await createServer({ root: appRoot, configFile: false, server: { middlewareMode: true }, appType: "custom" });

try {
  const { App } = await vite.ssrLoadModule("/web/main.jsx");
  const render = (session) => renderToStaticMarkup(React.createElement(App, { sessionFactory: () => session }));
  const button = (html, label) => [...html.matchAll(/<button\b[^>]*>[\s\S]*?<\/button>/g)]
    .map(([markup]) => markup).find((markup) => markup.includes(label));
  const enabled = (html, label) => {
    const markup = button(html, label);
    assert(markup, `missing button: ${label}`);
    return !/\bdisabled(?:=""|(?=\s|>))/.test(markup);
  };
  const session = new MockCompanionSession(ready, { requestPrefix: "render-test" });

  let html = render(session);
  assert.match(html, /Regiekonsole · v0-Mock/);
  assert.match(html, /Mock verbunden/);
  assert.match(html, /Spiel starten/);
  assert.match(html, /Aktives Profil/);
  assert.match(html, /Boot-ID<\/dt><dd>boot-20261006-0001/);
  assert.match(html, /Speicherstand<\/dt><dd>Aktiv = gespeichert/);
  assert.match(html, /Aktuelle Revisionen<\/dt><dd>Laufzeit 3 · Konfiguration 6/);
  assert.match(html, /Firmware v2/);
  assert.match(html, /Kein Gerätezugang/);
  assert.match(html, /Mock-Statusfixture/);
  assert.match(html, /Nur Lesen<\/option>/);
  assert.match(html, /Erwartete Revisionen<\/dt><dd>Konfiguration 1, Zustand 0/);
  assert.equal(enabled(html, "Spiel starten"), true);
  assert.equal(enabled(html, "Pausieren"), false);
  assert.equal(enabled(html, "Fortsetzen"), false);
  assert.equal(enabled(html, "Zurücksetzen"), false);
  assert.equal(enabled(html, "Zum Finale (Mock)"), false);

  const mismatched = structuredClone(ready);
  mismatched.profile.storedMatchesActive = false;
  html = render(new MockCompanionSession(mismatched));
  assert.match(html, /Speicherstand<\/dt><dd>Aktiver Stand weicht ab/);
  assert.equal(enabled(html, "Spiel starten"), false);

  session.submit({ type: "start" });
  html = render(session);
  assert.match(html, /Mockstatus fortschalten/);
  assert.match(html, /Anfrage render-test-0001/);
  assert.match(html, /Boot boot-20261006-0001 · Laufzeit 3 · Konfiguration 6/);
  assert.match(html, /Ergebnis: accepted/);
  assert.match(html, /Anfrage wiederholen<\/button>/);
  assert.match(html, /Eine Firmwareausführung ist nicht bestätigt/);
  for (const label of ["Spiel starten", "Pausieren", "Fortsetzen", "Zurücksetzen", "Zum Finale (Mock)"]) {
    assert.equal(enabled(html, label), false);
  }

  session.advanceMock();
  html = render(session);
  assert.equal(enabled(html, "Spiel starten"), false);
  assert.equal(enabled(html, "Pausieren"), true);
  assert.equal(enabled(html, "Fortsetzen"), false);
  assert.equal(enabled(html, "Zurücksetzen"), true);
  assert.equal(enabled(html, "Zum Finale (Mock)"), true);
  assert.doesNotMatch(html, /Mockstatus fortschalten/);

  session.setConnected(false);
  html = render(session);
  assert.match(html, /Mock getrennt/);
  assert.match(html, /Zuletzt gelesener Zustand/);
  for (const label of ["Spiel starten", "Pausieren", "Fortsetzen", "Zurücksetzen", "Zum Finale (Mock)"]) {
    assert.equal(enabled(html, label), false);
  }

  const conflict = new MockCompanionSession(ready);
  conflict.submit({ type: "start" }, { simulateStaleRuntime: true });
  html = render(conflict);
  assert.match(html, /Gerätezustand hat sich geändert; Status neu laden und erneut prüfen/);
  assert.match(html, /Status neu laden<\/button>/);
  assert.match(html, /Ergebnis: rejected \(conflict\)/);
  assert.doesNotMatch(html, /Anfrage wiederholen<\/button>/);

  const readonly = new MockCompanionSession(JSON.parse(readFileSync(path.join(appRoot, "../contract/v0/device-status.readonly.json"), "utf8")));
  html = render(readonly);
  assert.match(html, /Aktives Profil/);
  assert.equal(enabled(html, "Spiel starten"), false);
  assert.equal(enabled(html, "Pausieren"), false);
} finally {
  await vite.close();
  if (previousWindow === undefined) delete globalThis.window;
  else globalThis.window = previousWindow;
}

console.log("React companion render tests passed");
