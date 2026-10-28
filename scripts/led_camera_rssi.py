"""Pure RSSI-to-NearGlow host adapter. No serial, camera, radio or effect implementation."""
import re

U32 = 0xFFFFFFFF
CAPABILITY = "LARP-RSSI/1 1 2000 3000"


def normalize_peer(value):
    if not isinstance(value, str):
        raise ValueError("RSSI peer must be a MAC address")
    text = value.replace(":", "").replace("-", "").lower()
    if not re.fullmatch(r"[0-9a-f]{12}", text) or int(text, 16) == 0 or int(text[:2], 16) & 1:
        raise ValueError("RSSI peer must be a nonzero unicast MAC address")
    return text


def uint(text):
    if not re.fullmatch(r"[0-9]+", text):
        raise ValueError("Invalid unsigned measurement field")
    value = int(text)
    if value > U32:
        raise ValueError("Measurement integer overflow")
    return value


def flag(text, maximum=1):
    value = uint(text)
    if value > maximum:
        raise ValueError("Invalid measurement flag")
    return value


def strength(text):
    if not re.fullmatch(r"-?[0-9]+", text) or not -127 <= int(text) <= 0:
        raise ValueError("Invalid RSSI strength")
    return int(text)


def map_level(filtered):
    return max(0, min(1000, 20 * (filtered + 90)))


