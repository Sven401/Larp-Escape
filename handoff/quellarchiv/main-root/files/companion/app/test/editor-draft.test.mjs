import assert from "node:assert/strict";
import {
  addScene,
  createDraft,
  moveScene,
  parseDraft,
  removeScene,
  serializeDraft,
  updateDraft,
  updateScene,
  validateDraft,
  validateWorkingDraft,
} from "../src/editorDraft.mjs";

const original = createDraft("draft-1");
const withScene = addScene(original, "scene-1");
assert.equal(original.scenes.length, 0);
assert.equal(withScene.scenes.length, 1);
const renamed = updateScene(updateDraft(withScene, { title: "Die Gruft" }), "scene-1", {
  title: "Eingang", goal: "Tür finden", clue: "Symbol an der Wand",
});
assert.deepEqual(parseDraft(serializeDraft(renamed)), renamed);
assert.equal(removeScene(renamed, "scene-1").scenes.length, 0);
assert.throws(() => addScene(withScene, "scene-1"), /eindeutig/);
assert.throws(() => parseDraft(JSON.stringify({ ...renamed, version: 2 })), /version/i);
assert.throws(() => validateDraft({ ...renamed, scenes: [{ ...renamed.scenes[0], goal: "x".repeat(601) }] }), /600/);
assert.throws(() => updateScene(renamed, "missing", { title: "X" }), /nicht gefunden/);
assert.throws(() => parseDraft("x".repeat(128 * 1024 + 1)), /zu groß/);

const ordered = addScene(addScene(addScene(createDraft("ordered"), "first"), "second"), "third");
const incomplete = { ...ordered, scenes: ordered.scenes.map((scene) => scene.id === "second" ? { ...scene, title: "" } : scene) };
const moved = moveScene(incomplete, "second", "up");
assert.deepEqual(moved.scenes.map((scene) => scene.id), ["second", "first", "third"]);
assert.equal(moved.scenes[0].title, "");
assert.deepEqual(incomplete.scenes.map((scene) => scene.id), ["first", "second", "third"]);
assert.deepEqual(moveScene(moved, "second", "down").scenes.map((scene) => scene.id), ["first", "second", "third"]);
assert.equal(moveScene(incomplete, "first", "up"), incomplete);
assert.equal(moveScene(incomplete, "third", "down"), incomplete);
assert.throws(() => moveScene(incomplete, "missing", "up"), /nicht gefunden/);
assert.throws(() => moveScene(incomplete, "second", "left"), /Bewegungsrichtung/);

const editing = updateScene(updateDraft(ordered, { title: "" }), "first", { title: "" });
assert.equal(validateWorkingDraft(editing), editing);
assert.equal(addScene(editing, "fourth").scenes.length, 4);
assert.equal(removeScene(editing, "second").scenes.length, 2);
assert.deepEqual(moveScene(editing, "second", "up").scenes.map((scene) => scene.id), ["second", "first", "third"]);
assert.throws(() => validateDraft(editing), /Szenarioname/);
assert.throws(() => serializeDraft(editing), /Szenarioname/);
assert.throws(() => parseDraft(JSON.stringify(editing)), /Szenarioname/);
assert.throws(() => removeScene(editing, "missing"), /nicht gefunden/);
assert.throws(() => updateScene(editing, "first", { goal: "x".repeat(601) }), /600/);
const completed = updateScene(updateDraft(editing, { title: "Testentwurf" }), "first", { title: "Anfang" });
assert.deepEqual(parseDraft(serializeDraft(completed)), completed);

const entries = new Map();
globalThis.window = {
  localStorage: {
    getItem: (key) => entries.get(key) ?? null,
    setItem: (key, value) => entries.set(key, value),
  },
};
const { loadDraft, saveDraft } = await import("../web/draftStorage.mjs");
assert.equal(loadDraft().saved, false);
saveDraft(renamed);
assert.deepEqual(loadDraft(), { draft: renamed, saved: true });
delete globalThis.window;
console.log("Editor draft tests passed");
