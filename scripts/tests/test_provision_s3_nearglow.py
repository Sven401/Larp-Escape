"""USB setup counterexamples; no PySerial dependency and no hardware access."""
from collections import deque
from contextlib import redirect_stderr, redirect_stdout
from copy import deepcopy
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "s3_nearglow_setup", Path(__file__).resolve().parents[1] / "provision_s3_nearglow.py")
setup = importlib.util.module_from_spec(SPEC)
# dataclasses consult the defining module on some Python releases.
import sys
sys.modules[SPEC.name] = setup
SPEC.loader.exec_module(setup)

A = "02:00:00:00:00:01"
B = "02:00:00:00:00:02"
C = "02:00:00:00:00:03"
PRIVATE = "NEVER_PRINT_PRIVATE_SERIAL_CONTENT"


def status(mac=A, peer="00:00:00:00:00:00", paired=False):
    return {"type": "info", "protocol": 1, "mac": mac, "peer": peer,
            "paired": paired, "armed": False, "persistedArmed": False,
            "durableKnown": True, "fresh": False, "ageMs": None,
            "filteredRssi": None, "level": 0, "fault": 0, "radio": True,
            "txIdle": True, "outputKnown": True, "outputPending": False,
            "black": True, "rgb": [0, 0, 0], "restartRequired": False}


class Clock:
    def __init__(self):
        self.now = 0

    def __call__(self):
        return self.now

    def sleep(self, duration):
        self.now += duration


class Device:
    def __init__(self, mac):
        self.info = status(mac)
        self.commands = []
        self.hooks = {}
        self.on_reboot = None
        self.open_failures = 0
        self.ready = False
        self.boot_lines = []

    def respond(self, command):
        op = command.split(" ", 1)[0]
        self.commands.append(command)
        if op in self.hooks:
            return self.hooks[op](command)
        if op == "INFO":
            return deepcopy(self.info)
        if op == "PAIR":
            self.info.update(peer=command.split()[1], paired=True, armed=False,
                             persistedArmed=False, restartRequired=True)
            return {"type": "ack", "op": op, "durable": True, "armed": False,
                    "restartRequired": True}
        if op == "REBOOT":
            self.info.update(restartRequired=False, radio=True)
            self.ready = True
            if self.on_reboot:
                self.on_reboot(self)
            return {"type": "ack", "op": op, "armed": False}
        if op in ("ARM", "STOP"):
            armed = op == "ARM"
            self.info.update(armed=armed, persistedArmed=armed)
            if not armed:
                self.info.update(black=True, rgb=[0, 0, 0], outputKnown=True, outputPending=False)
            return {"type": "ack", "op": op, "durable": True, "armed": armed}
        raise AssertionError("Unexpected fake operation")

    def operations(self):
        return [command.split(" ", 1)[0] for command in self.commands]


class FakeSerial:
    def __init__(self, factory, **kwargs):
        self.factory, self.kwargs = factory, kwargs
        self.port = kwargs["port"]
        self.dtr = self.rts = None
        self.queue = deque()
        self.closed = False

    def open(self):
        if self.kwargs["port"] is not None or self.dtr is not True or self.rts is not False:
            raise AssertionError("Native USB control lines were not set before open")
        self.device = self.factory.devices[self.port]
        if self.device.open_failures:
            self.device.open_failures -= 1
            raise OSError(PRIVATE)
        self.factory.opens.append(self.port)

    def close(self):
        self.closed = True

    def reset_input_buffer(self):
        self.queue.clear()

    def write(self, data):
        command = data.decode("ascii").removesuffix("\n")
        if self.factory.before_command:
            self.factory.before_command(command)
        if self.device.ready:
            self.queue.append(b'{"type":"ready","protocol":1}\n')
            self.device.ready = False
        self.queue.extend(self.device.boot_lines)
        self.device.boot_lines = []
        result = self.device.respond(command)
        if result is not None:
            self.queue.append(result if isinstance(result, bytes) else json.dumps(result).encode() + b"\n")
        return len(data)

    def flush(self):
        pass

    def readline(self, size):
        self.factory.clock.sleep(0.01 if self.queue else 0.25)
        if not self.queue:
            return b""
        line = self.queue.popleft()
        if len(line) > size:
            self.queue.appendleft(line[size:])
        return line[:size]


