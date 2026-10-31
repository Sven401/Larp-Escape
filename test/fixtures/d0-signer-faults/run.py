"""Run the real D0 adapter against a noncryptographic PSA signer API seam."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--build-dir', default='private/d0-signer-faults')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    build = Path(args.build_dir).resolve()
    build.mkdir(parents=True, exist_ok=True)
    compiler = Path(args.compiler).resolve()
    env = dict(os.environ)
    env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
    binary = build / ('lifecycle.exe' if os.name == 'nt' else 'lifecycle')
    adapter = root / 'lib/PsaD0FactSigner'
    subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                    '-I' + str(here / 'stubs'), '-I' + str(root / 'include'), '-I' + str(adapter),
                    str(here / 'main.cpp'), str(adapter / 'PsaD0FactSigner.cpp'),
                    '-o', str(binary)], check=True, env=env)
    subprocess.run([str(binary)], check=True, env=env)


if __name__ == '__main__':
    main()
