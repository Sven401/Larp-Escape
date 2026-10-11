# Szenario-Referenz aus dem Vite-Prototyp

Dieser kuratierte Ausschnitt erhält die editierbaren Regeln für JSON-v1-
Entwürfe und Leseproben aus dem bisherigen Vite-Prototyp. `editorDraft.mjs`
und `readingPreview.mjs` sowie die beiden Fachtests sind unverändert übernommen.
Die Android-App implementiert die entsprechenden Regeln eigenständig in Kotlin.
Die Kotlinfixture `vite-scenario-v1.json` stammt aus einem synthetischen
Vite-Export; sie belegt keine Übernahme gespeicherter Browserdaten.

Aus dem Produktroot, ohne zusätzliche Node-Pakete:

```powershell
node companion/app/test/editor-draft.test.mjs
node companion/app/test/reading-preview.test.mjs
```

Dieser Ausschnitt enthält keine Weboberfläche oder Startkonfiguration.
Die ursprünglichen unversionierten Vite-Quellen und Browserdaten bleiben im
ursprünglichen Arbeitsstand erhalten. Dateihashes stehen im
[Quellenmanifest](../source-manifest.json).
