#include "DeviceRuntime/ProgressJournal.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace larp {
namespace devices {
namespace {
constexpr std::size_t CrcOffset = 4088;
constexpr std::size_t MarkerOffset = 4092;
constexpr std::size_t PayloadOffset = 128;
const std::uint8_t HeaderMagic[] = {'L', 'A', 'R', 'P', 'D', '1', 'H', '1'};
const std::uint8_t RecordMagic[] = {'L', 'A', 'R', 'P', 'D', '1', 'R', '1'};
const std::uint8_t Marker[] = {'D', 'O', 'N', 'E'};

std::uint64_t get(const std::uint8_t *p, std::size_t n) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < n; ++i)
        value |= static_cast<std::uint64_t>(p[i]) << (i * 8);
    return value;
}
void put(std::uint8_t *p, std::uint64_t value, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        p[i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::uint32_t crc(const std::uint8_t *p, std::size_t n, bool header = false) {
    std::uint32_t value = 0xffffffffU;
    for (std::size_t i = 0; i < n; ++i) {
        // The only mutable header bytes are its one-shot reservation anchors.
        // A record's predecessor binds the stable header CRC, not these anchors.
        value ^= header && i >= 80 && i < 143 ? 0xff : p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xedb88320U : 0U);
    }
    return ~value;
}
bool erased(const std::uint8_t *p, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
        if (p[i] != 0xff)
            return false;
    return true;
}
template <std::size_t N> bool nonzero(const std::array<std::uint8_t, N> &id) {
    for (auto byte : id)
        if (byte)
            return true;
    return false;
}
bool validOrigin(const ProgressOrigin &origin) {
    return nonzero(origin.event) && nonzero(origin.box) && nonzero(origin.epoch);
}
bool validKind(ProgressKind kind) {
    const auto value = static_cast<unsigned>(kind);
    return value >= 1 && value <= 5;
}
ProgressOutcome envelope(const std::uint8_t *p, const std::uint8_t *magic) {
    if (std::memcmp(p, magic, 8) != 0)
        return ProgressOutcome::Corrupt;
    if (get(p + 8, 2) != 1)
        return ProgressOutcome::Unsupported;
    if (std::memcmp(p + MarkerOffset, Marker, sizeof(Marker)) != 0 ||
        get(p + CrcOffset, 4) != crc(p, CrcOffset, magic == HeaderMagic))
        return ProgressOutcome::Corrupt;
    return ProgressOutcome::Read;
}
bool decodeRecord(const std::uint8_t *p, std::uint8_t index, ProgressRecord &record) {
    record = ProgressRecord{};
    record.kind = static_cast<ProgressKind>(p[10]);
    record.issueCount = p[11];
    record.payloadBytes = static_cast<std::uint16_t>(get(p + 12, 2));
    record.revision = get(p + 16, 8);
    record.expectedRevision = get(p + 32, 8);
    record.expectedLastIssued = get(p + 40, 8);
    std::copy(p + 48, p + 64, record.operationId.begin());
    std::copy(p + 64, p + 96, record.requestDigest.begin());
    record.profileRevision = get(p + 96, 8);
    record.trustRevision = get(p + 104, 8);
    record.index = index;
    if (!validKind(record.kind) || record.issueCount > ProgressLimits::FactsPerCommit ||
        !nonzero(record.operationId) || record.payloadBytes > ProgressLimits::PayloadBytes ||
        !erased(p + 14, 2) || !erased(p + 116, PayloadOffset - 116) ||
        !erased(p + PayloadOffset + record.payloadBytes,
                CrcOffset - PayloadOffset - record.payloadBytes) ||
        record.expectedRevision != index ||
        record.revision != static_cast<std::uint64_t>(index) + 1 ||
        record.expectedLastIssued > std::numeric_limits<std::uint64_t>::max() - record.issueCount ||
        get(p + 24, 8) != record.expectedLastIssued + record.issueCount)
        return false;
    if (record.issueCount) {
        record.firstIssued = record.expectedLastIssued + 1;
        record.lastIssued = record.expectedLastIssued + record.issueCount;
    }
    return true;
}
bool sameRequest(const ProgressRecord &record, const ProgressRequest &request,
                 const std::uint8_t *payload) {
    return record.operationId == request.operationId &&
           record.requestDigest == request.requestDigest && record.kind == request.kind &&
           record.expectedRevision == request.expectedRevision &&
           record.expectedLastIssued == request.expectedLastIssued &&
           record.profileRevision == request.profileRevision &&
           record.trustRevision == request.trustRevision &&
           record.issueCount == request.issueCount && record.payloadBytes == request.payloadBytes &&
           (!record.payloadBytes ||
            std::memcmp(payload, request.payload, record.payloadBytes) == 0);
}
ProgressResult immediate(ProgressOutcome outcome) {
    ProgressResult result;
    result.outcome = outcome;
    return result;
}
} // namespace

