"""D1 host transactions and process exits; no device, codec or power-cut proof."""
from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from d1_journal_model import (Journal, Rejected, Uncertain, MAX, SNAPSHOT_BYTES,
                              digest, encoded, fact_id, prefix, stream_key)


def ident(n, size=16):
    return ("%02X" % n) * size


def context():
    return dict(event=ident(1), box=ident(2), epoch=ident(3), run=ident(4),
                package=ident(5, 32), station=ident(6), binding=ident(7, 32), generation=1)


def fact(sequence=1, **changes):
    c = context()
    f = dict(event=c["event"], origin=ident(20), epoch=ident(21), sequence=sequence,
             run=c["run"], package=c["package"], station=ident(22), generation=1,
             binding=ident(23, 32), type=1, subject=ident(24), dependencies=[], unused=[])
    f.update(changes)
    return f


def active(f):
    # Trusted fixture observations, not a verifier or a wire authorization bit.
    return "active"


def yes():
    return True


def publish(j, operation=40, subject=41, targets=(ident(42),), expected=None):
    return j.publish(ident(operation), ident(subject), targets, yes,
                     expected_context=expected or context())


def make_effect(j):
    f = fact()
    j.receive(f, active)
    j.apply(fact_id(f), ident(30), 1, active, effect=ident(31))
    return next(iter(j.snapshot()["effects"]))


def worker(path, action, point):
    j = Journal(path)
    def crash(name):
        if name == point:
            os._exit(79)
    j.hook = crash
    if action == "publish":
        publish(j)
    elif action == "apply":
        j.apply(fact_id(fact()), ident(30), 1, active,
                followup=ident(32), targets=(ident(42),), effect=ident(31))
    elif action == "compact":
        j.compact()
    elif action == "switch":
        c = context(); c["run"] = ident(50); c["package"] = ident(51, 32)
        j.switch_context(c, j.snapshot()["revision"], yes)
    elif action == "claim":
        j.claim_effect(next(iter(j.snapshot()["effects"])), yes)
    elif action == "ack":
        s = j.snapshot(); fid = next(iter(s["outbox"]))
        j.acknowledge(fid, s["facts"][fid]["digest"], ident(42), ident(42), "durable_received", yes)
    elif action == "override":
        j.override(ident(40), ident(41), ident(30), 1, yes, expected_context=context(),
                   followup=ident(32), targets=(ident(42),), effect=ident(31))
    j.close()
    raise AssertionError("requested crash point was not reached")


