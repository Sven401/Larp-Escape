"""Decoded v2 release/owner reference; no codec, crypto, device or flash proof.

Ordinary v1 behavior remains in p0_rules_model. This separate extension models
the new paired release rules, exact owner snapshots and abstract journal cuts.
Symbolic tokens/operation IDs below are NOT production entropy or D1 allocation.
"""
import copy
import hashlib
import itertools
import json
from dataclasses import asdict, dataclass, replace

from p0_rules_model import PlanError, expr

# Ideal unique-symbol oracle for reference traces across owner-object loss.
# Neither token entropy nor durable operation-ID allocation is proved by this.
_REFERENCE_IDS = itertools.count(1)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, default=lambda x: asdict(x),
                                    separators=(",", ":")).encode()).hexdigest()


@dataclass(frozen=True)
class Context:
    event: str
    run: str
    station: str
    box: str
    epoch: str
    deployment: str
    binding: str
    content: str
    profile_revision: int


@dataclass(frozen=True)
class GmAuthority:
    """Owner projection of an already verified CURRENT GM, not a client role.

    The reference injects these values; it does not verify credentials or roles.
    Connection handle alone, peer identity alone and a valid signature are not
    sufficient substitutes for this full original owner snapshot.
    """
    peer: str
    principal: int
    role_epoch: int
    connection_token: int
    connection_handle: int
    registration: int
    lifecycle: int
    registry_revision: int


@dataclass(frozen=True)
class AdmittedFact:
    """Fact-owner input, AFTER trust/context/replay admission, never raw COSE."""
    active_context: Context
    operation: str
    subject: str
    fact_id: str
    payload_digest: str
    fact_type: int


@dataclass(frozen=True)
class OwnerFences:
    trust: int = 1
    resources: int = 1
    capacity: int = 1
    profile: int = 1


@dataclass(frozen=True)
class OwnerView:
    context: Context
    revision: int
    incarnation: str


@dataclass(frozen=True)
class Grant:
    token: str
    context: Context
    authority: GmAuthority
    original: str
    fallback: str
    stable: str
    original_generation: int
    fallback_generation: int
    expected_state: str
    rights_revision: int
    fences: OwnerFences
    program_digest: str
    prepare_request: str
    prepare_digest: str
    prepared_at: int
    expires: int


@dataclass(frozen=True)
class AuthorizedOverride:
    grant: Grant
    operation: str
    request: str


@dataclass(frozen=True)
class Plan:
    expected_state: str
    context: Context
    operation: str
    after: dict
    content_digest: str
    authorization: AuthorizedOverride | None


def minimal(family):
    """Exact mathematical reference, not the bounded C++ proof workspace."""
    sets = set(family)
    return [s for s in sets if not any(t < s for t in sets)]


def certificates(node, state, event):
    value = expr(node, state, event)  # Eager numeric/type failures still matter.
    op = node[0]
    if op == "fact":
        fact = state["facts"].get(node[1])
        return value, [frozenset([fact.fact_id])] if fact else []
    if op in ("literal", "value", "input", "port_is"):
        return value, [frozenset()] if value is not None else []
    children = [certificates(n, state, event) for n in node[1:]]
    if value is None:
        return value, []
    if op == "not":
        return value, children[0][1]
    if (op == "any" and value is True) or (op == "all" and value is False):
        decisive = [proof for truth, proofs in children if truth is value for proof in proofs]
        return value, minimal(decisive)
    return value, minimal(frozenset().union(*parts)
                          for parts in itertools.product(*(p for _, p in children)))


def unique_claims(rules):
    seen = set()
    for rule in rules:
        if rule["stable"] in seen:
            raise PlanError("group_claim_conflict")
        seen.add(rule["stable"])


def positive_fact_guard(node):
    return node[0] == "fact" or (node[0] in ("all", "any")
        and 2 <= len(node)-1 <= 8 and all(positive_fact_guard(child) for child in node[1:]))


def uses_sample(node):
    return node[0] in ("input", "port_is") or any(
        uses_sample(child) for child in node[1:] if isinstance(child, list))


