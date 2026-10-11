#pragma once

#include "GameRuntime/Pn532IsoDepFrame.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace larp {
namespace runtime {

// A port implementation must return the complete *normal* response frame and
// its actual byte count. It must bound ACK and response waits together by
// timeoutMs. The pinned Adafruit PN532 1.3.4 API does not meet this contract.
class Pn532RawPort {
public:
    virtual ~Pn532RawPort() = default;
    virtual bool command(const std::uint8_t *request, std::size_t requestLength,
                         std::uint32_t timeoutMs, std::uint8_t *frame,
                         std::size_t frameCapacity, std::size_t &received) = 0;
};

class Pn532Clock {
public:
    virtual ~Pn532Clock() = default;
    virtual std::uint32_t nowMs() const = 0;
};

// One-use transport for the harmless capability APDUs. The owner must hold an
// exclusive reader ticket and keep the game parked for the entire run. This
// class provides no PN532 driver, CSPRNG, role, or diagnostic permission.
class Pn532IsoDepAdapter final {
public:
    enum class State : std::uint8_t {
        Idle, AwaitTarget, AwaitSelect, AwaitChallenge, Complete, Failed
    };
    enum class Result : std::uint8_t {
        Ok, NoTarget, Busy, InvalidState, InvalidInput, Timeout,
        PortFailure, InvalidFrame, UnsupportedTarget, DeviceStatus,
        WrongLength, InsufficientCapacity, InvalidApdu
    };

    Pn532IsoDepAdapter(Pn532RawPort &port, const Pn532Clock &clock)
        : port_(port), clock_(clock) {}

    bool begin(std::uint32_t ticket, bool exclusiveReader, bool gameParked,
               std::uint32_t durationMs) {
        clear();
        if (!ticket || !exclusiveReader || !gameParked || !durationMs ||
            durationMs > 30000U) return false;
        ticket_ = ticket;
        startMs_ = clock_.nowMs();
        durationMs_ = durationMs;
        state_ = State::AwaitTarget;
        return true;
    }

    Result pollTarget(std::uint32_t ticket, bool exclusiveReader,
                      bool gameParked, IsoDepTarget &out) {
        out = IsoDepTarget();
        if (!owned(ticket, exclusiveReader, gameParked, State::AwaitTarget))
            return fail(Result::Busy);
        constexpr std::uint8_t command[] = {0x4a, 0x01, 0x00};
        std::array<std::uint8_t, 64> frame = {};
        std::size_t received = 0;
        const auto result = issue(command, sizeof(command), frame, received);
        if (result != Result::Ok) return fail(result);
        IsoDepTarget parsed;
        const auto parsedResult = parseIsoDepTarget(frame.data(), received, parsed);
        if (parsedResult == IsoDepFrameResult::NoTarget) return Result::NoTarget;
        if (parsedResult != IsoDepFrameResult::Ok)
            return fail(map(parsedResult));
        out = parsed;
        state_ = State::AwaitSelect;
        return Result::Ok;
    }

    Result selectApplication(std::uint32_t ticket, bool exclusiveReader,
                             bool gameParked, std::uint8_t *out,
                             std::size_t capacity) {
        constexpr std::uint8_t select[] = {
            0x00, 0xa4, 0x04, 0x00, 0x08, 0xf0, 0x4c, 0x41,
            0x52, 0x50, 0x4e, 0x46, 0x43, 0x00
        };
        const auto result = exchange(ticket, exclusiveReader, gameParked,
                                     State::AwaitSelect, State::AwaitChallenge,
                                     select, sizeof(select), out, capacity);
        if (result != Result::Ok) return result;
        bool nonzeroRunId = false;
        for (std::size_t i = 3; i < 19; ++i) nonzeroRunId |= out[i] != 0;
        if (!validApduPrefixAndStatus(out) || !nonzeroRunId) {
            wipe(out, capacity);
            return fail(Result::InvalidApdu);
        }
        return Result::Ok;
    }