class JournalCases(unittest.TestCase):
    def setUp(self):
        self.base = (Path(__file__).resolve().parents[2] / ".pio/d1-reference-tests").resolve()
        self.base.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="case-", dir=self.base)
        self.root = Path(self.temporary.name).resolve()
        # Verify the exact recursive-cleanup target before registering cleanup.
        assert self.root.is_relative_to(self.base) and self.root != self.base
        self.addCleanup(self.temporary.cleanup)
        self.path = self.root / "state.db"
        self.j = Journal.create(self.path, context())
        self.addCleanup(lambda: self.j.close() if self.j else None)

    def reopen(self):
        self.j.close()
        self.j = Journal(self.path)

    def test_explicit_creation_and_missing_store_never_format(self):
        before = self.path.read_bytes()
        with self.assertRaises(Rejected):
            Journal.create(self.path, context())
        with self.assertRaises(sqlite3.Error):
            Journal(self.root / "missing.db")
        self.assertEqual(before, self.path.read_bytes())
        self.assertFalse((self.root / "missing.db").exists())

    def test_orphan_backend_journal_blocks_fresh_creation(self):
        p = self.root / "orphan.db"
        Path(str(p) + "-journal").write_bytes(b"unresolved")
        with self.assertRaises(Rejected):
            Journal.create(p, context())
        self.assertFalse(p.exists())

    def test_publish_survives_reopen_and_duplicate_has_no_write(self):
        fid = publish(self.j)
        self.reopen()
        old = self.j.snapshot()
        self.assertEqual(fid, publish(self.j))
        self.assertEqual(old, self.j.snapshot())
        self.assertEqual(1, old["issued"])
        self.assertEqual(1, len(old["localProgress"]))
        self.assertIn(ident(42), old["outbox"][fid])

    def test_local_operation_conflict_cannot_allocate_again(self):
        publish(self.j)
        old = self.j.snapshot()
        with self.assertRaisesRegex(Rejected, "operation_conflict"):
            publish(self.j, subject=43)
        self.assertEqual(old, self.j.snapshot())

    def test_unsigned_uint64_maximum_does_not_wrap(self):
        self.j._transaction(lambda s: s.update(issued=MAX - 1))
        fid = publish(self.j)
        self.assertEqual(MAX, self.j.snapshot()["facts"][fid]["fact"]["sequence"])
        self.reopen()
        with self.assertRaisesRegex(Rejected, "sequence_exhausted"):
            publish(self.j, operation=44)
        self.assertEqual(MAX, self.j.snapshot()["issued"])

    def test_pending_duplicate_is_resumed_after_reboot(self):
        f = fact()
        self.assertEqual("durable_received", self.j.receive(f, active))
        self.reopen()
        self.assertEqual("pending", self.j.receive(f, active))
        self.assertEqual("applied", self.j.apply(fact_id(f), ident(30), 1, active))
        self.reopen()
        self.assertEqual("already_released", self.j.apply(fact_id(f), ident(30), 2, active))
        self.assertEqual(1, len(self.j.snapshot()["latches"]))

    def test_unverified_fact_changes_no_stream_or_state(self):
        old = self.j.snapshot()
        with self.assertRaisesRegex(Rejected, "trust"):
            self.j.receive(fact(), lambda f: "deny")
        self.assertEqual(old, self.j.snapshot())

    def test_same_id_different_content_quarantines_persistently(self):
        f = fact(); self.j.receive(f, active)
        self.assertEqual("conflict", self.j.receive(fact(subject=ident(25)), active))
        self.reopen()
        with self.assertRaisesRegex(Rejected, "quarantined"):
            self.j.apply(fact_id(f), ident(30), 1, active)
        self.assertEqual(digest(f), self.j.snapshot()["facts"][fact_id(f)]["digest"])

    def test_reverse_unused_conflict(self):
        closure = fact(3, type=3, subject=ident(20), unused=[[1, 2]])
        self.j.receive(closure, active)
        self.reopen()
        self.assertEqual(3, prefix(self.j.snapshot(), stream_key(closure)))
        self.assertEqual("conflict", self.j.receive(fact(), active))

    def test_unused_range_cannot_cover_received_fact(self):
        self.j.receive(fact(), active)
        self.assertEqual("conflict", self.j.receive(
            fact(3, type=3, subject=ident(20), unused=[[1, 2]]), active))

    def test_unused_duplicates_after_compaction_are_not_false_conflicts(self):
        f = fact(3, type=3, subject=ident(20), unused=[[1, 2]])
        self.j.receive(f, active)
        rev = self.j.snapshot()["revision"]
        self.assertEqual("control", self.j.receive(f, active))
        self.assertEqual(rev, self.j.snapshot()["revision"])
        self.assertEqual(1, self.j.compact())
        self.assertEqual("covered_not_applied", self.j.receive(f, active))
        self.assertEqual("conflict", self.j.receive(fact(), active))

    def test_new_closure_cannot_relabel_retired_real_fact(self):
        f = fact(); self.j.receive(f, active)
        self.j.apply(fact_id(f), ident(30), 1, active)
        self.j.compact()
        self.assertEqual("conflict", self.j.receive(
            fact(3, type=3, subject=ident(20), unused=[[1, 2]]), active))

    def test_gap_blocks_ordered_rule_but_explicit_monotone_rule_can_run(self):
        f = fact(3); self.j.receive(f, active)
        with self.assertRaisesRegex(Rejected, "gap"):
            self.j.apply(fact_id(f), ident(30), 1, active)
        self.assertEqual(0, prefix(self.j.snapshot(), stream_key(f)))
        self.j.apply(fact_id(f), ident(30), 1, active, ordered=False)

    def test_missing_dependency_is_not_invented_from_prefix_or_timeout(self):
        dep = fact(origin=ident(26)); f = fact(dependencies=[fact_id(dep)])
        self.j.receive(f, active)
        with self.assertRaisesRegex(Rejected, "dependency_missing"):
            self.j.apply(fact_id(f), ident(30), 1, active)
        self.reopen()
        self.j.receive(dep, active)
        self.j.apply(fact_id(f), ident(30), 1, active)
        self.assertEqual("applied", self.j.snapshot()["facts"][fact_id(f)]["phase"])

    def test_historical_fact_fills_stream_without_current_effect(self):
        old = fact(run=ident(50), package=ident(51, 32))
        with self.assertRaisesRegex(Rejected, "trust"):
            self.j.receive(old, active)
        self.j.receive(old, lambda f: "history")
        self.assertEqual(1, prefix(self.j.snapshot(), stream_key(old)))
        with self.assertRaises(Rejected):
            self.j.apply(fact_id(old), ident(30), 1, lambda f: "history")
        self.assertFalse(self.j.snapshot()["latches"])

    def test_history_does_not_satisfy_current_dependency(self):
        old = fact(origin=ident(26), run=ident(50), package=ident(51, 32))
        self.j.receive(old, lambda f: "history")
        f = fact(dependencies=[fact_id(old)]); self.j.receive(f, active)
        with self.assertRaisesRegex(Rejected, "dependency_missing"):
            self.j.apply(fact_id(f), ident(30), 1, active)

    def test_rule_latch_survives_different_fact_and_generation(self):
        self.j.receive(fact(), active)
        self.j.apply(fact_id(fact()), ident(30), 1, active, followup=ident(32), effect=ident(31))
        self.j.receive(fact(2), active)
        self.j.apply(fact_id(fact(2)), ident(30), 2, active, followup=ident(32), effect=ident(31))
        s = self.j.snapshot()
        self.assertEqual(1, s["issued"])
        self.assertEqual(1, len(s["effects"]))
        self.assertEqual(1, len(s["latches"]))

    def test_plan_rolls_back_all_rules_and_followups_together(self):
        f = fact(); self.j.receive(f, active); old = self.j.snapshot()
        plans = [dict(rule=ident(30), generation=1, followup=ident(32), targets=[ident(42)]),
                 dict(rule=ident(33), generation=0)]
        with self.assertRaises(Rejected):
            self.j.apply_plan(fact_id(f), plans, active)
        self.assertEqual(old, self.j.snapshot())
        plans[1]["generation"] = 1
        self.j.apply_plan(fact_id(f), plans, active)
        self.assertEqual(2, len(self.j.snapshot()["latches"]))

    def test_local_override_and_late_original_share_one_latch(self):
        options = dict(expected_context=context(), effect=ident(31))
        with self.assertRaisesRegex(Rejected, "trust"):
            self.j.override(ident(40), ident(41), ident(30), 1, lambda: False, **options)
        self.j.override(ident(40), ident(41), ident(30), 1, yes, **options)
        self.reopen()
        self.j.receive(fact(), active)
        self.j.apply(fact_id(fact()), ident(30), 1, active, effect=ident(31))
        self.assertEqual(1, self.j.snapshot()["issued"])
        self.assertEqual(1, len(self.j.snapshot()["effects"]))

    def test_revocation_at_actual_commit_rolls_back_prepared_application(self):
        self.j.receive(fact(), active); old = self.j.snapshot()
        permitted = [True]
        self.j.hook = lambda point: permitted.__setitem__(0, False) if point == "written" else None
        with self.assertRaisesRegex(Rejected, "trust"):
            self.j.apply(fact_id(fact()), ident(30), 1,
                         lambda f: "active" if permitted[0] else "deny", followup=ident(32))
        self.assertEqual(old, self.j.snapshot())

    def test_uncertain_reply_blocks_writes_and_operation_recovery_is_idempotent(self):
        self.j.hook = lambda p: (_ for _ in ()).throw(OSError("reply lost")) if p == "after_commit" else None
        with self.assertRaises(Uncertain):
            publish(self.j)
        with self.assertRaisesRegex(Rejected, "needs_recovery"):
            publish(self.j, operation=44)
        self.reopen()
        self.assertEqual(1, self.j.snapshot()["issued"])
        publish(self.j)
        self.assertEqual(1, self.j.snapshot()["issued"])

    def test_physical_effect_claim_then_reboot_is_unknown_never_replayed(self):
        effect = make_effect(self.j)
        self.j.claim_effect(effect, yes)
        self.reopen()
        self.assertEqual("unknown", self.j.snapshot()["effects"][effect]["status"])
        with self.assertRaisesRegex(Rejected, "effect_not_repeatable"):
            self.j.claim_effect(effect, yes)

    def test_observation_is_separate_from_preparation_and_claim(self):
        effect = make_effect(self.j)
        with self.assertRaises(Rejected):
            self.j.observe_effect(effect, True)
        with self.assertRaises(Rejected):
            self.j.claim_effect(effect, lambda: False)
        self.j.claim_effect(effect, yes)
        self.j.observe_effect(effect, True)
        self.reopen()
        self.assertEqual("observed", self.j.snapshot()["effects"][effect]["status"])

    def test_all_receipt_branches_do_not_imply_higher_success(self):
        for stage in ("transport_received", "rejected", "deferred", "effect_unknown"):
            j = Journal.create(self.root / (stage + ".db"), context())
            try:
                fid = publish(j); d = j.snapshot()["facts"][fid]["digest"]
                j.acknowledge(fid, d, ident(42), ident(42), stage, yes)
                self.assertFalse(j.snapshot()["outbox"][fid][ident(42)]["durable"])
                self.assertEqual(0, j.compact())
            finally:
                j.close()
        fid = publish(self.j); d = self.j.snapshot()["facts"][fid]["digest"]
        self.j.acknowledge(fid, d, ident(42), ident(42), "durable_received", yes)
        self.j.acknowledge(fid, d, ident(42), ident(42), "transport_received", yes)
        self.assertEqual(1, self.j.compact())
        self.assertEqual(1, len(self.j.snapshot()["localProgress"]))
        self.assertEqual(fid, publish(self.j))

    def test_relay_bad_digest_unlisted_target_or_unverified_receipt_cannot_release_data(self):
        fid = publish(self.j); d = self.j.snapshot()["facts"][fid]["digest"]
        cases = [(d, ident(42), ident(43), yes), ("00" * 32, ident(42), ident(42), yes),
                 (d, ident(43), ident(43), yes), (d, ident(42), ident(42), lambda: False)]
        for dg, target, origin, verified in cases:
            old = self.j.snapshot()
            with self.assertRaises(Rejected):
                self.j.acknowledge(fid, dg, target, origin, "applied", verified)
            self.assertEqual(old, self.j.snapshot())

    def test_receipt_conflict_preserves_evidence_and_prevents_compaction(self):
        fid = publish(self.j); d = self.j.snapshot()["facts"][fid]["digest"]
        self.j.acknowledge(fid, d, ident(42), ident(42), "applied", yes)
        self.j.acknowledge(fid, d, ident(42), ident(42), "rejected", yes)
        self.assertTrue(self.j.snapshot()["outbox"][fid][ident(42)]["conflict"])
        self.assertEqual(0, self.j.compact())

    def test_reverse_receipt_conflict_is_not_hidden_by_later_success(self):
        fid = publish(self.j); d = self.j.snapshot()["facts"][fid]["digest"]
        self.j.acknowledge(fid, d, ident(42), ident(42), "rejected", yes)
        self.j.acknowledge(fid, d, ident(42), ident(42), "applied", yes)
        self.assertTrue(self.j.snapshot()["outbox"][fid][ident(42)]["conflict"])
        self.assertEqual(0, self.j.compact())

    def test_intermediate_receipts_cannot_erase_rejection_across_reboot(self):
        fid = publish(self.j); d = self.j.snapshot()["facts"][fid]["digest"]
        self.j.acknowledge(fid, d, ident(42), ident(42), "rejected", yes)
        for stage in ("transport_received", "deferred", "effect_unknown"):
            self.j.acknowledge(fid, d, ident(42), ident(42), stage, yes)
        self.reopen()
        self.j.acknowledge(fid, d, ident(42), ident(42), "applied", yes)
        self.reopen()
        self.assertTrue(self.j.snapshot()["outbox"][fid][ident(42)]["conflict"])
        self.assertEqual(0, self.j.compact())

    def test_revocation_at_last_precommit_boundary_is_still_effective(self):
        self.j.receive(fact(), active); old = self.j.snapshot(); permitted = [True]
        self.j.hook = lambda p: permitted.__setitem__(0, False) if p == "before_commit" else None
        with self.assertRaisesRegex(Rejected, "trust"):
            self.j.apply(fact_id(fact()), ident(30), 1,
                         lambda f: "active" if permitted[0] else "deny")
        self.assertEqual(old, self.j.snapshot())

    def test_backend_page_exhaustion_never_formats_or_blindly_retries(self):
        pages = self.j.db.execute("PRAGMA page_count").fetchone()[0]
        self.j.db.execute("PRAGMA max_page_count=%d" % pages)
        confirmed = self.j.snapshot()
        for n in range(80, 112):
            try:
                publish(self.j, operation=n, subject=n)
                confirmed = self.j.snapshot()
            except Uncertain:
                break
        else:
            self.fail("configured SQLite page limit was not reached")
        with self.assertRaisesRegex(Rejected, "needs_recovery"):
            publish(self.j, operation=120)
        self.reopen()
        self.assertEqual(confirmed, self.j.snapshot())

    def test_compaction_keeps_pending_dependencies_and_does_not_skip_missing_numbers(self):
        dep = fact(origin=ident(26)); self.j.receive(dep, active)
        self.j.apply(fact_id(dep), ident(30), 1, active)
        f = fact(3, dependencies=[fact_id(dep)]); self.j.receive(f, active)
        self.assertEqual(0, self.j.compact())
        self.assertIn(fact_id(dep), self.j.snapshot()["facts"])
        self.assertEqual(0, prefix(self.j.snapshot(), stream_key(f)))

    def test_bounded_compaction_cycles_and_old_fact_never_reapply(self):
        for seq in range(1, 41):
            f = fact(seq); self.j.receive(f, active)
            self.j.apply(fact_id(f), ident(30), 1, active)
            self.j.compact()
        self.reopen()
        s = self.j.snapshot()
        self.assertEqual(40, next(iter(s["streams"].values()))["floor"])
        self.assertFalse(s["facts"])
        self.assertEqual(1, len(s["latches"]))
        self.assertEqual("covered_not_applied", self.j.receive(fact(), active))

    def test_capacity_rejection_preserves_previously_confirmed_state(self):
        for seq in range(1, 33):
            self.j.receive(fact(seq), active)
        old = self.j.snapshot()
        with self.assertRaisesRegex(Rejected, "capacity_facts"):
            self.j.receive(fact(33), active)
        self.reopen()
        self.assertEqual(old, self.j.snapshot())

    def test_stream_capacity_is_bounded_independently(self):
        for n in range(60, 68):
            self.j.receive(fact(origin=ident(n)), active)
        old = self.j.snapshot()
        with self.assertRaisesRegex(Rejected, "capacity_streams"):
            self.j.receive(fact(origin=ident(68)), active)
        self.assertEqual(old, self.j.snapshot())

    def test_effect_capacity_rolls_back_latch_and_new_sequence_together(self):
        for n in range(1, 17):
            f = fact(n); self.j.receive(f, active)
            self.j.apply(fact_id(f), ident(30 + n), 1, active, effect=ident(31))
        f = fact(17); self.j.receive(f, active); old = self.j.snapshot()
        with self.assertRaisesRegex(Rejected, "capacity_effects"):
            self.j.apply(fact_id(f), ident(60), 1, active, effect=ident(31), followup=ident(32))
        self.assertEqual(old, self.j.snapshot())
        self.assertEqual(0, self.j.snapshot()["issued"])

    def test_compaction_does_not_silently_evict_operation_replay_protection(self):
        for n in range(80, 112):
            publish(self.j, operation=n, targets=())
            self.j.compact()
        old = self.j.snapshot()
        self.assertFalse(old["facts"])
        self.assertEqual(32, len(old["operations"]))
        with self.assertRaisesRegex(Rejected, "capacity_operations"):
            publish(self.j, operation=112, targets=())
        self.assertEqual(old, self.j.snapshot())

    def test_reentrant_mutation_is_rejected_without_partial_publication(self):
        old = self.j.snapshot()
        self.j.hook = lambda p: self.j.compact() if p == "prepared" else None
        with self.assertRaisesRegex(Rejected, "busy"):
            publish(self.j)
        self.assertEqual(old, self.j.snapshot())

    def test_serialized_byte_budget_rolls_back_whole_action(self):
        old = self.j.snapshot()
        with patch("d1_journal_model.SNAPSHOT_BYTES", len(encoded(old)) + 30):
            with self.assertRaisesRegex(Rejected, "capacity_bytes"):
                publish(self.j)
        self.assertEqual(old, self.j.snapshot())

    def test_package_switch_keeps_latch_counter_outbox_and_rejects_old_local_intent(self):
        make_effect(self.j); publish(self.j)
        old = self.j.snapshot(); c = context(); c["package"] = ident(51, 32)
        self.j.switch_context(c, old["revision"], yes)
        self.assertEqual(old["latches"], self.j.snapshot()["latches"])
        self.assertEqual(old["outbox"], self.j.snapshot()["outbox"])
        self.assertEqual(old["issued"], self.j.snapshot()["issued"])
        with self.assertRaisesRegex(Rejected, "context"):
            publish(self.j, operation=45)
        f = fact(2, package=c["package"]); self.j.receive(f, active)
        self.j.apply(fact_id(f), ident(30), 2, active)
        self.assertEqual(1, len(self.j.snapshot()["effects"]))

    def test_new_run_cancels_unclaimed_effect_but_keeps_stream_and_unclear_effect(self):
        effect = make_effect(self.j)
        c = context(); c["run"] = ident(50); c["package"] = ident(51, 32)
        self.j.switch_context(c, self.j.snapshot()["revision"], yes)
        self.assertEqual("cancelled", self.j.snapshot()["effects"][effect]["status"])
        with self.assertRaises(Rejected):
            self.j.claim_effect(effect, yes)
        self.assertEqual(1, prefix(self.j.snapshot(), stream_key(fact())))

    def test_context_activation_needs_current_revision_authority_and_same_physical_identity(self):
        old = self.j.snapshot()
        for c, rev, auth in [(context(), 999, yes), (context(), 0, lambda: False),
                             (dict(context(), epoch=ident(52)), 0, yes)]:
            with self.assertRaises(Rejected):
                self.j.switch_context(c, rev, auth)
            self.assertEqual(old, self.j.snapshot())

    def test_unknown_format_and_corruption_never_load_empty_or_older_state(self):
        publish(self.j); self.j.close(); self.j = None
        db = sqlite3.connect(self.path); db.execute("PRAGMA user_version=99"); db.close()
        with self.assertRaisesRegex(Rejected, "unsupported"):
            Journal(self.path)
        db = sqlite3.connect(self.path); db.execute("PRAGMA user_version=1")
        db.execute("UPDATE snapshot SET payload=?", (b"{}",)); db.commit(); db.close()
        before = self.path.read_bytes()
        with self.assertRaisesRegex(Rejected, "corrupt"):
            Journal(self.path)
        self.assertEqual(before, self.path.read_bytes())

    def test_noncanonical_storage_with_recomputed_checksum_is_not_silently_migrated(self):
        self.j.close(); self.j = None
        db = sqlite3.connect(self.path)
        raw = db.execute("SELECT payload FROM snapshot").fetchone()[0]
        other = json.dumps(json.loads(raw), indent=2).encode("ascii")
        db.execute("UPDATE snapshot SET payload=?,digest=?",
                   (other, hashlib.sha256(other).hexdigest()))
        db.commit(); db.close()
        with self.assertRaisesRegex(Rejected, "noncanonical"):
            Journal(self.path)

    def test_invalid_range_or_dependency_cannot_enter_persistent_state(self):
        old = self.j.snapshot()
        for f in (fact(sequence=True), fact(dependencies=[fact_id(fact())]),
                  fact(3, type=3, subject=ident(20), unused=[[1, 3]])):
            with self.assertRaises(Rejected):
                self.j.receive(f, active)
        self.assertEqual(old, self.j.snapshot())

    def test_process_crash_matrix(self):
        stages = {
            "publish": ("allocated", "prepared", "written", "before_commit", "after_commit"),
            "apply": ("latched", "allocated", "intent", "prepared", "written", "before_commit", "after_commit"),
            "override": ("allocated", "latched", "intent", "prepared", "written", "before_commit", "after_commit"),
            "compact": ("compacted", "prepared", "written", "before_commit", "after_commit"),
            "switch": ("prepared", "written", "before_commit", "after_commit"),
            "claim": ("prepared", "written", "before_commit", "after_commit"),
            "ack": ("prepared", "written", "before_commit", "after_commit"),
        }
        exits = 0
        for action, points in stages.items():
            for point in points:
                with self.subTest(action=action, point=point):
                    path = self.root / (action + "-" + point + ".db")
                    j = Journal.create(path, context())
                    if action in ("apply", "compact"):
                        j.receive(fact(), active)
                    if action == "compact":
                        j.apply(fact_id(fact()), ident(30), 1, active)
                    if action in ("claim", "switch"):
                        make_effect(j)
                    if action == "ack":
                        publish(j)
                    old = j.snapshot(); j.close()
                    child = subprocess.run([sys.executable, str(Path(__file__).resolve()),
                        "--crash-worker", str(path), action, point], capture_output=True, text=True, timeout=15)
                    self.assertEqual(79, child.returncode, child.stdout + child.stderr)
                    exits += 1
                    j = Journal(path)
                    try:
                        s = j.snapshot()
                        if point != "after_commit":
                            self.assertEqual(old, s)
                        elif action in ("apply", "override"):
                            self.assertEqual(1, len(s["latches"]))
                            self.assertEqual(1 if action == "apply" else 2, s["issued"])
                            self.assertEqual(1, len(s["outbox"]))
                            self.assertEqual(1, len(s["effects"]))
                            self.assertEqual("prepared", next(iter(s["effects"].values()))["status"])
                        elif action == "publish":
                            self.assertEqual(1, s["issued"])
                            self.assertEqual(1, len(s["operations"]))
                            self.assertEqual(1, len(s["outbox"]))
                        elif action == "compact":
                            self.assertFalse(s["facts"])
                            self.assertEqual(1, next(iter(s["streams"].values()))["floor"])
                            self.assertEqual(old["latches"], s["latches"])
                        elif action == "switch":
                            self.assertEqual(ident(50), s["context"]["run"])
                            self.assertEqual(old["latches"], s["latches"])
                            self.assertEqual("cancelled", next(iter(s["effects"].values()))["status"])
                        elif action == "claim":
                            self.assertEqual("unknown", next(iter(s["effects"].values()))["status"])
                        elif action == "ack":
                            self.assertTrue(next(iter(s["outbox"].values()))[ident(42)]["durable"])
                    finally:
                        j.close()
        self.assertEqual(36, exits)
        print("D1: 36 actual process exits recovered; no device power-cut claim")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--crash-worker":
        worker(*sys.argv[2:])
    else:
        unittest.main()
