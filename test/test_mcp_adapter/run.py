"""Offline real-library MCP tests; no downloads or hardware access.

Normal path returns the test process exit code. --legacy-snapshot compiles
the old project adapter/port/controller files supplied by the reviewer and
runs intentionally failing assertions of the corrected behavior. A red
probe is a reproduction, never a passing CI test.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--deps', type=Path)
    parser.add_argument('--compiler', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--legacy-snapshot', type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    deps = args.deps.resolve() if args.deps else repo / '.pio/libdeps/host-mcp'
    args.output.mkdir(parents=True, exist_ok=True)
    originals = args.legacy_snapshot.resolve() if args.legacy_snapshot else repo
    checks = [('Adafruit MCP23017 Arduino Library', '2.3.2'), ('Adafruit BusIO', '1.17.4'), ('Unity', '2.6.1')]
    for folder, version in checks:
        properties = deps / folder / 'library.properties'
        if folder == 'Unity':
            info = json.loads((deps / folder / 'library.json').read_text(encoding='utf-8'))
            if info.get('name') != folder or info.get('version') != version:
                raise SystemExit('Unexpected Unity identity/version')
            print(f'{folder} {version}', flush=True)
            continue
        fields = dict(line.split('=', 1) for line in properties.read_text(encoding='utf-8').splitlines() if '=' in line)
        if fields.get('name') != folder or fields.get('version') != version:
            raise SystemExit(f'Unexpected dependency identity: {folder}: {fields.get("name")} {fields.get("version")}')
        print(f'{folder} {version}', flush=True)
    suite = repo / 'test/test_mcp_adapter'
    includes = [suite / 'host_stubs', originals / 'include', repo / 'include',
                originals / 'test/test_game_application', repo / 'test/test_game_application',
                originals / 'lib/MCPHandler', originals / 'lib/Keymatrixwrapper',
                originals / 'lib/GameState', repo / 'lib/GameState', repo / 'lib/MCPConfig', repo / 'lib/Machine',
                deps / 'Adafruit MCP23017 Arduino Library/src', deps / 'Adafruit BusIO', deps / 'Unity/src']
    sources = [suite / ('legacy_probe.cpp' if args.legacy_snapshot else 'test_main.cpp'),
               suite / 'host_stubs/HostRuntime.cpp', originals / 'lib/MCPHandler/MCPHandler.cpp',
               originals / 'lib/Keymatrixwrapper/Keymatrix.cpp', originals / 'lib/GameState/GameState.cpp',
               repo / 'lib/Machine/StateMachine.cpp', repo / 'lib/Machine/State.cpp', deps / 'Unity/src/unity.c']
    sources += sorted((deps / 'Adafruit MCP23017 Arduino Library/src').glob('*.cpp'))
    sources += sorted((deps / 'Adafruit BusIO').glob('*.cpp'))
    provenance = []
    for source in sources:
        provenance.append(f'{hashlib.sha256(source.read_bytes()).hexdigest()}  {source}')
    (args.output / 'sources.sha256').write_text('\n'.join(provenance) + '\n', encoding='utf-8')
    binary = args.output / ('mcp-legacy-probe.exe' if args.legacy_snapshot else 'mcp-regression.exe')
    command = [str(args.compiler), '-std=gnu++11', '-Wall', '-Wextra']
    if args.legacy_snapshot:
        command.append('-DMCP_LEGACY_PROBE')
    for include in includes:
        command += ['-I', str(include)]
    command += [str(source) for source in sources] + ['-o', str(binary)]
    environment = dict(os.environ)
    environment['PATH'] = str(args.compiler.parent) + os.pathsep + environment.get('PATH', '')
    (args.output / 'build-command.json').write_text(json.dumps(command, indent=2), encoding='utf-8')
    result = subprocess.run(command, capture_output=True, text=True, env=environment)
    (args.output / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    print(result.stdout + result.stderr, end='', flush=True)
    if result.returncode:
        return result.returncode
    result = subprocess.run([str(binary)], capture_output=True, text=True, env=environment)
    (args.output / 'test.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    print(result.stdout + result.stderr, end='', flush=True)
    return result.returncode

if __name__ == '__main__':
    raise SystemExit(main())
