#pragma once
// Included inside the independent driver's namespace, after its shared helpers.
bundle_review::Medium catalogMedium(Hash &hash) {
    bundle_review::Medium m;
    bundle_review::Session s(m);
    ProgressJournal low(s);
    check(low.initialize(origin()).outcome == ProgressOutcome::Initialized,
          "fault base initialize");
    Buffers b;
    Ids ids;
    rb::Journal bundle(low, hash, ids, b.work);
    Consumer sink;
    check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "fault base semantic recovery");
    b.set({0});
    Fence f;
    execute(bundle, low, f, key(hash, 1, {0}), 0);
    return m;
}
void reopened(const bundle_review::Medium &source, bool physical, unsigned applications) {
    auto medium = source;
    bundle_review::Session session(medium);
    ProgressJournal low(session);
    const auto r = low.recover(origin());
    if (!physical) {
        check(
            (r.outcome == ProgressOutcome::Corrupt || r.outcome == ProgressOutcome::Unsupported) &&
                !low.snapshot().ready,
            "occupied/torn record hard-fails physical recovery");
        return;
    }
    check(r.outcome == ProgressOutcome::Recovered, "durable new session low recovery");
    Hash hash;
    Buffers b;
    Ids ids;
    rb::Journal bundle(low, hash, ids, b.work);
    Consumer sink;
    check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "durable semantic recovery");
    check(sink.commits == 1 && sink.catalogs == 1 && sink.applications == applications,
          "only complete unique Final has application effect");
    check(low.snapshot().lastIssued == applications * 8,
          "only application Final allocates entire ID range");
    b.set({0});
    const auto app = key(hash, 2, {0});
    const auto programs = session.programs;
    if (applications) {
        const auto duplicate = bundle.begin(app, meta(), 8, 0);
        check(duplicate.outcome == rb::Outcome::Duplicate && duplicate.journal.firstIssued == 1 &&
                  duplicate.journal.lastIssued == 8 && session.programs == programs,
              "recovered Final exact duplicate with original assigned IDs");
    } else if (low.snapshot().records == 3) {
        const auto orphan = bundle.begin(app, meta(), 8, 0);
        check(orphan.outcome == rb::Outcome::Rejected &&
                  orphan.error == rb::Error::RecoveryRequired && session.programs == programs,
              "recovered inert orphan never revives old command");
    }
}
void cuts() {
    Hash hash;
    const auto baseline = catalogMedium(hash);
    std::size_t attempts = 0;
    constexpr std::size_t PhysicalBytes = 1 + 4092 + 4;
    for (bool through : {false, true})
        for (std::size_t cut = 0; cut <= PhysicalBytes * 2; ++cut) {
            auto medium = baseline;
            bundle_review::Session s(medium);
            s.writeThrough = through;
            ProgressJournal low(s);
            check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "cut open");
            Buffers b;
            Ids ids;
            ids.next = 1;
            rb::Journal bundle(low, hash, ids, b.work);
            Consumer sink;
            check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "cut index");
            const std::vector<std::uint8_t> bytes = {0};
            b.set(bytes);
            const auto k = key(hash, 2, bytes);
            check(bundle.begin(k, meta(), 8, 0).outcome == rb::Outcome::Prepared, "cut prepare");
            s.budget = cut;
            Fence f;
            auto r = bundle.step(f);
            if (r.outcome == rb::Outcome::Progress)
                r = bundle.step(f);
            const bool complete = cut == PhysicalBytes * 2;
            check(r.outcome == (complete ? rb::Outcome::Committed : rb::Outcome::Unknown),
                  "all uncertain cuts remain Unknown");
            check(bundle.unresolved() != complete, "Unknown retains lock");
            if (!complete) {
                const auto calls = s.programs;
                check(bundle.step(f).outcome != rb::Outcome::Committed && s.programs == calls,
                      "uncertain wrapper never retries physical write");
            }
            s.loseSession();
            const bool physical = cut == 0 || cut == PhysicalBytes || complete;
            reopened(medium, physical, complete ? 1 : 0);
            ++attempts;
        }
    std::cout << "CUTS " << attempts << '\n';
}
void postwrite() {
    Hash hash;
    const auto baseline = catalogMedium(hash);
    for (bool final : {false, true}) {
        auto medium = baseline;
        bundle_review::Session s(medium);
        ProgressJournal low(s);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "postwrite open");
        Buffers b;
        Ids ids;
        ids.next = 1;
        rb::Journal bundle(low, hash, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "postwrite index");
        b.set({0});
        const auto k = key(hash, 2, {0});
        check(bundle.begin(k, meta(), 8, 0).outcome == rb::Outcome::Prepared, "postwrite prepare");
        Fence f;
        if (final)
            check(bundle.step(f).outcome == rb::Outcome::Progress, "postwrite prior chunk");
        s.readAfterSync = s.syncs + 3;
        const auto r = bundle.step(f);
        check(r.outcome == rb::Outcome::Unknown && bundle.unresolved(),
              "postwrite scan failure is Unknown, not rejection");
        check(low.snapshot().last == ProgressOutcome::IoFailure,
              "actual postwrite lowlevel scan IO failure");
        ProgressJournal sameFailedBackend(s);
        check(sameFailedBackend.recover(origin()).outcome == ProgressOutcome::IoFailure,
              "new wrapper around failed session is no genuine reopen");
        s.loseSession();
        reopened(medium, true, final ? 1 : 0);
    }
    // Every sync phase, with failed-sync bytes either persisted or lost. The
    // intermediate successful syncs remain durable in both branches.
    for (bool final : {false, true})
        for (unsigned phase = 1; phase <= 3; ++phase)
            for (bool persist : {false, true}) {
                auto medium = baseline;
                bundle_review::Session s(medium);
                s.writeThrough = false;
                s.persistFailedSync = persist;
                ProgressJournal low(s);
                check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "sync open");
                Buffers b;
                Ids ids;
                ids.next = 1;
                rb::Journal bundle(low, hash, ids, b.work);
                Consumer sink;
                check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "sync index");
                b.set({0});
                check(bundle.begin(key(hash, 2, {0}), meta(), 8, 0).outcome ==
                          rb::Outcome::Prepared,
                      "sync prepare");
                Fence f;
                if (final)
                    check(bundle.step(f).outcome == rb::Outcome::Progress, "sync prior chunk");
                s.failSync = s.syncs + phase;
                const auto r = bundle.step(f);
                check(r.outcome == rb::Outcome::Unknown && bundle.unresolved(),
                      "failed sync Unknown locked");
                s.loseSession();
                const bool valid = phase == 1 ? !persist : phase == 3 && persist;
                reopened(medium, valid, final && phase == 3 && persist ? 1 : 0);
            }
    std::cout << "POSTWRITE 14\n";
}
void capacities() {
    Hash hash;
    const auto baseline = catalogMedium(hash);
    for (std::size_t reserved : {59u, 60u}) {
        auto medium = baseline;
        bundle_review::Session s(medium);
        ProgressJournal low(s);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "capacity open");
        Buffers b;
        Ids ids;
        ids.next = 1;
        rb::Journal bundle(low, hash, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "capacity index");
        b.set({0});
        const auto programs = s.programs;
        const auto r = bundle.begin(key(hash, 2, {0}), meta(), 8, reserved);
        check(r.outcome == (reserved == 59 ? rb::Outcome::Prepared : rb::Outcome::Deferred),
              "exact 63 slots incl reserved intents / +1");
        check(s.programs == programs, "capacity checks before any write");
        if (reserved == 59) {
            Fence f;
            check(bundle.step(f).outcome == rb::Outcome::Progress &&
                      bundle.step(f).outcome == rb::Outcome::Committed,
                  "exact slot reservation usable");
        }
    }
    // Caller-owned capacity shrinks while pointers/counts remain locally valid.
    auto medium = baseline;
    bundle_review::Session s(medium);
    ProgressJournal low(s);
    check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "pool open");
    Buffers b;
    Ids ids;
    ids.next = 1;
    rb::Journal bundle(low, hash, ids, b.work);
    Consumer sink;
    check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "pool index");
    b.set({0});
    check(bundle.begin(key(hash, 2, {0}), meta(), 8, 0).outcome == rb::Outcome::Prepared,
          "pool prepare");
    const auto programs = s.programs;
    b.work.records.capacity = b.work.records.count;
    Fence f;
    const auto r = bundle.step(f);
    check(r.outcome == rb::Outcome::Deferred && s.programs == programs && !bundle.unresolved(),
          "pool shrink detected before committing");
    std::cout << "CAPACITY 3\n";
}

