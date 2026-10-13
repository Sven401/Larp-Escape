import React from "react";
import { BookOpen, CircleAlert, Pencil } from "lucide-react";
import { Alert, AlertDescription, AlertTitle } from "./components/ui/alert";
import { Badge } from "./components/ui/badge";
import { Button } from "./components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "./components/ui/card";
import { buildReadingPreview } from "../src/readingPreview.mjs";

export function ReadingPreviewView({ draft, saved, onEdit }) {
  if (!draft) {
    return <section className="reading-preview" aria-label="Leseprobe">
      <Alert><AlertTitle>Kein lokaler Entwurf</AlertTitle><AlertDescription>Im Szenario kann ein neuer Entwurf begonnen werden.</AlertDescription></Alert>
      <Button variant="secondary" onClick={onEdit}><Pencil className="h-4 w-4" /> Zum Szenario</Button>
    </section>;
  }

  const preview = buildReadingPreview(draft);
  return (
    <section className="reading-preview" aria-label="Leseprobe">
      <header className="editor-heading">
        <div><p className="mode-kicker">Lokaler Entwurf · kein Gerätetest</p><h2>Leseprobe</h2></div>
        <Badge variant="secondary">{saved ? "Gespeichert" : "Ungespeichert"}</Badge>
      </header>
      <div className="reading-summary">
        <div><BookOpen className="h-5 w-5" /><div><h3>{preview.title}</h3><p>{preview.sceneCount} {preview.sceneCount === 1 ? "Szene" : "Szenen"}</p></div></div>
        <Button variant="outline" onClick={onEdit}><Pencil className="h-4 w-4" /> Bearbeiten</Button>
      </div>
      {preview.sceneCount === 0 ? (
        <Alert><AlertTitle>Noch keine Szenen</AlertTitle><AlertDescription>Die Leseprobe erscheint, sobald eine Szene angelegt ist.</AlertDescription></Alert>
      ) : (
        <>
          {preview.noteCount > 0 && <Alert><CircleAlert className="h-4 w-4" /><AlertTitle>Redaktionelle Lücken</AlertTitle><AlertDescription>{preview.noteCount} {preview.noteCount === 1 ? "Angabe fehlt" : "Angaben fehlen"} in der Szenenfolge. Das ist keine Gerätevalidierung.</AlertDescription></Alert>}
          <ol className="reading-timeline">
            {preview.scenes.map((scene) => (
              <li key={scene.id}>
                <span className="reading-index" aria-hidden="true">{scene.number}</span>
                <Card>
                  <CardHeader><CardTitle>{scene.title}</CardTitle></CardHeader>
                  <CardContent className="reading-scene-content">
                    <div><h4>Ziel der Szene</h4><p>{scene.goal || "Noch nicht beschrieben"}</p></div>
                    <div><h4>Hinweis für Spielende</h4><p>{scene.clue || "Noch nicht beschrieben"}</p></div>
                    {scene.missing.length > 0 && <p className="reading-missing">Offen: {scene.missing.join(" und ")}</p>}
                  </CardContent>
                </Card>
              </li>
            ))}
          </ol>
        </>
      )}
    </section>
  );
}
