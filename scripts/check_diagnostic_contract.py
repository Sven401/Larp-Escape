#!/usr/bin/env python3
"""Independent schema + semantic and fake lifecycle checks (jsonschema==4.23.0).

Run from any directory. No hardware, Android runtime, transport or durable IO.
"""
from copy import deepcopy
from datetime import datetime
from pathlib import Path
import json
import sys
import unittest

sys.dont_write_bytecode = True
from jsonschema import Draft202012Validator
from diagnostic_contract_model import (AppJournal, FakeFirmware, CORRELATION,
                                       canonical_bytes, export_report, import_report, verdict)

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = json.loads((ROOT / "docs/companion/diagnostic-v1.schema.json").read_text())
VALIDATOR = Draft202012Validator(SCHEMA)
FIXTURES = ROOT / "docs/companion/fixtures/diagnostic-v1"
RUN = "11111111-1111-4111-8111-111111111111"
GRANT = "22222222-2222-4222-8222-222222222222"
DEVICE = "0123456789ABCDEF0123456789ABCDEF"
BOOT = "0123456789ABCDEF"
START = "2026-10-10T12:00:00.000Z"
END = "2026-10-10T12:00:02.000Z"


def request(operation="wake_capture", step=1):
    return dict(schemaVersion=1, kind="diagnostic_request", runId=RUN, stepId=step,
                deviceId=DEVICE, bootId=BOOT, grantId=GRANT, operation=operation,
                timeoutMs=2000, parameters={"channel": 1} if operation == "wake_capture"
                else {"index": 0, "rgb": [16, 0, 0], "holdMs": 500})


def firmware():
    owner = FakeFirmware(DEVICE, BOOT)
    owner.authorize(RUN, GRANT, (7, 9), 1000, verified_gm=True,
                    explicit_consent=True, game_idle=True)
    return owner


def edges():
    return [dict(channel=1, active=True, rawHigh=True, deviceMs=1200),
            dict(channel=1, active=False, rawHigh=False, deviceMs=1300)]


def check_report(report):
    VALIDATOR.validate(report)
    grant = report["grant"]
    assert grant["runId"] == report["runId"]
    assert 0 < grant["expiresDeviceMs"] - grant["issuedDeviceMs"] <= 60000
    assert [fw["role"] for fw in report["firmware"]] == ["box", "receiver"]
    assert report["firmware"][0]["deviceId"] == report["deviceId"]
    for fw in report["firmware"]:
        if fw["source"] == "unavailable":
            assert fw["deviceId"] is None and fw["version"] is None
    if report["evidence"] == "device_and_user":
        assert report["firmware"][0]["source"] == "device"
        assert report["capabilities"]["source"] == "device"
        assert report["clock"]["appUtcSource"] == "system_clock"
        assert all(fw["source"] != "synthetic" for fw in report["firmware"])
    start, end = (datetime.fromisoformat(report[k]) for k in ("startedUtc", "endedUtc"))
    assert start <= end
    media_ids = {item["id"] for item in report["media"]}
    assert len(media_ids) == len(report["media"])
    previous = 0
    for step in report["steps"]:
        req, reply = step["request"], step["deviceReply"]
        assert req["stepId"] > previous
        previous = req["stepId"]
        assert all(req[k] == report[k] for k in ("runId", "deviceId", "bootId"))
        assert req["grantId"] == grant["grantId"]
        assert step["state"] != "pending", "final export cannot contain pending work"
        assert step["endedUtc"] is not None
        assert start <= datetime.fromisoformat(step["startedUtc"]) <= datetime.fromisoformat(step["endedUtc"]) <= end
        assert (reply is not None) == (step["state"] == "finished")
        if reply:
            assert all(reply[k] == req[k] for k in CORRELATION + ("operation",))
            assert grant["issuedDeviceMs"] <= reply["startedDeviceMs"] <= reply["endedDeviceMs"] < grant["expiresDeviceMs"]
            assert reply["endedDeviceMs"] - reply["startedDeviceMs"] < req["timeoutMs"]
            if req["operation"] == "led_single" and reply["outcome"] == "completed":
                assert reply["endedDeviceMs"] - reply["startedDeviceMs"] >= 500
        obs = step["observation"]
        assert set(obs["mediaIds"]) <= media_ids
        if obs["result"] != "not_observed":
            assert obs["recordedUtc"] is not None and obs["expected"] and obs["actual"]
        assert step["verdict"] == verdict(step), "stored verdict disagrees with evidence"


