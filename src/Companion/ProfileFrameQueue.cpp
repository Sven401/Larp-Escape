#include "Companion/ProfileFrameQueue.h"
namespace larp {
namespace companion {
namespace {
class Call {
  public:
    explicit Call(bool &busy) : busy_(busy) {
        busy_ = true;
    }
    ~Call() {
        busy_ = false;
    }

  private:
    bool &busy_;
};
} // namespace
ProfileDispatchResult ProfileFramePump::service() noexcept {
    ProfileDispatchResult result;
    if (busy_) {
        result.outcome = ProfileReceiveOutcome::Busy;
        return result;
    }
    Call call(busy_);
    result.outcome = source_.service();
    if (result.outcome == ProfileReceiveOutcome::Busy)
        return result;
    CopiedProfileFrame frame;
    if (!queue_.take(frame))
        return result;
    result.received = true;
    result.principal = frame.principal;
    result.epoch = frame.epoch;
    result.receivedAt = frame.receivedAt;
    if (clock_.nowMilliseconds() - frame.receivedAt >= ProfileSourceLimits::DeadlineMilliseconds) {
        result.outcome = ProfileReceiveOutcome::Expired;
        return result;
    }
    TrustedAuthority original;
    original.principal = frame.principal;
    original.epoch = frame.epoch;
    original.gameMaster = true; // Trusted queue attribution, never wire metadata.
    ProfileSourceEnvelope envelope;
    ProfileDataFrame data;
    if (decodeProfileSourceEnvelope(frame.bytes.data(), frame.count, envelope)) {
        result.metadata = true;
        result.transfer = envelope.transfer;
        result.outcome = source_.start(envelope, original, frame.receivedAt);
    } else if (decodeProfileDataFrame(frame.bytes.data(), frame.count, data)) {
        result.transfer = data.transfer;
        result.offset = data.offset;
        result.payloadBytes = static_cast<std::uint16_t>(data.count);
        result.outcome = source_.data(frame.bytes.data(), frame.count, original);
    } else {
        result.outcome = ProfileReceiveOutcome::InvalidFrame;
    }
    return result;
}
} // namespace companion
} // namespace larp
