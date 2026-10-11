export const DRAFT_VERSION = 1;
export const MAX_SCENES = 24;
export const MAX_TITLE_LENGTH = 80;
export const MAX_NOTE_LENGTH = 600;

export function createDraft(id) {
  return { version: DRAFT_VERSION, id, title: "Neues Szenario", scenes: [] };
}

function requireText(value, name, maxLength) {
  if (typeof value !== "string" || value.trim().length === 0 || value.length > maxLength) {
    throw new Error(`${name} muss 1 bis ${maxLength} Zeichen enthalten.`);
  }
}

function requireEditableText(value, name, maxLength) {
  if (typeof value !== "string" || value.length > maxLength) {
    throw new Error(`${name} darf höchstens ${maxLength} Zeichen enthalten.`);
  }
}

export function validateWorkingDraft(draft) {
  if (!draft || typeof draft !== "object" || Array.isArray(draft)) throw new Error("Ungültiger Entwurf.");
  if (draft.version !== DRAFT_VERSION) throw new Error("Diese Entwurfsversion wird nicht unterstützt.");
  requireText(draft.id, "Entwurfs-ID", 100);
  requireEditableText(draft.title, "Szenarioname", MAX_TITLE_LENGTH);
  if (!Array.isArray(draft.scenes) || draft.scenes.length > MAX_SCENES) {
    throw new Error(`Ein Entwurf darf höchstens ${MAX_SCENES} Szenen enthalten.`);
  }
  const ids = new Set();
  for (const scene of draft.scenes) {
    if (!scene || typeof scene !== "object" || Array.isArray(scene)) throw new Error("Ungültige Szene.");
    requireText(scene.id, "Szenen-ID", 100);
    requireEditableText(scene.title, "Szenentitel", MAX_TITLE_LENGTH);
    if (ids.has(scene.id)) throw new Error("Szenen-IDs müssen eindeutig sein.");
    ids.add(scene.id);
    for (const key of ["goal", "clue"]) {
      if (typeof scene[key] !== "string" || scene[key].length > MAX_NOTE_LENGTH) {
        throw new Error(`${key} darf höchstens ${MAX_NOTE_LENGTH} Zeichen enthalten.`);
      }
    }
  }
  return draft;
}

export function validateDraft(draft) {
  validateWorkingDraft(draft);
  requireText(draft.title, "Szenarioname", MAX_TITLE_LENGTH);
  for (const scene of draft.scenes) requireText(scene.title, "Szenentitel", MAX_TITLE_LENGTH);
  return draft;
}

export function addScene(draft, id) {
  validateWorkingDraft(draft);
  if (draft.scenes.length === MAX_SCENES) throw new Error("Die maximale Szenenzahl ist erreicht.");
  const next = { ...draft, scenes: [...draft.scenes, { id, title: "Neue Szene", goal: "", clue: "" }] };
  return validateWorkingDraft(next);
}

export function updateDraft(draft, changes) {
  validateWorkingDraft(draft);
  return validateWorkingDraft({ ...draft, ...changes, version: DRAFT_VERSION, id: draft.id, scenes: draft.scenes });
}

export function updateScene(draft, id, changes) {
  validateWorkingDraft(draft);
  if (!draft.scenes.some((scene) => scene.id === id)) throw new Error("Szene nicht gefunden.");
  const scenes = draft.scenes.map((scene) => scene.id === id
    ? { ...scene, ...changes, id: scene.id }
    : scene);
  return validateWorkingDraft({ ...draft, scenes });
}

export function removeScene(draft, id) {
  validateWorkingDraft(draft);
  if (!draft.scenes.some((scene) => scene.id === id)) throw new Error("Szene nicht gefunden.");
  return validateWorkingDraft({ ...draft, scenes: draft.scenes.filter((scene) => scene.id !== id) });
}

export function moveScene(draft, id, direction) {
  validateWorkingDraft(draft);
  if (direction !== "up" && direction !== "down") throw new Error("Ungültige Bewegungsrichtung.");
  const index = draft.scenes.findIndex((scene) => scene?.id === id);
  if (index < 0) throw new Error("Szene nicht gefunden.");
  const target = index + (direction === "up" ? -1 : 1);
  if (target < 0 || target >= draft.scenes.length) return draft;
  const scenes = [...draft.scenes];
  [scenes[index], scenes[target]] = [scenes[target], scenes[index]];
  return { ...draft, scenes };
}

export function parseDraft(json) {
  if (typeof json !== "string" || json.length > 128 * 1024) throw new Error("Entwurfsdatei ist zu groß.");
  const parsed = JSON.parse(json);
  validateDraft(parsed);
  return {
    version: DRAFT_VERSION,
    id: parsed.id,
    title: parsed.title,
    scenes: parsed.scenes.map(({ id, title, goal, clue }) => ({ id, title, goal, clue })),
  };
}

export function serializeDraft(draft) {
  validateDraft(draft);
  return `${JSON.stringify(draft, null, 2)}\n`;
}
