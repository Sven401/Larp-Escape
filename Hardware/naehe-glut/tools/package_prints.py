"""Create the user-facing print archive from explicitly named, verified inputs."""
from pathlib import Path
import hashlib
import json
import zipfile

BASE = Path(__file__).resolve().parents[1]
PARTS = ('base', 'tray', 'lid', 'diffuser', 'ring_holder', 'coupon')


def main():
    geometry = json.loads((BASE / 'mechanik/geometry-report.json').read_text())
    assert geometry['passed'] is True, 'Geometry check did not pass'
    assert geometry['pcb_step_sha256'] == hashlib.sha256(
        (BASE / 'pcb/exports/naehe-glut-r1.step').read_bytes()
    ).hexdigest(), 'Geometry report refers to an outdated PCB STEP'
    report = json.loads((BASE / 'mechanik/slice-report.json').read_text())
    assert {r['part'] for r in report['parts']} == set(PARTS)
    for row in report['parts']:
        assert row['slicer_return_code'] == 0 and not row['warning'], row['part']
        for path, key in (
            (f"mechanik/{row['part']}.stl", 'stl_sha256'),
            (f"mechanik/print/{row['part']}.3mf", 'project_3mf_sha256'),
        ):
            assert hashlib.sha256((BASE / path).read_bytes()).hexdigest() == row[key], path
        with zipfile.ZipFile(BASE / f"mechanik/print/{row['part']}.3mf") as z:
            names = [n for n in z.namelist() if n.endswith('.gcode')]
            assert len(names) == 1
            assert hashlib.sha256(z.read(names[0])).hexdigest() == row['gcode_sha256']
    paths = [f'mechanik/{name}.stl' for name in PARTS]
    paths += [f'mechanik/print/{name}.3mf' for name in PARTS]
    paths += ['mechanik/DRUCK.md', 'mechanik/MONTAGE.md',
              'mechanik/slice-report.json', 'mechanik/geometry-report.json',
              'mechanik/slicer/LICENSE-AGPL-3.0.txt',
              'mechanik/slicer/profile-sources.json',
              'mechanik/slicer/machine.json', 'mechanik/slicer/process.json',
              'mechanik/slicer/petg_hf.json', 'mechanik/slicer/petg_translucent.json',
              'tools/prepare_slicer_profiles.py', 'tools/run_bambu.py',
              'tools/slice_enclosure.py', 'tools/package_prints.py']
    output = BASE / 'fertigung'
    output.mkdir(exist_ok=True)
    target = output / 'NG-R1-Druckpaket.zip'
    entries = {}
    with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for relative in sorted(paths):
            raw = (BASE / relative).read_bytes()
            info = zipfile.ZipInfo(relative, (2026, 10, 31, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, raw)
            entries[relative] = hashlib.sha256(raw).hexdigest()
        z.writestr('SHA256.json', json.dumps(entries, indent=2) + '\n')
    with zipfile.ZipFile(target) as z:
        assert z.testzip() is None
        for relative, digest in entries.items():
            assert hashlib.sha256(z.read(relative)).hexdigest() == digest
    print(f'{target.name}: {len(paths)} verified files; SHA256 {hashlib.sha256(target.read_bytes()).hexdigest()}')


if __name__ == '__main__':
    main()
