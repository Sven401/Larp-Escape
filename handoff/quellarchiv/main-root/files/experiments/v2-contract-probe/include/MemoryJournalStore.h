#pragma once
#include "DeviceRuntime/ProfileJournal.h"
#include <algorithm>
#include <limits>
#include <string>
namespace host_journal {
// Native-only bytes and deterministic fault points, no filesystem durability claim.
// Test access is to fake backend records; private firmware profiles stay private.
class MemoryStore : public larp::devices::IJournalStore {
  public:
    using JournalOpen = larp::devices::JournalOpen;
    using JournalCommit = larp::devices::JournalCommit;
    using ReadStatus = larp::configuration::ReadStatus;
    bool failOpen[2] = {}, failTruncate = false, uncertainFinish = false;
    std::size_t failWriteAt = std::numeric_limits<std::size_t>::max(),
                failReadAt = std::numeric_limits<std::size_t>::max(), mutateOffset = 0;
    std::uint8_t failReadSlot = 255, lastWriteSlot = 255;
    unsigned openCalls = 0, closeCalls = 0, writeCalls = 0, failCloseCall = 0, mutateOpenCall = 0;
    std::string &record(std::uint8_t slot) {
        return records_[slot];
    }
    bool present(std::uint8_t slot) const {
        return present_[slot];
    }
    void replace(std::uint8_t slot, const std::string &bytes) {
        records_[slot] = bytes;
        present_[slot] = true;
    }
    void erase(std::uint8_t slot) {
        records_[slot].clear();
        present_[slot] = false;
    }
    void clearFaults() {
        failOpen[0] = failOpen[1] = failTruncate = uncertainFinish = false;
        failWriteAt = failReadAt = std::numeric_limits<std::size_t>::max();
        failReadSlot = lastWriteSlot = 255;
        openCalls = closeCalls = writeCalls = failCloseCall = mutateOpenCall = 0;
    }
    JournalOpen openRead(std::uint8_t slot) override {
        ++openCalls;
        if (open_ || slot > 1 || failOpen[slot])
            return JournalOpen::Failure;
        if (!present_[slot])
            return JournalOpen::Missing;
        if (mutateOpenCall == openCalls && mutateOffset < records_[slot].size())
            records_[slot][mutateOffset] ^= 1;
        slot_ = slot;
        position_ = 0;
        open_ = true;
        writing_ = false;
        return JournalOpen::Present;
    }
    bool closeRead() override {
        ++closeCalls;
        const bool okay = open_ && !writing_ && closeCalls != failCloseCall;
        open_ = false;
        return okay;
    }
    ReadStatus readByte(std::uint8_t &byte) override {
        if (!open_ || writing_ || (slot_ == failReadSlot && position_ == failReadAt))
            return ReadStatus::Failure;
        if (position_ == records_[slot_].size())
            return ReadStatus::End;
        byte = static_cast<std::uint8_t>(records_[slot_][position_++]);
        return ReadStatus::Byte;
    }
    bool beginWrite(std::uint8_t slot) override {
        if (open_ || slot > 1)
            return false;
        ++writeCalls;
        lastWriteSlot = slot;
        slot_ = slot;
        records_[slot].clear();
        present_[slot] = true;
        if (failTruncate)
            return false; // Requested slot may already have changed.
        open_ = writing_ = true;
        return true;
    }
    bool write(const char *bytes, std::size_t count) override {
        if (!open_ || !writing_)
            return false;
        const auto left =
            records_[slot_].size() < failWriteAt ? failWriteAt - records_[slot_].size() : 0;
        const auto copied = std::min(left, count);
        records_[slot_].append(bytes, copied);
        return copied == count; // A failing backend can have written a prefix.
    }
    JournalCommit finishWrite() override {
        const bool okay = open_ && writing_ && !uncertainFinish;
        open_ = writing_ = false;
        return okay ? JournalCommit::Confirmed : JournalCommit::Uncertain;
    }
    void abortWrite() override {
        open_ = writing_ = false;
    }

  private:
    std::string records_[2];
    bool present_[2] = {};
    std::size_t position_ = 0;
    std::uint8_t slot_ = 0;
    bool open_ = false, writing_ = false;
};
} // namespace host_journal
