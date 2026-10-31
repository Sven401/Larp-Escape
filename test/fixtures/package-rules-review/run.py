"""Independent, fixed canonical vectors for the bounded P1a-3 reader.

No product encoder/model is used to derive input bytes or expected outcomes.
"""
import argparse
import copy
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
    # Exercise every compact-context field at its highest supported index.
    baseline = json.loads(next(v[1] for v in vectors if v[0] == 'baseline'))
    template = baseline['stations'][0]
    stations = []
    for si in range(8):
        station = copy.deepcopy(template)
        station['id'] = 's' + str(si)
        station['rules'] = []
        stations.append(station)
    rule = copy.deepcopy(template['rules'][0])
    rule['actions'] = []
    rule['guard'] = {'op': 'literal', 'type': 'bool', 'value': True}
    for ri in range(64):
        item = copy.deepcopy(rule)
        item['id'] = 'r' + str(ri).zfill(2)
        stations[7]['rules'].append(item)
    baseline['stations'] = stations
    baseline['subjects'] = []
    cases = []
    last = stations[7]['rules'][63]
    last['guard'] = {'op': 'fact', 'subject': 'missing'}
    cases.append((copy.deepcopy(baseline), '/stations/7/rules/63/guard'))
    last['trigger'] = {'kind': 'fact', 'subjects': ['missing']}
    cases.append((copy.deepcopy(baseline), '/stations/7/rules/63/trigger'))
    last['trigger'] = copy.deepcopy(rule['trigger'])
    last['guard'] = copy.deepcopy(rule['guard'])
    last['actions'] = [{'op': 'emit', 'subject': 'out' + str(i)} for i in range(8)]
    baseline['subjects'] = [{'id': 'out' + str(i), 'originStation': 's7'} for i in range(7)]
    cases.append((copy.deepcopy(baseline), '/stations/7/rules/63/actions/7'))
    encoded = [json.dumps(data, sort_keys=True, separators=(',', ':')) for data, _ in cases]
    result = subprocess.run([str(programs['cli'])], input='\n'.join(encoded)+'\n',
                            text=True, capture_output=True, check=True, env=env)
    lines = result.stdout.splitlines()
    assert len(lines) == len(cases), result.stdout
    for line, (_, path) in zip(lines, cases):
        assert line.startswith('R ') and line.endswith(path), (path, line)
    print('3 maximum-index deferred diagnostic paths passed')
    print(f'{len(vectors)} fixed semantic vectors passed')
    selected = {'baseline', 'fallback', 'module', 'instance', 'fact-output-conflict(True, True)'}
    result = subprocess.run([str(programs['faults'])],
                            input='\n'.join(v[1] for v in vectors if v[0] in selected)+'\n',
                            text=True, encoding='utf8', capture_output=True, check=True, env=env)
    print(result.stdout.strip())


if __name__ == '__main__':
    main()
