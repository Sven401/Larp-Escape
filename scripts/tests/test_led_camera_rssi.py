"""RSSI adapter and single-owner integration checks. All devices are fakes."""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import queue
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
from led_camera_rssi import RssiAdapter, map_level, normalize_peer
import led_camera_interactive as host

TOKEN = "0123456789abcdef"
PEER = "a842e3ceded8"


def state(p=0, valid=0, lifecycle=1, generation=1):
    return {"revision": 3, "slots": [
        {"state": lifecycle, "gen": generation, "seq": 1, "p": p, "valid": valid,
         "rgb": [24, 8, 0], "gain": 1000, "duration_ms": 1000, "curve": 1},
        {"state": 0, "gen": 0, "p": 0, "valid": 0}]}


def success(adapter, sample_id=1, device_ms=1000, host_ms=20, raw=-60, filtered=-65, link=1):
    return adapter.receive(f"MEASURE {TOKEN} {sample_id} {device_ms} 0 1 {link} 1 1 {raw} {filtered} 7 {sample_id} {sample_id:032x} {PEER}", host_ms)


class RssiTests(unittest.TestCase):
    def adapter(self):
        return RssiAdapter(TOKEN, "A8:42:E3:CE:DE:D8")

    def test_mapping_uses_filtered_and_clamps_without_twinkle_fallback(self):
        self.assertEqual([map_level(p) for p in (-127, -90, -89, -65, -40, -1)], [0, 0, 20, 500, 1000, 1000])
        a = self.adapter()
        a.request(0)
        success(a, raw=-40, filtered=-65)
        a.observe_frame(1010)
        item, context = a.update(state())
        self.assertEqual((item["p"], item["valid"]), (500, 1))
        self.assertEqual(context["sample_id"], 1)

    def test_one_probe_at_a_time_and_independent_sample_ids(self):
        a = self.adapter()
        wire, sample = a.request(0)
        self.assertEqual(wire, f"RSSI {TOKEN} 1 {PEER}")
        self.assertIsNone(a.request(500))
        success(a)
        self.assertIsNone(a.request(499))
        wire, sample = a.request(500)
        self.assertEqual(sample["sample_id"], 2)
        self.assertEqual(wire, f"RSSI {TOKEN} 2 {PEER}")

    def test_failed_and_open_probes_preserve_fresh_success_until_exact_deadline(self):
        a = self.adapter()
        a.request(0)
        success(a, device_ms=1000)
        a.request(500)
        a.receive(f"MEASURE {TOKEN} 2 2000 13 1 0 0 0 0 0 0 0 {2:032x} {PEER}", 2500)
        a.observe_frame(3999)
        self.assertEqual(a.fresh()[:2], (True, 2999))
        self.assertIsNone(a.update(state(500, 1)))
        a.request(2600)  # A new open probe does not refresh the old success.
        a.observe_frame(4000)
        item, context = a.update(state(500, 1))
        self.assertEqual((item["p"], item["valid"]), (500, 0))
        self.assertEqual(context["sample_age_ms"], 3000)
        self.assertIsNone(a.update(state(500, 0)))

    def test_early_reply_link_zero_and_frame_clock_rollover(self):
        a = self.adapter()
        a.request(0)
        success(a, device_ms=0xFFFFFFF0, link=0)
        a.observe_frame(0xFFFFFFE0)
        self.assertEqual(a.fresh(), (True, 0, True))
        a.observe_frame(0x20)
        self.assertEqual(a.fresh(), (True, 48, False))
        a.observe_frame(0x10)  # Older buffered L cannot move the clock backwards.
        self.assertEqual(a.fresh()[1], 48)
        a.observe_frame((0xFFFFFFF0 + 3000) & 0xFFFFFFFF)
        self.assertFalse(a.fresh()[0])

    def test_only_active_confirmed_generation_and_manual_commands_have_priority(self):
        a = self.adapter()
        a.request(0)
        success(a)
        a.observe_frame(1005)
        for lifecycle in (0, 2, 3):
            self.assertIsNone(a.update(state(lifecycle=lifecycle)))
        self.assertIsNone(a.update(None))
        self.assertIsNone(a.update(state(), glow_pending=True))
        self.assertIsNone(a.update(state(), manual_queued=True))
        item, context = a.update(state(generation=4))
        self.assertEqual((item["op"], item["gen"]), ("update", 4))
        self.assertEqual(context["observed_generation"], 4)
        self.assertIsNone(a.update(state(500, 1, generation=4)))

    def test_coalescing_after_pending_uses_newest_success(self):
        a = self.adapter()
        a.request(0)
        success(a, filtered=-80)
        a.observe_frame(1100)
        self.assertIsNone(a.update(state(), glow_pending=True))
        a.request(500)
        success(a, sample_id=2, device_ms=1200, host_ms=520, filtered=-50)
        a.observe_frame(1220)
        item, context = a.update(state(200, 1))
        self.assertEqual((item["p"], context["sample_id"]), (800, 2))

    def test_wrong_token_stale_sample_and_wrong_peer_cannot_refresh(self):
        a = self.adapter()
        a.request(0)
        line = f"MEASURE {TOKEN} 1 1000 0 1 1 1 1 -60 -65 7 1 {1:032x} {PEER}"
        self.assertEqual(a.receive(line.replace(TOKEN, "f"*16), 10)["ignored"], "other-token")
        self.assertEqual(a.receive(line.replace(f"{TOKEN} 1 ", f"{TOKEN} 2 "), 10)["ignored"], "not-pending-sample")
        before = deepcopy(a.samples)
        with self.assertRaisesRegex(ValueError, "wrong peer"):
            a.receive(line.replace(PEER, "a842e3cededa"), 10)
        self.assertEqual(a.samples, before)
        self.assertIsNone(a.last_valid)
        success(a)
        self.assertEqual(a.receive(line, 30)["ignored"], "not-pending-sample")
        self.assertEqual(a.last_valid["result_host_ms"], 20)

    def test_nonce_and_success_validation_are_atomic(self):
        a = self.adapter()
        a.request(0)
        success(a)
        a.request(500)
        for invalid in (
            f"MEASURE {TOKEN} 2 1300 0 1 1 1 1 -60 -65 7 2 {1:032x} {PEER}",
            f"MEASURE {TOKEN} 2 1300 0 1 1 1 1 0 -65 7 2 {2:032x} {PEER}",
            f"MEASURE {TOKEN} 2 1300 13 1 1 0 1 -60 -65 7 2 {2:032x} {PEER}",
        ):
            before = deepcopy(a.samples)
            with self.assertRaises(ValueError): a.receive(invalid, 520)
            self.assertEqual(a.samples, before)
            self.assertEqual(a.last_valid["sample_id"], 1)

    def test_timeout_then_cancel_is_explicit_and_missing_result_stays_unresolved(self):
        a = self.adapter()
        a.request(0)
        a.receive(f"MEASURING {TOKEN} 1 100", 2)
        a.check_timeout(3000)
        with self.assertRaises(TimeoutError): a.check_timeout(3001)
        a.receive(f"MEASURE {TOKEN} 1 3200 14 1 0 0 0 0 0 0 0 {1:032x} {PEER}", 3005)
        self.assertFalse(a.close(3010))
        self.assertEqual(a.samples[0]["status"], "cancelled")
        self.assertIn("host_timeout_ms", a.samples[0])
        self.assertIsNone(a.update(state()))
        b = self.adapter()
        b.request(0)
        self.assertTrue(b.close(100))
        self.assertEqual(b.samples[0]["status"], "unresolved")
        self.assertIsNone(b.request(1000))

    def test_rejected_admission_is_journaled_without_clearing_last_good(self):
        a = self.adapter()
        a.request(0)
        success(a)
        a.request(500)
        a.receive(f"MEASURE_REJECT {TOKEN} 2 busy 1200", 510)
        self.assertEqual(a.samples[1]["status"], "rejected")
        a.observe_frame(1250)
        self.assertTrue(a.fresh()[0])
        self.assertEqual(a.last_valid["sample_id"], 1)

    def test_peer_validation(self):
        self.assertEqual(normalize_peer("A8:42:E3:CE:DE:D8"), PEER)
        for peer in ("00:00:00:00:00:00", "FF:FF:FF:FF:FF:FF", "oops"):
            with self.assertRaises(ValueError): normalize_peer(peer)

    def test_single_owner_rssi_exchange_drives_only_updates_with_confirmed_parameters(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            binary = root / "firmware.bin"
            binary.write_bytes(b"model-binary")
            command_path = root / "commands.jsonl"
            command_path.write_text(json.dumps({"op":"start","slot":0,"gen":1,"p":0,"valid":0,
                "rgb":[24,8,0],"gain":1000,"duration_ms":1000,"curve":1}) + "\n")
            output = root / "run"
            messages = queue.Queue()
            empty = "0 0 0 0 0 24 12 2 1000 1000 0"
            class Serial:
                is_open = False
                token = None
                buffer = b""
                writes = []
                revision = 1
                frame = 0
                def open(self): self.is_open = True
                def close(self): self.is_open = False
                def read(self, count):
                    result, self.buffer = self.buffer, b""
                    return result
                def write(self, value):
                    line = value.decode().strip()
                    self.writes.append(line)
                    f=line.split()
                    if line=="HELLO":
                        self.buffer += (f"LARP-CAMERA/1 23 32 GPIO15 12 {hashlib.md5(b'model-binary').hexdigest()}\nLARP-LIVE/1 2 23 32\nLARP-RSSI/1 1 2000 3000\n").encode()
                    elif f[0]=="LIVE":
                        self.token=f[1]
                        self.buffer += (f"LIVE {self.token} 1 0\nSTATE {self.token} 1 0 0 0 0 {empty} {empty}\n").encode()
                    elif f[0]=="GLOW":
                        self.revision+=1
                        slot=" ".join(["1",f[4],f[2],*f[6:]])
                        self.buffer += (f"APPLIED {self.token} {f[2]} {self.revision} 100\nSTATE {self.token} {self.revision} 100 0 0 0 {slot} {empty}\n").encode()
                        self.buffer += (f"L {self.token} {self.revision} {self.frame} 160 161 "+"000000"*23+"\n").encode()
                        self.frame+=1
                    elif f[0]=="RSSI":
                        self.buffer += (f"MEASURING {self.token} {f[2]} 105\nMEASURE {self.token} {f[2]} 130 0 1 1 1 1 -60 -65 7 1 {1:032x} {PEER}\n").encode()
                    elif line=="STOP":
                        if self.token is None:
                            self.buffer+=b"DONE 0000000000000000\n"
                        else:
                            self.buffer += (f"L {self.token} {self.revision+1} {self.frame} 180 181 "+"000000"*23+f"\nDONE {self.token}\n").encode()
            class Process:
                alive=False
                def start(self):
                    self.alive=True
                    messages.put({"type":"ready"})
                def is_alive(self): return self.alive
                def join(self, timeout):
                    self.alive=False
                    (output/"camera.json").write_text('{"status":"recorded","frames":[]}')
                def terminate(self): self.alive=False
            device=Serial()
            args=SimpleNamespace(firmware=binary,output=output,port="FAKE",commands=command_path,
                seconds=.04,camera=0,width=4,height=4,exposure=-7,rois=None,rssi_peer=PEER,rssi_slot=0)
            with patch.dict(sys.modules,{"serial":SimpleNamespace(Serial=lambda **kw:device)}), \
                 patch.object(host.mp,"Process",return_value=Process()), \
                 patch.object(host.mp,"Queue",return_value=messages), \
                 patch.object(host.mp,"Event",return_value=SimpleNamespace(set=lambda:None)):
                host.run(args)
            result=json.loads((output/"capture.json").read_text())
            glows=[line.split() for line in device.writes if line.startswith("GLOW ")]
            samples=[line.split() for line in device.writes if line.startswith("RSSI ")]
            self.assertEqual([f[2] for f in glows],["1","2"])
            self.assertEqual([f[5] for f in glows],["start","update"])
            self.assertEqual(glows[1][6:],["500","1","24","8","0","1000","1000","1"])
            self.assertEqual([f[2] for f in samples],["1"])
            self.assertEqual(result["commands"][1]["rssi"]["sample_id"],1)
            self.assertEqual(result["rssi"]["samples"][0]["filtered"],-65)
            self.assertTrue((output/"measurements.jsonl").exists())
            self.assertEqual(result["status"],"recorded")
            self.assertTrue(result["final_black_confirmed"])
            self.assertFalse(device.is_open)


    def test_host_stop_records_cancel_or_marks_missing_measurement_incomplete(self):
        for cancelled in (True, False):
            with self.subTest(cancelled=cancelled), tempfile.TemporaryDirectory() as temp:
                root=Path(temp)
                binary=root/"firmware.bin"; binary.write_bytes(b"model-binary")
                commands=root/"commands.jsonl"; commands.write_text("")
                output=root/"run"; messages=queue.Queue()
                empty="0 0 0 0 0 24 12 2 1000 1000 0"
                class Serial:
                    is_open=False; token=None; buffer=b""
                    def open(self): self.is_open=True
                    def close(self): self.is_open=False
                    def read(self,count):
                        value,self.buffer=self.buffer,b""
                        return value
                    def write(self,value):
                        line=value.decode().strip(); fields=line.split()
                        if line=="HELLO":
                            self.buffer+=(f"LARP-CAMERA/1 23 32 GPIO15 12 {hashlib.md5(b'model-binary').hexdigest()}\nLARP-LIVE/1 2 23 32\nLARP-RSSI/1 1 2000 3000\n").encode()
                        elif fields[0]=="LIVE":
                            self.token=fields[1]
                            self.buffer+=(f"LIVE {self.token} 1 0\nSTATE {self.token} 1 0 0 0 0 {empty} {empty}\n").encode()
                        elif fields[0]=="RSSI":
                            self.buffer+=(f"MEASURING {self.token} 1 100\n").encode()
                        elif line=="STOP":
                            if self.token is None: self.buffer+=b"DONE 0000000000000000\n"
                            else:
                                if cancelled:
                                    self.buffer+=(f"MEASURE {self.token} 1 200 14 1 0 0 0 0 0 0 0 {1:032x} {PEER}\n").encode()
                                self.buffer+=(f"L {self.token} 2 0 200 201 "+"000000"*23+f"\nDONE {self.token}\n").encode()
                class Process:
                    alive=False
                    def start(self):
                        self.alive=True; messages.put({"type":"ready"})
                    def is_alive(self): return self.alive
                    def join(self,timeout):
                        self.alive=False
                        (output/"camera.json").write_text('{"status":"recorded","frames":[]}')
                    def terminate(self): self.alive=False
                device=Serial()
                args=SimpleNamespace(firmware=binary,output=output,port="FAKE",commands=commands,
                    seconds=.02,camera=0,width=4,height=4,exposure=-7,rois=None,rssi_peer=PEER,rssi_slot=0)
                with patch.dict(sys.modules,{"serial":SimpleNamespace(Serial=lambda **kw:device)}), \
                     patch.object(host.mp,"Process",return_value=Process()), \
                     patch.object(host.mp,"Queue",return_value=messages), \
                     patch.object(host.mp,"Event",return_value=SimpleNamespace(set=lambda:None)):
                    if cancelled: host.run(args)
                    else:
                        with self.assertRaisesRegex(RuntimeError,"Trial incomplete"): host.run(args)
                report=json.loads((output/"capture.json").read_text())
                self.assertEqual(report["commands"],[])
                self.assertEqual(report["rssi"]["samples"][0]["status"],"cancelled" if cancelled else "unresolved")
                self.assertEqual(report["status"],"recorded" if cancelled else "incomplete")
                self.assertTrue(report["final_black_confirmed"])
                self.assertEqual(report.get("unresolved_measurements"),None if cancelled else [1])

    def test_missing_rssi_capability_prevents_live_and_measurement(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); binary=root/"firmware.bin"; binary.write_bytes(b"model-binary")
            class Serial:
                is_open=False; writes=[]; buffer=b""
                def open(self): self.is_open=True
                def close(self): self.is_open=False
                def write(self,value):
                    self.writes.append(value)
                    self.buffer+=(f"LARP-CAMERA/1 23 32 GPIO15 12 {hashlib.md5(b'model-binary').hexdigest()}\nLARP-LIVE/1 2 23 32\n").encode()
                def read(self,count):
                    value,self.buffer=self.buffer,b""
                    return value
            device=Serial(); tick=[0]
            def clock():
                tick[0]+=.1
                return tick[0]
            args=SimpleNamespace(firmware=binary,output=root/"run",port="FAKE",rssi_peer=PEER,rssi_slot=0)
            with patch.dict(sys.modules,{"serial":SimpleNamespace(Serial=lambda **kw:device)}), \
                 patch.object(host.time,"perf_counter",side_effect=clock), patch.object(host.time,"sleep"):
                with self.assertRaisesRegex(RuntimeError,"required LIVE/RSSI capability"): host.run(args)
            self.assertTrue(device.writes)
            self.assertTrue(all(line==b"HELLO\n" for line in device.writes))
            self.assertFalse(device.is_open)


if __name__ == "__main__":
    unittest.main()
