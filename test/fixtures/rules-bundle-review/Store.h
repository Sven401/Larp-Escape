#pragma once
#include "DeviceRuntime/ProgressJournal.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace bundle_review {
using namespace larp::devices;
// Independent session/durable NOR model. It does not qualify real hardware.
struct Medium {
    std::vector<std::uint8_t> durable;
    Medium() : durable(ProgressLimits::StorageBytes, 0xff) {}
};
struct Session final : IProgressStore {
    Medium &medium;
    std::vector<std::uint8_t> visible;
    bool online = true, writeThrough = true, persistFailedSync = false;
    std::size_t reads = 0, programs = 0, syncs = 0, programmed = 0;
    std::size_t failRead = 0, failProgram = 0, failSync = 0, readAfterSync = 0;
    std::size_t budget = std::numeric_limits<std::size_t>::max();
    explicit Session(Medium &m) : medium(m), visible(m.durable) {}
    std::size_t capacity() const override {
        return visible.size();
    }
    bool read(std::size_t offset, std::uint8_t *out, std::size_t count) override {
        ++reads;
        if (failRead && reads == failRead)
            online = false;
        if (!online || offset > visible.size() || count > visible.size() - offset)
            return false;
        std::memcpy(out, visible.data() + offset, count);
        return true;
    }
    bool program(std::size_t offset, const std::uint8_t *in, std::size_t count) override {
        ++programs;
        if (!online || (failProgram && programs == failProgram) || offset > visible.size() ||
            count > visible.size() - offset) {
            online = false;
            return false;
        }
        const auto written = std::min(count, budget);
        for (std::size_t i = 0; i < written; ++i) {
            if (visible[offset + i] != 0xff) {
                online = false;
                return false;
            }
            visible[offset + i] &= in[i];
            if (writeThrough)
                medium.durable[offset + i] = visible[offset + i];
            ++programmed;
        }
        budget -= written;
        if (written != count)
            online = false;
        return written == count;
    }
    bool sync() override {
        ++syncs;
        if (!online)
            return false;
        if (failSync && syncs == failSync) {
            if (persistFailedSync)
                medium.durable = visible;
            online = false;
            return false;
        }
        medium.durable = visible;
        if (readAfterSync && syncs == readAfterSync)
            failRead = reads + 1;
        return true;
    }
    // Simulated process/power loss discards this session. Callers must construct
    // a NEW Session(medium); setting online back to true is deliberately absent.
    void loseSession() {
        online = false;
    }
};
} // namespace bundle_review
