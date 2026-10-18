"""Exercise two already-installed probes. Never uploads or writes flash.

Uses pyserial; both devices reset at start and individually during restart cases.
Private JSONL contains device identifiers. Keep --output outside versioned files.
"""
import argparse
import json
from pathlib import Path
import time

import serial

parser = argparse.ArgumentParser()
parser.add_argument('--box', required=True)
parser.add_argument('--receiver', required=True)
parser.add_argument('--box-mac', required=True)
parser.add_argument('--receiver-mac', required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
log = (args.output / 'serial.jsonl').open('x', encoding='utf-8')


class Device:
    def __init__(self, name, port):
        self.name, self.events = name, []
        self.buffer = bytearray()
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=2)
        self.serial.dtr = self.serial.rts = False
        self.serial.port = port
        self.serial.open()

    def reset(self):
        self.serial.dtr = False
        self.serial.rts = True
        time.sleep(0.12)
        self.serial.rts = False

    def poll(self):
        self.buffer.extend(self.serial.read(self.serial.in_waiting))
        while b'\n' in self.buffer:
            raw, _, self.buffer = self.buffer.partition(b'\n')
            line = raw.decode('utf-8', errors='replace').strip()
            log.write(json.dumps({'time': time.monotonic(), 'device': self.name, 'line': line}) + '\n')
            log.flush()
            try:
                event = json.loads(line)
            except ValueError:
                if any(marker in line for marker in ['Guru Meditation', 'Brownout', 'abort()']):
                    raise AssertionError(f'{self.name}: firmware fault, see serial log')
                continue
            self.events.append(event)
            assert event.get('event') != 'error', event
            if event.get('event') == 'status':
                assert event['drops'] == 0, 'Callback queue lost events'

    def command(self, command, event_type):
        pump(0.05)
        start = len(self.events)
        log.write(json.dumps({'device': self.name, 'command': command}) + '\n')
        log.flush()
        self.serial.write((command + '\n').encode('ascii'))
        wait(lambda: any(e['event'] == event_type for e in self.events[start:]), 3)
        return next(e for e in self.events[start:] if e['event'] == event_type)


devices = []


def pump(seconds):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        for d in devices:
            d.poll()
        time.sleep(0.002)


def wait(condition, timeout):
    end = time.monotonic() + timeout
    while not condition():
        assert time.monotonic() < end, 'Timed out; see serial.jsonl'
        pump(0.01)


def configure(device, mac):
    assert device.command('PEER ' + mac, 'peer')['ok']


def transfer(sender, receiver):
    pump(0.08)
    si, ri = len(sender.events), len(receiver.events)
    assert sender.command('SEND', 'send')['accepted']
    wait(lambda: any(e['event'] == 'receive' for e in receiver.events[ri:])
         and any(e['event'] == 'receipt' for e in sender.events[si:]), 3)
    received = [e for e in receiver.events[ri:] if e['event'] == 'receive']
    receipts = [e for e in sender.events[si:] if e['event'] == 'receipt']
    assert len(received) == len(receipts) == 1
    r, ack = received[0], receipts[0]
    assert r['bytes'] == 32 and r['exact_activate'] and r['ack_accepted']
    assert (r['boot'], r['count']) == (ack['boot'], ack['count'])
    pump(0.08)
    assert any(e['event'] == 'delivery' and e['ok'] for e in sender.events[si:])
    return {'sender': sender.name, 'receiver': receiver.name, 'boot': r['boot'], 'count': r['count']}


report = {'complete': False, 'transfers': [], 'checks': {}}
try:
    box = Device('box', args.box)
    devices.append(box)
    receiver = Device('receiver', args.receiver)
    devices.append(receiver)
    for d in devices:
        d.reset()
    wait(lambda: all(any(e['event'] == 'ready' for e in d.events) for d in devices), 12)
    for d, mac in [(box, args.box_mac), (receiver, args.receiver_mac)]:
        ready = next(e for e in d.events if e['event'] == 'ready')
        assert ready['mac'].lower() == mac.lower() and ready['channel'] == 1
    configure(box, args.receiver_mac)
    configure(receiver, args.box_mac)
    initial = [d.command('STATUS', 'status') for d in devices]
    pump(2)
    assert all(d.command('STATUS', 'status')['rx'] == s['rx'] for d, s in zip(devices, initial))
    report['checks']['idle_no_activation'] = True
    for sender, target in [(box, receiver), (receiver, box)]:
        for _ in range(10):
            report['transfers'].append(transfer(sender, target))
    # MAC delivery can succeed while the receiver application rejects the source.
    configure(receiver, '02:00:00:00:00:01')
    before = receiver.command('STATUS', 'status')
    si = len(box.events)
    assert box.command('SEND', 'send')['accepted']
    pump(1.5)
    after = receiver.command('STATUS', 'status')
    assert after['rx'] == before['rx'] and after['rejected'] == before['rejected'] + 1
    assert any(e['event'] == 'delivery' and e['ok'] for e in box.events[si:])
    assert not any(e['event'] == 'receipt' for e in box.events[si:])
    report['checks']['link_ack_without_application_receipt'] = True
    configure(receiver, args.box_mac)
    report['transfers'].append(transfer(box, receiver))
    assert receiver.command('RADIO OFF', 'radio')['ok']
    si = len(box.events)
    assert box.command('SEND', 'send')['accepted']
    wait(lambda: any(e['event'] == 'delivery' and not e['ok'] for e in box.events[si:]), 3)
    pump(1)
    assert not any(e['event'] == 'receipt' for e in box.events[si:])
    report['checks']['receiver_radio_off_detected'] = True
    assert receiver.command('RADIO ON', 'radio')['ok']
    report['transfers'].append(transfer(box, receiver))
    for restarted, mac in [(receiver, args.box_mac), (box, args.receiver_mac)]:
        for d in devices:
            d.command('STATUS', 'status')  # Check drops before a reboot clears counters.
        old_boot = next(e['boot'] for e in reversed(restarted.events) if e['event'] == 'ready')
        start = len(restarted.events)
        restarted.reset()
        wait(lambda: any(e['event'] == 'ready' for e in restarted.events[start:]), 12)
        new_boot = next(e['boot'] for e in restarted.events[start:] if e['event'] == 'ready')
        assert new_boot != old_boot
        configure(restarted, mac)
        for _ in range(3):
            report['transfers'].append(transfer(box, receiver))
            report['transfers'].append(transfer(receiver, box))
        report['checks'][restarted.name + '_restart'] = True
    report['final_status'] = {d.name: d.command('STATUS', 'status') for d in devices}
    assert all(s['drops'] == 0 for s in report['final_status'].values())
    report['complete'] = True
    print(json.dumps({'complete': True, 'successful_transfers': len(report['transfers']), 'checks': report['checks']}, indent=2))
finally:
    (args.output / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    for d in devices:
        d.serial.close()
    log.close()
