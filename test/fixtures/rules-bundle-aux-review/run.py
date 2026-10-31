"""Focused independent real Journal/Bundle/Control audit; no product authority."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--compiler', type=Path)
    mode.add_argument('--binary', type=Path)
    parser.add_argument('--build-dir', type=Path, default=Path('private/rules-bundle-aux-review'))
    parser.add_argument('--bundle-source', type=Path,
                        help='Audit-only alternate source for a negative regression demonstration')
    parser.add_argument('--rawsha-kind', choices=['received', 'claim', 'result'])
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    root = here.parents[2]
    env = dict(os.environ)
    sources = ['DeviceRuntime/ProgressJournal.cpp', 'DeviceRuntime/RulesBundleJournal.cpp',
               'DeviceRuntime/RulesProgressControl.cpp', 'GameExecution/RulesPlanner.cpp',
               'GameExecution/RulesProgressCodec.cpp', 'GameConfiguration/PackageModules.cpp',
               'GameConfiguration/PackageDeclarations.cpp', 'GameConfiguration/CanonicalPackageJson.cpp']
    if args.binary:
        if args.bundle_source:
            parser.error('--bundle-source requires --compiler')
        binary = args.binary.resolve()
    else:
        build = args.build_dir.resolve()
        build.mkdir(parents=True, exist_ok=True)
        compiler = args.compiler.resolve()
        env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
        binary = build / ('review.exe' if os.name == 'nt' else 'review')
        subprocess.run([str(compiler), '-std=c++11', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                        '-I' + str(root / 'include'), str(here / 'main.cpp'),
                        *[str(args.bundle_source.resolve()) if source == 'DeviceRuntime/RulesBundleJournal.cpp'
                          and args.bundle_source else str(root / 'src' / source) for source in sources],
                        '-lbcrypt', '-o', str(binary)],
                       check=True, env=env, timeout=180)
    command = [str(binary)]
    if args.rawsha_kind:
        command += ['--rawsha', {'received': '1', 'claim': '3', 'result': '4'}[args.rawsha_kind]]
    subprocess.run(command, check=True, env=env, timeout=90)


if __name__ == '__main__':
    main()
