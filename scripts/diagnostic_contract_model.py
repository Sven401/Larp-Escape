"""Hardware-free v1 reference model. No BLE, GPIO, authorization or durable IO.

The trusted adapter arguments below are simulated evidence, never wire fields.
This is an executable contract oracle, not a production firmware adapter.
"""
from copy import deepcopy
import hashlib
import json


CORRELATION = ("runId", "stepId", "deviceId", "bootId", "grantId")
INTERRUPTIONS = {"timeout", "cancelled", "disconnected", "app_restarted", "device_restarted"}


def canonical_bytes(value):
    # v1 canonical profile: ASCII keys, Unicode text, integer numbers only.
    def check(item):
        if item is None or isinstance(item, bool):
            return
        if isinstance(item, int) and abs(item) <= 9007199254740991:
            return
        if isinstance(item, str) and not any(0xD800 <= ord(c) <= 0xDFFF for c in item):
            return
        if isinstance(item, list):
            for child in item:
                check(child)
            return
        if isinstance(item, dict) and all(isinstance(k, str) and k.isascii() for k in item):
            for child in item.values():
                check(child)
            return
        raise ValueError("outside canonical v1 value profile")
    check(value)
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True,
                      allow_nan=False).encode("ascii")


def export_report(report):
    payload = canonical_bytes(report)
    return canonical_bytes({"report": report, "sha256": hashlib.sha256(payload).hexdigest()}) + b"\n"


def import_report(payload):
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key")
            result[key] = value
        return result
    envelope = json.loads(payload, object_pairs_hook=unique_pairs)
    if set(envelope) != {"report", "sha256"}:
        raise ValueError("invalid envelope")
    if hashlib.sha256(canonical_bytes(envelope["report"])).hexdigest() != envelope["sha256"]:
        raise ValueError("checksum mismatch")
    return envelope["report"]  # Consumer must also validate schema and semantics.


class FakeFirmware:
    """Single serialized fake owner; tick/retire are reliable lifecycle inputs."""

    def __init__(self, device_id, boot_id):
        self.device_id, self.boot_id = device_id, boot_id
        self.grant = None
        self.binding = None
        self.pending = None
        self.last_step = 0
        self.led_on = False
        self.executions = 0
        self.used_runs, self.used_grants = set(), set()  # Test oracle history only.

    def authorize(self, run_id, grant_id, binding, now, *, verified_gm=False,
                  explicit_consent=False, game_idle=False, ttl_ms=30000):
        if (self.grant is not None or not verified_gm or not explicit_consent or not game_idle
                or not binding or not 1 <= ttl_ms <= 60000 or not run_id or not grant_id
                or run_id in self.used_runs or grant_id in self.used_grants):
            raise ValueError("test grant denied")
        self.binding = binding
        self.used_runs.add(run_id)
        self.used_grants.add(grant_id)
        self.last_step = 0
        self.grant = dict(runId=run_id, grantId=grant_id, issuedDeviceMs=now,
                          expiresDeviceMs=now + ttl_ms)
        return deepcopy(self.grant)

    def retire(self):
        self.led_on = False  # Model of required safe owner cleanup, not measured light.
        self.pending = None
        self.grant = None
        self.binding = None

    def tick(self, now):
        if self.grant and now >= self.grant["expiresDeviceMs"]:
            self.retire()
        elif self.pending and now >= self.pending["deadline"]:
            self.led_on = False
            self.pending = None
        elif (self.pending and self.pending["request"]["operation"] == "led_single"
              and now >= self.pending["started"] + 500):
            self.led_on = False

    def submit(self, request, binding, now):
        self.tick(now)
        if not self.grant or binding != self.binding:
            raise ValueError("no current grant")
        if now < self.grant["issuedDeviceMs"]:
            raise ValueError("clock moved backwards")
        if (request["deviceId"] != self.device_id or request["bootId"] != self.boot_id
                or any(request[k] != self.grant[k] for k in ("runId", "grantId"))):
            raise ValueError("stale context")
        if self.pending or request["stepId"] <= self.last_step:
            raise ValueError("busy or consumed step")
        operation, parameters = request["operation"], request["parameters"]
        if operation == "wake_capture":
            if parameters != {"channel": 1}:
                raise ValueError("unsupported wake channel")
        elif operation == "led_single":
            if parameters != {"index": 0, "rgb": [16, 0, 0], "holdMs": 500}:
                raise ValueError("unsupported LED sample")
        else:
            raise ValueError("unsupported operation")
        if not 1000 <= request["timeoutMs"] <= 10000:
            raise ValueError("invalid timeout")
        deadline = now + request["timeoutMs"]
        if deadline > self.grant["expiresDeviceMs"]:
            raise ValueError("grant shorter than step")
        self.last_step = request["stepId"]
        self.pending = dict(request=deepcopy(request), started=now, deadline=deadline)
        self.led_on = operation == "led_single"
        self.executions += 1

    def complete(self, now, *, wake_edges=None, render_ok=True):
        self.tick(now)
        if not self.pending:
            raise ValueError("no pending step")
        active = self.pending
        request = active["request"]
        if now < active["started"]:
            raise ValueError("clock moved backwards")
        if request["operation"] == "led_single":
            if now < active["started"] + 500:
                raise ValueError("hold not finished")
            self.led_on = False
            telemetry = dict(renderCalls=2 if render_ok else 0, restoredOff=render_ok,
                             supplyMillivolts=None)
            outcome = "completed" if render_ok else "failed"
        else:
            telemetry = dict(edges=deepcopy(wake_edges or []), overflow=False, activeLevel="high")
            outcome = "completed"
        self.pending = None
        return dict(schemaVersion=1, kind="diagnostic_reply",
                    **{key: request[key] for key in CORRELATION},
                    operation=request["operation"], outcome=outcome,
                    reason="none" if outcome == "completed" else "owner_error",
                    startedDeviceMs=active["started"], endedDeviceMs=now,
                    telemetry=telemetry)


