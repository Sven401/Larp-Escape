import React from "react";
import { ArrowDown, ArrowUp, Download, Plus, Save, Trash2, Upload } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "./components/ui/alert";
import { Badge } from "./components/ui/badge";
import { Button } from "./components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./components/ui/card";
import { addScene, createDraft, moveScene, parseDraft, removeScene, serializeDraft, updateDraft, updateScene } from "../src/editorDraft.mjs";
import { loadDraft, saveDraft } from "./draftStorage.mjs";

function initialEditorState() {
  try {
    return { ...loadDraft(), error: null };
  } catch (error) {
    return { draft: null, saved: false, error: `Gespeicherter Entwurf konnte nicht geöffnet werden: ${error.message}` };
  }
}

export function EditorView({ onDraftChange }) {
  const [initial] = React.useState(initialEditorState);
  const [draft, setDraft] = React.useState(initial.draft);
  const [notice, setNotice] = React.useState(initial.error);
  const [saved, setSaved] = React.useState(initial.saved);
  const importRef = React.useRef(null);

  React.useEffect(() => {
    onDraftChange?.({ draft, saved });
  }, [draft, saved, onDraftChange]);

  function change(next) {
    setDraft(next);
    setSaved(false);
    setNotice(null);
  }

  function save() {
    try {
      saveDraft(draft);
      setSaved(true);
      setNotice("Entwurf auf diesem Gerät gespeichert.");
    } catch (error) {
      setNotice(error.message);
    }
  }

  function exportFile() {
    try {
      const url = URL.createObjectURL(new Blob([serializeDraft(draft)], { type: "application/json" }));
      const link = document.createElement("a");
      link.href = url;
      link.download = "szenario-entwurf.json";
      link.click();
      setTimeout(() => URL.revokeObjectURL(url), 0);
    } catch (error) {
      setNotice(error.message);
    }
  }

  async function importFile(event) {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (!file) return;
    try {
      if (file.size > 128 * 1024) throw new Error("Entwurfsdatei ist zu groß.");
      change(parseDraft(await file.text()));
      setNotice("Entwurf geladen. Zum Behalten bitte speichern.");
    } catch (error) {
      setNotice(`Import fehlgeschlagen: ${error.message}`);
    }
  }

  if (!draft) {
    return <div className="editor-layout">
      <Alert variant="destructive"><AlertTitle>Entwurf nicht verfügbar</AlertTitle><AlertDescription>{notice}</AlertDescription></Alert>
      <Button onClick={() => { setDraft(createDraft(crypto.randomUUID())); setNotice("Neuer Entwurf geöffnet. Der bisherige Speicherstand bleibt bis zum Speichern erhalten."); }}>Neuen Entwurf beginnen</Button>
    </div>;
  }

  return (
    <section className="editor-layout" aria-label="Lokaler Szenarioentwurf">
      <div className="editor-heading">
        <div>
          <p className="text-sm text-muted-foreground">Lokaler Entwurf</p>
          <h2>Szenario</h2>
        </div>
        <Badge>{saved ? "Gespeichert" : "Ungespeichert"}</Badge>
      </div>
      <p className="text-sm text-muted-foreground">Szenen werden nur auf diesem Gerät bearbeitet. Eine Übertragung zum Spielgerät ist noch nicht eingerichtet.</p>
      <Card>
        <CardHeader><CardTitle>Grunddaten</CardTitle><CardDescription>Name und Aufbau des Szenarios</CardDescription></CardHeader>
        <CardContent>
          <label className="editor-field">Name
            <input maxLength={80} value={draft.title} onChange={(event) => change(updateDraft(draft, { title: event.target.value }))} />
          </label>
        </CardContent>
      </Card>
      <div className="editor-heading">
        <h3>Szenen <span className="text-muted-foreground">{draft.scenes.length}/24</span></h3>
        <Button variant="secondary" disabled={draft.scenes.length >= 24} onClick={() => {
          try { change(addScene(draft, crypto.randomUUID())); }
          catch (error) { setNotice(error.message); }
        }}><Plus className="h-4 w-4" /> Szene</Button>
      </div>
      {draft.scenes.length === 0 && <p className="editor-empty">Noch keine Szene angelegt.</p>}
      <div className="editor-scenes">
        {draft.scenes.map((scene, index) => (
          <Card key={scene.id}>
            <CardHeader>
              <div className="editor-heading">
                <CardTitle>Szene {index + 1}</CardTitle>
                <div className="scene-tools">
                  <Button variant="ghost" size="icon" title="Szene nach oben" aria-label={`Szene ${index + 1} nach oben`} disabled={index === 0} onClick={() => change(moveScene(draft, scene.id, "up"))}><ArrowUp className="h-4 w-4" /></Button>
                  <Button variant="ghost" size="icon" title="Szene nach unten" aria-label={`Szene ${index + 1} nach unten`} disabled={index === draft.scenes.length - 1} onClick={() => change(moveScene(draft, scene.id, "down"))}><ArrowDown className="h-4 w-4" /></Button>
                  <Button variant="ghost" size="icon" title="Szene entfernen" aria-label={`Szene ${index + 1} entfernen`} onClick={() => {
                    if (window.confirm("Diese Szene aus dem Entwurf entfernen?")) change(removeScene(draft, scene.id));
                  }}><Trash2 className="h-4 w-4" /></Button>
                </div>
              </div>
            </CardHeader>
            <CardContent className="editor-fields">
              <label className="editor-field">Titel
                <input maxLength={80} value={scene.title} onChange={(event) => change(updateScene(draft, scene.id, { title: event.target.value }))} />
              </label>
              <label className="editor-field">Ziel der Szene
                <textarea maxLength={600} rows={3} value={scene.goal} onChange={(event) => change(updateScene(draft, scene.id, { goal: event.target.value }))} />
              </label>
              <label className="editor-field">Hinweis für Spielende
                <textarea maxLength={600} rows={3} value={scene.clue} onChange={(event) => change(updateScene(draft, scene.id, { clue: event.target.value }))} />
              </label>
            </CardContent>
          </Card>
        ))}
      </div>
      {notice && <Alert><AlertTitle>Entwurf</AlertTitle><AlertDescription>{notice}</AlertDescription></Alert>}
      <div className="editor-actions">
        <Button onClick={save}><Save className="h-4 w-4" /> Speichern</Button>
        <Button variant="secondary" onClick={exportFile}><Download className="h-4 w-4" /> Exportieren</Button>
        <Button variant="secondary" onClick={() => importRef.current?.click()}><Upload className="h-4 w-4" /> Importieren</Button>
        <input ref={importRef} className="hidden" type="file" accept="application/json,.json" onChange={importFile} aria-label="Entwurfsdatei auswählen" />
      </div>
    </section>
  );
}
