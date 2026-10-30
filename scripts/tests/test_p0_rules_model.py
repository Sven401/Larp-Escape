"""Independent counterexamples for the limited P0 plan projection."""
import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from p0_rules_model import Model, PlanError, check_fixtures, expr


def spec(actions=None):
    return dict(values={"x": 0}, timers={"t": 10}, rules=[dict(
        id="r", priority=1, mode="once", trigger=["local", ["p"]],
        guard=["literal", True], actions=actions or [["set", "x", ["literal", 1]]])])


def event(id="boot:1"):
    return dict(id=id, kind="local", port="p", value=True)


def timer_rule(name, actions):
    return dict(id=f"rule_{name}", priority=1, mode="each",
                trigger=["timer", name], guard=["literal", True], actions=actions)


class ContractCases(unittest.TestCase):
    def test_reference_traces(self):
        self.assertEqual(check_fixtures(), 6)

    def test_unknown_does_not_become_success_by_negation(self):
        s = spec(); s["rules"][0]["guard"] = ["not", ["fact", "missing"]]
        m = Model(s); m.apply(event())
        self.assertEqual(m.state["values"]["x"], 0)
        self.assertEqual(m.state["latches"], [])

    def test_all_rules_read_old_snapshot(self):
        s = spec(); r = copy.deepcopy(s["rules"][0]); r["id"] = "other"
        r["guard"] = ["eq", ["value", "x"], ["literal", 1]]
        r["actions"] = [["effect", "light"]]; s["rules"].append(r)
        m = Model(s); m.apply(event())
        self.assertEqual(m.state["intents"], [])

    def test_multi_rule_conflict_rolls_back_latches_and_values(self):
        s = spec(); r = copy.deepcopy(s["rules"][0]); r["id"] = "other"
        r["priority"] = 0; s["rules"].append(r)
        m = Model(s); before = copy.deepcopy(m.state)
        with self.assertRaisesRegex(PlanError, "write_conflict"): m.apply(event())
        self.assertEqual(m.state, before)

    def test_overflow_does_not_leave_earlier_intent(self):
        m = Model(spec([["effect", "light"], ["add", "x", ["literal", 2147483648]]]))
        before = copy.deepcopy(m.state)
        with self.assertRaisesRegex(PlanError, "integer_range"): m.apply(event())
        self.assertEqual(m.state, before)

    def test_full_capacity_keeps_entire_input_uncommitted(self):
        s = spec([["set", "x", ["literal", 1]], ["effect", "light"]]); s["intent_capacity"] = 0
        m = Model(s); before = copy.deepcopy(m.state)
        with self.assertRaisesRegex(PlanError, "capacity"): m.apply(event())
        self.assertEqual(m.state, before)

    def test_plan_budget_does_not_apply_prefix(self):
        s = spec([["set", "x", ["literal", 1]], ["effect", "light"]]); s["plan_limit"] = 1
        m = Model(s); before = copy.deepcopy(m.state)
        with self.assertRaisesRegex(PlanError, "plan_budget"): m.apply(event())
        self.assertEqual(m.state, before)

    def test_once_latch_survives_changed_rule_generation(self):
        m = Model(spec()); m.apply(event()); m.spec["rules"][0]["generation"] = 2
        m.spec["rules"][0]["actions"] = [["set", "x", ["literal", 2]]]
        m.apply(event("boot:2")); self.assertEqual(m.state["values"]["x"], 1)

    def test_failed_due_timer_remains_pending(self):
        s = spec([["effect", "light"]]); s["intent_capacity"] = 0
        s["rules"][0]["trigger"] = ["timer", "t"]
        m = Model(s); m.state["timers"]["t"] = [10, 1]
        with self.assertRaisesRegex(PlanError, "capacity"): m.at(10)
        self.assertEqual(m.state["timers"], {"t": [10, 1]})
        self.assertEqual(m.state["operations"], [])

    def test_timer_without_guard_still_consumed_once(self):
        m = Model(spec()); m.state["timers"]["t"] = [10, 1]; m.at(10)
        self.assertEqual(m.state["timers"], {})
        self.assertEqual(m.state["operations"], ["timer:t:1"])

    def test_consuming_timer_can_rearm(self):
        s = spec([["arm", "t"]]); s["rules"][0]["trigger"] = ["timer", "t"]
        m = Model(s); m.state["timers"]["t"] = [10, 1]; m.state["timer_generations"]["t"] = 1
        m.at(10); self.assertEqual(m.state["timers"], {"t": [20, 2]})
        self.assertEqual(m.state["operations"], ["timer:t:1"])
        m.at(10)
        self.assertEqual(m.state["timers"], {"t": [20, 2]})
        self.assertEqual(m.state["operations"], ["timer:t:1"])

    def test_cancelled_due_timer_has_no_operation_or_effect(self):
        s = dict(values={"x": 0}, timers={"a": 10, "b": 10}, rules=[
            timer_rule("a", [["cancel", "b"]]),
            timer_rule("b", [["set", "x", ["literal", 1]],
                             ["emit", "solved"], ["effect", "light"]])])
        s["rules"][1]["mode"] = "once"
        m = Model(s)
        m.state["timers"] = {"b": [10, 1], "a": [10, 1]}
        m.state["timer_generations"] = {"a": 1, "b": 1}

        m.at(10)

        self.assertEqual(m.state["timers"], {})
        self.assertEqual(m.state["operations"], ["timer:a:1"])
        self.assertEqual(m.state["values"], {"x": 0})
        self.assertEqual(m.state["latches"], [])
        self.assertEqual(m.state["emissions"], [])
        self.assertEqual(m.state["intents"], [])

    def test_due_timers_use_deadline_then_id_order(self):
        s = dict(values={}, timers={"a": 10, "b": 10, "z": 5}, rules=[
            timer_rule(name, [["effect", name]]) for name in ("b", "a", "z")])
        m = Model(s)
        m.state["timers"] = {"b": [10, 1], "a": [10, 1], "z": [5, 1]}

        m.at(10)

        self.assertEqual(m.state["operations"], ["timer:z:1", "timer:a:1", "timer:b:1"])
        self.assertEqual(m.state["intents"], ["z", "a", "b"])

    def test_cancel_then_rearm_preserves_new_generation(self):
        s = dict(values={}, timers={"a": 10, "b": 10, "c": 10}, rules=[
            timer_rule("a", [["cancel", "c"]]),
            timer_rule("b", [["arm", "c"]]),
            timer_rule("c", [["effect", "light"]])])
        m = Model(s)
        m.state["timers"] = {name: [10, 1] for name in ("c", "b", "a")}
        m.state["timer_generations"] = {"a": 1, "b": 1, "c": 1}

        m.at(10)

        self.assertEqual(m.state["timers"], {"c": [20, 2]})
        self.assertEqual(m.state["operations"], ["timer:a:1", "timer:b:1"])
        self.assertEqual(m.state["intents"], [])
        m.at(19)
        self.assertEqual(m.state["timers"], {"c": [20, 2]})
        self.assertEqual(m.state["intents"], [])
        m.at(20)
        self.assertEqual(m.state["timers"], {})
        self.assertEqual(m.state["operations"], ["timer:a:1", "timer:b:1", "timer:c:2"])
        self.assertEqual(m.state["intents"], ["light"])

    def test_failed_later_timer_preserves_committed_prefix_and_pending_arms(self):
        s = dict(values={"x": 0}, timers={"a": 10, "b": 10, "c": 10}, rules=[
            timer_rule("a", [["add", "x", ["literal", 1]]]),
            timer_rule("b", [["cancel", "c"], ["effect", "light_b"]]),
            timer_rule("c", [["effect", "light_c"]])], intent_capacity=0)
        m = Model(s)
        m.state["timers"] = {name: [10, 1] for name in ("a", "b", "c")}
        m.state["timer_generations"] = {"a": 1, "b": 1, "c": 1}

        with self.assertRaisesRegex(PlanError, "capacity"):
            m.at(10)

        self.assertEqual(m.state["values"], {"x": 1})
        self.assertEqual(m.state["operations"], ["timer:a:1"])
        self.assertEqual(m.state["timers"], {"b": [10, 1], "c": [10, 1]})
        self.assertEqual(m.state["intents"], [])
        m.spec["intent_capacity"] = 1
        m.at(10)
        self.assertEqual(m.state["values"], {"x": 1})
        self.assertEqual(m.state["operations"], ["timer:a:1", "timer:b:1"])
        self.assertEqual(m.state["timers"], {})
        self.assertEqual(m.state["intents"], ["light_b"])

    def test_early_and_exact_deadline_are_different(self):
        s = spec(); s["rules"][0]["trigger"] = ["timer", "t"]
        m = Model(s); m.state["timers"]["t"] = [10, 1]
        m.at(9); self.assertEqual(m.state["values"]["x"], 0)
        m.at(10); self.assertEqual(m.state["values"]["x"], 1)

    def test_rejected_fact_cannot_enter_guard_snapshot(self):
        m = Model(spec()); e = dict(id="f1", kind="fact", subject="solved", accepted=False)
        self.assertEqual(m.apply(e), "rejected"); self.assertEqual(m.state["facts"], [])

    def test_replayed_each_input_is_not_a_second_increment(self):
        s = spec([["add", "x", ["literal", 1]]]); s["rules"][0]["mode"] = "each"
        m = Model(s); m.apply(event()); m.apply(event()); m.apply(event("newboot:1"))
        self.assertEqual(m.state["values"]["x"], 2)


if __name__ == "__main__":
    unittest.main()