class ReleaseModel:
    """Typed decoded release-group projection, separate from v1 timer gameplay."""
    def __init__(self, spec, context):
        self.spec = copy.deepcopy(spec)
        self.context = context
        self.rules = {r["id"]: r for r in self.spec["rules"]}
        self.groups = {}
        self._validate_groups()
        self.state = dict(values=copy.deepcopy(spec.get("values", {})), facts={}, solved=[],
                          latches={}, operations={}, emissions=[], intents=[], revision=0,
                          last_issued=0, mode="running", resource_revision=1, capacity_revision=1,
                          context=context)

    def program_digest(self):
        return digest(dict(spec=self.spec, active_rules=self.rules, groups=self.groups))

    def _validate_groups(self):
        if (self.spec["format"] != "larp.package.v2" or self.spec["ir"] != "larp.rules.v2"
                or self.spec["requires"].get("rules_manual_override") != 1):
            raise PlanError("version_gate")
        if len(self.rules) != len(self.spec["rules"]):
            raise PlanError("duplicate_rule")
        members = set()
        for original in self.rules.values():
            if original["offline"]["policy"] != "local_fallback":
                continue
            fallback = self.rules.get(original["offline"].get("rule"))
            if (not fallback or original["id"] == fallback["id"]
                    or original["id"] in members or fallback["id"] in members
                    or original["station"] != self.context.station
                    or fallback["station"] != original["station"]
                    or original["trigger"][0] != "fact" or fallback["trigger"] != ["override"]
                    or fallback["offline"] != {"policy": "local_continue"}
                    or original["mode"] != "once" or fallback["mode"] != "once"
                    or not original["stable"] or original["stable"] != fallback["stable"]):
                raise PlanError("group")
            if not positive_fact_guard(original["guard"]) or uses_sample(fallback["guard"]):
                raise PlanError("guard_context")
            expected = []
            emitted, ports = set(), set()
            for action in original["actions"]:
                if action[0] not in ("emit", "effect"):
                    raise PlanError("group_action")
                expected.append(["override", *action[1:]] if action[0] == "emit" else action)
                if action[0] == "emit":
                    if action[1] in emitted:
                        raise PlanError("subject_conflict")
                    emitted.add(action[1])
                elif action[1] in ports:
                    raise PlanError("resource_conflict")
                else:
                    ports.add(action[1])
                if action[0] == "emit" and not {1, 2}.issubset(self.spec["permissions"].get(action[1], [])):
                    raise PlanError("permission")
            if not emitted or fallback["actions"] != expected:
                raise PlanError("group_action")
            if original["stable"] in self.groups:
                raise PlanError("stable_collision")
            self.groups[original["stable"]] = (original["id"], fallback["id"])
            members.update((original["id"], fallback["id"]))
        producers, outputs = set(), set()
        for original, _ in self.groups.values():
            for action in self.rules[original]["actions"]:
                if action[0] == "emit":
                    if action[1] in producers:
                        raise PlanError("subject_conflict")
                    producers.add(action[1])
                elif action[1] in outputs:
                    raise PlanError("resource_conflict")
                else:
                    outputs.add(action[1])
        if members != set(self.rules):
            # This reference file covers release groups; general v1 rules are
            # still exercised by the unchanged six v1 gameplay traces.
            raise PlanError("reference_projection")

    def plan(self, incoming, outbox_capacity=64, intent_capacity=64):
        if self.state["context"] != self.context:
            raise PlanError("context")
        if any(f.active_context != self.context or f.fact_type not in (1, 2)
               for f in self.state["facts"].values()):
            raise PlanError("fact_admission")
        if self.state["mode"] != "running":
            raise PlanError("stopped")
        if isinstance(incoming, AdmittedFact):
            if incoming.active_context != self.context or incoming.fact_type not in (1, 2):
                raise PlanError("fact_admission")
            operation = incoming.operation
            event = dict(kind="fact", subject=incoming.subject)
            authorization = None
        elif isinstance(incoming, AuthorizedOverride):
            if incoming.grant.context != self.context:
                raise PlanError("context")
            operation = incoming.operation
            event = dict(kind="override", fallback=incoming.grant.fallback)
            authorization = incoming
        else:
            raise PlanError("owner_input_required")
        if operation in self.state["operations"]:
            if self.state["operations"][operation]["input"] != digest(incoming):
                raise PlanError("operation_conflict")
            return None
        old = copy.deepcopy(self.state)
        if isinstance(incoming, AdmittedFact):
            for known in old["facts"].values():
                if known.fact_id == incoming.fact_id and known.payload_digest != incoming.payload_digest:
                    raise PlanError("fact_conflict")
            old["facts"].setdefault(incoming.subject, incoming)
        after = copy.deepcopy(old)
        selected = []
        causes = {}
        for rule in self.rules.values():
            trigger = rule["trigger"]
            matches = (event["kind"] == "fact" and trigger[0] == "fact"
                       and event["subject"] in trigger[1]) or (
                       event["kind"] == "override" and rule["id"] == event["fallback"])
            if not matches or rule["stable"] in old["latches"]:
                continue
            truth, proofs = certificates(rule["guard"], old, event)
            if truth is True:
                if not proofs:
                    raise PlanError("missing_proof")
                selected.append(rule)
                causes[rule["id"]] = sorted(min(proofs, key=lambda p: (len(p), sorted(p))))
        unique_claims(selected)
        subjects, ports = set(), set()
        actions = 0
        for rule in sorted(selected, key=lambda r: (r["priority"], r["stable"])):
            dependencies = causes[rule["id"]]
            after["latches"][rule["stable"]] = dict(variant=rule["id"], generation=rule["generation"],
                                                      operation=operation, dependencies=dependencies)
            for action in rule["actions"]:
                actions += 1
                if actions > self.spec.get("plan_limit", 32):
                    raise PlanError("plan_budget")
                op, target = action[:2]
                if op in ("emit", "override"):
                    if target in subjects or target in old["solved"]:
                        raise PlanError("subject_conflict")
                    if len(dependencies) > 8:
                        raise PlanError("dependency_limit")
                    if op == "override" and not authorization:
                        raise PlanError("owner_input_required")
                    subjects.add(target)
                    after["solved"].append(target)
                    if after["last_issued"] == 2**64-1:
                        raise PlanError("sequence_exhausted")
                    after["last_issued"] += 1
                    after["emissions"].append(dict(subject=target, fact_type=2 if op == "override" else 1,
                        sequence=after["last_issued"], context=self.context, dependencies=dependencies,
                        operation=operation, authorization=authorization))
                elif op == "effect":
                    if target in ports:
                        raise PlanError("resource_conflict")
                    ports.add(target)
                    after["intents"].append(dict(port=target, resource=action[2], operation=operation,
                                                  state="prepared"))
        if len(after["emissions"]) > outbox_capacity or len(after["intents"]) > intent_capacity:
            raise PlanError("capacity")
        after["operations"][operation] = dict(input=digest(incoming), authorization=authorization)
        after["revision"] += 1
        return Plan(digest(self.state), self.context, operation, after, digest(after), authorization)

    def check_plan(self, plan):
        if (plan.context != self.context or plan.expected_state != digest(self.state)
                or plan.content_digest != digest(plan.after)):
            raise PlanError("stale")


