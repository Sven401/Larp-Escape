#include "DeviceRuntime/ProfileJournal.h"
#include <cstring>
#include <limits>
namespace larp {
namespace devices {
namespace {
using configuration::ReadStatus;
const std::size_t HeaderBytes = 32, FooterBytes = 24;
const char HeaderMagic[] = "LARPPRF1", FooterMagic[] = "LARPCMT1";
struct Record {
    JournalOpen open = JournalOpen::Missing;
    bool valid = false, unsupported = false, ioFailure = false;
    std::uint32_t generation = 0, count = 0, digest = 0;
    std::array<std::uint8_t, HeaderBytes> header = {};
};
std::uint32_t crcByte(std::uint32_t crc, std::uint8_t byte) {
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    return crc;
}
std::uint32_t digest(const std::uint8_t *bytes, std::size_t count) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t index = 0; index < count; ++index)
        crc = crcByte(crc, bytes[index]);
    return crc ^ 0xffffffffu;
}
std::uint32_t get(const std::uint8_t *bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}
void put(std::uint8_t *bytes, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index)
        bytes[index] = static_cast<std::uint8_t>(value >> (index * 8));
}
ReadStatus read(configuration::IProfileReader &source, std::uint8_t *bytes, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        const auto status = source.readByte(bytes[index]);
        if (status != ReadStatus::Byte)
            return status;
    }
    return ReadStatus::Byte;
}
bool decode(Record &record) {
    const auto *bytes = record.header.data();
    if (std::memcmp(bytes, HeaderMagic, 8) || get(bytes + 28) != digest(bytes, 28))
        return false;
    if (get(bytes + 8) != 1) {
        record.unsupported = true;
        return false;
    }
    record.generation = get(bytes + 12);
    record.count = get(bytes + 16);
    record.digest = get(bytes + 20);
    return record.generation && record.count &&
           record.count <= configuration::ImportLimits::DocumentBytes && !get(bytes + 24);
}
bool footerMatches(const std::uint8_t *bytes, const Record &record) {
    return !std::memcmp(bytes, FooterMagic, 8) && get(bytes + 8) == record.generation &&
           get(bytes + 12) == record.digest && get(bytes + 16) == record.count &&
           get(bytes + 20) == digest(bytes, 20);
}
Record inspect(IJournalStore &store, std::uint8_t slot) {
    Record result;
    result.open = store.openRead(slot);
    if (result.open != JournalOpen::Present) {
        result.ioFailure = result.open == JournalOpen::Failure;
        return result;
    }
    auto status = read(store, result.header.data(), result.header.size());
    if (status == ReadStatus::Byte && decode(result)) {
        std::uint32_t crc = 0xffffffffu;
        for (std::uint32_t index = 0; index < result.count; ++index) {
            std::uint8_t byte = 0;
            status = store.readByte(byte);
            if (status != ReadStatus::Byte)
                break;
            crc = crcByte(crc, byte);
        }
        std::array<std::uint8_t, FooterBytes> footer = {};
        if (status == ReadStatus::Byte && (crc ^ 0xffffffffu) == result.digest) {
            status = read(store, footer.data(), footer.size());
            if (status == ReadStatus::Byte && footerMatches(footer.data(), result)) {
                std::uint8_t extra = 0;
                status = store.readByte(extra);
                result.valid = status == ReadStatus::End;
            }
        }
    }
    result.ioFailure = status == ReadStatus::Failure;
    if (!store.closeRead())
        result.ioFailure = true;
    return result;
}
// Ends the JSON stream only AFTER envelope/footer/EOF and close all succeeded.
// Therefore successful import never leaves a ready draft on a close/CRC failure.
class RecordReader : public configuration::IProfileReader {
  public:
    RecordReader(IJournalStore &store, const Record &record) : store_(store), record_(record) {}
    ~RecordReader() {
        if (!closed_ && !store_.closeRead())
            ioFailure_ = true;
    }
    ReadStatus readByte(std::uint8_t &byte) override {
        if (closed_)
            return good_ ? ReadStatus::End : ReadStatus::Failure;
        if (count_ < record_.count) {
            const auto status = store_.readByte(byte);
            if (status == ReadStatus::Byte) {
                ++count_;
                crc_ = crcByte(crc_, byte);
                return status;
            }
            ioFailure_ = status == ReadStatus::Failure;
            close();
            return ReadStatus::Failure;
        }
        std::array<std::uint8_t, FooterBytes> footer = {};
        auto status = read(store_, footer.data(), footer.size());
        good_ = status == ReadStatus::Byte && footerMatches(footer.data(), record_) &&
                (crc_ ^ 0xffffffffu) == record_.digest;
        if (good_) {
            status = store_.readByte(byte);
            good_ = status == ReadStatus::End;
        }
        ioFailure_ = status == ReadStatus::Failure;
        close();
        return good_ ? ReadStatus::End : ReadStatus::Failure;
    }
    void close() {
        if (!closed_) {
            if (!store_.closeRead()) {
                ioFailure_ = true;
                good_ = false;
            }
            closed_ = true;
        }
    }
    bool ioFailure() const {
        return ioFailure_;
    }

