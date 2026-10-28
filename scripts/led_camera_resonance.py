"""Assess independently recorded synthetic Wake scenarios against real camera pixels."""
import argparse
import json
from pathlib import Path

import numpy as np
from led_camera import digest, regions
from led_camera_assess import Run, event_axis, sample, sync_delay

LEVELS = [0, 1, 2, 4, 8, 16, 24, 32]
ZONES = np.array([1]*7 + [3]*6 + [2]*6 + [0]*4)
MASKS = [0, 1, 2, 4, 8, 3, 5, 9, 6, 10, 12, 7, 11, 13, 14, 15]
TRANSITIONS = [(1,15,4,1), (1,14,4,0), (1,15,4,0), (1,13,4,0), (1,15,4,0),
               (1,11,4,0), (1,15,4,0), (1,7,4,0), (1,15,4,0), (0,15,4,0),
               (1,15,4,0), (1,255,4,0), (1,15,4,0), (1,15,4,1),
               (1,15,0,1), (1,1,0,0), (1,3,4,1), (1,3,0,1),
               (1,15,4,1), (1,15,3,1), (1,15,4,1), (1,15,1,1)]


def load_config(run, path):
    config = json.loads(path.read_text())
    regions(config, *run.size)
    if config.get('mapping_status') != 'verified' or config['levels'] != LEVELS:
        raise ValueError('Unverified index map/levels')
    for key in ('firmware_sha256', 'settings', 'roi_layout_sha256', 'camera_index'):
        if config.get(key) != run.meta.get(key):
            raise ValueError('Calibration changed: '+key)
    return config