class ReferenceJournal:
    """Abstract atomic disk image and qualified recovery oracle, NOT flash IO.

    The injected cuts stipulate whether this reference transaction committed.
    Real software may not infer noncommit from record absence; it must obtain
    the qualified backend's recovery outcome under its dirty-tail rules.
    """
    def __init__(self, model):
        self.context = model.context
        self.disk = copy.deepcopy(model.state)
        self.unknown = None

    def submit(self, model, plan, fence, cut="committed"):
        self.check_context(model)
        if self.unknown:
            raise PlanError("commit_unknown")
        model.check_plan(plan)
        fence()  # Exact final owner admission/expiry point, immediately before write.
        if cut == "before_write":
            return "pending"
        if cut not in ("committed", "unknown_absent", "unknown_present"):
            raise PlanError("reference_cut")
        if cut != "unknown_absent":
            self.disk = copy.deepcopy(plan.after)
        if cut.startswith("unknown"):
            self.unknown = (plan.operation, cut == "unknown_present")
            return "commit_unknown"
        model.state = copy.deepcopy(self.disk)
        return "committed"

    def recover(self, model):
        self.check_context(model)
        if not self.unknown:
            raise PlanError("not_unknown")
        operation, committed = self.unknown
        model.state = copy.deepcopy(self.disk)
        self.unknown = None
        return operation, "committed" if committed else "not_committed"

    def attempt(self, model, index):
        self.check_context(model)
        if self.unknown or self.disk["intents"][index]["state"] != "prepared":
            raise PlanError("intent_not_prepared")
        self.disk["intents"][index]["state"] = "attempted"
        model.state = copy.deepcopy(self.disk)

    def restart(self, model):
        self.check_context(model)
        for intent in self.disk["intents"]:
            if intent["state"] == "attempted":
                intent["state"] = "unknown"
        model.state = copy.deepcopy(self.disk)
        model.state["mode"] = "recovery_required"

    def check_context(self, model):
        if self.context != model.context or self.disk["context"] != self.context:
            raise PlanError("journal_context")


