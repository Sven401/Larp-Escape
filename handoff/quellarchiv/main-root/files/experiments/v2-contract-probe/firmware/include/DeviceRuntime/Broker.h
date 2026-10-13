#pragma once
#include "DeviceRuntime/Channel.h"
namespace larp {
namespace devices {
struct BrokerSnapshot {
    std::uint32_t generation = 0, accepted = 0, rejected = 0, received = 0, staleReplies = 0,
                  timedOut = 0, lastSequence = 0;
    std::uint8_t pendingReads = 0;
};
// Application-owner adapter for Session. Only copied channel traffic crosses
// tasks. It owns no driver and never blocks on a hardware read or queue wait.
class Broker : public execution::IProfileDevices {
  public:
    explicit Broker(IChannel &channel) : channel_(channel) {}
    Broker(const Broker &) = delete;
    Broker &operator=(const Broker &) = delete;
    // Plan must come from successful compilePlan and remain unmodified.
    // Rebinding requires a stopped Session, all device owners quiescent, and a
    // newer generation. Owners must adopt it before accepting new commands.
    bool configure(const Plan &plan, std::uint32_t generation);
    void service(); // At most ReplyDrainPerService replies; call before each tick.
    BrokerSnapshot snapshot() const;
    bool sample(const configuration::Component &component,
                execution::DeviceSample &value) noexcept override;
    PollOutcome pollCard(const execution::PollRequest &request,
                         domain::CardUid &uid) noexcept override;
    PollOutcome pollCrystals(const execution::PollRequest &request,
                             ports::CrystalInputBatch &batch) noexcept override;
    ActionOutcome playTrack(ComponentId component, std::uint16_t track,
                            const execution::ActionContext &context) noexcept override;
    ActionOutcome wakeAudio(ComponentId component,
                            const execution::ActionContext &context) noexcept override;
    ActionOutcome applyEffect(const configuration::Effect &effect, configuration::ActionId binding,
                              const execution::ActionContext &context) noexcept override;
    ActionOutcome sendActivation(ComponentId component,
                                 const execution::ActionContext &context) noexcept override;

  private:
    struct Pending {
        Reply reply;
        std::uint32_t issued = 0;
        bool active = false, ready = false;
    };
    IChannel &channel_;
    Plan plan_;
    std::array<Pending, Limits::Components> pending_ = {};
    std::array<std::uint32_t, Limits::Components> playbackReservations_ = {};
    BrokerSnapshot status_;
    bool configured_ = false, servicing_ = false;
    std::uint8_t slot(ComponentId id) const;
    bool issue(Command &command);
    PollOutcome poll(const execution::PollRequest &request, CommandKind kind, Reply &reply);
    ActionOutcome output(Command &command, Owner owner);
};
} // namespace devices
} // namespace larp
