import { createDraft, parseDraft, serializeDraft } from "../src/editorDraft.mjs";

const STORAGE_KEY = "larp-escape-local-scenario-draft-v1";

export function loadDraft() {
  const saved = window.localStorage.getItem(STORAGE_KEY);
  return { draft: saved ? parseDraft(saved) : createDraft(crypto.randomUUID()), saved: Boolean(saved) };
}

export function saveDraft(draft) {
  window.localStorage.setItem(STORAGE_KEY, serializeDraft(draft));
}