ProgressResult ProgressJournal::finish(ProgressOutcome outcome) {
    state_.last = outcome;
    state_.operating = false;
    if (outcome == ProgressOutcome::Corrupt || outcome == ProgressOutcome::Unsupported ||
        outcome == ProgressOutcome::IoFailure || outcome == ProgressOutcome::WriteUncertain) {
        state_.ready = false;
        state_.poisoned = true;
    }
    return immediate(outcome);
}
ProgressResult ProgressJournal::finishRecord(ProgressOutcome outcome,
                                             const ProgressRecord &record) {
    auto result = finish(outcome);
    result.revision = record.revision;
    result.firstIssued = record.firstIssued;
    result.lastIssued = record.lastIssued;
    result.index = record.index;
    return result;
}

ProgressOutcome ProgressJournal::scan(const ProgressOrigin &origin) {
    const bool hadConfirmedState = knownState_;
    const auto confirmedRecords = state_.records;
    const auto confirmedIssued = state_.lastIssued;
    const auto confirmedCrc = tailCrc_;
    state_.ready = false;
    if (knownState_ && (origin.event != state_.origin.event || origin.box != state_.origin.box ||
                        origin.epoch != state_.origin.epoch))
        return ProgressOutcome::WrongOrigin;
    if (store_.capacity() != ProgressLimits::StorageBytes)
        return ProgressOutcome::Unsupported;
    if (!store_.read(0, scratch_.data(), scratch_.size()))
        return ProgressOutcome::IoFailure;
    const bool empty = erased(scratch_.data(), scratch_.size());
    ProgressOrigin stored;
    std::uint32_t previous = 0;
    std::uint8_t reserved = 0;
    if (!empty) {
        const auto result = envelope(scratch_.data(), HeaderMagic);
        if (result != ProgressOutcome::Read)
            return result;
        const auto *p = scratch_.data();
        if (!erased(p + 10, 2) || get(p + 12, 4) != ProgressLimits::SlotBytes ||
            get(p + 16, 4) != ProgressLimits::Slots ||
            get(p + 20, 4) != ProgressLimits::PayloadBytes || !erased(p + 24, 8) ||
            !erased(p + 143, CrcOffset - 143))
            return ProgressOutcome::Corrupt;
        // Each anchor is programmed exactly once. Zero bytes form one prefix.
        bool freeAnchor = false;
        for (std::size_t index = 0; index < ProgressLimits::Records; ++index) {
            const auto anchor = p[80 + index];
            if ((anchor != 0 && anchor != 0xff) || (anchor == 0 && freeAnchor))
                return ProgressOutcome::Corrupt;
            if (anchor == 0)
                ++reserved;
            else
                freeAnchor = true;
        }
        std::copy(p + 32, p + 48, stored.event.begin());
        std::copy(p + 48, p + 64, stored.box.begin());
        std::copy(p + 64, p + 80, stored.epoch.begin());
        if (!validOrigin(stored))
            return ProgressOutcome::Corrupt;
        if (stored.event != origin.event || stored.box != origin.box ||
            stored.epoch != origin.epoch)
            return ProgressOutcome::WrongOrigin;
        previous = static_cast<std::uint32_t>(get(p + CrcOffset, 4));
        if (hadConfirmedState && confirmedRecords == 0 && previous != confirmedCrc)
            return ProgressOutcome::Corrupt;
    }
    bool gap = false;
    std::uint8_t count = 0;
    std::uint64_t issued = 0;
    for (std::size_t slot = 1; slot < ProgressLimits::Slots; ++slot) {
        if (!store_.read(slot * ProgressLimits::SlotBytes, scratch_.data(), scratch_.size()))
            return ProgressOutcome::IoFailure;
        if (erased(scratch_.data(), scratch_.size())) {
            gap = true;
            continue;
        }
        if (empty || gap)
            return ProgressOutcome::Corrupt;
        const auto result = envelope(scratch_.data(), RecordMagic);
        if (result != ProgressOutcome::Read)
            return result;
        ProgressRecord record;
        if (!decodeRecord(scratch_.data(), count, record) || record.expectedLastIssued != issued ||
            get(scratch_.data() + 112, 4) != previous)
            return ProgressOutcome::Corrupt;
        for (std::size_t i = 0; i < count; ++i)
            if (operationIds_[i] == record.operationId)
                return ProgressOutcome::Corrupt;
        operationIds_[count] = record.operationId;
        ++count;
        issued += record.issueCount;
        previous = static_cast<std::uint32_t>(get(scratch_.data() + CrcOffset, 4));
        if (hadConfirmedState && count == confirmedRecords && previous != confirmedCrc)
            return ProgressOutcome::Corrupt;
    }
    if (empty)
        return hadConfirmedState ? ProgressOutcome::Corrupt : ProgressOutcome::Empty;
    if (count != reserved ||
        (hadConfirmedState && (count < confirmedRecords || issued < confirmedIssued)))
        return ProgressOutcome::Corrupt;
    state_.origin = stored;
    state_.revision = count;
    state_.records = count;
    state_.lastIssued = issued;
    state_.ready = true;
    knownState_ = true;
    tailCrc_ = previous;
    return ProgressOutcome::Recovered;
}