    Result challenge(std::uint32_t ticket, bool exclusiveReader, bool gameParked,
                     const std::uint8_t *nonce, std::size_t nonceLength,
                     std::uint8_t *out, std::size_t capacity) {
        if (!nonce || nonceLength != 16) {
            wipe(out, capacity);
            return fail(Result::InvalidInput);
        }
        std::array<std::uint8_t, 21> apdu = {0x80,0x10,0x00,0x00,0x10};
        for (std::size_t i = 0; i < nonceLength; ++i) apdu[5+i] = nonce[i];
        const auto result = exchange(ticket, exclusiveReader, gameParked,
                                     State::AwaitChallenge, State::Complete,
                                     apdu.data(), apdu.size(), out, capacity);
        if (result != Result::Ok) return result;
        std::uint8_t difference = 0;
        for (std::size_t i = 0; i < nonceLength; ++i)
            difference |= static_cast<std::uint8_t>(out[i+3] ^ nonce[i]);
        if (!validApduPrefixAndStatus(out) || difference) {
            wipe(out, capacity);
            return fail(Result::InvalidApdu);
        }
        return Result::Ok;
    }

    void cancel() { clear(); }
    State state() const { return state_; }

private:
    static void wipe(std::uint8_t *out, std::size_t capacity) {
        if (out) for (std::size_t i = 0; i < capacity; ++i) out[i] = 0;
    }
    static bool validApduPrefixAndStatus(const std::uint8_t *apdu) {
        return apdu[0] == 0x4e && apdu[1] == 0x43 && apdu[2] == 0x01 &&
               apdu[19] == 0x90 && apdu[20] == 0x00;
    }
    static Result map(IsoDepFrameResult result) {
        switch (result) {
        case IsoDepFrameResult::NoTarget: return Result::NoTarget;
        case IsoDepFrameResult::UnsupportedTarget: return Result::UnsupportedTarget;
        case IsoDepFrameResult::DeviceStatus: return Result::DeviceStatus;
        case IsoDepFrameResult::WrongLength: return Result::WrongLength;
        case IsoDepFrameResult::InsufficientCapacity: return Result::InsufficientCapacity;
        default: return Result::InvalidFrame;
        }
    }
    bool expired() const {
        return static_cast<std::uint32_t>(clock_.nowMs() - startMs_) >= durationMs_;
    }
    bool owned(std::uint32_t ticket, bool exclusiveReader, bool gameParked,
               State required) const {
        return state_ == required && ticket == ticket_ && ticket != 0 &&
               exclusiveReader && gameParked;
    }
    Result fail(Result result) {
        ticket_ = 0;
        state_ = State::Failed;
        return result;
    }
    Result issue(const std::uint8_t *command, std::size_t commandLength,
                 std::array<std::uint8_t, 64> &frame, std::size_t &received) {
        if (expired()) return Result::Timeout;
        const std::uint32_t remaining = durationMs_ -
            static_cast<std::uint32_t>(clock_.nowMs() - startMs_);
        const std::uint32_t timeout = remaining < 1000U ? remaining : 1000U;
        if (!timeout || !port_.command(command, commandLength, timeout,
                                       frame.data(), frame.size(), received))
            return expired() ? Result::Timeout : Result::PortFailure;
        if (expired()) return Result::Timeout;
        if (received > frame.size()) return Result::InvalidFrame;
        return Result::Ok;
    }
    Result exchange(std::uint32_t ticket, bool exclusiveReader, bool gameParked,
                    State required, State next, const std::uint8_t *apdu,
                    std::size_t apduLength, std::uint8_t *out,
                    std::size_t capacity) {
        wipe(out, capacity);
        if (!owned(ticket, exclusiveReader, gameParked, required))
            return fail(Result::Busy);
        if (!out || capacity < 21 || !apdu || apduLength > 21)
            return fail(Result::InsufficientCapacity);
        std::array<std::uint8_t, 23> command = {};
        command[0] = 0x40;
        command[1] = 0x01; // Tg=1 from validated InListPassiveTarget
        for (std::size_t i = 0; i < apduLength; ++i) command[i+2] = apdu[i];
        std::array<std::uint8_t, 64> frame = {};
        std::size_t received = 0;
        const auto result = issue(command.data(), apduLength+2, frame, received);
        if (result != Result::Ok) return fail(result);
        const auto parsed = parseIsoDepExchange(frame.data(), received, 21,
                                                out, capacity);
        if (parsed != IsoDepFrameResult::Ok) return fail(map(parsed));
        state_ = next;
        return Result::Ok;
    }
    void clear() {
        ticket_ = startMs_ = durationMs_ = 0;
        state_ = State::Idle;
    }

    Pn532RawPort &port_;
    const Pn532Clock &clock_;
    std::uint32_t ticket_ = 0, startMs_ = 0, durationMs_ = 0;
    State state_ = State::Idle;
};

} // namespace runtime
} // namespace larp
