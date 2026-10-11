#include "CompanionIntegration/NfcCapabilityV1.h"

namespace larp {
namespace nfc_capability {
namespace {
bool populated(const Nonce &value) noexcept {
    for (std::uint8_t byte : value) if (byte != 0) return true;
    return false;
}

bool responseMatches(const std::uint8_t *bytes, std::size_t size,
                     const Nonce &expected) noexcept {
    if (!bytes || size != 21 || bytes[0] != 0x4e || bytes[1] != 0x43 ||
        bytes[2] != 0x01 || bytes[19] != 0x90 || bytes[20] != 0x00)
        return false;
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (bytes[3 + i] != expected[i]) return false;
    return true;
}
} // namespace

SelectFrame selectFrame() noexcept {
    return {{0x00, 0xa4, 0x04, 0x00, 0x08, 0xf0, 0x4c, 0x41,
             0x52, 0x50, 0x4e, 0x46, 0x43, 0x00}};
}

ChallengeFrame challengeFrame(const Nonce &challenge) noexcept {
    ChallengeFrame result = {{0x80, 0x10, 0x00, 0x00, 0x10}};
    for (std::size_t i = 0; i < challenge.size(); ++i)
        result[5 + i] = challenge[i];
    return result;
}

bool validSelectResponse(const std::uint8_t *bytes, std::size_t size,
                         Nonce &runId) noexcept {
    if (!bytes || size != 21 || bytes[0] != 0x4e || bytes[1] != 0x43 ||
        bytes[2] != 0x01 || bytes[19] != 0x90 || bytes[20] != 0x00)
        return false;
    Nonce parsed = {};
    for (std::size_t i = 0; i < parsed.size(); ++i) parsed[i] = bytes[3 + i];
    if (!populated(parsed)) return false;
    runId = parsed;
    return true;
}

bool validEchoResponse(const std::uint8_t *bytes, std::size_t size,
                       const Nonce &challenge) noexcept {
    return responseMatches(bytes, size, challenge);
}

bool Probe::begin(const ReaderContext &context, const Nonce &challenge, std::uint64_t nowMs,
                  std::uint32_t durationMs) noexcept {
    if (result_ != Result::NotStarted) return false;
    if (!context.exclusiveReader || !context.gameParked) {
        result_ = Result::ReaderBusy;
        return false;
    }
    if (!context.ownerTicket || !context.readerSession || !context.bleGeneration ||
        !durationMs || durationMs > 30000 || !populated(challenge)) {
        result_ = Result::InvalidStart;
        return false;
    }
    context_ = context;
    challenge_ = challenge;
    startedMs_ = nowMs;
    durationMs_ = durationMs;
    result_ = Result::AwaitSelect;
    return true;
}

bool Probe::current(const ReaderContext &context, std::uint64_t nowMs) noexcept {
    if (result_ != Result::AwaitSelect && result_ != Result::AwaitEcho) return false;
    if (!context.exclusiveReader || !context.gameParked ||
        context.ownerTicket != context_.ownerTicket) result_ = Result::OwnerChanged;
    else if (context.readerSession != context_.readerSession)
        result_ = Result::SessionChanged;
    else if (context.bleGeneration != context_.bleGeneration)
        result_ = Result::BleGenerationChanged;
    else if (nowMs < startedMs_ || nowMs - startedMs_ >= durationMs_)
        result_ = Result::Timeout;
    return result_ == Result::AwaitSelect || result_ == Result::AwaitEcho;
}

bool Probe::onSelectResponse(const ReaderContext &context, std::uint64_t nowMs,
                             const std::uint8_t *bytes, std::size_t size) noexcept {
    if (!current(context, nowMs)) return false;
    if (result_ != Result::AwaitSelect) {
        result_ = Result::MalformedSelect;
        return false;
    }
    Nonce receivedRunId = {};
    if (!validSelectResponse(bytes, size, receivedRunId) ||
        receivedRunId == challenge_) {
        result_ = Result::MalformedSelect;
        return false;
    }
    runId_ = receivedRunId;
    result_ = Result::AwaitEcho;
    return true;
}

bool Probe::onEchoResponse(const ReaderContext &context, std::uint64_t nowMs,
                           const std::uint8_t *bytes, std::size_t size) noexcept {
    if (!current(context, nowMs)) return false;
    if (result_ != Result::AwaitEcho) {
        result_ = Result::MalformedEcho;
        return false;
    }
    if (!validEchoResponse(bytes, size, challenge_)) {
        result_ = Result::MalformedEcho;
        return false;
    }
    result_ = Result::ProbeEchoOk;
    return true;
}

void Probe::deactivate() noexcept {
    if (result_ == Result::AwaitSelect || result_ == Result::AwaitEcho)
        result_ = Result::Deactivated;
}

void Probe::tick(const ReaderContext &context, std::uint64_t nowMs) noexcept {
    current(context, nowMs);
}
} // namespace nfc_capability
} // namespace larp