  private:
    IJournalStore &store_;
    const Record &record_;
    std::uint32_t count_ = 0, crc_ = 0xffffffffu;
    bool closed_ = false, good_ = false, ioFailure_ = false;
};
class DigestWriter : public configuration::IProfileWriter {
  public:
    explicit DigestWriter(IJournalStore *store = nullptr) : store_(store) {}
    bool write(const char *bytes, std::size_t count) override {
        if ((!bytes && count) || count > configuration::ImportLimits::DocumentBytes - count_)
            return false;
        if (store_ && !store_->write(bytes, count))
            return false;
        for (std::size_t index = 0; index < count; ++index)
            crc_ = crcByte(crc_, static_cast<std::uint8_t>(bytes[index]));
        count_ += static_cast<std::uint32_t>(count);
        return true;
    }
    std::uint32_t count() const {
        return count_;
    }
    std::uint32_t digest() const {
        return crc_ ^ 0xffffffffu;
    }

  private:
    IJournalStore *store_;
    std::uint32_t count_ = 0, crc_ = 0xffffffffu;
};
bool write(IJournalStore &store, const std::uint8_t *bytes, std::size_t count) {
    return store.write(reinterpret_cast<const char *>(bytes), count);
}
DraftResult rejected(DraftOutcome outcome) {
    DraftResult result;
    result.outcome = outcome;
    return result;
}
} // namespace
const char *journalOutcomeName(JournalOutcome value) {
    static const char *const names[] = {"restored",
                                        "saved",
                                        "empty",
                                        "busy",
                                        "controller_rejected",
                                        "needs_restore",
                                        "corrupt",
                                        "unsupported",
                                        "ambiguous",
                                        "io_failure",
                                        "write_uncertain",
                                        "counter_exhausted"};
    const unsigned index = static_cast<unsigned>(value);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown_journal_outcome";
}
JournalResult ProfileJournal::finish(JournalOutcome outcome, const DraftResult &result) {
    state_.operating = false;
    state_.last = outcome;
    JournalResult value;
    value.outcome = outcome;
    value.controller = result;
    return value;
}
JournalResult ProfileJournal::restoreDraft(std::uint32_t expectedRevision, bool older) {
    if (state_.operating) {
        JournalResult result;
        result.outcome = JournalOutcome::Busy;
        return result;
    }
    const auto storage = controller_.storage();
    if (controller_.busy() || !storage.initialized || controller_.gameRunning() ||
        expectedRevision != storage.draftRevision ||
        storage.draftRevision == std::numeric_limits<std::uint32_t>::max()) {
        DraftOutcome outcome = DraftOutcome::Busy;
        if (!storage.initialized)
            outcome = DraftOutcome::Uninitialized;
        else if (controller_.gameRunning())
            outcome = DraftOutcome::GameRunning;
        else if (expectedRevision != storage.draftRevision)
            outcome = DraftOutcome::StaleRevision;
        else if (!controller_.busy())
            outcome = DraftOutcome::CounterExhausted;
        return finish(JournalOutcome::ControllerRejected, rejected(outcome));
    }
    const auto previous = state_;
    state_.operating = true;
    state_.initialized = state_.writable = false;
    const std::array<Record, 2> records = {{inspect(store_, 0), inspect(store_, 1)}};
    if (records[0].ioFailure || records[1].ioFailure)
        return finish(JournalOutcome::IoFailure);
    const bool unsupported = records[0].unsupported || records[1].unsupported;
    state_.maximumGeneration = 0;
    for (const auto &record : records)
        if (record.valid && record.generation > state_.maximumGeneration)
            state_.maximumGeneration = record.generation;
    if (records[0].valid && records[1].valid && records[0].generation == records[1].generation)
        return finish(JournalOutcome::Ambiguous);
    if (!records[0].valid && !records[1].valid) {
        if (unsupported)
            return finish(JournalOutcome::Unsupported);
        if (records[0].open != JournalOpen::Missing || records[1].open != JournalOpen::Missing)
            return finish(JournalOutcome::Corrupt);
        state_.initialized = state_.writable = true;
        // Missing previously confirmed data is not a fresh empty store.
        if (state_.protectedSlot != 255) {
            state_.initialized = state_.writable = false;
            return finish(JournalOutcome::Corrupt);
        }
        state_.generation = 0;
        state_.slot = 255;
        state_.selectedUnconfirmed = false;
        return finish(JournalOutcome::Empty);
    }
    // Explicit older reload is relative to the LAST selected valid record,
    // never an automatic reactivation after a failed hardware boot.
    const std::uint32_t ceiling = older ? previous.generation : 0;
    if (older && (!previous.initialized || previous.slot == 255))
        return finish(JournalOutcome::NeedsRestore);
    const std::uint8_t first =
        records[1].valid && (!records[0].valid || records[1].generation > records[0].generation)
            ? 1
            : 0;
    DraftResult last;
    for (unsigned pass = 0; pass < 2; ++pass) {
        const std::uint8_t slot = pass ? static_cast<std::uint8_t>(1 - first) : first;
        const auto &record = records[slot];
        if (!record.valid || (older && record.generation >= ceiling))
            continue;
        if (store_.openRead(slot) != JournalOpen::Present)
            return finish(JournalOutcome::IoFailure);
        std::array<std::uint8_t, HeaderBytes> header = {};
        const auto status = read(store_, header.data(), header.size());
        if (status != ReadStatus::Byte || header != record.header) {
            const bool closed = store_.closeRead();
            return finish(status == ReadStatus::Failure || !closed ? JournalOutcome::IoFailure
                                                                   : JournalOutcome::Corrupt);
        }
        RecordReader source(store_, record);
        last = controller_.importDraft(source, controller_.storage().draftRevision);
        source.close();
        if (source.ioFailure())
            return finish(JournalOutcome::IoFailure, last);
        if (last) {
            state_.slot = slot;
            state_.generation = record.generation;
            state_.selectedUnconfirmed = state_.writeUncertain && slot != state_.protectedSlot;
            if (!state_.writeUncertain)
                state_.protectedSlot = slot;
            state_.initialized = true;
            state_.writable = !unsupported;
            return finish(JournalOutcome::Restored, last);
        }
        if (last.outcome != DraftOutcome::ImportFailed)
            return finish(JournalOutcome::ControllerRejected, last);
    }
    return finish(unsupported ? JournalOutcome::Unsupported : JournalOutcome::Corrupt, last);
}
JournalResult ProfileJournal::saveActive(std::uint32_t expectedRevision) {
    if (state_.operating) {
        JournalResult result;
        result.outcome = JournalOutcome::Busy;
        return result;
    }
    if (!state_.initialized || !state_.writable)
        return finish(JournalOutcome::NeedsRestore);
    if (state_.maximumGeneration == std::numeric_limits<std::uint32_t>::max())
        return finish(JournalOutcome::CounterExhausted);
    const auto active = controller_.activation();
    if (active.phase != ActivationPhase::Ready || !active.hardwareReady ||
        active.finish != ActivationFinish::Applied)
        return finish(JournalOutcome::ControllerRejected,
                      rejected(DraftOutcome::ActivationRejected));
    state_.operating = true;
    DigestWriter measured;
    const auto measuredResult = controller_.writeActive(measured, expectedRevision);
    if (!measuredResult)
        return finish(JournalOutcome::ControllerRejected, measuredResult);
    // A readable but unconfirmed new selection must not make the old confirmed
    // record the next destructive target. Retry always keeps that old slot.
    const std::uint8_t slot =
        state_.protectedSlot == 255 ? 0 : static_cast<std::uint8_t>(1 - state_.protectedSlot);
    const std::uint32_t generation = state_.maximumGeneration + 1;
    std::array<std::uint8_t, HeaderBytes> header = {};
    std::memcpy(header.data(), HeaderMagic, 8);
    put(header.data() + 8, 1);
    put(header.data() + 12, generation);
    put(header.data() + 16, measured.count());
    put(header.data() + 20, measured.digest());
    put(header.data() + 28, digest(header.data(), 28));
    // Once a destructive open is attempted, any failure needs a full rescan.
    state_.initialized = state_.writable = false;
    state_.writeUncertain = true;
    if (!store_.beginWrite(slot)) {
        store_.abortWrite();
        return finish(JournalOutcome::WriteUncertain);
    }
    DigestWriter output(&store_);
    DraftResult serialized = rejected(DraftOutcome::OutputFailed);
    bool good = write(store_, header.data(), header.size());
    if (good) {
        serialized = controller_.writeActive(output, expectedRevision);
        good = static_cast<bool>(serialized) && output.count() == measured.count() &&
               output.digest() == measured.digest();
    }
    std::array<std::uint8_t, FooterBytes> footer = {};
    std::memcpy(footer.data(), FooterMagic, 8);
    put(footer.data() + 8, generation);
    put(footer.data() + 12, measured.digest());
    put(footer.data() + 16, measured.count());
    put(footer.data() + 20, digest(footer.data(), 20));
    if (!good || !write(store_, footer.data(), footer.size())) {
        store_.abortWrite();
        return finish(JournalOutcome::WriteUncertain, serialized);
    }
    if (store_.finishWrite() != JournalCommit::Confirmed)
        return finish(JournalOutcome::WriteUncertain, serialized);
    state_.slot = state_.protectedSlot = slot;
    state_.generation = state_.maximumGeneration = generation;
    state_.writeUncertain = state_.selectedUnconfirmed = false;
    state_.initialized = state_.writable = true;
    return finish(JournalOutcome::Saved, serialized);
}
} // namespace devices
} // namespace larp