struct HashPort final : IPackageDigest {
    Hash actual;
    int failure = 0;
    std::function<void()> callback;
    bool begin() override {
        if (callback) {
            auto fn = callback;
            callback = {};
            fn();
        }
        return failure != 1 && actual.begin();
    }
    bool update(const std::uint8_t *p, std::size_t n) override {
        return failure != 2 && actual.update(p, n);
    }
    bool finish(std::uint8_t p[32], std::size_t &n) override {
        if (failure == 3)
            return false;
        const bool ok = actual.finish(p, n);
        if (failure == 4)
            n = 31;
        return ok;
    }
    bool abort() override {
        const bool ok = actual.abort();
        return ok && failure != 5;
    }
};
void auxiliary(ProgressJournal &low, Hash &h, unsigned id = 0x55) {
    ProgressRequest r;
    r.kind = ProgressKind::Received;
    r.operationId.fill(static_cast<std::uint8_t>(id));
    const std::vector<std::uint8_t> body = {0};
    r.requestDigest = digest(h, body);
    r.payload = body.data();
    r.payloadBytes = body.size();
    r.expectedRevision = low.snapshot().revision;
    r.expectedLastIssued = low.snapshot().lastIssued;
    r.profileRevision = 17;
    r.trustRevision = 23;
    LowFence f;
    check(low.commit(r, f).outcome == ProgressOutcome::Committed,
          "actual auxiliary physical commit");
}
void probe(rb::Journal &bundle, const rb::Key &k, rb::Workspace &w) {
    const auto a = w.bundles.count, b = w.records.count, c = w.staged.count;
    Consumer inner;
    Fence fence;
    const auto first = bundle.begin(k, meta(), 8, 0), second = bundle.step(fence);
    // Use a local consumer; a busy recovery must not even abort somebody else's transaction.
    const auto third = bundle.recover(inner);
    for (auto r : {first, second, third})
        check(r.outcome == rb::Outcome::Deferred && r.error == rb::Error::Busy,
              "callback reentry rejected Busy");
    check(w.bundles.count == a && w.records.count == b && w.staged.count == c &&
              inner.begins == 0 && inner.aborts == 0 && inner.commits == 0,
          "reentry preserves caller pools and provisional consumer");
}
void callbacks() {
    Hash real;
    const auto baseline = catalogMedium(real);
    // Key and HeaderMeta may alias mutable external input but the operation freezes
    // their values at call entry, before hash/identity backend callbacks.
    for (bool identity : {false, true}) {
        auto medium = baseline;
        bundle_review::Session s(medium);
        ProgressJournal low(s);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "copy open");
        Buffers b;
        Ids ids;
        ids.next = 1;
        HashPort h;
        rb::Journal bundle(low, h, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "copy index");
        b.set({0});
        auto k = key(real, 2, {0});
        const auto saved = k;
        auto m = meta();
        auto mutate = [&]() {
            k = rb::Key{};
            m = rb::HeaderMeta{};
        };
        if (identity)
            ids.callback = mutate;
        else
            h.callback = mutate;
        check(bundle.begin(k, m, 8, 0).outcome == rb::Outcome::Prepared,
              "entry copies survive callback mutation");
        Fence f;
        check(bundle.step(f).outcome == rb::Outcome::Progress &&
                  bundle.step(f).outcome == rb::Outcome::Committed,
              "copied begin finishes");
        ProgressRecord r;
        std::uint8_t p[3072];
        check(low.readRecord(3, r, p, sizeof(p)).outcome == ProgressOutcome::Read &&
                  r.operationId == saved.operation && r.profileRevision == 17 &&
                  r.trustRevision == 23,
              "original metadata and identity actually persisted");
    }
    for (unsigned source = 0; source < 4; ++source) {
        auto medium = baseline;
        bundle_review::Session s(medium);
        ProgressJournal low(s);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "reentry open");
        Buffers b;
        Ids ids;
        ids.next = 1;
        HashPort h;
        rb::Journal bundle(low, h, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "reentry index");
        b.set({0});
        const auto k = key(real, 2, {0});
        auto nested = [&]() { probe(bundle, k, b.work); };
        if (source == 0)
            h.callback = nested;
        if (source == 1)
            ids.callback = nested;
        check(bundle.begin(k, meta(), 8, 0).outcome == rb::Outcome::Prepared,
              "reentry outer prepare");
        Fence f;
        if (source == 2)
            f.callback = nested;
        check(bundle.step(f).outcome == rb::Outcome::Progress &&
                  bundle.step(f).outcome == rb::Outcome::Committed,
              "reentry outer committed");
        if (source == 3) {
            Consumer c;
            c.beforePrepare = nested;
            check(bundle.recover(c).outcome == rb::Outcome::Recovered && c.commits == 1,
                  "consumer reentry outer recovery");
        }
    }
    // A prepare callback deliberately appends another actual lowlevel record.
    // The final journal snapshot must still match before consumer publication.
    {
        auto medium = baseline;
        bundle_review::Session s(medium);
        ProgressJournal low(s);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "prepare snapshot open");
        Buffers b;
        Ids ids;
        rb::Journal bundle(low, real, ids, b.work);
        Consumer sink;
        sink.beforePrepare = [&]() { auxiliary(low, real); };
        const auto r = bundle.recover(sink);
        check(r.outcome == rb::Outcome::Rejected && r.error == rb::Error::Conflict &&
                  sink.commits == 0 && sink.aborts == 1,
              "post-prepare snapshot prevents stale publication");
        Consumer good;
        check(bundle.recover(good).outcome == rb::Outcome::Recovered && good.commits == 1 &&
                  good.auxiliaries == 1,
              "post-prepare mutation healthy complete reuse");
    }
    std::cout << "CALLBACKS 7\n";
}
void randomHash() {
    Hash real;
    const auto baseline = catalogMedium(real);
    for (int fault = 1; fault <= 5; ++fault) {
        auto medium = baseline;
        bundle_review::Session s(medium);
        ProgressJournal low(s);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "hash open");
        Buffers b;
        Ids ids;
        ids.next = 1;
        HashPort h;
        rb::Journal bundle(low, h, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "hash index");
        b.set({0});
        const auto k = key(real, 2, {0});
        const auto writes = s.programs;
        h.failure = fault;
        const auto r = bundle.begin(k, meta(), 8, 0);
        check(r.outcome == rb::Outcome::Rejected && r.error == rb::Error::Crypto &&
                  s.programs == writes && !bundle.active() && !bundle.unresolved(),
              "hash begin/update/finish/length/abort failure before write");
        h.failure = 0;
        check(bundle.begin(k, meta(), 8, 0).outcome == rb::Outcome::Prepared,
              "hash failure healthy reuse");
        Fence f;
        check(bundle.step(f).outcome == rb::Outcome::Progress &&
                  bundle.step(f).outcome == rb::Outcome::Committed,
              "hash healthy reuse actual commit");
    }
    for (unsigned fault = 0; fault < 4; ++fault) {
        auto medium = baseline;
        bundle_review::Session s(medium);
        ProgressJournal low(s);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "random open");
        Buffers b;
        Ids ids;
        ids.next = 1;
        rb::Journal bundle(low, real, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "random index");
        std::vector<std::uint8_t> bytes(2049, 0x33);
        bytes[0] = 0x59;
        bytes[1] = 0x07;
        bytes[2] = 0xfe;
        b.set(bytes);
        const auto k = key(real, 2, bytes);
        const auto writes = s.programs;
        ids.fail = fault == 0;
        ids.zero = fault == 1;
        ids.collision = fault == 2;
        if (fault == 3)
            ids.next = 0;
        const auto r = bundle.begin(k, meta(), 8, 0);
        check(r.outcome == rb::Outcome::Rejected && r.error == rb::Error::Conflict &&
                  s.programs == writes,
              "random failure/zero/duplicate/existing ID no writes");
        check(ids.calls <= 2, "random collision no unbounded retry");
        ids.fail = ids.zero = ids.collision = false;
        ids.next = 4;
        check(bundle.begin(k, meta(), 8, 0).outcome == rb::Outcome::Prepared,
              "random failure healthy reuse");
        Fence f;
        for (unsigned i = 0; i <= k.chunks; ++i) {
            auto q = bundle.step(f);
            check(q.outcome == (i < k.chunks ? rb::Outcome::Progress : rb::Outcome::Committed),
                  "random healthy reuse actual commit");
        }
    }
    std::cout << "HASH_RANDOM 9\n";
}

