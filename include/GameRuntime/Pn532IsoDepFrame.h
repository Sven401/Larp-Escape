#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace runtime {

// Raw, unpadded PN532 *normal* response frames. The transport must report the
// actual number of bytes read, including DCS and postamble. An ACK, a padded
// 64-byte SPI read, an extended frame, or a partly received frame is rejected.
enum class IsoDepFrameResult : std::uint8_t {
    Ok, NoTarget, UnsupportedTarget, InvalidFrame, DeviceStatus, WrongLength,
    InsufficientCapacity
};

struct IsoDepTarget {
    std::array<std::uint8_t, 10> uid = {};
    std::uint8_t uidLength = 0;
    std::array<std::uint8_t, 48> ats = {};
    std::uint8_t atsLength = 0;
};

inline bool validPn532NormalResponse(const std::uint8_t *frame,
                                     std::size_t received,
                                     std::uint8_t responseCode,
                                     std::size_t &dataLength) {
    dataLength = 0;
    if (!frame || received < 10 || received > 64 || frame[0] != 0 ||
        frame[1] != 0 || frame[2] != 0xff)
        return false;
    const std::size_t length = frame[3];
    if (length < 3 || length > 57 || length + 7 != received ||
        static_cast<std::uint8_t>(frame[3] + frame[4]) != 0 ||
        frame[5] != 0xd5 || frame[6] != responseCode ||
        frame[length + 6] != 0)
        return false;
    std::uint8_t sum = 0;
    for (std::size_t i = 5; i <= length + 5; ++i)
        sum = static_cast<std::uint8_t>(sum + frame[i]);
    if (sum != 0)
        return false;
    dataLength = length;
    return true;
}

// InListPassiveTarget for MaxTg=1, BrTy=106-kbps Type A. The ISO-DEP bit of
// SEL_RES requires the PN532 to append a complete ATS (TL, T0, ...).
inline IsoDepFrameResult parseIsoDepTarget(const std::uint8_t *frame,
                                          std::size_t received,
                                          IsoDepTarget &out) {
    out = IsoDepTarget();
    std::size_t length = 0;
    if (!validPn532NormalResponse(frame, received, 0x4b, length))
        return IsoDepFrameResult::InvalidFrame;
    if (frame[7] == 0)
        return length == 3 ? IsoDepFrameResult::NoTarget
                           : IsoDepFrameResult::InvalidFrame;
    if (frame[7] != 1 || length < 14 || frame[8] != 1)
        return IsoDepFrameResult::InvalidFrame;
    const std::size_t uidLength = frame[12];
    if (uidLength != 4 && uidLength != 7 && uidLength != 10)
        return IsoDepFrameResult::UnsupportedTarget;
    if (length < 8 + uidLength + 2)
        return IsoDepFrameResult::InvalidFrame;
    const std::size_t atsLength = length - 8 - uidLength;
    if (atsLength > out.ats.size() || frame[13 + uidLength] != atsLength)
        return IsoDepFrameResult::InvalidFrame;
    if ((frame[11] & 0x20) == 0)
        return IsoDepFrameResult::UnsupportedTarget;
    for (std::size_t i = 0; i < uidLength; ++i)
        out.uid[i] = frame[13 + i];
    for (std::size_t i = 0; i < atsLength; ++i)
        out.ats[i] = frame[13 + uidLength + i];
    out.uidLength = static_cast<std::uint8_t>(uidLength);
    out.atsLength = static_cast<std::uint8_t>(atsLength);
    return IsoDepFrameResult::Ok;
}