bool ProgressJournal::writeSlot(std::size_t slot) {
    const auto offset = slot * ProgressLimits::SlotBytes;
    // Marker is a separate final program after body durability. No operation
    // after the first program may be retried in this instance on uncertainty.
    return store_.program(offset, scratch_.data(), MarkerOffset) && store_.sync() &&
           store_.program(offset + MarkerOffset, Marker, sizeof(Marker)) && store_.sync();
}

bool ProgressJournal::reserveRecord(std::uint8_t index) {
    // Each byte is programmed once, from FF to 00, without overlapping a write.
    // The store guarantees programming does not change any unrequested bytes.
    const std::uint8_t value = 0;
    return store_.program(80 + index, &value, 1) && store_.sync();
}

ProgressResult ProgressJournal::initialize(const ProgressOrigin &origin) {
    if (state_.operating)
        return immediate(ProgressOutcome::Busy);
    if (state_.poisoned)
        return immediate(state_.last);
    if (!validOrigin(origin))
        return immediate(ProgressOutcome::Invalid);
    if (state_.ready)
        return immediate(ProgressOutcome::Conflict);
    state_.operating = true;
    const auto scanned = scan(origin);
    if (scanned != ProgressOutcome::Empty) {
        state_.ready = false;
        return finish(scanned == ProgressOutcome::Recovered ? ProgressOutcome::Conflict : scanned);
    }
    scratch_.fill(0xff);
    auto *p = scratch_.data();
    std::copy(HeaderMagic, HeaderMagic + 8, p);
    put(p + 8, 1, 2);
    put(p + 12, ProgressLimits::SlotBytes, 4);
    put(p + 16, ProgressLimits::Slots, 4);
    put(p + 20, ProgressLimits::PayloadBytes, 4);
    std::copy(origin.event.begin(), origin.event.end(), p + 32);
    std::copy(origin.box.begin(), origin.box.end(), p + 48);
    std::copy(origin.epoch.begin(), origin.epoch.end(), p + 64);
    put(p + CrcOffset, crc(p, CrcOffset, true), 4);
    if (!writeSlot(0))
        return finish(ProgressOutcome::WriteUncertain);
    const auto verified = scan(origin);
    if (verified != ProgressOutcome::Recovered)
        return finish(verified == ProgressOutcome::Empty ? ProgressOutcome::Corrupt : verified);
    return finish(ProgressOutcome::Initialized);
}

