"""Read-only pad/net contract audit; does not establish routed connectivity."""
from pathlib import Path
import argparse, collections, hashlib, json, re
from cad_environment import configure
configure()
import pcbnew

BASE = Path(__file__).resolve().parents[1]
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pcb', type=Path, default=BASE/'naehe-glut-r2b.kicad_pcb')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    circuit_path = BASE/'circuit.json'
    protected = {args.pcb.resolve(), circuit_path.resolve(), Path(__file__).resolve()}
    if args.output.resolve() in protected:
        parser.error('--output must not overwrite an input or this audit script')
    before = {'pcb': sha(args.pcb), 'circuit': sha(circuit_path)}
    circuit = json.loads(circuit_path.read_text(encoding='utf-8-sig'))
    board = pcbnew.LoadBoard(str(args.pcb))
    expected = {}
    issues = []
    for part in circuit['parts']:
        ref = part['ref']
        if ref in expected:
            issues.append({'issue': 'duplicate_circuit_reference', 'ref': ref})
        expected[ref] = part
    actual = collections.defaultdict(list)
    for footprint in board.GetFootprints():
        actual[footprint.GetReference()].append(footprint)
    for ref in sorted(set(expected) ^ set(actual)):
        issues.append({'issue': 'reference_set_mismatch', 'ref': ref,
                       'in_circuit': ref in expected, 'in_pcb': ref in actual})
    net_pads = collections.Counter(str(pad.GetNetname()) for fp in board.GetFootprints() for pad in fp.Pads())
    routed_nets = {str(item.GetNetname()) for item in board.GetTracks()}
    all_zones = list(board.Zones()) + [zone for fp in board.GetFootprints() for zone in fp.Zones()]
    zoned_nets = {str(zone.GetNetname()) for zone in all_zones if not zone.GetIsRuleArea()}
    nc_named_nets = []
    checked = 0
    duplicate_pad_groups = []
    mechanical_pads = 0
    for ref in sorted(set(expected) & set(actual)):
        if len(actual[ref]) != 1:
            issues.append({'issue': 'duplicate_pcb_reference', 'ref': ref, 'count': len(actual[ref])})
            continue
        pads = collections.defaultdict(list)
        for pad in actual[ref][0].Pads():
            number = str(pad.GetNumber())
            net = str(pad.GetNetname())
            if not number or (ref == 'J1' and number == 'MP'):
                mechanical_pads += 1
                if net:
                    issues.append({'issue': 'unnumbered_pad_has_net', 'ref': ref, 'net': net})
                continue
            pads[number].append(net)
        pins = {str(k): v for k, v in expected[ref]['pins'].items()}
        for number in sorted(set(pads) ^ set(pins)):
            issues.append({'issue': 'pad_number_set_mismatch', 'ref': ref, 'pad': number,
                           'in_circuit': number in pins, 'in_pcb': number in pads})
        for number in sorted(set(pads) & set(pins)):
            desired = '' if pins[number] is None else '/' + pins[number].lstrip('/')
            if len(pads[number]) > 1:
                duplicate_pad_groups.append({'ref': ref, 'pad': number, 'count': len(pads[number])})
            for net in pads[number]:
                checked += 1
                nc_isolated = (not desired and bool(re.fullmatch(r'unconnected-\(' + re.escape(ref) + r'-.+-Pad' + re.escape(number) + r'\)', net))
                               and net_pads[net] == 1 and net not in routed_nets and net not in zoned_nets)
                if nc_isolated:
                    nc_named_nets.append(net)
                if net != desired and not nc_isolated:
                    issues.append({'issue': 'pad_net_mismatch', 'ref': ref, 'pad': number,
                                   'expected': desired, 'actual': net})
    after = {'pcb': sha(args.pcb), 'circuit': sha(circuit_path)}
    if before != after:
        issues.append({'issue': 'input_changed_during_check', 'before': before, 'after': after})
    report = {'status': 'pass' if not issues else 'fail', 'pcb_path': str(args.pcb.resolve()),
              'checker_sha256': sha(Path(__file__)), 'kicad_version': pcbnew.GetBuildVersion(),
              'input_sha256': before, 'reference_count_expected': len(expected),
              'reference_count_pcb': len(actual), 'physical_numbered_pads_checked': checked,
              'mechanical_pads_without_net': mechanical_pads, 'isolated_named_nc_nets': nc_named_nets, 'duplicate_pad_groups': duplicate_pad_groups,
              'issues': issues, 'limits': 'Pad net assignments only; no routed connectivity, DRC, manufacturing or functional release.'}
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    print(json.dumps({'status': report['status'], 'references': len(actual), 'pads': checked, 'issues': len(issues)}))
    raise SystemExit(0 if not issues else 1)

if __name__ == '__main__':
    main()



