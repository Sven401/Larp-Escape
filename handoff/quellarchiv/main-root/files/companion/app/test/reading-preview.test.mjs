import assert from "node:assert/strict";
import { addScene, createDraft, moveScene } from "../src/editorDraft.mjs";
import { buildReadingPreview } from "../src/readingPreview.mjs";

const empty = createDraft("preview-draft");
assert.deepEqual(buildReadingPreview(empty), {
  title: "Neues Szenario",
  sceneCount: 0,
  noteCount: 0,
  scenes: [],
});

const partial = addScene(empty, "scene-1");
partial.title = "";
partial.scenes[0].title = "";
partial.scenes[0].goal = "  ";
partial.scenes[0].clue = "  Ein sichtbarer Hinweis.  ";
const preview = buildReadingPreview(partial);
assert.equal(preview.title, "Unbenanntes Szenario");
assert.equal(preview.sceneCount, 1);
assert.equal(preview.noteCount, 3);
assert.equal(preview.scenes[0].title, "Szene 1 ohne Titel");
assert.equal(preview.scenes[0].goal, null);
assert.equal(preview.scenes[0].clue, "Ein sichtbarer Hinweis.");
assert.deepEqual(preview.scenes[0].missing, ["Titel", "Ziel"]);
assert.equal(partial.scenes[0].clue, "  Ein sichtbarer Hinweis.  ");

const complete = addScene(createDraft("complete-draft"), "scene-1");
complete.title = "Die Krypta";
Object.assign(complete.scenes[0], { title: "Eingang", goal: "Tor öffnen", clue: "Rune am Rahmen" });
assert.equal(buildReadingPreview(complete).noteCount, 0);
assert.equal(buildReadingPreview(complete).scenes[0].number, 1);

const sequence = addScene(addScene(createDraft("sequence"), "first"), "second");
sequence.scenes[0].title = "Anfang";
sequence.scenes[1].title = "Finale";
const reordered = buildReadingPreview(moveScene(sequence, "second", "up"));
assert.deepEqual(reordered.scenes.map((scene) => [scene.id, scene.number]), [["second", 1], ["first", 2]]);

assert.throws(() => buildReadingPreview({ ...complete, version: 99 }));
assert.throws(() => buildReadingPreview({ ...complete, scenes: [null] }));

console.log("Reading preview tests passed");
