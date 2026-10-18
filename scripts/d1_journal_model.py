"""Executable D1 reference, not firmware, a D0 codec or a security verifier.

Only the Python standard library is used. SQLite provides a host transaction
oracle; it is NOT selected as an ESP32 dependency. Fact digests below hash a
canonical JSON projection, NOT the D0 CBOR payload. Policy callbacks simulate
the already verified D0/#28 boundary. See docs/veranstaltung/d1-persistenz-v1.md.
"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import re
import sqlite3


MAX = (1 << 64) - 1
APPLICATION_ID = 0x4C443131
LIMITS = {"streams": 8, "facts": 32, "operations": 32, "latches": 64,
          "outbox": 32, "effects": 16, "localProgress": 64}
SNAPSHOT_BYTES = 128 * 1024


class Rejected(Exception):
    """A known rejection with no committed change."""


class Uncertain(Exception):
    """Commit may have happened; stop writes and inspect after recovery."""


def encoded(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=True, allow_nan=False).encode("ascii")


def digest(value):
    return hashlib.sha256(encoded(value)).hexdigest().upper()


def number(value, low=0):
    return type(value) is int and low <= value <= MAX


def ident(value, size=16):
    return (type(value) is str and re.fullmatch(r"[0-9A-F]{%d}" % (size * 2), value)
            and (size != 16 or value != "0" * 32))


def require(condition, reason):
    if not condition:
        raise Rejected(reason)


def stream_key(fact):
    return ":".join(fact[k] for k in ("event", "origin", "epoch"))


def fact_id(fact):
    return stream_key(fact) + ":%016X" % fact["sequence"]


def valid_ref(value):
    parts = value.split(":") if type(value) is str else []
    return (len(parts) == 4 and all(ident(p) for p in parts[:3]) and
            re.fullmatch(r"[0-9A-F]{16}", parts[3]) and int(parts[3], 16) > 0)


def checked_fact(value):
    f = deepcopy(value)
    fields = {"event", "origin", "epoch", "sequence", "run", "package",
              "station", "generation", "binding", "type", "subject",
              "dependencies", "unused"}
    require(type(f) is dict and set(f) == fields, "format")
    require(all(ident(f[k]) for k in
                ("event", "origin", "epoch", "run", "station", "subject")), "format")
    require(all(ident(f[k], 32) for k in ("package", "binding")), "format")
    require(number(f["sequence"], 1) and number(f["generation"], 1), "format")
    require(type(f["type"]) is int and f["type"] in (1, 2, 3), "format")
    deps = f["dependencies"]
    require(type(deps) is list and len(deps) <= 8 and all(valid_ref(d) for d in deps), "dependencies")
    require(len(set(deps)) == len(deps) and fact_id(f) not in deps and
            all(d.split(":")[0] == f["event"] for d in deps), "dependencies")
    require(type(f["unused"]) is list, "ranges")
    if f["type"] == 3:
        require(f["subject"] == f["origin"] and not deps and
                1 <= len(f["unused"]) <= 8, "ranges")
        last = 0
        for pair in f["unused"]:
            require(type(pair) is list and len(pair) == 2 and
                    all(number(n, 1) for n in pair) and
                    last < pair[0] <= pair[1] < f["sequence"], "ranges")
            last = pair[1]
    else:
        require(not f["unused"], "ranges")
    require(len(encoded(f)) <= 2048, "fact_bytes")
    return f


def checked_context(value):
    require(type(value) is dict and set(value) ==
            {"event", "box", "epoch", "run", "package", "station", "binding", "generation"}, "context")
    require(all(ident(value[k]) for k in ("event", "box", "epoch", "run", "station")), "context")
    require(ident(value["package"], 32) and ident(value["binding"], 32) and
            number(value["generation"], 1), "context")


def latch_key(context, rule):
    require(ident(rule), "rule")
    return ":".join(context[k] for k in ("event", "run", "station")) + ":" + rule


def normalize(ranges):
    result = []
    for first, last in sorted(ranges):
        if result and first <= result[-1][1] + 1:
            result[-1][1] = max(last, result[-1][1])
        else:
            result.append([first, last])
    require(len(result) <= 16, "range_capacity")
    return result


def prefix(state, key):
    s = state["streams"][key]
    intervals = deepcopy(s["unused"])
    intervals += [[r["fact"]["sequence"]] * 2 for r in state["facts"].values()
                  if stream_key(r["fact"]) == key]
    end = s["floor"]
    for first, last in sorted(intervals):
        if first > end + 1:
            break
        end = max(end, last)
    return end


class Journal:
    """One host owner. Never construct from a network-supplied database path."""

    def __init__(self, path, *, hook=None):
        self.path = Path(path).resolve()
        self.hook = hook or (lambda point: None)
        self.uncertain = False
        self.operating = False
        if self.path.exists():
            require(self.path.stat().st_size <= 256 * 4096, "backend_capacity")
        self.db = sqlite3.connect(self.path.as_uri() + "?mode=rw", uri=True,
                                  isolation_level=None, timeout=0)
        try:
            require(self.db.execute("PRAGMA user_version").fetchone()[0] == 1 and
                    self.db.execute("PRAGMA application_id").fetchone()[0] == APPLICATION_ID, "unsupported")
            require(self.db.execute("PRAGMA page_size").fetchone()[0] == 4096, "unsupported_pages")
            require(self.db.execute("PRAGMA integrity_check").fetchone()[0] == "ok", "corrupt")
            require(self.db.execute("SELECT type,name FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'").fetchall()
                    == [("table", "snapshot")], "unsupported_schema")
            self.db.execute("PRAGMA journal_mode=DELETE")
            self.db.execute("PRAGMA synchronous=FULL")
            self.db.execute("PRAGMA max_page_count=256")
            self.snapshot()
            # Claim is persisted BEFORE the caller may touch a physical owner.
            # A claim interrupted anywhere before result storage stays unknown.
            if any(e["status"] == "attempted" for e in self.snapshot()["effects"].values()):
                def recover(s):
                    for e in s["effects"].values():
                        if e["status"] == "attempted":
                            e["status"] = "unknown"
                    return "recovered_unknown"
                self._transaction(recover)
        except BaseException:
            self.db.close()
            raise

    @classmethod
    def create(cls, path, context):
        """Explicit fresh host fixture, not an automatic boot/format policy."""
        checked_context(context)
        p = Path(path).resolve()
        require(not any(Path(str(p) + suffix).exists() for suffix in
                        ("", "-journal", "-wal", "-shm")), "already_exists")
        # Exclusive creation; no existing database or companion file is removed.
        with p.open("xb"):
            pass
        db = sqlite3.connect(str(p), isolation_level=None)
        try:
            db.execute("PRAGMA page_size=4096")
            db.execute("PRAGMA max_page_count=256")
            db.execute("PRAGMA journal_mode=DELETE")
            db.execute("PRAGMA synchronous=FULL")
            db.execute("BEGIN IMMEDIATE")
            db.execute("CREATE TABLE snapshot (id INTEGER PRIMARY KEY CHECK(id=1), payload BLOB NOT NULL, digest TEXT NOT NULL)")
            state = {k: {} for k in LIMITS}
            state.update(context=deepcopy(context), issued=0, revision=0)
            raw = encoded(state)
            db.execute("INSERT INTO snapshot VALUES (1,?,?)", (raw, hashlib.sha256(raw).hexdigest()))
            db.execute("PRAGMA user_version=1")
            db.execute("PRAGMA application_id=%d" % APPLICATION_ID)
            db.commit()
        finally:
            db.close()
        return cls(p)

    def close(self):
        self.db.close()

    def snapshot(self):
        row = self.db.execute("SELECT payload,digest FROM snapshot WHERE id=1").fetchone()
        require(row is not None and len(row[0]) <= SNAPSHOT_BYTES and
                hashlib.sha256(row[0]).hexdigest() == row[1], "corrupt")
        state = json.loads(row[0])
        require(encoded(state) == row[0], "noncanonical")
        self._validate(state)
        return state

    @staticmethod
    def _validate(s):
        require(set(s) == set(LIMITS) | {"context", "issued", "revision"}, "corrupt")
        checked_context(s["context"])
        require(number(s["issued"]) and number(s["revision"]), "corrupt")
        for name, limit in LIMITS.items():
            require(type(s[name]) is dict and len(s[name]) <= limit, "capacity_" + name)
        for key, stream in s["streams"].items():
            require(len(key.split(":")) == 3 and all(ident(x) for x in key.split(":")), "corrupt")
            require(number(stream["floor"]) and type(stream["blocked"]) is bool and
                    stream["unused"] == normalize(stream["unused"]) and
                    all(number(a, 1) and number(b, 1) and a <= b < MAX
                        for a, b in stream["unused"]), "corrupt")
            c = s["context"]
            if key == ":".join(c[k] for k in ("event", "box", "epoch")):
                require(stream["floor"] <= s["issued"], "corrupt")
        for key, row in s["facts"].items():
            f = checked_fact(row["fact"])
            require(key == fact_id(f) and row["digest"] == digest(f) and
                    stream_key(f) in s["streams"] and
                    row["phase"] in ("pending", "applied", "history", "control"), "corrupt")
            stream = s["streams"][stream_key(f)]
            require(not any(a <= f["sequence"] <= b for a, b in stream["unused"]), "corrupt")
            if f["origin"] == s["context"]["box"] and f["epoch"] == s["context"]["epoch"]:
                require(f["sequence"] <= s["issued"], "corrupt")
        for key, targets in s["outbox"].items():
            require(key in s["facts"] and len(targets) <= 8 and all(ident(t) for t in targets), "corrupt")
            require(all(type(t["durable"]) is bool and type(t["rejected"]) is bool and
                        type(t["conflict"]) is bool and
                        t["stage"] in ("none", "transport_received", "durable_received", "applied",
                                       "effect_observed", "rejected", "deferred", "effect_unknown")
                        for t in targets.values()), "corrupt")
        for op in s["operations"].values():
            require(valid_ref(op["id"]) and ident(op["request"], 32) and
                    ident(op["factDigest"], 32), "corrupt")
        for key, row in s["latches"].items():
            require(len(key.split(":")) == 4 and all(ident(k) for k in key.split(":")) and
                    valid_ref(row["trigger"]) and ident(row["digest"], 32) and
                    number(row["generation"], 1), "corrupt")
        for row in s["localProgress"].values():
            require(valid_ref(row["id"]) and ident(row["digest"], 32), "corrupt")
        require(all(e["latch"] in s["latches"] and
                    e["status"] in ("prepared", "attempted", "observed", "unknown", "cancelled")
                    for e in s["effects"].values()), "corrupt")
        require(len(encoded(s)) <= SNAPSHOT_BYTES, "capacity_bytes")

    def _transaction(self, action, guard=lambda: True, expected=None):
        require(not self.uncertain, "needs_recovery")
        require(not self.operating, "busy")
        self.operating = True
        attempted = False
        try:
            self.db.execute("BEGIN IMMEDIATE")
            s = self.snapshot()
            previous = deepcopy(s)
            require(expected is None or s["revision"] == expected, "stale_revision")
            require(guard(), "trust")
            result = action(s)
            self.hook("prepared")
            if s == previous:
                require(guard(), "trust")
                self.db.rollback()
                return result
            require(s["revision"] < MAX, "revision_exhausted")
            s["revision"] += 1
            self._validate(s)
            raw = encoded(s)
            self.db.execute("UPDATE snapshot SET payload=?,digest=? WHERE id=1",
                            (raw, hashlib.sha256(raw).hexdigest()))
            self.hook("written")
            self.hook("before_commit")
            require(guard(), "trust")
            attempted = True
            self.db.commit()
            self.hook("after_commit")
            return result
        except BaseException as error:
            try:
                if self.db.in_transaction:
                    self.db.rollback()
            except sqlite3.Error:
                self.uncertain = True
                raise Uncertain("rollback failed; storage must be inspected") from error
            if attempted or isinstance(error, sqlite3.Error):
                self.uncertain = True
                raise Uncertain("commit outcome requires recovery") from error
            raise
        finally:
            self.operating = False

    @staticmethod
    def _stream(s, f):
        return s["streams"].setdefault(stream_key(f), {"floor": 0, "unused": [], "blocked": False})

    def receive(self, fact, policy):
        f = checked_fact(fact)
        def guard():
            context = self.snapshot()["context"]
            active = (f["run"], f["package"]) == (context["run"], context["package"])
            return f["event"] == context["event"] and policy(f) == ("active" if active else "history")
        def action(s):
            st = self._stream(s, f)
            require(not st["blocked"], "quarantined")
            old = s["facts"].get(fact_id(f))
            known = old["digest"] if old else next((o["factDigest"] for o in s["operations"].values()
                                                   if o["id"] == fact_id(f)), None)
            conflict = ((known is not None and known != digest(f)) or
                        any(a <= f["sequence"] <= b for a, b in st["unused"]))
            if old and not conflict:
                return old["phase"]
            if not conflict and f["sequence"] <= st["floor"]:
                return "covered_not_applied"
            if f["type"] == 3:
                conflict = conflict or any(a <= r["fact"]["sequence"] <= b
                    for a, b in f["unused"] for r in s["facts"].values()
                    if stream_key(r["fact"]) == stream_key(f))
                # Retired positions have real contents; do not relabel them unused.
                conflict = conflict or any(a <= st["floor"] and
                    not any(x <= a and min(b, st["floor"]) <= y for x, y in st["unused"])
                    for a, b in f["unused"])
            if conflict:
                st["blocked"] = True
                return "conflict"
            phase = "control" if f["type"] == 3 else "pending"
            if phase == "pending" and (f["run"], f["package"]) != (s["context"]["run"], s["context"]["package"]):
                phase = "history"
            s["facts"][fact_id(f)] = dict(fact=f, digest=digest(f), phase=phase)
            if f["type"] == 3:
                st["unused"] = normalize(st["unused"] + f["unused"])
            return "durable_received"
        return self._transaction(action, guard)

    def _publish(self, s, subject, targets, kind=1):
        c = s["context"]
        require(ident(subject) and len(targets) <= 8 and len(set(targets)) == len(targets) and
                all(ident(t) for t in targets), "targets")
        require(s["issued"] < MAX, "sequence_exhausted")
        s["issued"] += 1
        f = checked_fact(dict(event=c["event"], origin=c["box"], epoch=c["epoch"],
            sequence=s["issued"], run=c["run"], package=c["package"], station=c["station"],
            generation=c["generation"], binding=c["binding"], type=kind, subject=subject,
            dependencies=[], unused=[]))
        require(not self._stream(s, f)["blocked"], "quarantined")
        s["facts"][fact_id(f)] = dict(fact=f, digest=digest(f), phase="applied")
        progress = ":".join(c[k] for k in ("event", "run", "station")) + ":" + subject
        s["localProgress"].setdefault(progress, dict(id=fact_id(f), digest=digest(f)))
        if targets:
            s["outbox"][fact_id(f)] = {t: dict(durable=False, rejected=False, stage="none", conflict=False)
                                      for t in targets}
        self.hook("allocated")
        return fact_id(f)

    def publish(self, operation, subject, targets, authorized, *, expected_context):
        require(ident(operation), "operation")
        def action(s):
            require(all(s["context"][k] == expected_context[k] for k in
                        ("event", "run", "package")), "context")
            request = digest([expected_context, subject, sorted(targets)])
            old = s["operations"].get(operation)
            if old:
                require(old["request"] == request, "operation_conflict")
                return old["id"]
            result = self._publish(s, subject, targets)
            s["operations"][operation] = dict(id=result, request=request,
                                               factDigest=s["facts"][result]["digest"])
            return result
        return self._transaction(action, authorized)

    def _release(self, s, reference, rule, generation, ordered=True,
                 followup=None, targets=(), effect=None):
        require(number(generation, 1), "rule_generation")
        row = s["facts"][reference]
        f, c = row["fact"], s["context"]
        require(not s["streams"][stream_key(f)]["blocked"], "quarantined")
        require((f["run"], f["package"]) == (c["run"], c["package"]) and
                row["phase"] in ("pending", "applied") and f["type"] != 3, "context")
        key = latch_key(c, rule)
        if key in s["latches"]:
            return "already_released"
        for dep in f["dependencies"]:
            r = s["facts"].get(dep)
            require(r and r["phase"] in ("pending", "applied") and
                    (r["fact"]["run"], r["fact"]["package"]) == (c["run"], c["package"]), "dependency_missing")
        require(not ordered or prefix(s, stream_key(f)) >= f["sequence"], "gap")
        s["latches"][key] = dict(trigger=reference, digest=row["digest"], generation=generation)
        self.hook("latched")
        if followup is not None:
            self._publish(s, followup, targets)
        if effect is not None:
            require(ident(effect), "effect")
            eid = digest([key, effect])
            s["effects"][eid] = dict(latch=key, action=effect, status="prepared",
                                      context=deepcopy(c), source=deepcopy(f))
        self.hook("intent")
        return "applied"

    def apply_plan(self, reference, plans, policy):
        require(type(plans) is list and 1 <= len(plans) <= 8 and
                len({p["rule"] for p in plans}) == len(plans), "plan")
        def guard():
            s = self.snapshot()
            row = s["facts"].get(reference)
            return bool(row and policy(row["fact"]) == "active")
        def action(s):
            results = [self._release(s, reference, **p) for p in plans]
            s["facts"][reference]["phase"] = "applied"
            return "applied" if "applied" in results else "already_released"
        return self._transaction(action, guard)

    def apply(self, reference, rule, generation, policy, **options):
        return self.apply_plan(reference, [dict(rule=rule, generation=generation, **options)], policy)

    def override(self, operation, subject, rule, generation, authorized, *, expected_context,
                 followup=None, targets=(), effect=None):
        require(ident(operation), "operation")
        def action(s):
            require(all(s["context"][k] == expected_context[k] for k in
                        ("event", "run", "package")), "context")
            request = digest([expected_context, subject, rule, generation, followup, targets, effect])
            old = s["operations"].get(operation)
            if old:
                require(old["request"] == request, "operation_conflict")
                return "already_released"
            if latch_key(s["context"], rule) in s["latches"]:
                return "already_released"
            fid = self._publish(s, subject, (), kind=2)
            result = self._release(s, fid, rule, generation, ordered=False,
                                   followup=followup, targets=targets, effect=effect)
            s["operations"][operation] = dict(id=fid, request=request,
                                               factDigest=s["facts"][fid]["digest"])
            return result
        return self._transaction(action, authorized)

    def claim_effect(self, effect, allowed):
        def action(s):
            row = s["effects"][effect]
            c = s["context"]
            require(row["latch"].startswith(":".join(c[k] for k in ("event", "run", "station")) + ":") and
                    row["status"] == "prepared", "effect_not_repeatable")
            row["status"] = "attempted"
            return deepcopy(row)
        return self._transaction(action, allowed)

    def observe_effect(self, effect, observed):
        require(type(observed) is bool, "effect_observation")
        def action(s):
            row = s["effects"][effect]
            require(row["status"] == "attempted", "effect_not_attempted")
            row["status"] = "observed" if observed else "unknown"
            return row["status"]
        return self._transaction(action)

    def acknowledge(self, reference, fact_digest, target, origin, stage, verified):
        require(stage in ("transport_received", "durable_received", "applied", "effect_observed",
                          "rejected", "deferred", "effect_unknown"), "receipt_stage")
        def action(s):
            require(origin == target and reference in s["outbox"] and target in s["outbox"][reference] and
                    s["facts"][reference]["digest"] == fact_digest, "receipt_binding")
            row = s["outbox"][reference][target]
            durable = stage in ("durable_received", "applied", "effect_observed")
            row["rejected"] |= stage == "rejected"
            row["durable"] |= durable
            row["conflict"] |= row["rejected"] and row["durable"]
            row["stage"] = stage
            return "recorded"
        return self._transaction(action, verified)

    def compact(self, retained=()):
        """Internal proved prefix only. No externally supplied bootstrap floor."""
        require(len(retained) <= LIMITS["facts"] and all(valid_ref(r) for r in retained), "pins")
        def action(s):
            pins = set(retained)
            pins.update(dep for r in s["facts"].values() if r["phase"] == "pending"
                        for dep in r["fact"]["dependencies"])
            count = 0
            for key, stream in s["streams"].items():
                floor = prefix(s, key)
                pending = [r["fact"]["sequence"] for r in s["facts"].values()
                           if stream_key(r["fact"]) == key and r["phase"] == "pending"]
                if pending:
                    floor = min(floor, min(pending) - 1)
                stream["floor"] = max(stream["floor"], floor)
                for fid, row in list(s["facts"].items()):
                    targets = s["outbox"].get(fid, {})
                    if (stream_key(row["fact"]) == key and row["fact"]["sequence"] <= floor and
                            fid not in pins and row["phase"] != "pending" and
                            all(t["durable"] and not t["conflict"] for t in targets.values())):
                        s["outbox"].pop(fid, None)
                        del s["facts"][fid]
                        count += 1
            self.hook("compacted")
            return count
        return self._transaction(action)

    def switch_context(self, context, expected, authorized):
        checked_context(context)
        def action(s):
            old = s["context"]
            require(all(context[k] == old[k] for k in ("event", "box", "epoch", "station")), "requires_migration")
            s["context"] = deepcopy(context)
            for row in s["facts"].values():
                f = row["fact"]
                if (f["run"], f["package"]) != (context["run"], context["package"]) and row["phase"] == "pending":
                    row["phase"] = "history"
            for row in s["effects"].values():
                if row["status"] == "prepared" and old["run"] != context["run"]:
                    row["status"] = "cancelled"
            # Latches, counters, pending deliveries and effects are never reset.
            return "context_activated"
        return self._transaction(action, authorized, expected)