class OverrideOwner:
    """Serialized reference owner. Authority changes are injected by its trusted
    lifecycle owner, never a field supplied in a confirmation request.
    """
    def __init__(self, model, authority, journal=None, ttl=30, slots=4):
        self.model = model
        self.authority = authority
        self.rights_revision = 1
        self.journal = journal or ReferenceJournal(model)
        self.ttl, self.slots = ttl, slots
        self.grants = {}
        self.prepare_requests = {}
        self.restored = False
        self.clock = None  # Optional trusted test clock at the final owner fence.
        self.fences = OwnerFences(profile=model.context.profile_revision)
        self.incarnation = f"symbolic-owner-{next(_REFERENCE_IDS)}"

    def replace_authority(self, authority):
        if self.journal.unknown:
            raise PlanError("commit_unknown")
        self.authority = authority
        self.rights_revision += 1
        self._retire_unusable()

    def replace_fences(self, **changes):
        if self.journal.unknown:
            raise PlanError("commit_unknown")
        next_fences = replace(self.fences, **changes)
        if any(not 0 < value <= 2**64-1 or value < getattr(self.fences, field)
               for field, value in asdict(next_fences).items()):
            raise PlanError("fence_revision")
        self.fences = next_fences
        self._retire_unusable()

    def _retire_unusable(self, now=None):
        if self.journal.unknown:
            raise PlanError("commit_unknown")
        for entry in self.grants.values():
            if entry["state"] not in ("prepared", "pending"):
                continue
            grant = entry["grant"]
            invalid = (grant.authority != self.authority or grant.rights_revision != self.rights_revision
                       or grant.fences != self.fences or grant.context != self.model.context
                       or grant.program_digest != self.model.program_digest())
            expired = now is not None and now > grant.expires
            if invalid or expired:
                # Reference pending means known prewrite/capacity deferral. A real
                # durably received pending operation needs a durable cancellation
                # receipt, not just deletion of a volatile token. Unknown is never
                # displaced; the entry/phase identity remains terminal for replay.
                entry["state"] = "rejected"

    def _available(self):
        if self.journal.unknown:
            raise PlanError("commit_unknown")
        if (not isinstance(self.authority, GmAuthority) or not self.authority.peer
                or any(getattr(self.authority, field) <= 0 for field in
                       ("principal", "role_epoch", "connection_token", "registration", "lifecycle", "registry_revision"))):
            raise PlanError("unauthorized")

    def view(self):
        self._available()
        return OwnerView(self.model.context, self.model.state["revision"], self.incarnation)

    def prepare(self, original, observed, now, request):
        self._available()
        if not 0 <= now <= 2**64-1:
            raise PlanError("clock_overflow")
        self._retire_unusable(now)
        if observed.incarnation != self.incarnation:
            raise PlanError("owner_incarnation")
        request_digest = digest(dict(original=original, view=observed,
            authority=self.authority, rights=self.rights_revision,
            program=self.model.program_digest()))
        if request in self.prepare_requests:
            prior_digest, token = self.prepare_requests[request]
            if prior_digest != request_digest:
                raise PlanError("prepare_request_conflict")
            if observed.context != self.model.context:
                raise PlanError("context")
            return token  # Never replace the original token or its absolute deadline.
        if self.model.state["mode"] != "running":
            raise PlanError("stopped")
        if observed.context != self.model.context or observed.revision != self.model.state["revision"]:
            raise PlanError("stale")
        if not 0 <= now <= 2**64-1 or not 0 < self.ttl <= 2**64-1-now:
            raise PlanError("clock_overflow")
        if sum(g["state"] in ("prepared", "pending") for g in self.grants.values()) >= self.slots:
            raise PlanError("capacity")
        rule = self.model.rules[original]
        if self.model.groups[rule["stable"]][0] != original:
            raise PlanError("not_original")
        fallback = self.model.rules[rule["offline"]["rule"]]
        token = f"symbolic-token-{next(_REFERENCE_IDS)}"
        grant = Grant(token, self.model.context, self.authority, original, fallback["id"], rule["stable"],
                      rule["generation"], fallback["generation"], digest(self.model.state),
                      self.rights_revision, self.fences, self.model.program_digest(), request,
                      request_digest, now, now+self.ttl)
        self.grants[token] = dict(grant=grant, state="prepared", request=None, operation=None, plan=None)
        self.prepare_requests[request] = (request_digest, token)
        return token

    def _fence(self, grant, now):
        self._available()
        if self.authority != grant.authority or self.rights_revision != grant.rights_revision:
            raise PlanError("unauthorized")
        if now < grant.prepared_at or now > grant.expires:
            raise PlanError("expired")
        if grant.context != self.model.context or grant.expected_state != digest(self.model.state):
            raise PlanError("stale")
        if grant.fences != self.fences or grant.program_digest != self.model.program_digest():
            raise PlanError("stale")

    def confirm(self, token, request, now, cut="committed", outbox_capacity=64, intent_capacity=64):
        self._available()
        entry = self.grants.get(token)
        if entry is None:
            raise PlanError("unknown_token")
        grant = entry["grant"]
        if self.authority != grant.authority or self.rights_revision != grant.rights_revision:
            raise PlanError("unauthorized")
        if grant.context != self.model.context:
            raise PlanError("context")
        if grant.program_digest != self.model.program_digest():
            raise PlanError("stale")
        if entry["request"] is not None and request != entry["request"]:
            raise PlanError("request_conflict")
        if entry["state"] in ("committed", "rejected", "not_committed", "already_released"):
            return entry["state"]
        try:
            # Returning the existing release is not committing a stale plan.
            if grant.context == self.model.context and grant.stable in self.model.state["latches"]:
                if now < grant.prepared_at or now > grant.expires:
                    raise PlanError("expired")
                entry["state"], entry["request"] = "already_released", request
                return entry["state"]
            self._fence(grant, now)
            if entry["operation"] is None:
                entry["operation"] = f"symbolic-operation-{next(_REFERENCE_IDS)}"
                entry["request"] = request
            incoming = AuthorizedOverride(grant, entry["operation"], request)
            if entry["plan"] is None:
                entry["plan"] = self.model.plan(incoming, outbox_capacity, intent_capacity)
            if (len(entry["plan"].after["emissions"]) > outbox_capacity
                    or len(entry["plan"].after["intents"]) > intent_capacity):
                raise PlanError("capacity")
            entry["state"] = self.journal.submit(self.model, entry["plan"],
                lambda: self._fence(grant, self.clock() if self.clock else now), cut)
            return entry["state"]
        except PlanError as error:
            if str(error) != "capacity":
                entry["state"] = "rejected"
            else:
                entry["state"] = "pending"
            raise

    def recover(self):
        operation, outcome = self.journal.recover(self.model)
        for entry in self.grants.values():
            if entry["operation"] == operation:
                entry["state"] = outcome
        if self.restored:
            self.journal.restart(self.model)
            self.restored = False
        # Recovery is independent of volatile grants. A new process can expose
        # an existing durable receipt to a newly authorized reader, never rebuild
        # an executable confirmation from an old token.
        return outcome

    @classmethod
    def restore(cls, model, journal):
        """New owner after actual object loss; no connection or token survives."""
        journal.check_context(model)
        owner = cls(model, None, journal)
        owner.restored = True
        if not journal.unknown:
            journal.restart(model)
        return owner

    def apply_fact(self, fact):
        if self.journal.unknown:
            raise PlanError("commit_unknown")
        plan = self.model.plan(fact)
        if plan is None:
            return "duplicate"
        return self.journal.submit(self.model, plan, lambda: None)

    def read_operation(self, operation):
        """Freshly authorized read only; it never executes an old Confirm."""
        self._available()
        record = self.model.state["operations"][operation]
        authorization = record["authorization"]
        if authorization and authorization.grant.context != self.model.context:
            raise PlanError("context")
        return copy.deepcopy(record)

    def restart(self):
        if self.journal.unknown:
            raise PlanError("commit_unknown")
        self.journal.restart(self.model)
        self.authority = None
        self.rights_revision += 1
        for entry in self.grants.values():
            if entry["state"] in ("prepared", "pending"):
                entry["state"] = "rejected"
