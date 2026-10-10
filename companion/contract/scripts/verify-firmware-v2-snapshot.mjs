import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync, readdirSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../firmware-v2");
const manifest = readFileSync(path.join(root, "SNAPSHOT.md"), "utf8");
const entries = [...manifest.matchAll(/^\| `((?:schemas|fixtures)\/[^`/]+)` \| `([A-F0-9]{64})` \|$/gm)];
assert.equal(entries.length, 12, "firmware v2 snapshot must list all 12 source files");

const listed = new Set();
for (const [, name, expected] of entries) {
  assert(!listed.has(name), `duplicate snapshot entry: ${name}`);
  listed.add(name);
  const actual = createHash("sha256").update(readFileSync(path.join(root, name))).digest("hex").toUpperCase();
  assert.equal(actual, expected, `snapshot content changed: ${name}`);
}

for (const directory of ["schemas", "fixtures"]) {
  for (const filename of readdirSync(path.join(root, directory))) {
    assert(listed.has(`${directory}/${filename}`), `unlisted snapshot file: ${directory}/${filename}`);
  }
}

console.log("Firmware v2 snapshot integrity passed");
