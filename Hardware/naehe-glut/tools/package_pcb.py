"""Package reviewed CAM, then hash PCB sources, outputs and checks."""
import hashlib
from pathlib import Path
import zipfile

base = Path(__file__).resolve().parents[1] / 'pcb'
cam = base / 'exports/gerber'
files = sorted(cam.iterdir())
assert len(files) == 10 and all(p.is_file() for p in files)
archive = base / 'exports/naehe-glut-r1-fertigung.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for p in files:
        z.write(p, p.name)
    z.write(base / 'FERTIGUNG.md', 'FERTIGUNG.md')
manifest = base / 'SHA256SUMS.txt'
entries = []
for p in sorted(base.rglob('*')):
    relative = p.relative_to(base)
    editor_state = (
        p.name.endswith(('.kicad_prl', '.lck', '.bak', '.bck', '~'))
        or p.name.startswith(('~', '_autosave-', '.~lock.'))
        or any(part.endswith('-backups') or part in ('backups', '__pycache__')
               for part in relative.parts)
    )
    if p.is_file() and p != manifest and not editor_state:
        entries.append(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(base).as_posix()}')
manifest.write_text('\n'.join(entries) + '\n', encoding='utf-8')
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
print(f'{archive.name}: {hashlib.sha256(archive.read_bytes()).hexdigest()}')
print(f'{len(entries)} files hashed')
