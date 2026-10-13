# Vite-Referenz: frühere Companion-App

Status: befristete Mock-, Dokument- und Gestaltungsreferenz. Die verbindliche
Projekt-App liegt unter [Android](../android/README.md). Neue Produktfunktionen
werden dort entwickelt. Vite bleibt bis zur geprüften Übernahme der benötigten
Funktionen und Browserentwürfe erhalten.

Der folgende Text dokumentiert den bisherigen Prototypstand. Seine Aussagen
über fehlendes Android/BLE gelten für diesen Webprototyp; den aktuellen
Projektstand beschreibt [OPEN-DEPENDENCIES](../OPEN-DEPENDENCIES.md).
Alle folgenden Prüf-/Startbefehle beginnen im Produkt-Repositoryroot.
Browserentwürfe vor einer späteren Stilllegung als JSON exportieren; ihr
localStorage-Inhalt ist weder in Git noch in der Androidinstallation enthalten.

---
# Larp-Escape Companion

Session B owns the app side of the companion work.

This folder starts with a minimal, testable contract instead of a full Android
editor. The current slice lets an app-like layer read a mocked device status,
derive the narrow game-master controls and send deduplicated requests with
boot id, request id and expected revisions.

## Current slice

- `contract/v0/` defines the draft JSON fixtures for the first vertical cut,
  including ready/running/paused/readonly status examples.
- `contract/scripts/validate-contract.js` checks the fixtures without external
  packages.
- `app/src/mockDevice.js` simulates accepted, duplicate and conflict results.
  Accepted requests are queued; only the explicit test-only `advanceMock()`
  step changes the mock status. Direct calls with current revisions but without
  access, capability or phase permission are rejected as `not_allowed`, not
  misreported as revision conflicts.
- `app/src/domain.js` keeps request creation and allowed-actions logic free of
  Android, BLE or filesystem dependencies.
- `app/src/screenModel.js` is a UI-near game-master screen model for the first
  Android companion page: connection, profile, runtime, feedback and the few
  currently allowed live actions. A disconnected mock retains the last status
  for display but exposes no live controls.
- `app/index.html` and `app/web/` provide a Vite-visible UI slice for the same
  narrow Spielleitung status and control flow. `app/web/domain.mjs` and
  `app/web/mockDevice.mjs` and `app/web/presentation.mjs` are browser adapters
  for the transport-free rules, mock execution and connection state.
  `app/web/mockSession.mjs` owns the UI workflow for submitting, repeating,
  reloading and disconnecting without a real transport.
  The developer Technik view can load ready/running/paused/readonly v0 fixtures
  into a fresh local mock session. This test control is not a device status
  fetch; the mock connection state is preserved.
  `app/test/browser-mock.test.mjs` compares actions, presentation, requests,
  results and status transitions with the Node path across fixtures.
  `app/test/react-render.test.mjs` renders the actual React page through Vite
  against mock-session states, without a browser or extra test dependency.
  All five narrow live commands remain visible; the derived state disables
  commands that cannot currently be sent.
  The minimal Vite setup intentionally has no CommonJS plugin or extra config.
- `app/src/editorDraft.mjs` defines a local, versioned creative scenario draft
  with at most 24 scenes. The Szenario view edits names, scene goals and clues,
  reorders scenes, saves explicitly in browser storage and imports/exports
  draft JSON. The Leseprobe follows that local order. This draft is not a
  firmware profile and cannot be sent to a device. Editing permits temporarily
  empty titles while preserving IDs and size limits; save/import/export still
  require a complete valid local draft.
- `app/src/readingPreview.mjs` derives an editorial scene sequence from the
  current local draft. The Entwickler `Leseprobe` view includes unsaved edits,
  marks missing titles, goals and clues, and offers no device or playtest action.
- `contract/firmware-v2/` preserves Session A's schemas and host-generated
  fixtures as a read-only source snapshot. `app/src/firmwareV2Status.mjs`
  projects selected status fields and rejects older samples from the same boot.
  The Entwickler `Firmware v2` view compares Ready/Running fixtures without
  granting game-master access or issuing v2 commands.
- `contract/scripts/verify-firmware-v2-snapshot.mjs` checks every copied v2
  source file against the SHA-256 inventory in `SNAPSHOT.md`; it does not
  replace firmware host tests or validate the complete JSON schemas.