ProgressResult ProgressJournal::recover(const ProgressOrigin &origin) {
    if (state_.operating)
        return immediate(ProgressOutcome::Busy);
    if (state_.poisoned)
        return immediate(state_.last);
    if (!validOrigin(origin))
        return immediate(ProgressOutcome::Invalid);
    state_.operating = true;
    return finish(scan(origin));
}

ProgressOutcome ProgressJournal::loadRecord(std::uint8_t index, ProgressRecord &record) {
    if (!store_.read((static_cast<std::size_t>(index) + 1) * ProgressLimits::SlotBytes,
                     scratch_.data(), scratch_.size()))
        return ProgressOutcome::IoFailure;
    const auto result = envelope(scratch_.data(), RecordMagic);
    if (result != ProgressOutcome::Read)
        return result;
    if (!decodeRecord(scratch_.data(), index, record) || operationIds_[index] != record.operationId)
        return ProgressOutcome::Corrupt;
    return ProgressOutcome::Read;
}

ProgressResult ProgressJournal::commit(const ProgressRequest &request,
                                       IProgressCommitFence &fence) {
    if (state_.operating)
        return immediate(ProgressOutcome::Busy);
    if (state_.poisoned)
        return immediate(state_.last);
    if (!state_.ready)
        return immediate(ProgressOutcome::NeedsRecovery);
    if (!nonzero(request.operationId) || !validKind(request.kind) ||
        request.issueCount > ProgressLimits::FactsPerCommit ||
        request.payloadBytes > ProgressLimits::PayloadBytes ||
        (request.payloadBytes && request.payload == nullptr))
        return immediate(ProgressOutcome::Invalid);
    state_.operating = true;
    const auto origin = state_.origin;
    const auto scanned = scan(origin);
    if (scanned != ProgressOutcome::Recovered)
        return finish(scanned == ProgressOutcome::Empty ? ProgressOutcome::Corrupt : scanned);
    for (std::uint8_t i = 0; i < state_.records; ++i) {
        if (operationIds_[i] != request.operationId)
            continue;
        ProgressRecord previous;
        const auto loaded = loadRecord(i, previous);
        if (loaded != ProgressOutcome::Read)
            return finish(loaded);
        if (!sameRequest(previous, request, scratch_.data() + PayloadOffset))
            return finish(ProgressOutcome::Conflict);
        return finishRecord(ProgressOutcome::Duplicate, previous);
    }
    if (request.expectedRevision != state_.revision ||
        request.expectedLastIssued != state_.lastIssued)
        return finish(ProgressOutcome::Stale);
    if (state_.records == ProgressLimits::Records)
        return finish(ProgressOutcome::Full);
    if (state_.revision == std::numeric_limits<std::uint64_t>::max() ||
        state_.lastIssued > std::numeric_limits<std::uint64_t>::max() - request.issueCount)
        return finish(ProgressOutcome::CounterExhausted);

    scratch_.fill(0xff);
    auto *p = scratch_.data();
    std::copy(RecordMagic, RecordMagic + 8, p);
    put(p + 8, 1, 2);
    p[10] = static_cast<std::uint8_t>(request.kind);
    p[11] = request.issueCount;
    put(p + 12, request.payloadBytes, 2);
    put(p + 16, state_.revision + 1, 8);
    put(p + 24, state_.lastIssued + request.issueCount, 8);
    put(p + 32, request.expectedRevision, 8);
    put(p + 40, request.expectedLastIssued, 8);
    std::copy(request.operationId.begin(), request.operationId.end(), p + 48);
    std::copy(request.requestDigest.begin(), request.requestDigest.end(), p + 64);
    put(p + 96, request.profileRevision, 8);
    put(p + 104, request.trustRevision, 8);
    put(p + 112, tailCrc_, 4);
    if (request.payloadBytes)
        std::copy(request.payload, request.payload + request.payloadBytes, p + PayloadOffset);
    put(p + CrcOffset, crc(p, CrcOffset), 4);
    ProgressRecord next;
    if (!decodeRecord(p, state_.records, next))
        return finish(ProgressOutcome::Invalid);
    if (!fence.allow(next))
        return finish(ProgressOutcome::Denied);
    if (!reserveRecord(next.index) || !writeSlot(static_cast<std::size_t>(next.index) + 1))
        return finish(ProgressOutcome::WriteUncertain);
    const auto verified = scan(origin);
    if (verified != ProgressOutcome::Recovered || state_.records != next.index + 1)
        return finish(verified == ProgressOutcome::Recovered || verified == ProgressOutcome::Empty
                          ? ProgressOutcome::Corrupt
                          : verified);
    return finishRecord(ProgressOutcome::Committed, next);
}

