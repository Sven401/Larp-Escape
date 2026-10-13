# Quellarchiv: hardware

Dieser Branch zweigt vom kuratierten Übergabestand ab. Die ursprünglichen Commitspitzen sind als Git-Vorfahren erreichbar, während der Projektbaum den kuratierten Stand zeigt. Offene Änderungen sind unverändert als Patch und/oder Dateisnapshot unter `handoff/quellarchiv/` erhalten; die Prüfsummen und relativen Quellpfade stehen in `manifest.json`.

Die Inhalte sind nicht als lauffähiger Gesamtstand oder Abnahme freigegeben. Den Patch nur in einer separaten Kopie des im Manifest genannten Quell-HEAD anwenden.

## Erhaltene Branchspitzen

- `codex/reichstagsbox-hardware`: `5d3779eb99516bcf6c73ae519d7da36e480636dc`

Der lokale EasyEDA-MCP-Klon basiert auf `dcf6704eb2b484ab3bbc0f8c73f71b0d58e87b00` (`https://github.com/vlab-software/easyeda_mcp.git`). Nur die drei projektspezifischen Änderungen sind als Patch erhalten. Die `.eext`-Ausgabe ist erzeugbar und wurde nicht übernommen. Der KiCad-POC bleibt ein ungerouteter Strukturversuch, keine PCB-Freigabe.