def color_calibration(root, calibration, output):
    run = Run(root)
    if run.meta['mode'] != 'resonance-colors' or len(run.events) != 22:
        raise ValueError('Incomplete independent resonance color ramp')
    config = load_config(run, calibration)
    for stage, event in enumerate(run.events):
        expected = np.zeros((23, 3), int)
        if stage < 6 and stage % 2 == 0:
            expected[:] = 16
        if 7 <= stage < 21 and stage % 2 == 1:
            value = LEVELS[1+(stage-7)//2]
            expected[:] = [value//6, value*2//3, value]
        if event['stage'] != stage or not np.array_equal(event['pixels'], expected):
            raise ValueError('Wrong independent color calibration frame')
    timing = sync_delay(run)
    points = [(p['x'], p['y']) for p in config['leds']]
    masks = [p['offsets'] for p in config['leds']]
    table = [np.zeros((23, 3))]
    for stage in range(7, 21, 2):
        values, clipped = sample(run.stable(stage, timing['delay_ms']), points, masks)
        dark, _ = sample(run.stable(stage+1, timing['delay_ms']), points, masks)
        if np.any(clipped):
            raise ValueError('Resonance color reference clipped')
        table.append(np.maximum(values-dark, 0))
    table = np.array(table)
    if np.any(np.diff(table.mean(axis=2), axis=0) < -(table[-1].mean(axis=1)*.15+3)):
        raise ValueError('Nonmonotonic independent resonance color response')
    if np.any(table[-1, :, 2] < 10):
        raise ValueError('Insufficient resonance blue signal')
    config['responses']['resonance'] = table.tolist()
    config['resonance_color_capture_sha256'] = digest(root/'capture.json')
    config['resonance_color_timing'] = timing
    output.write_text(json.dumps(config, indent=2), encoding='utf-8')
    print('Independent resonance color reference recorded for 23 LEDs')


def check_trace(events, wakes):
    """Spatial/color/settled rules independent of the engine's nearest-origin code."""
    checked = 0
    for event in events:
        candidates = [w for w in wakes if w['device_ms'] <= event['submitted_ms']]
        if not candidates:
            continue
        wake = candidates[-1]
        elapsed = event['submitted_ms']-wake['device_ms']
        if elapsed < 70 or event['stage'] != 100+wake['sequence']:
            continue
        pixels = np.array(event['pixels'])
        if wake['effect'] == 0:
            if pixels.any():
                raise ValueError('Stop/Wake without Start emitted light')
        elif wake['effect'] == 4:
            active = (1 << ZONES) & wake['mask'] if wake['valid'] and wake['mask'] <= 15 else np.zeros(23)
            if not np.array_equal(pixels[:, 2] > 0, active > 0):
                raise ValueError('Wake mask/validity lit the wrong physical indices')
            if np.any(pixels[:, 0] != pixels[:, 2]//6) or np.any(pixels[:, 1] != pixels[:, 2]*2//3):
                raise ValueError('Wrong resonance RGB color')
            lit = pixels[active > 0, 2]
            if lit.size and (lit.min() < 4 or lit.max() > 24):
                raise ValueError('Resonance brightness outside 4..24')
            if wake['valid'] and wake['mask'] == 15 and elapsed >= 850:
                if not np.all(pixels == [4, 16, 24]):
                    raise ValueError('All-four illumination failed to settle')
        checked += 1
    if checked < 10:
        raise ValueError('Too few semantic frame observations')
    for wake in wakes:
        if wake['effect'] != 4 or not wake['valid'] or not 0 < wake['mask'] < 15:
            continue
        samples = [e for e in events if e['stage']==100+wake['sequence']]
        if len(samples)<2 or samples[-1]['submitted_ms']-samples[0]['submitted_ms']<2400:
            continue
        blue = np.array([e['pixels'] for e in samples])[:, :, 2]
        for led in range(23):
            if wake['mask'] & (1 << ZONES[led]):
                if blue[:,led].min()>5 or blue[:,led].max()<22:
                    raise ValueError('Active region failed a complete breathing excursion')
    return checked


def assess(root, calibration, output):
    run = Run(root)
    if run.meta['mode'] not in ('resonance', 'resonance-transitions'):
        raise ValueError('Not a resonance scenario')
    config = load_config(run, calibration)
    if digest(root/'wake-events.json') != run.meta['wake_events_sha256']:
        raise ValueError('Changed synthetic Wake evidence')
    wakes = json.loads((root/'wake-events.json').read_text())
    actual = [(int(w['valid']), w['mask'], w['effect'], int(w['restart'])) for w in wakes]
    expected = [(1,m,4,int(i==0)) for i,m in enumerate(MASKS)] if run.meta['mode']=='resonance' else TRANSITIONS
    complete = run.meta['expected_stop_reason'] == 'finished'
    if not actual or actual != expected[:len(actual)] or (complete and len(actual)!=len(expected)):
        raise ValueError('Missing, reordered or altered synthetic input scenario')
    if np.any(run.events[-1]['pixels']):
        raise ValueError('Missing final transmitted black frame')
    semantic_frames = check_trace(run.events[:-1], wakes)
    timing = sync_delay(run)
    points = [(p['x'], p['y']) for p in config['leds']]
    masks = [p['offsets'] for p in config['leds']]
    observed, clipping = [], []
    for i in range(len(run.times)):
        values, clipped = sample(run.image(i), points, masks)
        observed.append(values)
        clipping.append(clipped)
    observed = np.array(observed)
    led_timing = [sync_delay(run, observed[:, led].mean(axis=1)) for led in range(23)]
    observed -= np.median(observed[run.times < run.meta['command_sent_ms']], axis=0)
    axis = event_axis(run)
    pixels = np.array([e['pixels'] for e in run.events])
    target = np.zeros_like(observed)
    for led in range(23):
        lookup = np.searchsorted(axis+led_timing[led]['delay_ms'], run.times, side='right')-1
        for frame, event_index in enumerate(lookup):
            if event_index < 0 or run.events[event_index]['stage'] < 100:
                continue
            stage = run.events[event_index]['stage']-100
            effect = wakes[stage]['effect']
            rgb = pixels[event_index, led]
            family = 'cool' if effect==3 else 'white' if effect==1 else 'resonance'
            lut = np.array(config['responses'][family])
            for channel in range(3):
                target[frame, led, channel] = np.interp(rgb[2], LEVELS, lut[:, led, channel])
    first = next(i for i,e in enumerate(run.events) if e['stage'] >= 100)
    selected = run.times >= axis[first]+timing['delay_ms']-200
    measured_times = run.times[selected]
    final_dark = []
    for led in range(23):
        off = run.times >= axis[-1]+led_timing[led]['delay_ms']+200
        if np.sum(off) < 10:
            raise ValueError('Insufficient optical final darkness')
        final_dark.append(np.percentile(np.maximum(observed[off,led].mean(axis=1),0),95))
    def smooth(a):
        return (a[:-2]+a[1:-1]+a[2:])/3
    actual_curve = smooth(observed[selected]).mean(axis=2)
    target_curve = smooth(target[selected]).mean(axis=2)
    full = np.array(config['responses']['white'])[-1].mean(axis=1)
    errors = np.abs(smooth(observed[selected])-smooth(target[selected])).mean(axis=2)/full
    results = []
    for led in range(23):
        correlation = float(np.corrcoef(actual_curve[:,led], target_curve[:,led])[0,1])
        mae = float(errors[:,led].mean())
        p95 = float(np.percentile(errors[:,led],95))
        dark = float(final_dark[led]/full[led])
        passed = np.isfinite(correlation) and correlation>=.90 and mae<=.12 and p95<=.25 and dark<=.05
        results.append(dict(led=led, passed=bool(passed), correlation=correlation if np.isfinite(correlation) else None,
                            normalized_mae=mae, normalized_p95=p95, final_dark_fraction=dark, timing=led_timing[led]))
    max_clip = float(np.max(np.array(clipping)[selected]))
    result = dict(schema=1, mode=run.meta['mode'], passed=all(r['passed'] for r in results) and max_clip==0,
                  stop_reason=run.meta['observed_stop_reason'], semantic_frames=semantic_frames, input_steps=len(wakes),
                  firmware_sha256=run.meta['firmware_sha256'], capture_sha256=digest(root/'capture.json'),
                  calibration_sha256=digest(calibration), maximum_clipped_fraction=max_clip, leds=results,
                  criteria=dict(correlation_min=.90, normalized_mae_max=.12, normalized_p95_max=.25, final_dark_max=.05, clipping_max=0),
                  timing=timing, scope='Isolated synthetic Wake observations and real optical output; no physical sensor, BLE or gameplay acceptance')
    output.write_text(json.dumps(result, indent=2, allow_nan=False), encoding='utf-8')
    np.savez_compressed(output.with_suffix('.npz'), times=measured_times, observed=observed[selected], expected=target[selected])
    print(json.dumps(dict(passed=result['passed'], input_steps=len(wakes), stop=result['stop_reason'],
                          failed_leds=[r['led'] for r in results if not r['passed']], maximum_clipped_fraction=max_clip)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['colors', 'assess'])
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--calibration', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    (color_calibration if args.command=='colors' else assess)(args.capture,args.calibration,args.output)
