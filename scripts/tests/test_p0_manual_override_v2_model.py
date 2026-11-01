"""New manual-release reference traces; no third game or productive owner proof."""
import copy
import itertools
import sys
import unittest
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from p0_rules_model import PlanError, check_fixtures
from p0_manual_override_v2_model import (
    AdmittedFact, Context, GmAuthority, OverrideOwner, ReleaseModel, unique_claims,
)


def context():
    return Context("event", "run", "station", "box", "epoch", "deployment", "binding", "content", 1)


def authority():
    return GmAuthority("peer", 7, 11, 13, 5, 17, 19, 23)


def spec():
    return dict(format="larp.package.v2", ir="larp.rules.v2",
                requires={"rules_manual_override": 1}, permissions={"released": [1, 2]}, rules=[
        dict(id="/original", stable="01"*16, station="station", generation=2, mode="once", priority=10,
             trigger=["fact", ["remote"]], guard=["fact", "remote"],
             offline=dict(policy="local_fallback", rule="/manual"),
             actions=[["emit", "released"], ["effect", "light", "green"]]),
        dict(id="/manual", stable="01"*16, station="station", generation=3, mode="once", priority=20,
             trigger=["override"], guard=["literal", True], offline=dict(policy="local_continue"),
             actions=[["override", "released"], ["effect", "light", "green"]])])


def setup(s=None):
    model = ReleaseModel(s or spec(), context())
    return model, OverrideOwner(model, authority())


_PREPARE_PHASE_IDS = itertools.count(1)


def prepare(owner, now=0, request=None):
    return owner.prepare("/original", owner.view(), now,
                         request or f"prepare-phase-{next(_PREPARE_PHASE_IDS)}")


def fact(model, operation="incoming-1", subject="remote", fact_type=1):
    return AdmittedFact(model.context, operation, subject, "fact-"+operation, "digest-"+operation, fact_type)