ProgressResult ProgressJournal::readRecord(std::uint8_t index, ProgressRecord &record,
                                           std::uint8_t *payload, std::size_t capacity) {
    record = ProgressRecord{};
    if (state_.operating)
        return immediate(ProgressOutcome::Busy);
    if (state_.poisoned)
        return immediate(state_.last);
    if (!state_.ready)
        return immediate(ProgressOutcome::NeedsRecovery);
    if (index >= state_.records)
        return immediate(ProgressOutcome::Invalid);
    state_.operating = true;
    const auto origin = state_.origin;
    const auto scanned = scan(origin);
    if (scanned != ProgressOutcome::Recovered)
        return finish(scanned == ProgressOutcome::Empty ? ProgressOutcome::Corrupt : scanned);
    ProgressRecord loaded;
    const auto result = loadRecord(index, loaded);
    if (result != ProgressOutcome::Read)
        return finish(result);
    if (capacity < loaded.payloadBytes || (loaded.payloadBytes && payload == nullptr))
        return finish(ProgressOutcome::Invalid);
    if (loaded.payloadBytes)
        std::copy(scratch_.data() + PayloadOffset,
                  scratch_.data() + PayloadOffset + loaded.payloadBytes, payload);
    record = loaded;
    return finishRecord(ProgressOutcome::Read, loaded);
}

const char *progressOutcomeName(ProgressOutcome outcome) {
    switch (outcome) {
    case ProgressOutcome::Initialized:
        return "initialized";
    case ProgressOutcome::Recovered:
        return "recovered";
    case ProgressOutcome::Committed:
        return "committed";
    case ProgressOutcome::Duplicate:
        return "duplicate";
    case ProgressOutcome::Read:
        return "read";
    case ProgressOutcome::Empty:
        return "empty";
    case ProgressOutcome::NeedsRecovery:
        return "needs_recovery";
    case ProgressOutcome::Busy:
        return "busy";
    case ProgressOutcome::Invalid:
        return "invalid";
    case ProgressOutcome::WrongOrigin:
        return "wrong_origin";
    case ProgressOutcome::Stale:
        return "stale";
    case ProgressOutcome::Denied:
        return "denied";
    case ProgressOutcome::Conflict:
        return "conflict";
    case ProgressOutcome::Full:
        return "full";
    case ProgressOutcome::CounterExhausted:
        return "counter_exhausted";
    case ProgressOutcome::Corrupt:
        return "corrupt";
    case ProgressOutcome::Unsupported:
        return "unsupported";
    case ProgressOutcome::IoFailure:
        return "io_failure";
    case ProgressOutcome::WriteUncertain:
        return "write_uncertain";
    }
    return "invalid";
}

} // namespace devices
} // namespace larp
