import { DRAFT_VERSION, MAX_SCENES } from "./editorDraft.mjs";

export function buildReadingPreview(draft) {
  if (!draft || draft.version !== DRAFT_VERSION || !Array.isArray(draft.scenes) || draft.scenes.length > MAX_SCENES) {
    throw new Error("Ungültiger lokaler Entwurf.");
  }
  const scenes = draft.scenes.map((scene, index) => {
    if (!scene || typeof scene.id !== "string" || typeof scene.title !== "string" || typeof scene.goal !== "string" || typeof scene.clue !== "string") {
      throw new Error("Ungültige lokale Szene.");
    }
    const title = scene.title.trim();
    const goal = scene.goal.trim();
    const clue = scene.clue.trim();
    return {
      id: scene.id,
      number: index + 1,
      title: title || `Szene ${index + 1} ohne Titel`,
      goal: goal || null,
      clue: clue || null,
      missing: [!title && "Titel", !goal && "Ziel", !clue && "Hinweis"].filter(Boolean),
    };
  });
  const title = typeof draft.title === "string" ? draft.title.trim() : "";
  return {
    title: title || "Unbenanntes Szenario",
    sceneCount: scenes.length,
    noteCount: (title ? 0 : 1) + scenes.reduce((count, scene) => count + scene.missing.length, 0),
    scenes,
  };
}