class ContractTests(unittest.TestCase):
    def test_schema_and_golden_roundtrip(self):
        Draft202012Validator.check_schema(SCHEMA)
        paths = sorted(FIXTURES.glob("*.json"))
        self.assertEqual({p.stem for p in paths}, {"completed", "failed", "not-tested", "timeout",
                                                 "cancelled", "disconnected", "app_restarted",
                                                 "device_restarted"})
        for path in paths:
            with self.subTest(path=path.name):
                raw = path.read_bytes()
                report = import_report(raw)
                check_report(report)
                self.assertEqual(raw, export_report(report))
                self.assertEqual(report, import_report(export_report(report)))

    def test_positive_golden_replays_both_operations(self):
        report = import_report((FIXTURES / "completed.json").read_bytes())
        owner = firmware()
        for step in report["steps"]:
            req = step["request"]
            owner.submit(req, (7, 9), 1100 if req["stepId"] == 1 else 2000)
            reply = owner.complete(1400 if req["stepId"] == 1 else 2500, wake_edges=edges())
            self.assertEqual(step["deviceReply"], reply)
            app = AppJournal(req, START)
            self.assertTrue(app.receive(reply, END))
            self.assertEqual(app.step["verdict"], "unclear")
            app.observe("matches", "expected signal", "observed signal", END)
            self.assertEqual(app.step["verdict"], "passed")

    def test_each_grant_precondition_is_required(self):
        for key in ("verified_gm", "explicit_consent", "game_idle"):
            kw = dict(verified_gm=True, explicit_consent=True, game_idle=True)
            kw[key] = False
            with self.subTest(missing=key), self.assertRaises(ValueError):
                FakeFirmware(DEVICE, BOOT).authorize(RUN, GRANT, (7, 9), 0, **kw)

    def test_grant_cannot_be_extended_or_replaced(self):
        with self.assertRaises(ValueError):
            firmware().authorize(RUN, GRANT, (7, 9), 2000, verified_gm=True,
                                 explicit_consent=True, game_idle=True)

    def test_expiry_fences_pending_led(self):
        owner = firmware()
        owner.submit(request("led_single"), (7, 9), 1100)
        owner.tick(31000)
        self.assertFalse(owner.led_on)
        self.assertIsNone(owner.grant)
        with self.assertRaises(ValueError):
            owner.submit(request("led_single", 2), (7, 9), 31001)

    def test_led_hold_has_independent_device_deadline(self):
        owner = firmware()
        owner.submit(request("led_single"), (7, 9), 1100)
        self.assertTrue(owner.led_on)
        owner.tick(1600)
        self.assertFalse(owner.led_on)
        self.assertIsNotNone(owner.pending)

    def test_step_must_fit_grant(self):
        with self.assertRaises(ValueError):
            firmware().submit(request(), (7, 9), 30000)

    def test_original_binding_required(self):
        for binding in ((7, 10), (8, 9), None):
            with self.subTest(binding=binding), self.assertRaises(ValueError):
                firmware().submit(request(), binding, 1100)

    def test_all_correlation_parts_fenced(self):
        for key in ("runId", "grantId", "deviceId", "bootId"):
            req = request()
            req[key] = "stale"
            with self.subTest(key=key), self.assertRaises(ValueError):
                firmware().submit(req, (7, 9), 1100)

    def test_duplicate_and_busy_never_execute(self):
        owner = firmware()
        owner.submit(request(), (7, 9), 1100)
        for req in (request(), request("led_single", 2)):
            with self.assertRaises(ValueError):
                owner.submit(req, (7, 9), 1200)
        owner.complete(1400, wake_edges=edges())
        with self.assertRaises(ValueError):
            owner.submit(request(), (7, 9), 1500)
        self.assertEqual(owner.executions, 1)

    def test_timeout_then_new_step_requires_explicit_new_id(self):
        owner = firmware()
        owner.submit(request("led_single"), (7, 9), 1100)
        owner.tick(3100)
        with self.assertRaises(ValueError):
            owner.submit(request("led_single"), (7, 9), 3200)
        owner.submit(request("led_single", 2), (7, 9), 3200)
        self.assertEqual(owner.executions, 2)

    def test_retire_is_idempotent_and_requires_new_grant(self):
        owner = firmware()
        owner.submit(request("led_single"), (7, 9), 1100)
        owner.retire()
        owner.retire()
        self.assertFalse(owner.led_on)
        with self.assertRaises(ValueError):
            owner.submit(request("led_single", 2), (7, 9), 1200)

    def test_every_interruption_preserves_unknown_outcome_and_late_reply(self):
        for reason in ("timeout", "cancelled", "disconnected", "app_restarted", "device_restarted"):
            owner = firmware()
            req = request("led_single")
            owner.submit(req, (7, 9), 1100)
            reply = owner.complete(1600)
            app = AppJournal(req, START)
            app.interrupt(reason, END)
            saved = json.loads(json.dumps(app.step))
            restored = AppJournal(req, START)
            restored.step = saved
            self.assertFalse(restored.receive(reply, END))
            self.assertEqual(restored.step["state"], reason)
            self.assertEqual(restored.step["verdict"], "unclear")
            self.assertEqual(len(restored.step["ignoredReplies"]), 1)
            self.assertEqual(owner.executions, 1)

    def test_foreign_and_duplicate_reply_cannot_finish_current_step(self):
        owner = firmware()
        owner.submit(request(), (7, 9), 1100)
        reply = owner.complete(1400, wake_edges=edges())
        for key in CORRELATION + ("operation",):
            app = AppJournal(request(), START)
            foreign = deepcopy(reply)
            foreign[key] = "foreign"
            self.assertFalse(app.receive(foreign, END))
            self.assertEqual(app.step["state"], "pending")
        app = AppJournal(request(), START)
        self.assertTrue(app.receive(reply, END))
        self.assertFalse(app.receive(reply, END))

    def test_wake_foreign_missing_reordered_and_overflow_edges_fail(self):
        for data in ([], edges()[:1], list(reversed(edges())),
                     edges() + [dict(channel=2, active=True, rawHigh=True, deviceMs=1350)]):
            owner = firmware()
            owner.submit(request(), (7, 9), 1100)
            reply = owner.complete(1400, wake_edges=data)
            app = AppJournal(request(), START)
            app.receive(reply, END)
            app.observe("matches", "expected", "observed", END)
            self.assertEqual(app.step["verdict"], "failed")
        reply["telemetry"] = dict(edges=edges(), overflow=True, activeLevel="high")
        app = AppJournal(request(), START)
        app.receive(reply, END)
        self.assertEqual(app.step["verdict"], "failed")

    def test_owner_error_or_user_mismatch_fails(self):
        for render_ok in (False, True):
            owner = firmware()
            owner.submit(request("led_single"), (7, 9), 1100)
            app = AppJournal(request("led_single"), START)
            app.receive(owner.complete(1600, render_ok=render_ok), END)
            app.observe("mismatch", "red LED 0 only", "no light", END)
            self.assertEqual(app.step["verdict"], "failed")

    def test_tampering_and_duplicate_json_keys_rejected(self):
        data = (FIXTURES / "completed.json").read_bytes()
        with self.assertRaises(ValueError):
            import_report(data.replace(b'"passed"', b'"failed"', 1))
        with self.assertRaises(ValueError):
            import_report(b'{"report":{},"report":{},"sha256":"bad"}')

    def test_canonical_unicode_and_noninteger_rejection(self):
        self.assertEqual(canonical_bytes({"text": "Gr\u00fc\u00dfe \U0001f600"}),
                         b'{"text":"Gr\\u00fc\\u00dfe \\ud83d\\ude00"}')
        for invalid in (1.0, float("nan"), "\ud800", 9007199254740992):
            with self.assertRaises(ValueError):
                canonical_bytes({"value": invalid})

    def test_retired_run_and_grant_cannot_be_reauthorized(self):
        owner = firmware()
        owner.retire()
        with self.assertRaises(ValueError):
            owner.authorize(RUN, GRANT, (7, 10), 2000, verified_gm=True,
                            explicit_consent=True, game_idle=True)

    def test_raw_polarity_disagreement_cannot_pass(self):
        owner = firmware()
        owner.submit(request(), (7, 9), 1100)
        wrong = edges()
        wrong[0]["rawHigh"] = False
        app = AppJournal(request(), START)
        app.receive(owner.complete(1400, wake_edges=wrong), END)
        app.observe("matches", "expected", "observed", END)
        self.assertEqual(app.step["verdict"], "failed")

    def test_schema_rejects_privilege_injection_unknown_ops_and_bounds(self):
        report = import_report((FIXTURES / "completed.json").read_bytes())
        for key, value in (("principal", 7), ("operation", "nfc_write"),
                           ("timeoutMs", 60001), ("stepId", 0)):
            bad = deepcopy(report)
            bad["steps"][0]["request"][key] = value
            self.assertFalse(VALIDATOR.is_valid(bad))

    def test_semantics_reject_forged_success_context_and_step_reuse(self):
        report = import_report((FIXTURES / "completed.json").read_bytes())
        for mutation in (lambda r: r["steps"][0]["observation"].update(result="not_observed"),
                         lambda r: r["steps"][0]["deviceReply"].update(runId=GRANT),
                         lambda r: r["steps"][1]["request"].update(stepId=1),
                         lambda r: r.update(evidence="device_and_user"),
                         lambda r: r["grant"].update(expiresDeviceMs=999999)):
            bad = deepcopy(report)
            mutation(bad)
            with self.assertRaises(AssertionError):
                check_report(bad)


if __name__ == "__main__":
    unittest.main(verbosity=2)