void fullSlotsFence() {
    Hash h;
    const auto baseline = catalogMedium(h);
    for (unsigned extra : {59u, 60u}) {
        auto medium = baseline;
        bundle_review::Session session(medium);
        ProgressJournal low(session);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "full slots low open");
        for (unsigned i = 1; i <= extra; ++i)
            auxiliary(low, h, i);
        Buffers b;
        Ids ids;
        ids.next = 1;
        rb::Journal bundle(low, h, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered && sink.auxiliaries == extra,
              "actual occupied slot reducer");
        b.set({0});
        const auto programs = session.programs;
        const auto r = bundle.begin(key(h, 2, {0}), meta(), 8, 0);
        check(r.outcome == (extra == 59 ? rb::Outcome::Prepared : rb::Outcome::Deferred) &&
                  session.programs == programs,
              "physical 63 slots / +1 before writes");
        if (extra == 59) {
            Fence f;
            check(bundle.step(f).outcome == rb::Outcome::Progress &&
                      bundle.step(f).outcome == rb::Outcome::Committed &&
                      low.snapshot().records == 63,
                  "physical slot 63 valid unique Final");
        }
    }
    for (bool final : {false, true}) {
        auto medium = baseline;
        bundle_review::Session session(medium);
        ProgressJournal low(session);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "fence open");
        Buffers b;
        Ids ids;
        ids.next = 1;
        rb::Journal bundle(low, h, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "fence index");
        b.set({0});
        check(bundle.begin(key(h, 2, {0}), meta(), 8, 0).outcome == rb::Outcome::Prepared,
              "fence prepare");
        Fence f;
        if (final)
            check(bundle.step(f).outcome == rb::Outcome::Progress, "fence prior chunk");
        f.allowed = false;
        const auto programs = session.programs;
        const auto r = bundle.step(f);
        check(r.outcome == rb::Outcome::Rejected && r.error == rb::Error::Fence &&
                  session.programs == programs && !bundle.active() && !bundle.unresolved(),
              "revocation before current physical write");
        session.loseSession();
        reopened(medium, true, 0);
    }
    for (unsigned bad = 0; bad < 5; ++bad) {
        auto medium = baseline;
        bundle_review::Session session(medium);
        ProgressJournal low(session);
        check(low.recover(origin()).outcome == ProgressOutcome::Recovered, "limits open");
        Buffers b;
        Ids ids;
        ids.next = 1;
        rb::Journal bundle(low, h, ids, b.work);
        Consumer sink;
        check(bundle.recover(sink).outcome == rb::Outcome::Recovered, "limits index");
        std::vector<std::uint8_t> bytes(bad == 0 ? 32769 : 1, 0);
        b.set(bytes);
        auto k = key(h, 2, bytes);
        const auto programs = session.programs;
        if (bad == 2)
            b.work.recovered.data = b.work.staged.data;
        auto r = bundle.begin(k, meta(), bad == 1 ? 9 : 8, bad == 3 ? 64 : 0);
        if (bad < 4) {
            check((r.outcome == rb::Outcome::Rejected || r.outcome == rb::Outcome::Deferred) &&
                      session.programs == programs && ids.calls == 0,
                  "size/issues/overlap/reserved limit before IO or randomness");
        } else {
            check(r.outcome == rb::Outcome::Prepared, "immutable staged prepare");
            b.stage[0] ^= 1;
            Fence f;
            r = bundle.step(f);
            check(r.outcome == rb::Outcome::Rejected && r.error == rb::Error::DigestMismatch &&
                      session.programs == programs,
                  "staged mutation never writes");
        }
    }
    std::cout << "SLOTS_FENCE_LIMITS 9\n";
}
void faults() {
    capacities();
    postwrite();
    callbacks();
    randomHash();
    fullSlotsFence();
    cuts();
}