// InDataExchange response: D5 41 Status APDU... . A nonzero status, PN532
// chaining/NAD flag, an unexpected APDU length or a small output buffer fails
// closed. In particular, SW1/SW2 alone is a valid *length*, not a success SW.
inline IsoDepFrameResult parseIsoDepExchange(const std::uint8_t *frame,
                                             std::size_t received,
                                             std::size_t expectedApduLength,
                                             std::uint8_t *out,
                                             std::size_t capacity) {
    if (out && capacity)
        for (std::size_t i = 0; i < capacity; ++i) out[i] = 0;
    std::size_t length = 0;
    if (!validPn532NormalResponse(frame, received, 0x41, length))
        return IsoDepFrameResult::InvalidFrame;
    if (frame[7] != 0)
        return IsoDepFrameResult::DeviceStatus;
    const std::size_t apduLength = length - 3;
    if (expectedApduLength < 2 || expectedApduLength > 21 ||
        apduLength != expectedApduLength)
        return IsoDepFrameResult::WrongLength;
    if (!out || capacity < apduLength)
        return IsoDepFrameResult::InsufficientCapacity;
    for (std::size_t i = 0; i < apduLength; ++i)
        out[i] = frame[8 + i];
    return IsoDepFrameResult::Ok;
}

// Bounded fake for reader-ticket lifetime. This is a pure host-side contract;
// it neither drives the PN532 nor grants an app/firmware diagnostic role.
class IsoDepFrameSession {
public:
    enum class Stage : std::uint8_t { Idle, AwaitTarget, AwaitExchange, Complete, Failed };

    bool begin(std::uint32_t ticket, std::uint32_t nowMs, std::uint32_t timeoutMs) {
        clear();
        if (!ticket || !timeoutMs || timeoutMs > 30000)
            return false;
        ticket_ = ticket;
        startedMs_ = nowMs;
        timeoutMs_ = timeoutMs;
        stage_ = Stage::AwaitTarget;
        return true;
    }

    IsoDepFrameResult onTarget(std::uint32_t ticket, std::uint32_t nowMs,
                               const std::uint8_t *frame, std::size_t received) {
        if (stage_ != Stage::AwaitTarget || ticket != ticket_ || expired(nowMs)) {
            fail();
            return IsoDepFrameResult::InvalidFrame;
        }
        IsoDepTarget parsed;
        const auto result = parseIsoDepTarget(frame, received, parsed);
        if (result != IsoDepFrameResult::Ok) {
            fail();
            return result;
        }
        target_ = parsed;
        stage_ = Stage::AwaitExchange;
        return result;
    }

    IsoDepFrameResult onExchange(std::uint32_t ticket, std::uint32_t nowMs,
                                 const std::uint8_t *frame, std::size_t received,
                                 std::size_t expected, std::uint8_t *out,
                                 std::size_t capacity) {
        if (stage_ != Stage::AwaitExchange || ticket != ticket_ || expired(nowMs)) {
            if (out && capacity)
                for (std::size_t i = 0; i < capacity; ++i) out[i] = 0;
            fail();
            return IsoDepFrameResult::InvalidFrame;
        }
        const auto result = parseIsoDepExchange(frame, received, expected, out, capacity);
        if (result != IsoDepFrameResult::Ok) {
            fail();
            return result;
        }
        target_ = IsoDepTarget();
        stage_ = Stage::Complete;
        return result;
    }

    void tick(std::uint32_t nowMs) {
        if ((stage_ == Stage::AwaitTarget || stage_ == Stage::AwaitExchange) &&
            expired(nowMs)) fail();
    }
    void clear() {
        target_ = IsoDepTarget();
        ticket_ = startedMs_ = timeoutMs_ = 0;
        stage_ = Stage::Idle;
    }
    Stage stage() const { return stage_; }
    bool target(IsoDepTarget &out) const {
        out = IsoDepTarget();
        if (stage_ != Stage::AwaitExchange) return false;
        out = target_;
        return true;
    }

private:
    bool expired(std::uint32_t nowMs) const {
        return static_cast<std::uint32_t>(nowMs - startedMs_) > timeoutMs_;
    }
    void fail() {
        target_ = IsoDepTarget();
        ticket_ = 0;
        stage_ = Stage::Failed;
    }
    IsoDepTarget target_ = {};
    std::uint32_t ticket_ = 0, startedMs_ = 0, timeoutMs_ = 0;
    Stage stage_ = Stage::Idle;
};

} // namespace runtime
} // namespace larp
