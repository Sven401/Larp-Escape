"""Statically type-checked decoded P0 projection assumed; no codec/compiler/D0/storage."""
import copy
import json
from pathlib import Path


class PlanError(ValueError):
    pass


def integer(value):
    if type(value) is not int or not -(2**31) <= value < 2**31:
        raise PlanError("integer_range")
    return value


def expr(node, state, event):
    op = node[0]
    if op == "literal":
        return node[1]
    if op == "value":
        return state["values"][node[1]]
    if op == "input":
        return event.get("value")
    if op == "port_is":
        return event.get("port") == node[1]
    if op == "fact":
        return True if node[1] in state["facts"] else None
    args = [expr(child, state, event) for child in node[1:]]
    if op == "not":
        return None if args[0] is None else not args[0]
    if op == "all":
        return False if False in args else None if None in args else True
    if op == "any":
        return True if True in args else None if None in args else False
    if None in args:
        return None
    a, b = args
    if op == "add":
        return integer(integer(a) + integer(b))
    if op == "sub":
        return integer(integer(a) - integer(b))
    if type(a) is not type(b):
        raise PlanError("type")
    if op == "eq":
        return a == b
    if op == "lt":
        return a < b
    if op == "le":
        return a <= b
    if op == "gt":
        return a > b
    if op == "ge":
        return a >= b
    raise PlanError("operator")


class Model:
    def __init__(self, spec):
        self.spec = spec
        self.state = dict(values=copy.deepcopy(spec["values"]), facts=[],
                          latches=[], operations=[], emissions=[], intents=[],
                          timers={}, timer_generations={}, now=0)

    def apply(self, event):
        # Transactional RAM copy only. It does not simulate a flash commit.
        if event.get("accepted", True) is not True:
            return "rejected"
        if event["id"] in self.state["operations"]:
            return "duplicate"
        old = copy.deepcopy(self.state)
        if event["kind"] == "fact":
            if event["subject"] not in old["facts"]:
                old["facts"].append(event["subject"])
        new = copy.deepcopy(old)
        if event["kind"] == "timer":
            new["timers"].pop(event["timer"])
        writes, ports, subjects = set(), set(), set()
        actions = 0
        for rule in sorted(self.spec["rules"], key=lambda r: (r["priority"], r["id"])):
            trigger = rule["trigger"]
            matches = trigger[0] == event["kind"] and (
                event.get("port") in trigger[1] if trigger[0] == "local" else
                trigger[1] == event.get("timer") if trigger[0] == "timer" else
                event.get("subject") in trigger[1])
            if not matches or rule["id"] in old["latches"]:
                continue
            if expr(rule["guard"], old, event) is not True:
                continue
            if rule["mode"] == "once":
                new["latches"].append(rule["id"])
            for action in rule["actions"]:
                actions += 1
                if actions > self.spec.get("plan_limit", 32):
                    raise PlanError("plan_budget")
                op, target = action[:2]
                if op in ("set", "add", "arm", "cancel"):
                    key = ("timer" if op in ("arm", "cancel") else "value", target)
                    if key in writes:
                        raise PlanError("write_conflict")
                    writes.add(key)
                if op in ("set", "add"):
                    result = expr(action[2], old, event)
                    if op == "add":
                        result = integer(integer(old["values"][target]) + integer(result))
                    if result is None or type(result) is not type(old["values"][target]):
                        raise PlanError("type")
                    if type(result) is int:
                        integer(result)
                    new["values"][target] = result
                elif op == "arm":
                    if target in new["timers"]:
                        raise PlanError("timer_active")
                    generation = new["timer_generations"].get(target, 0) + 1
                    new["timer_generations"][target] = generation
                    new["timers"][target] = [new["now"] + self.spec["timers"][target], generation]
                elif op == "cancel":
                    new["timers"].pop(target, None)
                elif op == "emit":
                    if target in subjects:
                        raise PlanError("subject_conflict")
                    subjects.add(target)
                    if target not in new["emissions"]:
                        new["emissions"].append(target)
                elif op == "effect":
                    if target in ports:
                        raise PlanError("resource_conflict")
                    ports.add(target)
                    new["intents"].append(target)
                else:
                    raise PlanError("action")
        if len(new["intents"]) > self.spec.get("intent_capacity", 64):
            raise PlanError("capacity")
        new["operations"].append(event["id"])
        self.state = new
        return "applied"

    def at(self, now, event=None):
        if now < self.state["now"]:
            raise PlanError("time_backwards")
        self.state["now"] = now
        due = sorted((deadline, name, generation)
                     for name, (deadline, generation) in self.state["timers"].items()
                     if deadline <= now)
        for _, name, generation in due:
            self.apply(dict(kind="timer", timer=name, id=f"timer:{name}:{generation}"))
        if event:
            return self.apply(event)
        return "advanced"


def check_fixtures():
    path = Path(__file__).resolve().parents[1] / "docs/spielpakete/fixtures/p0-v1.json"
    cases = json.loads(path.read_text(encoding="utf-8"))
    for case in cases["traces"]:
        model = Model(case["spec"])
        for step in case["steps"]:
            model.at(step["at"], step.get("event"))
            for key, expected in step["expect"].items():
                assert model.state[key] == expected, (case["id"], key, model.state[key], expected)
    return len(cases["traces"])


if __name__ == "__main__":
    print(f"P0: {check_fixtures()} decoded reference traces passed (no device/codec proof)")
