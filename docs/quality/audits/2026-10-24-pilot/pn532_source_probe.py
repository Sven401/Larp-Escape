"""Verify real local PN532 sources and repair/restore only a temporary copy."""
import argparse
import datetime
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--result', type=Path, required=True)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('repair', args.repo / 'scripts/patch_pn532.py')
    repair = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(repair)
    replacement = args.repo / 'scripts/patches/pn532-1.3.4/read_detected.cpp'
    before = {name: (args.library / name).read_bytes()
              for name in ('library.properties', 'Adafruit_PN532.cpp', repair.BACKUP)}
    with tempfile.TemporaryDirectory(prefix='larp-pn532-real-source-') as temporary:
        copy = Path(temporary)
        for name in before:
            shutil.copyfile(args.library / name, copy / name)
        target = copy / 'Adafruit_PN532.cpp'
        source_hash = repair.digest(target.read_text(encoding='utf-8'))
        original_hash = repair.digest((copy / repair.BACKUP).read_text(encoding='utf-8'))
        assert original_hash == repair.PRISTINE_SHA256
        assert repair.patch(copy, replacement) == (source_hash == repair.PRISTINE_SHA256)
        patched_hash = repair.digest(target.read_text(encoding='utf-8'))
        assert patched_hash == repair.PATCHED_SHA256
        assert repair.patch(copy, replacement) is False
        assert repair.patch(copy, replacement, restore=True) is True
        assert repair.digest(target.read_text(encoding='utf-8')) == original_hash
        assert repair.patch(copy, replacement) is True
        assert repair.digest(target.read_text(encoding='utf-8')) == patched_hash
    assert all((args.library / name).read_bytes() == data for name, data in before.items())
    result = {'observed_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'input_normalized_sha256': source_hash, 'original_normalized_sha256': original_hash,
              'patched_normalized_sha256': patched_hash,
              'verify_restore_reapply': 'passed', 'original_cache_unchanged': True,
              'limits': 'Local cached source with real pins; no fresh download, adapter execution or hardware test'}
    args.result.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