- `app/src/firmwareV2Reply.mjs` classifies v2 reply fixtures passively. The
  Firmware v2 view distinguishes engine execution, historical duplicate,
  request-number conflict and consumed stale-state replies, and displays the
  source request's expected revisions where that fixture exists. It does not
  send, retry or claim physical device effects.

Run:

```powershell
node companion/contract/scripts/validate-contract.js
node companion/contract/scripts/verify-firmware-v2-snapshot.mjs
node companion/app/test/app-slice.test.js
node companion/app/test/browser-mock.test.mjs
node companion/app/test/mock-session.test.mjs
node companion/app/test/react-render.test.mjs
node companion/app/test/editor-draft.test.mjs
node companion/app/test/reading-preview.test.mjs
node companion/app/test/firmware-v2-status.test.mjs
node companion/app/test/firmware-v2-reply.test.mjs
cd companion/app
pnpm install
pnpm run dev
pnpm exec vite build
```

Wenn der Codex-`pnpm`-Wrapper ein vorhandenes `node_modules` ohne TTY neu
anlegen moechte, nichts automatisch bereinigen. Bei bereits installierten
Abhaengigkeiten lassen sich die Tests wie oben direkt mit Node ausfuehren und
der Build mit `node node_modules/vite/bin/vite.js build` pruefen. Dieser Weg
veraendert den Paketbestand nicht.

Für die sichtbare Konflikt-Rückmeldung kann der Entwurf lokal mit
`http://127.0.0.1:5173/?scenario=conflict` geöffnet werden. Der erste Request
erzeugt dort einen Mock-Konflikt; nach `Status neu laden` kann ein neuer Request
angenommen werden. Beim Konflikt wird derselbe veraltete Request nicht als
Wiederholungsaktion angeboten. `Anfrage wiederholen` erlaubt nach Annahme
oder unklarem Mock-Fehler nur denselben Request; nach Annahme zeigt der Mock
darauf eine Duplicate-Antwort.
Nach `accepted` bleiben Spielleiteraktionen gesperrt, bis
`Mockstatus fortschalten` den naechsten angenommenen Befehl im Mock anwendet.
Dieser Knopf ist ein Testmittel und keine Firmware- oder Spielleiteraktion.
`Trennen` und `Verbinden` simulieren ausschließlich den Mock-Verbindungszustand;
bei Trennung zeigt die UI den zuletzt gelesenen Status und sperrt alle Befehle.
Die letzte Anfrage-Rueckmeldung bleibt dabei sichtbar.

## Boundaries

- Firmware remains authoritative for state, validation and execution.
- The visible Spielleitung controls still belong to the clearly labelled v0
  mock. Firmware v2 uses different request fields, command names, revisions
  and authorization rules; the two paths are not interchangeable.
- An `accepted` result never advances the mock status by itself. The separate
  mock step models a later status observation without defining a firmware
  completion event or transport message.
- Transport is not chosen yet; BLE is only a likely prototype candidate.
- Session A now selects queued replies for its current verified connection,
  but a bearer must recheck before physical send; authentication, wire reply
  framing and Android integration are still absent (`abbbeff`, firmware
  `docs/companion/reply-routing.md`).
- This is not a native Android project yet and does not claim BLE, board or
  hardware verification.
- Profile mapping, graph transitions, conditions, effects and hardware editing
  remain open until Session A confirms a matching firmware contract or mock.
- Browser storage belongs to the current browser profile. Export the draft JSON
  to move or back up a scenario; saving does not activate a firmware profile.

## UI modes

The Spielleitung mode follows the first Stitch console concept: current state,
connection, active profile, a few currently allowed controls and correlated
request feedback. It also shows Boot-ID, stored-profile match and the current
runtime/configuration revisions used for the next mock request. The Entwickler
mode follows the second Stitch concept for
local scene work, adds a read-only Leseprobe inspired by the Stitch test-run
timeline, and keeps technical status in a separate read-only view. The
Leseprobe has no timings, checkpoints or execution controls because the local
draft and the firmware contract do not define them.
The additional Firmware v2 view is a technical fixture comparison. It shows
numeric state/component IDs and revisions rather than inventing names from a
profile document the app does not have.
Progress, timers, graph editing and pin/bus controls from the exploratory
screens are not displayed as device facts because v0 has no data or write
contract for them. The UI reads its initial mock status from the v0 fixture.
