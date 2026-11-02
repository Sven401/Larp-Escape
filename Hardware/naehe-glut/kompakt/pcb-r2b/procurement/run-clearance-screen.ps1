param([string]$KicadBin='C:\Users\seide\.codex\worktrees\naehe-glut-lieferziel\Larp Escape\.tmp\cad-tools\kicad\bin')
if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'CLEARANCE-SCREEN.json')) { throw 'This folder contains frozen evidence. Copy the entire R2b directory (including parent PCB/PRO inputs) to a separate work area and remove only generated DRC/report files there before rerunning.' }
$ErrorActionPreference='Stop'
$screenDir=Join-Path $PSScriptRoot 'clearance-screen-8b383237'
$sourceDir=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force -Path (Join-Path $screenDir 'kicad-config') | Out-Null
$env:KICAD_CONFIG_HOME=Join-Path $screenDir 'kicad-config'
Copy-Item -LiteralPath (Join-Path $sourceDir 'naehe-glut-r2b.kicad_pcb') -Destination (Join-Path $screenDir 'screen.kicad_pcb')
Copy-Item -LiteralPath (Join-Path $sourceDir 'naehe-glut-r2b.kicad_pro') -Destination (Join-Path $screenDir 'screen.kicad_pro')
$rulesPath=Join-Path $screenDir 'screen.kicad_dru'
$normalRules=Get-Content -LiteralPath $rulesPath -Raw
try {
 & (Join-Path $KicadBin 'kicad-cli.exe') pcb drc --format json --output (Join-Path $screenDir 'drc-all-layers.json') (Join-Path $screenDir 'screen.kicad_pcb')
 if ($LASTEXITCODE -ne 0) {throw 'Native DRC command failed'}
 Copy-Item -LiteralPath (Join-Path $screenDir 'positive-control.kicad_dru') -Destination $rulesPath
 & (Join-Path $KicadBin 'kicad-cli.exe') pcb drc --format json --output (Join-Path $screenDir 'positive-control.json') (Join-Path $screenDir 'screen.kicad_pcb')
 if ($LASTEXITCODE -ne 0) {throw 'Positive control command failed'}
} finally {
 [System.IO.File]::WriteAllText($rulesPath,$normalRules,[System.Text.UTF8Encoding]::new($false))
}
& (Join-Path $KicadBin 'python.exe') (Join-Path $PSScriptRoot 'summarize_clearance.py')