class ManualReleaseTraces(unittest.TestCase):
    def test_six_v1_traces_are_unchanged(self):
        self.assertEqual(check_fixtures(), 6)

    def test_prepare_has_no_game_or_disk_effect(self):
        model, owner = setup()
        before, disk = copy.deepcopy(model.state), copy.deepcopy(owner.journal.disk)
        token = prepare(owner)
        self.assertEqual(model.state, before)
        self.assertEqual(owner.journal.disk, disk)
        self.assertEqual(owner.grants[token]["state"], "prepared")

    def test_manual_then_late_original_one_t2_one_effect(self):
        model, owner = setup()
        token = prepare(owner)
        self.assertEqual(owner.confirm(token, "confirm-1", 1), "committed")
        self.assertEqual(owner.apply_fact(fact(model)), "committed")
        self.assertEqual(model.state["solved"], ["released"])
        self.assertEqual(len(model.state["latches"]), 1)
        self.assertEqual([f["fact_type"] for f in model.state["emissions"]], [2])
        self.assertEqual(model.state["emissions"][0]["dependencies"], [])
        self.assertEqual([(i["port"], i["resource"]) for i in model.state["intents"]], [("light", "green")])
        proof = model.state["emissions"][0]["authorization"]
        self.assertEqual(proof.grant.authority, authority())
        self.assertEqual(proof.request, "confirm-1")
        self.assertNotIn("released", model.state["facts"])  # No recursive self-loopback.

    def test_original_wins_between_prepare_and_confirm(self):
        model, owner = setup()
        token = prepare(owner)
        owner.apply_fact(fact(model))
        before = copy.deepcopy(model.state)
        self.assertEqual(owner.confirm(token, "confirm-1", 1), "already_released")
        self.assertEqual(model.state, before)
        self.assertEqual([f["fact_type"] for f in model.state["emissions"]], [1])
        self.assertEqual(model.state["emissions"][0]["dependencies"], ["fact-incoming-1"])

    def test_original_without_connected_gm_still_works(self):
        model, owner = setup()
        owner.replace_authority(None)
        owner.apply_fact(fact(model))
        self.assertEqual(len(model.state["emissions"]), 1)
        with self.assertRaisesRegex(PlanError, "unauthorized"):
            prepare(owner)

    def test_replayed_confirm_returns_receipt_not_new_fact(self):
        model, owner = setup()
        token = prepare(owner)
        owner.confirm(token, "confirm-1", 1)
        before = copy.deepcopy(model.state)
        self.assertEqual(owner.confirm(token, "confirm-1", 1000), "committed")
        self.assertEqual(model.state, before)  # Expiry cannot undo a committed release.
        with self.assertRaisesRegex(PlanError, "request_conflict"):
            owner.confirm(token, "changed-command", 2)

    def test_two_claims_in_one_plan_are_conflict_not_winner(self):
        model, _ = setup()
        before = copy.deepcopy(model.state)
        # Typed-plan assembly seam: both variants must never be selected by a
        # valid single input, but a malformed assembly cannot silently win.
        with self.assertRaisesRegex(PlanError, "group_claim_conflict"):
            unique_claims([model.rules["/original"], model.rules["/manual"]])
        self.assertEqual(model.state, before)

    def test_pure_effect_pair_without_t2_is_not_a_group(self):
        s = spec()
        for rule in s["rules"]:
            rule["actions"] = [["effect", "light", "green"]]
        with self.assertRaisesRegex(PlanError, "group_action"):
            setup(s)

    def test_shared_id_requires_exact_group_and_actions(self):
        for edit in ("identity", "station", "action", "chain", "each", "write"):
            with self.subTest(edit=edit):
                s = spec()
                fallback = s["rules"][1]
                if edit == "identity": fallback["stable"] = "02"*16
                if edit == "station": fallback["station"] = "other"
                if edit == "action": fallback["actions"][1][2] = "red"
                if edit == "chain": fallback["offline"] = dict(policy="local_fallback", rule="/original")
                if edit == "each": fallback["mode"] = "each"
                if edit == "write": s["rules"][0]["actions"] = [["set", "v", ["literal", 1]]]
                with self.assertRaises(PlanError): setup(s)

    def test_duplicate_subject_in_group_is_rejected(self):
        s = spec()
        s["rules"][0]["actions"].append(["emit", "released"])
        s["rules"][1]["actions"].append(["override", "released"])
        with self.assertRaisesRegex(PlanError, "subject_conflict"):
            setup(s)

    def test_type2_permission_is_required_but_does_not_authorize(self):
        s = spec()
        s["permissions"]["released"] = [1]
        with self.assertRaisesRegex(PlanError, "permission"):
            setup(s)
        model, owner = setup()
        owner.replace_authority(None)
        with self.assertRaisesRegex(PlanError, "unauthorized"):
            prepare(owner)
        with self.assertRaisesRegex(PlanError, "owner_input_required"):
            model.plan(dict(kind="override", admin=True, subject="released"))

    def test_signature_checked_object_is_not_owner_admission(self):
        model, _ = setup()
        with self.assertRaisesRegex(PlanError, "owner_input_required"):
            model.plan(dict(scope="SignatureCheckedUnauthenticated", fact_type=2, subject="released"))

    def test_invalid_authority_snapshot_does_not_become_gm(self):
        for field in ("principal", "role_epoch", "connection_token", "registration", "lifecycle"):
            _, owner = setup()
            owner.replace_authority(replace(authority(), **{field: 0}))
            with self.assertRaisesRegex(PlanError, "unauthorized"):
                prepare(owner)

    def test_original_positive_guard_and_no_sample_in_override_guard(self):
        for index, guard in ((0, ["not", ["fact", "remote"]]), (1, ["input"]),
                             (1, ["port_is", "button"])):
            s = spec()
            s["rules"][index]["guard"] = guard
            with self.assertRaisesRegex(PlanError, "guard_context"):
                setup(s)

    def test_both_admitted_fact_types_can_trigger_original(self):
        for fact_type in (1, 2):
            model, owner = setup()
            owner.apply_fact(fact(model, fact_type=fact_type))
            self.assertEqual(model.state["emissions"][0]["fact_type"], 1)
        for fact_type in (0, 3):
            model, owner = setup()
            with self.assertRaisesRegex(PlanError, "fact_admission"):
                owner.apply_fact(fact(model, fact_type=fact_type))

    def test_role_revoke_and_same_peer_regrant_never_revive_token(self):
        model, owner = setup()
        token = prepare(owner)
        before = copy.deepcopy(model.state)
        owner.replace_authority(None)
        owner.replace_authority(replace(authority(), role_epoch=12))
        with self.assertRaisesRegex(PlanError, "unauthorized"):
            owner.confirm(token, "confirm-1", 1)
        self.assertEqual(model.state, before)

    def test_exact_original_connection_not_just_recycled_handle(self):
        for changes in (dict(connection_token=99), dict(lifecycle=99), dict(registry_revision=99)):
            model, owner = setup()
            token = prepare(owner)
            owner.replace_authority(replace(authority(), **changes))
            with self.assertRaisesRegex(PlanError, "unauthorized"):
                owner.confirm(token, "confirm-1", 1)
            self.assertEqual(model.state["emissions"], [])

    def test_identical_authority_value_after_owner_revision_change_is_stale(self):
        _, owner = setup()
        token = prepare(owner)
        owner.replace_authority(None)
        owner.replace_authority(authority())
        with self.assertRaisesRegex(PlanError, "unauthorized"):
            owner.confirm(token, "confirm-1", 1)

    def test_copied_context_cannot_retarget_old_grant_or_cached_receipt(self):
        for committed in (False, True):
            for field in ("run", "deployment", "binding", "station", "content"):
                model, owner = setup()
                token = prepare(owner)
                if committed: owner.confirm(token, "confirm-1", 1)
                model.context = replace(model.context, **{field: "replacement"})
                with self.assertRaisesRegex(PlanError, "context"):
                    owner.confirm(token, "confirm-1", 2)

    def test_prepare_rejects_old_catalog_view_before_looking_up_target(self):
        model, owner = setup()
        with self.assertRaisesRegex(PlanError, "stale"):
            owner.prepare("/original", replace(owner.view(), context=replace(model.context, deployment="old")), 0, "prepare-old")
        self.assertEqual(owner.grants, {})

    def test_absolute_deadline_exact_boundary_and_no_retry_refresh(self):
        _, owner = setup()
        token = prepare(owner)
        self.assertEqual(owner.confirm(token, "confirm-1", 30), "committed")
        model, owner = setup()
        token = prepare(owner)
        with self.assertRaisesRegex(PlanError, "expired"):
            owner.confirm(token, "confirm-1", 31)
        self.assertEqual(owner.confirm(token, "confirm-1", 1), "rejected")
        self.assertEqual(model.state["emissions"], [])

    def test_deadline_is_checked_at_final_owner_fence(self):
        model, owner = setup()
        token = prepare(owner)
        owner.clock = lambda: 31
        with self.assertRaisesRegex(PlanError, "expired"):
            owner.confirm(token, "confirm-1", 1)
        self.assertEqual(model.state, owner.journal.disk)
        self.assertEqual(model.state["emissions"], [])

    def test_deadline_overflow_does_not_create_grant(self):
        _, owner = setup()
        with self.assertRaisesRegex(PlanError, "clock_overflow"):
            prepare(owner, 2**64-10)
        self.assertEqual(owner.grants, {})

    def test_state_and_resource_revision_changes_invalidate_prepared_plan(self):
        for field in ("revision", "resource_revision", "capacity_revision"):
            model, owner = setup()
            token = prepare(owner)
            model.state[field] += 1
            with self.assertRaisesRegex(PlanError, "stale"):
                owner.confirm(token, "confirm-1", 1)
            self.assertEqual(model.state["emissions"], [])

    def test_prepare_replay_returns_original_token_and_deadline(self):
        model, owner = setup()
        token = prepare(owner, request="same-prepare")
        replay = prepare(owner, now=20, request="same-prepare")
        self.assertEqual(replay, token)
        self.assertEqual(owner.grants[token]["grant"].expires, 30)
        with self.assertRaisesRegex(PlanError, "expired"):
            owner.confirm(token, "confirm-1", 31)
        self.assertEqual(model.state["emissions"], [])

    def test_revoked_prepared_slots_do_not_block_new_gm(self):
        _, owner = setup()
        old = [prepare(owner) for _ in range(4)]
        with self.assertRaisesRegex(PlanError, "capacity"):
            prepare(owner)
        owner.replace_authority(replace(authority(), role_epoch=12))
        token = prepare(owner, 1)
        self.assertNotIn(token, old)
        self.assertTrue(all(owner.grants[t]["state"] == "rejected" for t in old))
        with self.assertRaisesRegex(PlanError, "unauthorized"):
            owner.confirm(old[0], "old-confirm", 1)

    def test_expired_prepared_slots_are_terminal_without_refreshing_replay(self):
        _, owner = setup()
        old = [prepare(owner, request=f"old-prepare-{i}") for i in range(4)]
        token = prepare(owner, 31)
        self.assertNotIn(token, old)
        self.assertTrue(all(owner.grants[t]["state"] == "rejected" for t in old))
        self.assertEqual(prepare(owner, 32, request="old-prepare-0"), old[0])
        self.assertEqual(owner.confirm(old[0], "old-confirm", 32), "rejected")

    def test_live_pending_slot_is_not_silently_evicted(self):
        model, owner = setup()
        owner.slots = 1
        token = prepare(owner)
        owner.confirm(token, "confirm-1", 1, cut="before_write")
        with self.assertRaisesRegex(PlanError, "capacity"):
            prepare(owner, 2)
        self.assertEqual(owner.grants[token]["state"], "pending")
        self.assertEqual(model.state["emissions"], [])

    def test_prepare_phase_id_cannot_rebind_payload_or_new_authority(self):
        model, owner = setup()
        prepare(owner, request="same-prepare")
        with self.assertRaisesRegex(PlanError, "prepare_request_conflict"):
            owner.prepare("/manual", owner.view(), 1, "same-prepare")
        owner.replace_authority(replace(authority(), role_epoch=12))
        with self.assertRaisesRegex(PlanError, "prepare_request_conflict"):
            prepare(owner, 2, request="same-prepare")

    def test_independent_owner_fences_change_without_game_state_change(self):
        for field in ("trust", "resources", "capacity", "profile"):
            model, owner = setup()
            token = prepare(owner)
            before = copy.deepcopy(model.state)
            owner.replace_fences(**{field: 2})
            self.assertEqual(model.state, before)
            self.assertEqual(owner.confirm(token, "confirm-1", 1), "rejected")
            self.assertEqual(model.state, before)

    def test_final_callback_rechecks_live_fences_not_confirm_copy(self):
        model, owner = setup()
        token = prepare(owner)
        def final_clock():
            owner.replace_fences(resources=2)
            return 1
        owner.clock = final_clock
        with self.assertRaisesRegex(PlanError, "stale"):
            owner.confirm(token, "confirm-1", 1)
        self.assertEqual(model.state["emissions"], [])

    def test_changed_rule_program_does_not_retarget_old_grant(self):
        for field, value in (("generation", 9), ("actions", [["override", "other"]])):
            model, owner = setup()
            token = prepare(owner)
            model.rules["/manual"][field] = value
            with self.assertRaisesRegex(PlanError, "stale"):
                owner.confirm(token, "confirm-1", 1)
            self.assertEqual(model.state["emissions"], [])

    def test_capacity_deferred_keeps_same_operation_and_full_plan(self):
        model, owner = setup()
        token = prepare(owner)
        before = copy.deepcopy(model.state)
        with self.assertRaisesRegex(PlanError, "capacity"):
            owner.confirm(token, "confirm-1", 1, intent_capacity=0)
        operation = owner.grants[token]["operation"]
        self.assertEqual(model.state, before)
        self.assertEqual(owner.confirm(token, "confirm-1", 2), "committed")
        self.assertEqual(model.state["emissions"][0]["operation"], operation)
        self.assertEqual(len(model.state["operations"]), 1)

    def test_known_prewrite_failure_reuses_plan_and_operation(self):
        model, owner = setup()
        token = prepare(owner)
        self.assertEqual(owner.confirm(token, "confirm-1", 1, cut="before_write"), "pending")
        operation = owner.grants[token]["operation"]
        self.assertEqual(owner.confirm(token, "confirm-1", 2), "committed")
        self.assertEqual(model.state["emissions"][0]["operation"], operation)

    def test_unknown_commit_blocks_every_retry_and_authority_change(self):
        model, owner = setup()
        token = prepare(owner)
        self.assertEqual(owner.confirm(token, "confirm-1", 1, cut="unknown_present"), "commit_unknown")
        for action in (lambda: owner.confirm(token, "confirm-1", 100), lambda: prepare(owner),
                       lambda: owner.apply_fact(fact(model)), lambda: owner.replace_authority(None)):
            with self.assertRaisesRegex(PlanError, "commit_unknown"): action()
        self.assertEqual(owner.recover(), "committed")
        self.assertEqual(owner.confirm(token, "confirm-1", 100), "committed")
        self.assertEqual(len(model.state["emissions"]), 1)

    def test_qualified_absent_recovery_does_not_auto_retry_old_operation(self):
        model, owner = setup()
        token = prepare(owner)
        self.assertEqual(owner.confirm(token, "confirm-1", 1, cut="unknown_absent"), "commit_unknown")
        operation = owner.grants[token]["operation"]
        self.assertEqual(owner.recover(), "not_committed")
        self.assertEqual(owner.confirm(token, "confirm-1", 2), "not_committed")
        self.assertEqual(model.state["emissions"], [])
        fresh = prepare(owner, 3)
        owner.confirm(fresh, "confirm-2", 4)
        self.assertNotEqual(model.state["emissions"][0]["operation"], operation)

    def test_postcrash_read_is_separate_from_reexecuting_old_confirm(self):
        model, owner = setup()
        token = prepare(owner)
        owner.confirm(token, "confirm-1", 1)
        operation = owner.grants[token]["operation"]
        owner.restart()
        owner.replace_authority(replace(authority(), role_epoch=12, connection_token=14))
        with self.assertRaisesRegex(PlanError, "unauthorized"):
            owner.confirm(token, "confirm-1", 2)
        record = owner.read_operation(operation)
        self.assertEqual(record["authorization"].grant.authority, authority())
        self.assertEqual(len(model.state["emissions"]), 1)
        with self.assertRaisesRegex(PlanError, "stopped"):
            prepare(owner, 3)

    def test_actual_owner_loss_recovers_unknown_without_ram_grant(self):
        for cut, outcome in (("unknown_present", "committed"), ("unknown_absent", "not_committed")):
            with self.subTest(cut=cut):
                model, owner = setup()
                token = prepare(owner)
                owner.confirm(token, "confirm-1", 1, cut=cut)
                operation = owner.grants[token]["operation"]
                journal = owner.journal
                restored_model = ReleaseModel(spec(), context())
                restored_owner = OverrideOwner.restore(restored_model, journal)
                self.assertEqual(restored_owner.grants, {})
                self.assertIsNone(restored_owner.authority)
                self.assertEqual(restored_owner.recover(), outcome)
                self.assertEqual(restored_model.state["mode"], "recovery_required")
                restored_owner.replace_authority(replace(authority(), role_epoch=12, connection_token=14))
                with self.assertRaisesRegex(PlanError, "unknown_token"):
                    restored_owner.confirm(token, "confirm-1", 2)
                if outcome == "committed":
                    receipt = restored_owner.read_operation(operation)
                    self.assertEqual(receipt["authorization"].grant.authority, authority())
                    self.assertEqual(len(restored_model.state["emissions"]), 1)
                else:
                    self.assertEqual(restored_model.state["emissions"], [])
                with self.assertRaisesRegex(PlanError, "stopped"):
                    prepare(restored_owner, 3)

    def test_old_prepare_view_cannot_rebind_after_process_loss(self):
        model, owner = setup()
        view = owner.view()
        token = owner.prepare("/original", view, 0, "old-prepare")
        fresh = OverrideOwner.restore(ReleaseModel(spec(), context()), owner.journal)
        fresh.replace_authority(replace(authority(), role_epoch=12, connection_token=14))
        with self.assertRaisesRegex(PlanError, "owner_incarnation"):
            fresh.prepare("/original", view, 1, "old-prepare")
        with self.assertRaisesRegex(PlanError, "unknown_token"):
            fresh.confirm(token, "old-confirm", 1)

    def test_combined_unknown_commit_and_older_attempted_effect_after_owner_loss(self):
        for cut in ("unknown_present", "unknown_absent"):
            s = spec()
            second = copy.deepcopy(s["rules"])
            second[0]["id"], second[1]["id"] = "/original2", "/manual2"
            second[0]["offline"]["rule"] = "/manual2"
            for rule in second:
                rule["stable"] = "02"*16
                rule["actions"][0][1] = "released2"
                rule["actions"][1][1] = "light2"
            s["rules"].extend(second)
            s["permissions"]["released2"] = [1, 2]
            model, owner = setup(s)
            token = prepare(owner)
            owner.confirm(token, "confirm-1", 1)
            owner.journal.attempt(model, 0)
            next_token = owner.prepare("/original2", owner.view(), 2, "prepare-2")
            owner.confirm(next_token, "confirm-2", 3, cut=cut)
            recovered = ReleaseModel(s, context())
            fresh = OverrideOwner.restore(recovered, owner.journal)
            fresh.recover()
            self.assertEqual(recovered.state["intents"][0]["state"], "unknown")
            self.assertEqual(recovered.state["mode"], "recovery_required")
            self.assertEqual(len(recovered.state["emissions"]), 2 if cut == "unknown_present" else 1)
            with self.assertRaisesRegex(PlanError, "intent_not_prepared"):
                fresh.journal.attempt(recovered, 0)

    def test_restore_cannot_attach_old_disk_to_another_context(self):
        for field in ("run", "deployment", "binding", "station", "epoch"):
            model, owner = setup()
            token = prepare(owner)
            owner.confirm(token, "confirm-1", 1, cut="unknown_present")
            wrong = ReleaseModel(spec(), replace(context(), **{field: "other"})) if field != "station" else None
            if wrong is None:
                # The group's station is statically checked too; use a fully
                # matching new station projection to reach the journal boundary.
                s = spec()
                for rule in s["rules"]: rule["station"] = "other"
                wrong = ReleaseModel(s, replace(context(), station="other"))
            with self.assertRaisesRegex(PlanError, "journal_context"):
                OverrideOwner.restore(wrong, owner.journal)
            self.assertIsNotNone(owner.journal.unknown)

    def test_old_stored_fact_cannot_join_new_fact_in_another_run(self):
        s = spec()
        s["rules"][0]["trigger"] = ["fact", ["a", "b"]]
        s["rules"][0]["guard"] = ["all", ["fact", "a"], ["fact", "b"]]
        model, owner = setup(s)
        owner.apply_fact(fact(model, subject="a"))
        newer = replace(context(), run="new-run")
        model.context = newer
        model.state["context"] = newer
        with self.assertRaisesRegex(PlanError, "fact_admission"):
            model.plan(AdmittedFact(newer, "new-input", "b", "new-b", "new-digest", 1))
        self.assertEqual(model.state["emissions"], [])

    def test_attempted_effect_becomes_unknown_without_replay_or_new_latch(self):
        model, owner = setup()
        token = prepare(owner)
        owner.confirm(token, "confirm-1", 1)
        owner.journal.attempt(model, 0)
        owner.restart()
        self.assertEqual(model.state["intents"][0]["state"], "unknown")
        self.assertEqual(len(model.state["latches"]), 1)
        with self.assertRaisesRegex(PlanError, "intent_not_prepared"):
            owner.journal.attempt(model, 0)

    def test_generation_change_keeps_shared_latch(self):
        model, owner = setup()
        owner.apply_fact(fact(model))
        model.rules["/original"]["generation"] = 8
        model.rules["/manual"]["generation"] = 9
        owner.apply_fact(fact(model, operation="incoming-2"))
        self.assertEqual(len(model.state["latches"]), 1)
        self.assertEqual(len(model.state["emissions"]), 1)
        self.assertEqual(model.state["latches"]["01"*16]["generation"], 2)

    def test_version_and_capability_gates_are_explicit(self):
        for edit in ("package", "ir", "capability"):
            s = spec()
            if edit == "package": s["format"] = "larp.package.v1"
            if edit == "ir": s["ir"] = "larp.rules.v1"
            if edit == "capability": s["requires"] = {}
            with self.assertRaisesRegex(PlanError, "version_gate"): setup(s)


if __name__ == "__main__":
    unittest.main()
