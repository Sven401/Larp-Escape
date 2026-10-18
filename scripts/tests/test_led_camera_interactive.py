"""Pure host-contract tests: no serial device or camera is opened."""
import importlib.util
import json
import hashlib
import queue
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location("led_camera_interactive", SCRIPTS / "led_camera_interactive.py")
interactive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(interactive)
TOKEN = "0123456789abcdef"


class InteractiveTests(unittest.TestCase):
    def parameters(self):
        return {"op": "start", "slot": 0, "gen": 1, "p": 1000, "valid": 1,
                "rgb": [24, 12, 2], "gain": 1000, "duration_ms": 1000, "curve": 0}

    def state(self):
        return interactive.parse_state(
            f"STATE {TOKEN} 1 400 4 4 4 1 1 3 1000 1 24 12 2 1000 1000 0 "
            "0 0 0 0 0 0 0 0 0 0 0", TOKEN)

    def test_partial_eof_waits_and_complete_append_is_read_once(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "commands.jsonl"
            tail = interactive.CommandTail(path)
            self.assertEqual(tail.read(), [])
            path.write_bytes(b'{"op":"sta')
            self.assertEqual(tail.read(), [])
            with path.open("ab") as stream:
                stream.write(b'tus"}\n{"op":"finish"}\n')
            self.assertEqual(tail.read(), [{"op": "status"}, {"op": "finish"}])
            self.assertEqual(tail.read(), [])

    def test_truncation_and_long_lines_reject(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "commands.jsonl"
            path.write_text('{"op":"status"}\n')
            tail = interactive.CommandTail(path)
            tail.read()
            path.write_text("")
            with self.assertRaisesRegex(ValueError, "truncated"):
                tail.read()
            path.write_bytes(b"x" * 4097)
            with self.assertRaisesRegex(ValueError, "4096"):
                interactive.CommandTail(path).read()

    def test_malformed_json_has_no_wire_command(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "commands.jsonl"
            path.write_text('not-json\n42\n{"op":"status"}\n')
            records = interactive.CommandTail(path).read()
            self.assertEqual(len(records), 3)
            for item in records[:2]:
                with self.assertRaises(ValueError):
                    interactive.encode_command(item, TOKEN, 1, None)
            self.assertEqual(interactive.encode_command(records[2], TOKEN, 1, None), f"STATUS {TOKEN}")

    def test_start_is_complete_and_explicit(self):
        self.assertEqual(interactive.encode_command(self.parameters(), TOKEN, 7, None),
                         f"GLOW {TOKEN} 7 0 1 start 1000 1 24 12 2 1000 1000 0")
        for field in ("gen", "p", "valid", "rgb", "gain", "duration_ms", "curve"):
            with self.subTest(field=field):
                item = self.parameters()
                del item[field]
                with self.assertRaises(ValueError):
                    interactive.encode_command(item, TOKEN, 7, None)

    def test_update_inherits_only_confirmed_state(self):
        item = {"op": "update", "slot": 0, "p": 500}
        with self.assertRaises(ValueError):
            interactive.encode_command(item, TOKEN, 4, None)
        self.assertEqual(interactive.encode_command(item, TOKEN, 4, self.state()),
                         f"GLOW {TOKEN} 4 0 1 update 500 1 24 12 2 1000 1000 0")

    def test_stop_repeats_confirmed_full_parameters(self):
        item = {"op": "stop", "slot": 0}
        self.assertEqual(interactive.encode_command(item, TOKEN, 4, self.state()),
                         f"GLOW {TOKEN} 4 0 1 stop 1000 1 24 12 2 1000 1000 0")
        item["rgb"] = [32, 32, 32]
        with self.assertRaisesRegex(ValueError, "repeat"):
            interactive.encode_command(item, TOKEN, 4, self.state())

    def test_unknown_fields_and_out_of_range_fail_before_send(self):
        for key, value in [("p", 1001), ("valid", True), ("duration_ms", 0),
                           ("curve", 2), ("rgb", [33, 0, 0]), ("typo", 2)]:
            with self.subTest(field=key):
                item = self.parameters()
                item[key] = value
                with self.assertRaises(ValueError):
                    interactive.encode_command(item, TOKEN, 1, None)

    def test_state_contains_actual_params_and_identity(self):
        state = self.state()
        self.assertEqual(state["revision"], 1)
        self.assertEqual(state["background"], [4, 4, 4])
        self.assertEqual(state["slots"][0]["rgb"], [24, 12, 2])
        self.assertEqual(state["slots"][1]["state"], 0)
        with self.assertRaises(ValueError):
            interactive.parse_state(f"STATE {TOKEN} 1", TOKEN)

    def test_frame_preserves_revision_uint32_wrap_and_rejects_bad_pixels(self):
        line = f"L {TOKEN} 4 7 4294967290 10 " + "180c02" * 23
        frame = interactive.parse_live_frame(line, TOKEN)
        self.assertEqual((frame["revision"], frame["sequence"]), (4, 7))
        self.assertEqual(frame["pixels"], [[24, 12, 2]] * 23)
        for bad in (line.replace("180c02", "ff0c02"), line[:-2],
                    line.replace("4294967290 10", "100 2201")):
            with self.assertRaises(ValueError):
                interactive.parse_live_frame(bad, TOKEN)


    def test_wrong_image_closes_port_without_start_or_control_commands(self):
        class FakeSerial:
            def __init__(self, **kwargs):
                self.is_open = False
                self.writes = []
                self.once = True
            def open(self): self.is_open = True
            def close(self): self.is_open = False
            def write(self, value): self.writes.append(value)
            def read(self, count):
                if self.once:
                    self.once = False
                    return b"LARP-CAMERA/1 23 32 GPIO15 999 wronghash\nLARP-LIVE/1 2 23 32\n"
                return b""
        device = FakeSerial()
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            binary = root / "firmware.bin"
            binary.write_bytes(b"model-binary")
            args = SimpleNamespace(firmware=binary, output=root / "run", port="FAKE")
            with patch.dict(sys.modules, {"serial": SimpleNamespace(Serial=lambda **kw: device)}):
                with self.assertRaisesRegex(RuntimeError, "Runtime image differs"):
                    interactive.run(args)
            self.assertFalse(device.is_open)
            self.assertEqual(device.writes, [b"HELLO\n"])
            result = json.loads((args.output / "capture.json").read_text())
            self.assertEqual(result["status"], "incomplete")
            self.assertIn("Runtime image differs", result["error"])


    def test_locked_preview_does_not_stop_capture_or_heartbeat(self):
        class Frame:
            shape = (4, 4, 3)
            def mean(self, axis=None): return [1, 2, 3] if axis == (0, 1) else .1
            def __ge__(self, value): return self
            def any(self, axis=None): return self
        class Cap:
            def isOpened(self): return True
            def set(self, *args): return True
            def get(self, *args): return 0
            def read(self): return True, Frame()
            def release(self): pass
        class Stop:
            count = 0
            def is_set(self):
                self.count += 1
                return self.count > 2
        def write_image(path, frame, params=None):
            Path(path).write_bytes(b"image")
            return True
        cv2 = SimpleNamespace(__version__="fake", VideoCapture=lambda *a: Cap(),
                              imwrite=write_image, VideoWriter_fourcc=lambda *a: 0)
        for name in ("CAP_DSHOW", "CAP_PROP_FRAME_WIDTH", "CAP_PROP_FRAME_HEIGHT", "CAP_PROP_FPS",
                     "CAP_PROP_AUTO_EXPOSURE", "CAP_PROP_EXPOSURE", "CAP_PROP_AUTO_WB", "CAP_PROP_GAIN",
                     "CAP_PROP_BACKLIGHT", "CAP_PROP_SHARPNESS", "CAP_PROP_GAMMA", "CAP_PROP_FOURCC",
                     "IMWRITE_PNG_COMPRESSION"):
            setattr(cv2, name, 1)
        with tempfile.TemporaryDirectory() as temp:
            messages = queue.Queue()
            config = dict(output=temp, camera=0, width=4, height=4, exposure=-7)
            with patch.dict(sys.modules, {"cv2": cv2}), patch.object(interactive.os, "replace", side_effect=PermissionError("viewer lock")):
                interactive.camera_worker(config, interactive.time.perf_counter(), Stop(), messages)
            report = json.loads((Path(temp) / "camera.json").read_text())
            self.assertEqual(report["status"], "recorded")
            self.assertEqual(len(report["frames"]), 2)
            self.assertGreaterEqual(report["preview_warning_count"], 1)
            self.assertTrue(all("sha256" in frame for frame in report["frames"]))
            types = []
            while not messages.empty(): types.append(messages.get()["type"])
            self.assertIn("frame", types)
            self.assertNotIn("error", types)

    def test_preview_metadata_replace_is_atomic_and_failure_is_warning(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "latest.json").write_text('{"old":true}')
            real_replace = interactive.os.replace
            def replace(source, destination):
                if Path(destination).name == "latest.json": raise PermissionError("metadata reader")
                return real_replace(source, destination)
            def write_image(path, frame):
                Path(path).write_bytes(b"new-image")
                return True
            with patch.object(interactive.os, "replace", side_effect=replace):
                warnings = interactive.publish_preview(root, None, {"t_ms": 42}, write_image)
            self.assertEqual(json.loads((root / "latest.json").read_text()), {"old": True})
            self.assertEqual(warnings[0]["file"], "latest.json")
            self.assertEqual((root / "latest.png").read_bytes(), b"new-image")

    def test_deadline_resolves_late_ack_or_marks_unresolved_incomplete(self):
        for late_ack in (False, True):
            with self.subTest(late_ack=late_ack), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                binary = root / "firmware.bin"
                binary.write_bytes(b"model-binary")
                command_path = root / "commands.jsonl"
                command_path.write_text(json.dumps(self.parameters()) + "\n")
                output = root / "run"
                messages = queue.Queue()
                state0 = "0 0 0 0 0 0 0 0 0 0 0"
                class Serial:
                    is_open = False
                    token = None
                    buffer = b""
                    def open(self): self.is_open = True
                    def close(self): self.is_open = False
                    def read(self, count):
                        result, self.buffer = self.buffer, b""
                        return result
                    def write(self, value):
                        line = value.decode().strip()
                        if line == "HELLO":
                            self.buffer += (f"LARP-CAMERA/1 23 32 GPIO15 12 {hashlib.md5(b'model-binary').hexdigest()}\nLARP-LIVE/1 2 23 32\n").encode()
                        elif line.startswith("LIVE "):
                            self.token = line.split()[1]
                            self.buffer += (f"LIVE {self.token} 1 0\nSTATE {self.token} 1 0 0 0 0 {state0} {state0}\n").encode()
                        elif line == "STOP":
                            if self.token is None:
                                self.buffer += b"DONE 0000000000000000\n"
                            else:
                                if late_ack:
                                    self.buffer += (f"APPLIED {self.token} 1 2 0\nSTATE {self.token} 2 0 0 0 0 1 1 1 1000 1 24 12 2 1000 1000 0 {state0}\n").encode()
                                self.buffer += (f"L {self.token} 3 0 1 2 " + "000000" * 23 + f"\nDONE {self.token}\n").encode()
                class Process:
                    alive = False
                    def start(self):
                        self.alive = True
                        messages.put({"type": "ready"})
                    def is_alive(self): return self.alive
                    def join(self, timeout):
                        self.alive = False
                        (output / "camera.json").write_text('{"status":"recorded","frames":[]}')
                    def terminate(self): self.alive = False
                device = Serial()
                args = SimpleNamespace(firmware=binary, output=output, port="FAKE", commands=command_path,
                                       seconds=.015, camera=0, width=4, height=4, exposure=-7, rois=None)
                with patch.dict(sys.modules, {"serial": SimpleNamespace(Serial=lambda **kw: device)}), \
                     patch.object(interactive.mp, "Process", return_value=Process()), \
                     patch.object(interactive.mp, "Queue", return_value=messages), \
                     patch.object(interactive.mp, "Event", return_value=SimpleNamespace(set=lambda: None)):
                    if late_ack:
                        interactive.run(args)
                    else:
                        with self.assertRaisesRegex(RuntimeError, "Trial incomplete"):
                            interactive.run(args)
                result = json.loads((output / "capture.json").read_text())
                self.assertTrue(result["final_black_confirmed"])
                self.assertEqual(result["commands"][0]["result"], "APPLIED" if late_ack else "UNRESOLVED")
                self.assertEqual(result["status"], "recorded" if late_ack else "incomplete")
                self.assertFalse(device.is_open)


if __name__ == "__main__":
    unittest.main()
