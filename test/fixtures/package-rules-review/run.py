"""Independent, fixed canonical vectors for the bounded P1a-3 reader.

No product encoder/model is used to derive input bytes or expected outcomes.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--compiler', required=True, help='C++11 GCC/Clang compiler')
    parser.add_argument('--build-dir', default='private/package-rules-review')
    args = parser.parse_args()
    fixtures = Path(__file__).resolve().parent
    root = fixtures.parents[2]
    build = Path(args.build_dir).resolve()
    build.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    compiler = Path(args.compiler).resolve()
    env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
    programs = {}
    for name in ('cli', 'faults'):
        binary = build / (name + ('.exe' if os.name == 'nt' else ''))
        command = [str(compiler), '-std=c++11', '-Wall', '-Wextra', '-Wpedantic',
                   '-Werror', '-I' + str(root / 'include'), str(fixtures / (name + '.cpp'))]
        command += [str(root / 'src/GameConfiguration' / filename) for filename in
                    ('CanonicalPackageJson.cpp', 'PackageDeclarations.cpp', 'PackageRules.cpp')]
        subprocess.run(command + ['-o', str(binary)], check=True, env=env)
        programs[name] = binary
    vectors = json.loads((fixtures / 'vectors.json').read_text(encoding='utf8'))
    result = subprocess.run([str(programs['cli'])], input='\n'.join(v[1] for v in vectors)+'\n',
                            text=True, encoding='utf8', capture_output=True, check=True, env=env)
    lines = result.stdout.splitlines()
    assert len(lines) == len(vectors), (len(lines), len(vectors), result.stderr)
    for (name, data, expected), line in zip(vectors, lines):
        parts = line.split()
        actual = parts[0]
        assert actual == expected, (name, expected, line)
        if name.startswith('effect-') and expected == 'R':
            assert line.endswith('/stations/0/rules/0/actions/0'), (name, line)
        if name == 'subject-origin-missing':
            assert line.endswith('/subjects/0/originStation'), (name, line)
        if name == 'missing-trigger-subject-path':
            assert line.endswith('/stations/0/rules/0/trigger'), (name, line)
        if name == 'foreign-value':
            assert line.endswith('/stations/0/rules/0/guard'), (name, line)
    print(f'{len(vectors)} fixed semantic vectors passed')
    selected = {'baseline', 'fallback', 'module', 'instance', 'fact-output-conflict(True, True)'}
    result = subprocess.run([str(programs['faults'])],
                            input='\n'.join(v[1] for v in vectors if v[0] in selected)+'\n',
                            text=True, encoding='utf8', capture_output=True, check=True, env=env)
    print(result.stdout.strip())


if __name__ == '__main__':
    main()