class Factory:
    def __init__(self):
        self.clock = Clock()
        self.devices = {"COM-A": Device(A), "COM-B": Device(B)}
        self.opens = []
        self.connections = []
        self.before_command = None

    def __call__(self, **kwargs):
        connection = FakeSerial(self, **kwargs)
        self.connections.append(connection)
        return connection

    def options(self):
        return {"factory": self, "clock": self.clock, "sleep": self.clock.sleep}


class SetupTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="s3-usb-tests-")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "private-pair.json"
        self.factory = Factory()
        self.a, self.b = self.factory.devices.values()

    def pair(self):
        return setup.pair_stations("COM-A", A, "COM-B", B, self.path, **self.factory.options())

    def paired(self, phase="paired"):
        self.a.info.update(peer=B, paired=True)
        self.b.info.update(peer=A, paired=True)
        self.path.write_text(json.dumps({"schemaVersion": 1, "protocol": 1,
            "macA": A, "macB": B, "secret": "42" * 32, "phase": phase}))

    def read_info(self, response):
        self.a.hooks["INFO"] = lambda unused: response
        return setup.station_info("COM-A", A, factory=self.factory, clock=self.factory.clock)

    def test_public_info_native_usb_and_real_booleans(self):
        self.assertEqual(self.read_info(status())["mac"], A)
        for field in setup.BOOL_FIELDS:
            with self.subTest(field=field):
                invalid = status(); invalid[field] = 1
                with self.assertRaises(setup.SetupError):
                    self.read_info(invalid)
        self.assertTrue(all(c.closed for c in self.factory.connections))

    def test_info_schema_measurement_and_output_counterexamples(self):
        invalids = [dict(status(), protocol=True), dict(status(), protocol=1.0),
                    dict(status(), mac="FF:FF:FF:FF:FF:FF"), dict(status(), peer="bad"),
                    dict(status(), paired=True), dict(status(), ageMs=True),
                    dict(status(), filteredRssi=-128), dict(status(), fresh=True),
                    dict(status(), rgb=[True, 0, 0]), dict(status(), rgb=[33, 0, 0]),
                    dict(status(), rgb=[1, 0, 0]), dict(status(), level=1001),
                    dict(status(), fault=-1), dict(status(), extra=PRIVATE)]
        missing = status(); del missing["radio"]; invalids.append(missing)
        for value in invalids:
            with self.subTest(case=len(value)):
                with self.assertRaises(setup.SetupError): self.read_info(value)
        duplicate = b'{"type":"info","type":"' + PRIVATE.encode() + b'"}\n'
        with self.assertRaises(setup.SetupError): self.read_info(duplicate)

    def test_serial_errors_oversize_partial_and_no_secret_leak(self):
        responses = [(PRIVATE * 100).encode() + b"\n", b'{"type":"info"}',
                     b'{"type":"error","reason":"' + PRIVATE.encode() + b'"}\n',
                     b'{"type":"ready","protocol":true}\n', None]
        for response in responses:
            with self.subTest(kind=type(response).__name__):
                out, err = io.StringIO(), io.StringIO()
                self.a.hooks["INFO"] = lambda unused, response=response: response
                with patch.object(setup, "_serial_factory", self.factory), redirect_stdout(out), redirect_stderr(err):
                    result = setup.main(["info", "--port", "COM-A", "--mac", A])
                self.assertEqual(result, 1)
                self.assertNotIn(PRIVATE, out.getvalue() + err.getvalue())
        self.a.open_failures = 1
        with self.assertRaises(setup.SetupError) as error:
            setup.station_info("COM-A", A, factory=self.factory, clock=self.factory.clock)
        self.assertNotIn(PRIVATE, str(error.exception))

    def test_same_ports_or_macs_and_wrong_identity_never_mutate(self):
        for ports, macs in ((("COM-A", "com-a"), (A, B)), (("COM-A", "COM-B"), (A, A))):
            with self.assertRaises(setup.SetupError):
                setup.pair_stations(*sum(zip(ports, macs), ()), self.path, **self.factory.options())
        self.a.info["mac"] = C
        with self.assertRaises(setup.SetupError): self.pair()
        self.assertFalse(self.path.exists())
        self.assertFalse(any(op in ("PAIR", "ARM", "REBOOT") for d in (self.a, self.b) for op in d.operations()))

    def test_output_must_be_new_and_outside_repository(self):
        self.path.write_text(PRIVATE)
        with self.assertRaises(setup.SetupError): self.pair()
        self.assertEqual(self.path.read_text(), PRIVATE)
        self.path.unlink()
        with patch.object(setup, "ROOT", self.path.parent):
            with self.assertRaises(setup.SetupError): self.pair()
        with self.assertRaises(setup.SetupError):
            setup.pair_stations("COM-A", A, "COM-B", B, self.path.parent / "absent" / "pair.json", **self.factory.options())
        self.assertEqual(self.factory.opens, [])

    def test_preflight_fault_unknown_storage_pending_output_reject(self):
        for field, value in (("fault", 2), ("durableKnown", False), ("armed", True),
                             ("persistedArmed", True), ("outputKnown", False),
                             ("outputPending", True), ("txIdle", False), ("restartRequired", True)):
            self.a.info = status(); self.a.info[field] = value
            with self.assertRaises(setup.SetupError): self.pair()
            self.assertNotIn("PAIR", self.a.operations())
        self.assertFalse(self.path.exists())

    def test_pair_persists_before_mutation_and_verifies_reboots(self):
        checked = []
        def before(command):
            if command.startswith("PAIR "):
                record = json.loads(self.path.read_text())
                self.assertEqual(record["phase"], "prepared")
                self.assertEqual(len(bytes.fromhex(record["secret"])), 32)
                self.assertEqual(command.split()[2], record["secret"])
                checked.append(True)
        self.factory.before_command = before
        self.a.on_reboot = lambda device: setattr(device, "open_failures", 1)
        with patch.object(setup.secrets, "token_bytes", return_value=b"\x42" * 32) as random:
            self.assertEqual(self.pair(), {"paired": True, "armed": False})
        random.assert_called_once_with(32)
        self.assertEqual(len(checked), 2)
        self.assertEqual(json.loads(self.path.read_text())["phase"], "paired")
        self.assertEqual(self.a.operations().count("PAIR"), 1)
        self.assertEqual(self.b.operations().count("PAIR"), 1)
        self.assertNotIn("ARM", self.a.operations() + self.b.operations())
        self.assertTrue(all(c.closed for c in self.factory.connections))

    def test_partial_pair_and_wrong_reboot_peer_keep_prepared_no_rekey(self):
        self.b.hooks["PAIR"] = lambda unused: {"type": "error", "op": "PAIR", "reason": "storage"}
        with self.assertRaises(setup.SetupError): self.pair()
        record = json.loads(self.path.read_text())
        self.assertEqual(record["phase"], "prepared")
        self.assertEqual(self.a.operations().count("PAIR"), 1)
        self.assertEqual(self.b.operations().count("PAIR"), 1)
        count = len(self.factory.connections)
        with self.assertRaises(setup.SetupError): self.pair()
        self.assertEqual(len(self.factory.connections), count)
        self.path.unlink(); self.factory = Factory(); self.a, self.b = self.factory.devices.values()
        self.b.on_reboot = lambda device: device.info.update(peer=C)
        with self.assertRaises(setup.SetupError): self.pair()
        self.assertEqual(json.loads(self.path.read_text())["phase"], "prepared")
        self.assertEqual(self.a.operations().count("PAIR"), 1)

    def test_pair_reboot_missing_port_bounded(self):
        self.a.on_reboot = lambda device: setattr(device, "open_failures", 1000)
        with self.assertRaises(setup.SetupError): self.pair()
        self.assertLess(self.factory.clock.now, 32)
        self.assertEqual(json.loads(self.path.read_text())["phase"], "prepared")
        self.assertEqual(self.a.operations().count("PAIR"), 1)

    def test_pair_waits_for_actual_boot_black_before_paired_phase(self):
        counts = {"after_restart": 0}
        def boot_pending(device):
            device.info.update(outputPending=True, outputKnown=False, black=False)
            def observed(unused):
                counts["after_restart"] += 1
                self.assertEqual(json.loads(self.path.read_text())["phase"], "prepared")
                if counts["after_restart"] >= 3:
                    device.info.update(outputPending=False, outputKnown=True, black=True)
                return deepcopy(device.info)
            device.hooks["INFO"] = observed
        self.a.on_reboot = boot_pending
        self.pair()
        self.assertEqual(counts["after_restart"], 3)
        self.assertEqual(json.loads(self.path.read_text())["phase"], "paired")

    def test_post_reboot_boot_text_is_bounded_and_does_not_repeat_pair(self):
        def boot_text(device):
            device.boot_lines = [b"ESP-ROM:esp32s3-20210327\n", b"I (21) boot: " + PRIVATE.encode() + b"\n"]
        self.a.on_reboot = boot_text
        self.b.on_reboot = boot_text
        self.pair()
        self.assertEqual(json.loads(self.path.read_text())["phase"], "paired")
        for device in (self.a, self.b):
            self.assertEqual(device.operations().count("PAIR"), 1)
            self.assertEqual(device.operations().count("REBOOT"), 1)
            self.assertNotIn("ARM", device.operations())

    def test_boot_text_tolerance_does_not_weaken_normal_info_or_json_checks(self):
        self.a.boot_lines = [b"ESP-ROM:" + PRIVATE.encode() + b"\n"]
        with self.assertRaises(setup.SetupError) as error:
            setup.station_info("COM-A", A, factory=self.factory, clock=self.factory.clock)
        self.assertNotIn(PRIVATE, str(error.exception))
        bad_lines = [b"{" + PRIVATE.encode() + b"\n", b"[]\n",
                     b'{"type":"error","op":"INFO","reason":"state"}\n',
                     PRIVATE.encode() * 100 + b"\n", b"ESP-ROM:incomplete"]
        for line in bad_lines:
            with self.subTest(line_length=len(line)):
                self.factory = Factory(); self.a, self.b = self.factory.devices.values()
                if self.path.exists(): self.path.unlink()
                self.a.on_reboot = lambda device, line=line: setattr(device, "boot_lines", [line])
                with self.assertRaises(setup.SetupError) as error: self.pair()
                self.assertNotIn(PRIVATE, str(error.exception))
                self.assertEqual(json.loads(self.path.read_text())["phase"], "prepared")
                self.assertEqual(self.a.operations().count("PAIR"), 1)
                self.assertEqual(self.b.operations().count("PAIR"), 1)

    def test_post_reboot_only_text_never_counts_as_ready(self):
        def no_info(device):
            device.hooks["INFO"] = lambda unused: b"ESP-ROM:" + PRIVATE.encode() + b"\n"
        self.a.on_reboot = no_info
        with self.assertRaises(setup.SetupError) as error: self.pair()
        self.assertNotIn(PRIVATE, str(error.exception))
        self.assertLess(self.factory.clock.now, 34)
        self.assertEqual(json.loads(self.path.read_text())["phase"], "prepared")
        self.assertEqual(self.a.operations().count("PAIR"), 1)

    def test_storage_failure_never_pairs_and_atomic_phase_failure_keeps_key(self):
        with patch.object(setup.os, "fsync", side_effect=OSError(PRIVATE)):
            with self.assertRaises(setup.SetupError) as error: self.pair()
        self.assertNotIn(PRIVATE, str(error.exception))
        self.assertNotIn("PAIR", self.a.operations() + self.b.operations())
        self.path.unlink()
        with patch.object(setup.os, "replace", side_effect=OSError(PRIVATE)):
            with self.assertRaises(setup.SetupError) as error: self.pair()
        self.assertNotIn(PRIVATE, str(error.exception))
        self.assertEqual(json.loads(self.path.read_text())["phase"], "prepared")
        self.assertEqual(self.a.operations().count("PAIR"), 1)
        self.assertEqual(self.b.operations().count("PAIR"), 1)

    def test_arm_requires_completed_pair_and_matching_peers(self):
        self.paired("prepared")
        with self.assertRaises(setup.SetupError):
            setup.arm_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertEqual(self.factory.opens, [])
        self.paired(); self.b.info["peer"] = C
        with self.assertRaises(setup.SetupError):
            setup.arm_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertNotIn("ARM", self.a.operations() + self.b.operations())

    def test_arm_success_and_partial_arm_attempts_stop_both(self):
        self.paired()
        self.assertEqual(setup.arm_stations("COM-A", "COM-B", self.path, **self.factory.options()),
                         {"armed": True, "durable": True})
        self.assertTrue(self.a.info["persistedArmed"] and self.b.info["persistedArmed"])
        self.a.info.update(armed=False, persistedArmed=False)
        self.b.info.update(armed=False, persistedArmed=False)
        self.b.hooks["ARM"] = lambda unused: {"type": "error", "op": "ARM", "reason": "storage"}
        with self.assertRaises(setup.SetupError) as error:
            setup.arm_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertIn("confirmed black", str(error.exception))
        self.assertIn("STOP", self.a.operations()); self.assertIn("STOP", self.b.operations())
        self.assertFalse(self.a.info["armed"] or self.b.info["armed"])

    def test_partial_arm_never_claims_safe_if_stop_black_missing(self):
        self.paired()
        self.b.hooks["STOP"] = lambda unused: {"type": "ack", "op": "STOP", "durable": True, "armed": False}
        # Output may become unavailable only after valid arming preflight.
        def fail_arm(unused):
            self.b.info.update(outputKnown=False, black=False)
            return {"type": "error", "op": "ARM", "reason": "storage"}
        self.b.hooks["ARM"] = fail_arm
        with self.assertRaises(setup.SetupError) as error:
            setup.arm_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertIn("could not be confirmed", str(error.exception))
        self.assertIn("STOP", self.a.operations()); self.assertIn("STOP", self.b.operations())

    def test_arm_ack_without_persisted_info_triggers_stop_both(self):
        self.paired()
        self.b.hooks["ARM"] = lambda unused: {"type": "ack", "op": "ARM", "durable": True, "armed": True}
        with self.assertRaises(setup.SetupError):
            setup.arm_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertIn("STOP", self.a.operations()); self.assertIn("STOP", self.b.operations())

    def test_stop_requires_durable_disarm_and_real_black_separately(self):
        self.paired("prepared")  # recovery STOP must work even after partial pairing
        self.a.info.update(armed=True, persistedArmed=True)
        self.b.info.update(armed=True, persistedArmed=True)
        def storage_failure(unused):
            self.a.info.update(armed=False, durableKnown=False, black=True, rgb=[0, 0, 0])
            return {"type": "error", "op": "STOP", "reason": "storage", "durable": False,
                    "restartPermission": "unknown"}
        self.a.hooks["STOP"] = storage_failure
        report = setup.stop_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertFalse(report.complete); self.assertFalse(report.durable_a)
        self.assertTrue(report.black_a and report.black_b)
        self.assertIn("STOP", self.b.operations())
        del self.a.hooks["STOP"]; self.a.info["durableKnown"] = True
        def pending_black(unused):
            self.b.info.update(armed=False, persistedArmed=False, outputPending=True, black=False)
            return {"type": "ack", "op": "STOP", "durable": True, "armed": False}
        self.b.hooks["STOP"] = pending_black
        report = setup.stop_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertTrue(report.durable_a and report.durable_b)
        self.assertTrue(report.black_a); self.assertFalse(report.black_b or report.complete)

    def test_stop_blank_timeout_is_failure_not_success(self):
        self.paired()
        self.a.hooks["STOP"] = lambda unused: None
        report = setup.stop_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertFalse(report.complete or report.durable_a)
        self.assertTrue(report.black_a)  # black is a separate current observation

    def test_stop_contradicted_storage_is_not_reported_durable(self):
        self.paired()
        def contradictory_ack(unused):
            self.a.info.update(durableKnown=False, outputKnown=False, black=False)
            return {"type": "ack", "op": "STOP", "durable": True, "armed": False}
        self.a.hooks["STOP"] = contradictory_ack
        report = setup.stop_stations("COM-A", "COM-B", self.path, **self.factory.options())
        self.assertFalse(report.durable_a or report.black_a or report.complete)

    def test_ack_false_integer_or_wrong_operation_rejected(self):
        for ack in ({"type": "ack", "op": "ARM", "durable": 1, "armed": True},
                    {"type": "ack", "op": "STOP", "durable": True, "armed": False},
                    {"type": "ack", "op": "ARM", "durable": True, "armed": 1}):
            with self.assertRaises(setup.SetupError): setup._validate_ack(ack, "ARM")
        with self.assertRaises(setup.SetupError):
            setup.validate_mac(A + "\nSTOP")



class BleExportTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="s3-ble-export-")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "ble-access.json"
        self.factory = Factory()
        self.a = self.factory.devices["COM-A"]
        self.a.info.update(peer=B, paired=True, armed=True, persistedArmed=True, black=False, rgb=[12,4,0])
        self.access = {"schemaVersion":1,"protocol":"LARP-PROX-BLE/1","deviceId":"101112131415161718191a1b1c1d1e1f","secret":bytes(range(32)).hex(),"name":"Larp Naeheglut","stationMac":A}
        self.a.hooks["BLE-ACCESS"] = lambda unused: deepcopy(self.access)

    def export(self):
        return setup.export_ble_access("COM-A", A, self.path, **self.factory.options())

    def test_explicit_private_export_preserves_autonomous_run(self):
        self.assertEqual(self.export(), {"exported":True,"stationMac":A})
        self.assertEqual(json.loads(self.path.read_text(encoding="utf-8")), self.access)
        self.assertEqual(self.a.operations(), ["INFO","BLE-ACCESS","INFO"])
        self.assertTrue(self.a.info["armed"])
        with self.assertRaises(setup.SetupError): self.export()
        self.assertEqual(self.a.operations(), ["INFO","BLE-ACCESS","INFO"])

    def test_bad_credential_or_changed_node_never_writes(self):
        for change in ({"schemaVersion":True},{"protocol":"wrong"},{"stationMac":B},{"secret":"00"*32},{"deviceId":"0"*32},{"extra":PRIVATE}):
            invalid = dict(self.access, **change)
            self.a.hooks["BLE-ACCESS"] = lambda unused, invalid=invalid: invalid
            with self.assertRaises(setup.SetupError): self.export()
            self.assertFalse(self.path.exists())
        self.a.hooks["BLE-ACCESS"] = lambda unused: (self.a.info.update(mac=B) or deepcopy(self.access))
        with self.assertRaises(setup.SetupError): self.export()
        self.assertFalse(self.path.exists())

    def test_no_serial_or_secret_output_on_cli_error(self):
        self.a.hooks["BLE-ACCESS"] = lambda unused: {"type":"error","reason":self.access["secret"]}
        output,error = io.StringIO(),io.StringIO()
        with patch.object(setup,"_serial_factory",self.factory),redirect_stdout(output),redirect_stderr(error):
            result=setup.main(["export-ble-access","--port","COM-A","--mac",A,"--output",str(self.path)])
        self.assertEqual(result,1)
        self.assertNotIn(self.access["secret"],output.getvalue()+error.getvalue())
        self.assertFalse(self.path.exists())

    def test_repository_destination_rejected_before_port_open(self):
        (Path(self.directory.name)/".git").mkdir()
        with self.assertRaises(setup.SetupError): self.export()
        self.assertFalse(self.factory.opens)

    def test_public_ble_info_separate_strict_schema(self):
        value={"type":"ble-info","protocol":1,"ready":True,"fault":0,"curveRevision":1,"appliedRevision":1,"curveDurableKnown":True,"points":[[-90,0],[-75,30],[-60,60],[-40,100]]}
        self.a.hooks["BLE-INFO"] = lambda unused: deepcopy(value)
        self.assertEqual(setup.station_ble_info("COM-A",A,factory=self.factory,clock=self.factory.clock),value)
        for change in ({"ready":1},{"protocol":True},{"curveRevision":0},{"secret":PRIVATE},{"points":[[-90,0],[-90,30],[-60,60],[-40,100]]}):
            invalid=dict(value,**change)
            with self.assertRaises(setup.SetupError): setup.validate_ble_info(invalid)

if __name__ == "__main__":
    unittest.main()
