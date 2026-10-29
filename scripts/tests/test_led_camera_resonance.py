import copy
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from led_camera_resonance import check_trace


class ResonanceTraceTests(unittest.TestCase):
    def fixture(self, mask=15, valid=True, effect=4):
        # Explicit documented zone partition, independently written fixture.
        zone_bits = [2]*7 + [8]*6 + [4]*6 + [1]*4
        pixels = [[4,16,24] if valid and mask<=15 and (mask&bit) and effect==4 else [0,0,0]
                  for bit in zone_bits]
        wake = dict(sequence=0, device_ms=100, valid=valid, mask=mask, effect=effect)
        events = [dict(submitted_ms=1000+i*33, stage=100, pixels=copy.deepcopy(pixels)) for i in range(10)]
        return events, [wake]

    def test_all_masks_and_unknowns(self):
        for mask in range(16):
            self.assertEqual(check_trace(*self.fixture(mask)),10)
        self.assertEqual(check_trace(*self.fixture(255)),10)
        self.assertEqual(check_trace(*self.fixture(15,False)),10)
        self.assertEqual(check_trace(*self.fixture(15,True,0)),10)

    def test_rejects_wrong_zone_color_and_unsettled_merge(self):
        for mask, replacement in ((1,(0,[4,16,24])), (15,(0,[4,15,24])), (15,(0,[3,13,20]))):
            events,wakes = self.fixture(mask)
            index,pixel = replacement
            events[0]['pixels'][index] = pixel
            with self.assertRaises(ValueError):
                check_trace(events,wakes)

    def test_rejects_unknown_and_stopped_light(self):
        for params in ((255,True,4),(15,False,4),(15,True,0)):
            events,wakes = self.fixture(*params)
            events[0]['pixels'][0] = [4,16,24]
            with self.assertRaises(ValueError):
                check_trace(events,wakes)

    def test_rejects_frozen_breathing_over_a_complete_period(self):
        events,wakes = self.fixture(1)
        for i,event in enumerate(events):
            event['submitted_ms'] = 1000+i*300
        with self.assertRaises(ValueError):
            check_trace(events,wakes)


if __name__ == '__main__':
    unittest.main()