class RssiAdapter:
    interval_ms = 500
    freshness_ms = 3000
    host_timeout_ms = 3000

    def __init__(self, token, peer, slot=0):
        if not re.fullmatch(r"[0-9a-f]{16}", token):
            raise ValueError("Invalid live token")
        if type(slot) is not int or slot not in (0, 1):
            raise ValueError("RSSI slot must be 0 or 1")
        self.token, self.peer, self.slot = token, normalize_peer(peer), slot
        self.next_id = 1
        self.pending = None
        self.samples = []
        self.last_sent_host_ms = None
        self.last_valid = None
        self.device_now = None
        self.nonces = set()
        self.closed = False

    def due(self, host_ms):
        return (not self.closed and self.pending is None and
                (self.last_sent_host_ms is None or host_ms-self.last_sent_host_ms >= self.interval_ms))

    def request(self, host_ms):
        if not self.due(host_ms):
            return None
        if self.next_id > U32:
            raise RuntimeError("RSSI sample ID exhausted")
        sample = {"sample_id": self.next_id, "peer": self.peer, "requested_host_ms": host_ms,
                  "status": "requested"}
        self.next_id += 1
        self.pending = sample
        self.samples.append(sample)
        self.last_sent_host_ms = host_ms
        return f"RSSI {self.token} {sample['sample_id']} {self.peer}", sample

    def receive(self, line, host_ms):
        fields = line.split()
        if not fields or fields[0] not in ("MEASURING", "MEASURE", "MEASURE_REJECT"):
            return None
        expected = {"MEASURING": 4, "MEASURE": 15, "MEASURE_REJECT": 5}[fields[0]]
        if len(fields) != expected:
            raise ValueError("Invalid measurement envelope")
        sample_id = uint(fields[2])
        event = {"event": fields[0], "sample_id": sample_id, "received_host_ms": host_ms,
                 "line": line}
        if fields[1] != self.token:
            return {**event, "ignored": "other-token"}
        if self.pending is None or sample_id != self.pending["sample_id"]:
            return {**event, "ignored": "not-pending-sample"}
        sample = self.pending
        if fields[0] == "MEASURING":
            sample.update(status="measuring", requested_device_ms=uint(fields[3]), measuring_host_ms=host_ms)
        elif fields[0] == "MEASURE_REJECT":
            if not re.fullmatch(r"[a-zA-Z0-9_-]{1,48}", fields[3]):
                raise ValueError("Invalid measurement rejection reason")
            sample.update(status="rejected", reason=fields[3], result_device_ms=uint(fields[4]),
                          result_host_ms=host_ms)
            self.pending = None
        else:
            device_ms, outcome = uint(fields[3]), uint(fields[4])
            if outcome not in (0, 10, 12, 13, 14):
                raise ValueError("Unknown measurement outcome")
            accepted, link = flag(fields[5]), flag(fields[6], 2)
            received, valid = flag(fields[7]), flag(fields[8])
            raw, filtered = strength(fields[9]), strength(fields[10])
            boot, count = uint(fields[11]), uint(fields[12])
            nonce, peer = fields[13].lower(), normalize_peer(fields[14])
            if not re.fullmatch(r"[0-9a-f]{32}", nonce) or peer != self.peer:
                raise ValueError("Invalid measurement nonce or wrong peer")
            success = outcome == 0 and accepted == 1 and received == 1 and valid == 1
            if outcome == 0 and (not success or raw >= 0 or filtered >= 0 or count == 0 or int(nonce, 16) == 0):
                raise ValueError("Inconsistent successful measurement")
            if outcome != 0 and valid:
                raise ValueError("Failed measurement cannot refresh RSSI")
            if int(nonce, 16) and nonce in self.nonces:
                raise ValueError("Measurement nonce reused across sample IDs")
            if int(nonce, 16):
                self.nonces.add(nonce)
            sample.update(status="cancelled" if outcome == 14 else "completed", result_device_ms=device_ms,
                          result_host_ms=host_ms, outcome=outcome, accepted=accepted, link=link,
                          received=received, rssi_valid=valid, raw=raw, filtered=filtered,
                          receiver_boot=boot, receiver_count=count, nonce=nonce)
            self.pending = None
            if success:
                sample["mapped_p"] = map_level(filtered)
                self.last_valid = sample
        return {**event, "sample": dict(sample)}

    def observe_frame(self, device_ms):
        if self.device_now is None or ((device_ms-self.device_now) & U32) < 0x80000000:
            self.device_now = device_ms

    def fresh(self):
        if self.last_valid is None or self.device_now is None:
            return False, None, False
        age = (self.device_now-self.last_valid["result_device_ms"]) & U32
        # A new MEASURE may arrive after an older L in the same serial batch.
        # Until the next L catches up, it cannot make that newer success stale.
        frame_behind = age >= 0x80000000
        if frame_behind:
            age = 0
        return age < self.freshness_ms, age, frame_behind

    def update(self, state, glow_pending=False, manual_queued=False):
        if self.closed or glow_pending or manual_queued or not state:
            return None
        slot = state["slots"][self.slot]
        if slot["state"] != 1:
            return None
        fresh, age, behind = self.fresh()
        level = self.last_valid["mapped_p"] if fresh else slot["p"]
        valid = int(fresh)
        if (slot["p"], slot["valid"]) == (level, valid):
            return None
        item = {"op": "update", "slot": self.slot, "gen": slot["gen"], "p": level, "valid": valid,
                "label": "RSSI fresh" if fresh else "RSSI unavailable or stale"}
        context = {"source": "rssi", "sample_id": self.last_valid["sample_id"] if self.last_valid else None,
                   "sample_age_ms": age, "frame_clock_behind_sample": behind,
                   "observed_generation": slot["gen"], "observed_revision": state["revision"]}
        return item, context

    def check_timeout(self, host_ms):
        if self.pending and host_ms-self.pending["requested_host_ms"] > self.host_timeout_ms:
            self.pending["host_timeout_ms"] = host_ms
            raise TimeoutError("RSSI result exceeded 3 seconds; stopping trial")

    def close(self, host_ms):
        self.closed = True
        if self.pending:
            self.pending.update(status="unresolved", closed_host_ms=host_ms,
                                reason="No final MEASURE/MEASURE_REJECT, including STOP cancellation")
            return True
        return False

    def snapshot(self):
        fresh, age, behind = self.fresh()
        return {"peer": self.peer, "slot": self.slot, "pending_sample_id": self.pending["sample_id"] if self.pending else None,
                "last_valid_sample_id": self.last_valid["sample_id"] if self.last_valid else None,
                "last_valid_filtered": self.last_valid["filtered"] if self.last_valid else None,
                "mapped_p": self.last_valid["mapped_p"] if self.last_valid else None,
                "fresh": fresh, "sample_age_ms": age, "device_frame_ms": self.device_now,
                "frame_clock_behind_sample": behind, "samples": len(self.samples), "closed": self.closed}
