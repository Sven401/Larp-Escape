#!/usr/bin/env python3
"""Naeheglut: executable design model, not firmware or a transport simulation.

Run: python docs/led/probes/naeheglut/model.py
Only the adjacent result.json and preview.svg are generated. Standard library.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import hashlib
import io
import json
from pathlib import Path
import platform
import sys
import unittest

DURATION_MS = 1000
SCALE = 1_000_000
U32_MASK = 0xFFFFFFFF
PIXELS = 23
COLOUR = (24, 12, 2)  # Final channel codes; no gamma or further correction here.
BACKGROUND = (4, 4, 4)
BLUE = (0, 0, 8)
LIMIT = 32
SOURCE_BASIS = "dead12040c196ff74ab4be17ba71451a9351a676"


def elapsed(now: int, then: int) -> int:
    """Unsigned 32-bit difference; service intervals must remain < 2**31 ms."""
    return (now - then) & U32_MASK


def is_uint(value: object, minimum: int = 0, maximum: int = U32_MASK) -> bool:
    return type(value) is int and minimum <= value <= maximum


def is_level(value: object) -> bool:
    # None deliberately means unavailable. Out-of-range/nonnumeric != unavailable.
    return value is None or is_uint(value, 0, 1000)


def target(level: int | None) -> int:
    return 0 if level is None else level * 1000


def contribution(x: int) -> tuple[int, int, int]:
    return tuple(channel * x // SCALE for channel in COLOUR)


def mix(*layers: tuple[int, int, int]) -> tuple[int, int, int]:
    # Sum in a wide type before saturation, never sum in an 8-bit channel.
    return tuple(min(LIMIT, sum(layer[c] for layer in layers)) for c in range(3))


@dataclass
class Instance:
    generation: int
    sequence: int
    a: int
    b: int
    t0: int
    state: str = "active"
    last_command: tuple[str, int | None] = ("start", None)

    def value(self, now: int) -> int:
        d = min(elapsed(now, self.t0), DURATION_MS)
        # Maximum numerator is 1_000_000_000. Firmware specification uses uint64.
        return (self.a * (DURATION_MS - d) + self.b * d) // DURATION_MS

    def retarget(self, new_target: int, now: int) -> None:
        if new_target != self.b:
            self.a = self.value(now)
            self.b = new_target
            self.t0 = now


class Owner:
    """Serialized mathematical owner. No real queue, threads, IPC or hardware."""
    def __init__(self, capacity: int = 2):
        self.capacity = capacity
        self.instances: dict[str, Instance] = {}
        self.epoch = 1
        self.output_enabled = True

    def snapshot(self) -> dict:
        return {"epoch": self.epoch, "output_enabled": self.output_enabled, "instances": {
            key: asdict(value) for key, value in self.instances.items()}}

    def service(self, now: int) -> None:
        for instance in self.instances.values():
            if instance.state == "stopping" and elapsed(now, instance.t0) >= DURATION_MS:
                instance.state = "stopped"
                instance.a = instance.b = 0
                instance.t0 = now
            elif instance.state == "active" and elapsed(now, instance.t0) >= DURATION_MS:
                # Settle completed ramps so even long-lived instances remain correct
                # across successive uint32 rollovers when serviced as contracted.
                instance.a = instance.b
                instance.t0 = now

    def valid_envelope(self, epoch, instance, generation, sequence, now) -> bool:
        return (epoch == self.epoch and type(epoch) is int
                and type(instance) is str and bool(instance)
                and is_uint(generation, 1) and is_uint(sequence, 1)
                and is_uint(now))

    def start(self, epoch, instance, generation, sequence, level, now) -> str:
        if not self.valid_envelope(epoch, instance, generation, sequence, now):
            return "reject:envelope"
        if not is_level(level):
            return "reject:level"
        current = self.instances.get(instance)
        if current and current.generation == generation:
            if sequence < current.sequence:
                return "reject:stale"
            if sequence == current.sequence:
                return ("duplicate" if current.last_command == ("start", level)
                        else "reject:conflict")
        if current and (current.state != "stopped" or generation <= current.generation):
            return "reject:lifecycle"
        if sum(i.state != "stopped" for i in self.instances.values()) >= self.capacity:
            return "reject:capacity"
        self.instances[instance] = Instance(
            generation, sequence, 0, target(level), now, last_command=("start", level))
        self.output_enabled = True
        return "accepted"

    def command_instance(self, epoch, instance, generation, sequence, now, payload):
        if not self.valid_envelope(epoch, instance, generation, sequence, now):
            return None, "reject:envelope"
        current = self.instances.get(instance)
        if not current or current.generation != generation:
            return None, "reject:generation"
        if sequence < current.sequence:
            return None, "reject:stale"
        if sequence == current.sequence:
            return None, ("duplicate" if payload == current.last_command else "reject:conflict")
        return current, "accepted"

    def update(self, epoch, instance, generation, sequence, level, now) -> str:
        if not is_level(level):
            return "reject:level"
        current, status = self.command_instance(
            epoch, instance, generation, sequence, now, ("update", level))
        if current is None:
            return status
        if current.state != "active":
            return "reject:lifecycle"
        current.retarget(target(level), now)
        current.sequence = sequence
        current.last_command = ("update", level)
        return "accepted"

    def stop(self, epoch, instance, generation, sequence, now) -> str:
        current, status = self.command_instance(
            epoch, instance, generation, sequence, now, ("stop", None))
        if current is None:
            return status
        if current.state == "stopped":
            return "reject:lifecycle"
        if current.state == "active":
            # Stop has its own 1000-ms lifetime even if already dark or fading to 0.
            current.a = current.value(now)
            current.b = 0
            current.t0 = now
            current.state = "stopping"
        current.sequence = sequence
        current.last_command = ("stop", None)
        return "accepted"

    def frame(self, now: int, blue: bool = True) -> list[tuple[int, int, int]]:
        self.service(now)
        if not self.output_enabled:
            return [(0, 0, 0)] * PIXELS
        layers = [BACKGROUND]
        if blue:
            layers.append(BLUE)
        layers.extend(contribution(i.value(now)) for i in self.instances.values()
                      if i.state != "stopped")
        pixel = mix(*layers)
        return [pixel for _ in range(PIXELS)]

    def reset(self) -> None:
        # Epoch is part of the owner envelope, not of K=(instance,generation).
        self.epoch += 1
        self.instances.clear()
        self.output_enabled = False


class DesignTests(unittest.TestCase):
    def make(self, level=1000, start=0):
        owner = Owner()
        self.assertEqual(owner.start(1, "near", 1, 1, level, start), "accepted")
        return owner

    def rgb(self, owner, at, name="near"):
        return contribution(owner.instances[name].value(at))

    def test_01_independent_full_lifecycle_vectors(self):
        owner = self.make()
        for at, expected in [(0, (0, 0, 0)), (250, (6, 3, 0)),
                             (500, (12, 6, 1)), (750, (18, 9, 1)),
                             (1000, (24, 12, 2))]:
            self.assertEqual(self.rgb(owner, at), expected)
        self.assertEqual(owner.update(1, "near", 1, 2, 500, 1000), "accepted")
        self.assertEqual(self.rgb(owner, 1500), (18, 9, 1))
        self.assertEqual(self.rgb(owner, 2000), (12, 6, 1))
        self.assertEqual(owner.update(1, "near", 1, 3, None, 2000), "accepted")
        self.assertEqual(self.rgb(owner, 2500), (6, 3, 0))
        self.assertEqual(self.rgb(owner, 3000), (0, 0, 0))
        self.assertEqual(owner.instances["near"].state, "active")
        self.assertEqual(owner.update(1, "near", 1, 4, 1000, 3500), "accepted")
        self.assertEqual(self.rgb(owner, 4000), (12, 6, 1))
        self.assertEqual(self.rgb(owner, 4500), (24, 12, 2))
        self.assertEqual(owner.stop(1, "near", 1, 5, 5000), "accepted")
        self.assertEqual(self.rgb(owner, 5500), (12, 6, 1))
        self.assertEqual(owner.frame(6000), [(4, 4, 12)] * 23)
        self.assertEqual(owner.update(1, "near", 1, 6, 1000, 6500), "reject:lifecycle")
        self.assertEqual(owner.start(1, "near", 2, 1, 500, 7000), "accepted")
        self.assertEqual(self.rgb(owner, 7500), (6, 3, 0))
        self.assertEqual(self.rgb(owner, 8000), (12, 6, 1))

    def test_02_all_1001_levels_range_mask_and_monotonicity(self):
        previous = (0, 0, 0)
        for p in range(1001):
            with self.subTest(level=p):
                owner = self.make(p)
                rgb = self.rgb(owner, 1000)
                self.assertEqual(rgb, (24 * p // 1000, 12 * p // 1000, 2 * p // 1000))
                self.assertTrue(all(before <= after <= cap for before, after, cap
                                    in zip(previous, rgb, COLOUR)))
                frame = owner.frame(1000)
                self.assertEqual(len(frame), 23)
                self.assertEqual(frame, [mix(BACKGROUND, BLUE, rgb)] * 23)
                previous = rgb
        self.assertEqual(previous, COLOUR)

    def test_03_ramp_all_milliseconds_monotone_and_exact_end(self):
        owner = self.make()
        values = [owner.instances["near"].value(t) for t in range(1002)]
        self.assertEqual(values[0], 0)
        self.assertEqual(values[1000:], [1_000_000, 1_000_000])
        self.assertTrue(all(a <= b for a, b in zip(values, values[1:])))
        self.assertEqual(owner.update(1, "near", 1, 2, 0, 1000), "accepted")
        values = [owner.instances["near"].value(t) for t in range(1000, 2002)]
        self.assertEqual(values[:1], [1_000_000])
        self.assertEqual(values[1000:], [0, 0])
        self.assertTrue(all(a >= b for a, b in zip(values, values[1:])))

    def test_04_mid_ramp_update_has_no_jump_and_uses_exact_scalar(self):
        owner = self.make()
        before = owner.instances["near"].value(333)
        self.assertEqual(before, 333000)
        self.assertEqual(owner.update(1, "near", 1, 2, 123, 333), "accepted")
        self.assertEqual(owner.instances["near"].value(333), 333000)
        self.assertEqual(owner.instances["near"].value(833), 228000)
        self.assertEqual(owner.instances["near"].value(1333), 123000)
        self.assertEqual(self.rgb(owner, 833), (5, 2, 0))

    def test_05_repeated_equal_target_does_not_stall(self):
        owner = self.make()
        for seq, at in enumerate(range(20, 1000, 20), 2):
            self.assertEqual(owner.update(1, "near", 1, seq, 1000, at), "accepted")
            self.assertEqual(owner.instances["near"].t0, 0)
        self.assertEqual(self.rgb(owner, 1000), COLOUR)

    def test_06_repeated_invalid_does_not_restart_fade(self):
        owner = self.make()
        self.assertEqual(owner.update(1, "near", 1, 2, None, 1000), "accepted")
        self.assertEqual(owner.update(1, "near", 1, 3, None, 1500), "accepted")
        self.assertEqual(owner.instances["near"].t0, 1000)
        self.assertEqual(self.rgb(owner, 2000), (0, 0, 0))
        self.assertEqual(owner.instances["near"].state, "active")

    def test_07_render_cadences_and_skipped_frames_share_values(self):
        checkpoints = [333, 777, 1000, 1333, 1777, 2000, 2444]
        results = []
        for cadence in (17, 33, 67, None):
            owner = self.make()
            observations = {}
            regular = set(range(0, 2445, cadence)) if cadence else set()
            for at in sorted(regular | set(checkpoints) | {400, 1200, 1700}):
                if at == 400:
                    owner.update(1, "near", 1, 2, 650, at)
                if at == 1200:
                    owner.update(1, "near", 1, 3, None, at)
                if at == 1700:
                    owner.update(1, "near", 1, 4, 900, at)
                frame = owner.frame(at)
                if at in checkpoints:
                    observations[at] = frame
            results.append(observations)
        self.assertTrue(all(value == results[0] for value in results[1:]))

    def test_08_uint32_rollover_start_and_stop(self):
        start = 0xFFFFFF00
        owner = self.make(start=start)
        self.assertEqual(self.rgb(owner, (start + 500) & U32_MASK), (12, 6, 1))
        self.assertEqual(self.rgb(owner, (start + 1000) & U32_MASK), COLOUR)
        self.assertEqual(owner.stop(1, "near", 1, 2, (start + 1000) & U32_MASK), "accepted")
        self.assertEqual(self.rgb(owner, (start + 1500) & U32_MASK), (12, 6, 1))
        self.assertEqual(owner.frame((start + 2000) & U32_MASK), [(4, 4, 12)] * 23)
        self.assertEqual(owner.instances["near"].state, "stopped")

    def test_09_malformed_levels_reject_atomically(self):
        owner = self.make()
        for bad in (-1, 1001, 1.5, True, "500", float("nan")):
            before = owner.snapshot()
            self.assertEqual(owner.update(1, "near", 1, 2, bad, 500), "reject:level")
            self.assertEqual(owner.snapshot(), before)
            self.assertEqual(owner.start(1, "other", 1, 1, bad, 500), "reject:level")
            self.assertEqual(owner.snapshot(), before)
        self.assertEqual(owner.update(1, "near", 1, 2, None, 500), "accepted")
        self.assertEqual(owner.instances["near"].b, 0)
        self.assertEqual(owner.instances["near"].a, 500000)

    def test_10_duplicate_and_older_sequences_reject_atomically(self):
        owner = self.make()
        self.assertEqual(owner.update(1, "near", 1, 5, 500, 100), "accepted")
        for seq in (1, 4, 5):
            before = owner.snapshot()
            expected = "reject:conflict" if seq == 5 else "reject:stale"
            self.assertEqual(owner.update(1, "near", 1, seq, 999, 200), expected)
            self.assertEqual(owner.stop(1, "near", 1, seq, 200), expected)
            self.assertEqual(owner.snapshot(), before)

    def test_11_old_generation_cannot_change_new_instance(self):
        owner = self.make()
        owner.stop(1, "near", 1, 2, 1000)
        owner.frame(2000)
        self.assertEqual(owner.start(1, "near", 2, 1, 500, 2000), "accepted")
        before = owner.snapshot()
        self.assertEqual(owner.update(1, "near", 1, 999, 1000, 2100), "reject:generation")
        self.assertEqual(owner.stop(1, "near", 1, 1000, 2100), "reject:generation")
        self.assertEqual(owner.start(1, "near", 1, 1001, 1000, 2100), "reject:lifecycle")
        self.assertEqual(owner.snapshot(), before)

    def test_12_two_simultaneous_instances_and_targeted_stop(self):
        owner = self.make()
        self.assertEqual(owner.start(1, "second", 1, 1, 1000, 0), "accepted")
        self.assertEqual(owner.frame(1000, blue=False), [(32, 28, 8)] * 23)
        self.assertEqual(owner.frame(1000), [(32, 28, 16)] * 23)
        owner.stop(1, "near", 1, 2, 1000)
        self.assertEqual(owner.frame(2000), [(28, 16, 14)] * 23)
        self.assertEqual(owner.instances["second"].state, "active")
        self.assertEqual(owner.stop(1, "second", 1, 2, 2000), "accepted")
        self.assertEqual(owner.frame(3000), [(4, 4, 12)] * 23)

    def test_13_clipping_uses_wide_sum_and_is_order_independent(self):
        self.assertEqual(mix(BACKGROUND, BLUE, (12, 6, 1)), (16, 10, 13))
        self.assertEqual(mix(BACKGROUND, BLUE, COLOUR), (28, 16, 14))
        self.assertEqual(mix(BACKGROUND, BLUE), (4, 4, 12))
        self.assertEqual(mix(BACKGROUND, COLOUR, COLOUR), (32, 28, 8))
        self.assertEqual(mix(*([(24, 12, 2)] * 16)), (32, 32, 32))
        self.assertEqual(mix(BACKGROUND, COLOUR, BLUE), mix(BLUE, BACKGROUND, COLOUR))

    def test_14_capacity_and_rejected_start_are_atomic(self):
        owner = self.make()
        self.assertEqual(owner.start(1, "second", 1, 1, 500, 0), "accepted")
        before = owner.snapshot()
        self.assertEqual(owner.start(1, "third", 1, 1, 1000, 100), "reject:capacity")
        self.assertEqual(owner.start(1, "near", 2, 1, 0, 100), "reject:lifecycle")
        self.assertEqual(owner.snapshot(), before)
        owner.stop(1, "near", 1, 2, 200)
        before = owner.snapshot()
        self.assertEqual(owner.start(1, "near", 2, 1, 999, 300), "reject:lifecycle")
        self.assertEqual(owner.start(1, "third", 1, 1, 999, 300), "reject:capacity")
        self.assertEqual(owner.snapshot(), before)
        owner.frame(1200)
        self.assertEqual(owner.start(1, "third", 1, 1, 999, 1200), "accepted")

    def test_15_repeated_stop_does_not_restart_tween(self):
        owner = self.make()
        self.assertEqual(owner.stop(1, "near", 1, 2, 1000), "accepted")
        self.assertEqual(owner.stop(1, "near", 1, 3, 1500), "accepted")
        self.assertEqual(owner.instances["near"].t0, 1000)
        before = owner.snapshot()
        self.assertEqual(owner.update(1, "near", 1, 4, 1000, 1500), "reject:lifecycle")
        self.assertEqual(owner.snapshot(), before)
        self.assertEqual(owner.frame(2000), [(4, 4, 12)] * 23)
        self.assertEqual(owner.instances["near"].state, "stopped")

    def test_16_hard_reset_clears_layers_and_fences_old_epoch(self):
        owner = self.make()
        owner.start(1, "second", 1, 1, 500, 0)
        owner.reset()
        self.assertEqual(owner.snapshot(), {"epoch": 2, "output_enabled": False, "instances": {}})
        self.assertEqual(owner.frame(1000), [(0, 0, 0)] * 23)
        before = owner.snapshot()
        self.assertEqual(owner.start(1, "near", 1, 1, 1000, 1000), "reject:envelope")
        self.assertEqual(owner.update(1, "near", 1, 99, 1000, 1000), "reject:envelope")
        self.assertEqual(owner.stop(1, "near", 1, 100, 1000), "reject:envelope")
        self.assertEqual(owner.snapshot(), before)
        self.assertEqual(owner.frame(1000), [(0, 0, 0)] * 23)
        self.assertEqual(owner.start(2, "near", 1, 1, 500, 1000), "accepted")
        self.assertEqual(self.rgb(owner, 1000), (0, 0, 0))
        self.assertEqual(owner.frame(1000), [(4, 4, 12)] * 23)

    def test_17_zero_and_invalid_start_are_active_and_consume_capacity(self):
        owner = self.make(0)
        self.assertEqual(owner.start(1, "second", 1, 1, None, 0), "accepted")
        self.assertEqual(owner.frame(2000), [(4, 4, 12)] * 23)
        self.assertEqual([i.state for i in owner.instances.values()], ["active", "active"])
        self.assertEqual(owner.start(1, "third", 1, 1, 1000, 2000), "reject:capacity")
        self.assertEqual(owner.update(1, "near", 1, 2, 1000, 2000), "accepted")
        self.assertEqual(self.rgb(owner, 2500), (12, 6, 1))

    def test_18_frames_are_fresh_and_do_not_accumulate(self):
        owner = self.make(500)
        for at in (1000, 1001, 1018, 1051, 2000, 5000):
            self.assertEqual(owner.frame(at), [(16, 10, 13)] * 23)

    def test_19_service_across_long_uptime_preserves_completed_target(self):
        owner = self.make()
        for absolute in (1000, 1_000_000_000, 2_000_000_000, 3_000_000_000,
                         4_000_000_000, 4_294_967_296, 5_000_000_000):
            self.assertEqual(owner.frame(absolute & U32_MASK), [(28, 16, 14)] * 23)

    def test_20_stop_dark_instance_still_needs_new_generation(self):
        owner = self.make(0)
        self.assertEqual(owner.stop(1, "near", 1, 2, 100), "accepted")
        self.assertEqual(owner.instances["near"].state, "stopping")
        owner.frame(1100)
        self.assertEqual(owner.start(1, "near", 1, 3, 500, 1100), "reject:lifecycle")
        self.assertEqual(owner.start(1, "near", 2, 1, 500, 1100), "accepted")


    def test_21_exact_latest_duplicates_have_no_effect(self):
        owner = self.make()
        before = owner.snapshot()
        self.assertEqual(owner.start(1, "near", 1, 1, 1000, 300), "duplicate")
        self.assertEqual(owner.snapshot(), before)
        self.assertEqual(owner.update(1, "near", 1, 2, 500, 300), "accepted")
        before = owner.snapshot()
        self.assertEqual(owner.update(1, "near", 1, 2, 500, 700), "duplicate")
        self.assertEqual(owner.snapshot(), before)
        self.assertEqual(owner.stop(1, "near", 1, 3, 1300), "accepted")
        before = owner.snapshot()
        self.assertEqual(owner.stop(1, "near", 1, 3, 1700), "duplicate")
        self.assertEqual(owner.snapshot(), before)
        owner.frame(2300)
        before = owner.snapshot()
        self.assertEqual(owner.stop(1, "near", 1, 3, 2500), "duplicate")
        self.assertEqual(owner.snapshot(), before)


    def test_22_continuously_changing_input_reaches_last_target_after_last_change(self):
        owner = self.make(0)
        # Independent repeated integer recurrence at 100-ms boundaries:
        # floor((9 * previous_scalar + previous_target) / 10).
        expected = [0, 10000, 29000, 56100, 90490, 131441,
                    178296, 230466, 287419, 348677]
        for seq, (at, x) in enumerate(zip(range(100, 1001, 100), expected), 2):
            self.assertEqual(owner.instances["near"].value(at), x)
            self.assertEqual(owner.update(1, "near", 1, seq, at, at), "accepted")
            self.assertEqual(owner.instances["near"].value(at), x)
        self.assertEqual(self.rgb(owner, 1000), (8, 4, 0))
        self.assertEqual(owner.instances["near"].value(1500), 674338)
        self.assertEqual(self.rgb(owner, 1500), (16, 8, 1))
        self.assertEqual(owner.instances["near"].value(2000), 1_000_000)
        self.assertEqual(self.rgb(owner, 2000), COLOUR)

    def test_23_first_stop_during_invalid_fade_starts_own_duration(self):
        owner = self.make()
        self.assertEqual(owner.update(1, "near", 1, 2, None, 1000), "accepted")
        self.assertEqual(owner.instances["near"].value(1500), 500000)
        self.assertEqual(owner.stop(1, "near", 1, 3, 1500), "accepted")
        self.assertEqual(owner.instances["near"].value(1500), 500000)
        self.assertEqual(owner.instances["near"].t0, 1500)
        self.assertEqual(owner.stop(1, "near", 1, 4, 1750), "accepted")
        self.assertEqual(owner.instances["near"].t0, 1500)
        self.assertEqual(owner.instances["near"].value(2000), 250000)
        self.assertEqual(self.rgb(owner, 2000), (6, 3, 0))
        self.assertEqual(owner.instances["near"].state, "stopping")
        self.assertEqual(owner.frame(2500), [(4, 4, 12)] * 23)
        self.assertEqual(owner.instances["near"].state, "stopped")


def demo_samples() -> list[dict]:
    owner = Owner()
    events = {0: ("start", 1000), 1000: ("update", 500), 2000: ("update", None),
              3500: ("update", 1000), 5000: ("stop", None), 7000: ("start", 500)}
    sequence, generation = 0, 0
    samples = []
    for at in range(0, 8001, 250):
        owner.service(at)
        if at in events:
            action, level = events[at]
            if action == "start":
                generation += 1
                sequence = 1
                owner.start(1, "near", generation, sequence, level, at)
            else:
                sequence += 1
                if action == "update":
                    owner.update(1, "near", generation, sequence, level, at)
                else:
                    owner.stop(1, "near", generation, sequence, at)
        instance = owner.instances["near"]
        samples.append({"t_ms": at, "x": instance.value(at),
                        "effect_rgb": list(contribution(instance.value(at))),
                        "mixed_rgb": list(owner.frame(at)[0]), "state": instance.state})
    return samples


def write_preview(path: Path, samples: list[dict]) -> None:
    # SVG is a numerical diagram, not an optical preview or calibrated LED image.
    points = " ".join(f'{70 + s["t_ms"] / 10:.1f},{290 - s["x"] / 5000:.1f}'
                      for s in samples)
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="960" height="450" viewBox="0 0 960 450">',
             '<rect width="960" height="450" fill="#101820"/>',
             '<g fill="#e9eff4" font-family="sans-serif">',
             '<text x="40" y="36" font-size="24">Naeheglut: berechneter Ablauf</text>',
             '<text x="40" y="62" font-size="15">Simulation / Kanalcodes; keine optische Abnahme und keine Helligkeitskalibrierung</text>',
             '<path d="M70 90V290H870" fill="none" stroke="#97a4ad"/>',
             '<text x="25" y="96" font-size="12">100%</text>',
             '<text x="40" y="294" font-size="12">0%</text>',
             f'<polyline points="{points}" fill="none" stroke="#f0b64a" stroke-width="3"/>']
    for ms, label in [(0, "Start"), (1000, "p=500"), (2000, "ungueltig"),
                      (3500, "p=1000"), (5000, "Stop"), (7000, "Neustart")]:
        x = 70 + ms / 10
        parts.append(f'<text x="{x}" y="315" font-size="12">{ms / 1000:g}s</text>')
        parts.append(f'<text x="{x}" y="335" font-size="12">{label}</text>')
    for idx, ms in enumerate((0, 1000, 2000, 3000, 4500, 6000, 8000)):
        sample = next(s for s in samples if s["t_ms"] == ms)
        x = 45 + idx * 130
        rgb = sample["effect_rgb"]
        parts.append(f'<rect x="{x}" y="365" width="100" height="25" fill="rgb({rgb[0]},{rgb[1]},{rgb[2]})" stroke="#80909b"/>')
        parts.append(f'<text x="{x}" y="410" font-size="12">{ms / 1000:g}s: {tuple(rgb)}</text>')
    parts.append('</g></svg>')
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")


def main() -> int:
    output = io.StringIO()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(DesignTests)
    result = unittest.TextTestRunner(stream=output, verbosity=2).run(suite)
    samples = demo_samples()
    source = Path(__file__).resolve()
    report = {
        "scope": "Entwurfs-Rechnerprobe; keine Firmware-, Queue-, Transport-, Geraete- oder optische Pruefung",
        "run_at_utc": datetime.now(timezone.utc).isoformat(),
        "python": platform.python_version(),
        "source_basis_commit": SOURCE_BASIS,
        "model_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "tests_run": result.testsRun,
        "failures": len(result.failures), "errors": len(result.errors),
        "skipped": len(result.skipped), "successful": result.wasSuccessful(),
        "levels_enumerated": 1001,
        "render_cadences_ms": [17, 33, 67, "skipped_between_observations"],
        "constants": {"duration_ms": DURATION_MS, "scale": SCALE, "pixels": PIXELS,
                      "colour": COLOUR, "background": BACKGROUND, "blue": BLUE,
                      "channel_limit": LIMIT, "active_capacity": 2},
        "test_names": sorted(name for name in dir(DesignTests) if name.startswith("test_")),
        "unittest_output": output.getvalue(),
        "timeline": samples,
        "limitations": [
            "Kein Import von Produktcode; Python-Ganzzahlen ersetzen nicht den C++-Typnachweis.",
            "Zeitabstaende zwischen Service-Aufrufen muessen kleiner als 2**31 ms bleiben.",
            "Besitzer-Epoch, Generation und Sequenz sind Entwurfsvertrag, kein bestehender Wire-Befehl.",
            "Keine Aussage zur Originalpixelgleichheit, elektrischen Helligkeit, Gamma oder Goldwahrnehmung.",
            "Mischgrenze32 und Kapazitaet2 sind isolierte Probeannahmen, keine globale Firmware-Freigabe."]}
    (source.parent / "result.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    write_preview(source.parent / "preview.svg", samples)
    print(output.getvalue(), end="")
    print(f"model_sha256={report['model_sha256']}")
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
