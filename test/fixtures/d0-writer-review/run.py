"""Real Fact writer signatures, independent preserved payloads and Python verification."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]

def head(major, value):
    if value < 24:
        return bytes([(major << 5) | value])
    for size, tag in ((1, 24), (2, 25), (4, 26), (8, 27)):
        if value < 1 << (size * 8):
            return bytes([(major << 5) | tag]) + value.to_bytes(size, 'big')
    raise ValueError('too large')

def take(data, offset, major):
    first = data[offset]
    assert first >> 5 == major
    offset += 1
    value = first & 31
    if value >= 24:
        size = 1 << (value - 24)
        value = int.from_bytes(data[offset:offset + size], 'big')
        offset += size
    return value, offset

def blob(data, offset):
    size, start = take(data, offset, 2)
    return data[start:start + size], start + size

def verify(line, expected):
    fields = line.split()
    assert len(fields) == 9, fields
    if expected is None:
        assert fields[0] == '0' and fields[1] != '0' and fields[2] == '1', fields
        assert fields[3:5] == ['-', '-'] and fields[5:] == ['0' * 64] * 4
        return
    assert fields[:3] == ['1', '0', '0'], fields
    packet = bytes.fromhex(fields[3])
    assert packet[:2] == b'\xd2\x84'
    protected, offset = blob(packet, 2)
    assert packet[offset] == 0xa0
    payload, offset = blob(packet, offset + 1)
    signature, offset = blob(packet, offset)
    assert offset == len(packet) and len(signature) == 64
    assert payload.hex() == fields[4] == expected['payloadHex']
    assert hashlib.sha256(payload).hexdigest() == fields[5] == expected['payloadDigest']
    key = ec.derive_private_key(1, ec.SECP256R1()).public_key()
    numbers = key.public_numbers()
    x, y = numbers.x.to_bytes(32, 'big'), numbers.y.to_bytes(32, 'big')
    assert fields[7:] == [x.hex(), y.hex()]
    cose_key = b'\xa5\x01\x02\x03\x26\x20\x01\x21\x58\x20' + x + b'\x22\x58\x20' + y
    kid = hashlib.sha256(cose_key).digest()
    assert len(cose_key) == 77 and kid.hex() == fields[6]
    assert protected == b'\xa2\x01\x26\x04\x58\x20' + kid
    aad = b'larp.event.v1'
    message = b'\x84\x6aSignature1' + head(2, len(protected)) + protected + head(2, len(aad)) + aad + head(2, len(payload)) + payload
    der = utils.encode_dss_signature(int.from_bytes(signature[:32], 'big'), int.from_bytes(signature[32:], 'big'))
    key.verify(der, message, ec.ECDSA(hashes.SHA256()))

def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--binary')
    parser.add_argument('--compiler')
    args = parser.parse_args()
    env = dict(os.environ)
    if args.binary:
        binary = Path(args.binary).resolve()
    else:
        compiler = Path(args.compiler).resolve()
        env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
        binary = ROOT / 'private/d0-writer-review/cli.exe'
        binary.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(compiler), '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-I' + str(ROOT / 'include'), str(HERE / 'cli.cpp'),
            str(ROOT / 'src/Companion/D0Fact.cpp'), str(ROOT / 'src/Companion/D0FactWriter.cpp'),
            '-lbcrypt', '-o', str(binary)], check=True, env=env)
    cases = [case for case in json.loads((ROOT / 'test/fixtures/d0-fact-review/vectors.json').read_text())['cases'] if case['expected'] == 'SignatureCheckedUnauthenticated']
    requests, expected = [], []
    def add(case, mode, outcome):
        requests.append(f"{case['key']['x']} {case['key']['y']} {case['coseHex']} {mode}")
        expected.append(outcome)
    for case in cases:
        add(case, 0, case)
    reference = cases[0]
    for mode in (1, 2, 3, 4, *range(10, 20)):
        add(reference, mode, None)
        add(reference, 0, reference)
    process = subprocess.run([str(binary)], input='\n'.join(requests) + '\n', text=True,
                             capture_output=True, check=True, env=env)
    lines = process.stdout.splitlines()
    assert len(lines) == len(expected), (process.stdout, process.stderr)
    for line, outcome in zip(lines, expected):
        verify(line, outcome)
    print(f'{len(cases)} independently preserved payloads with actual signed roundtrips; '
          f'14 rejection/reuse pairs; all signatures independently verified; 0 mismatches')

if __name__ == '__main__':
    main()