def telemetry_matches(request, reply):
    data = reply["telemetry"]
    if data is None:
        return False
    if request["operation"] == "led_single":
        return data["renderCalls"] == 2 and data["restoredOff"]
    edges = data["edges"]
    return (not data["overflow"] and len(edges) == 2
            and [e["channel"] for e in edges] == [1, 1]
            and [e["active"] for e in edges] == [True, False]
            and all(e["active"] == (e["rawHigh"] == (data["activeLevel"] == "high")) for e in edges)
            and reply["startedDeviceMs"] <= edges[0]["deviceMs"] < edges[1]["deviceMs"]
            <= reply["endedDeviceMs"])


def verdict(step):
    if step["state"] == "not_tested":
        return "not_tested"
    if step["state"] in INTERRUPTIONS or step["state"] == "pending":
        return "unclear"
    reply = step["deviceReply"]
    if reply is None or reply["outcome"] in {"cancelled", "rejected"}:
        return "unclear"
    if reply["outcome"] == "failed" or not telemetry_matches(step["request"], reply):
        return "failed"
    observation = step["observation"]
    if observation["result"] == "mismatch":
        return "failed"
    return "passed" if observation["result"] == "matches" else "unclear"


class AppJournal:
    """Plain in-memory journal model; caller persists intent BEFORE sending."""

    def __init__(self, request, utc):
        self.step = dict(request=deepcopy(request), state="pending", startedUtc=utc,
                         endedUtc=None, deviceReply=None, ignoredReplies=[],
                         observation=dict(source="user", result="not_observed", expected="",
                                          actual="", recordedUtc=None, mediaIds=[]),
                         verdict="unclear")

    def receive(self, reply, utc):
        matches = all(reply[key] == self.step["request"][key] for key in CORRELATION)
        matches = matches and reply["operation"] == self.step["request"]["operation"]
        if self.step["state"] != "pending" or not matches:
            self.step["ignoredReplies"].append(deepcopy(reply))
            return False
        self.step["deviceReply"] = deepcopy(reply)
        self.step["state"] = "finished"
        self.step["endedUtc"] = utc
        self.step["verdict"] = verdict(self.step)
        return True

    def interrupt(self, reason, utc):
        if reason not in INTERRUPTIONS:
            raise ValueError("invalid interruption")
        if self.step["state"] == "pending":
            self.step["state"], self.step["endedUtc"] = reason, utc
            self.step["verdict"] = "unclear"

    def observe(self, result, expected, actual, utc):
        if result not in {"matches", "mismatch", "not_observed"}:
            raise ValueError("invalid observation")
        self.step["observation"].update(result=result, expected=expected, actual=actual,
                                        recordedUtc=utc)
        self.step["verdict"] = verdict(self.step)
