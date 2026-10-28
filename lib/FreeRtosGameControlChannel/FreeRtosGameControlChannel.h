#pragma once
#include "Companion/CommandIngress.h"
#include <freertos/FreeRTOS.h>
namespace larp {
namespace companion {
// Process-lifetime bounded value storage; begin before tasks/callbacks. No
// blocking API, allocation or ISR use. One short spinlock covers authority and
// all copied FIFO/mailbox state; no FreeRTOS API is called while it is held. Trusted transport
// owner attaches principal and exclusively drains/routes replies by principal AND connectionEpoch.
// Application alone receive/send/publish.
class FreeRtosGameControlChannel : public IGameControlChannel,
                                   public ICommandIngress,
                                   public IAuthorityPublisher {
  public:
    FreeRtosGameControlChannel() = default;
    FreeRtosGameControlChannel(const FreeRtosGameControlChannel &) = delete;
    FreeRtosGameControlChannel &operator=(const FreeRtosGameControlChannel &) = delete;
    bool begin();
    // Single verified transport owner; observer/disconnect also advance epoch.
    AuthorityUpdate updateAuthority(const TrustedAuthority &authority);
    AuthorityUpdate advanceAuthority(std::uint32_t principal, bool gameMaster,
                                     TrustedAuthority &published) noexcept override;
    AuthorityUpdate advanceAuthorityIfCurrent(const TrustedAuthority &expected,
                                              std::uint32_t principal, bool gameMaster,
                                              TrustedAuthority &published) noexcept override;
    AuthorityUpdate closeAuthorityIfCurrent(const TrustedAuthority &expected,
                                            TrustedAuthority &published) noexcept override;
    void suspendAuthority() noexcept override;
    bool readAuthority(TrustedAuthority &authority) const noexcept override;
    bool receive(TrustedGameRequest &request) noexcept override;
    bool send(const RoutedGameReply &reply) noexcept override;
    bool publish(const RoutedConfiguredStatus &status) noexcept override;
    bool submit(const TrustedGameRequest &request);
    // Complete immutable message from a trusted authenticated transport owner.
    // principal and epoch come from that specific connection, NEVER from JSON.
    // Admission rechecks the current authority after decode. Decode success is
    // not execution; a full queue leaves the command unadmitted for exact retry.
    GameSubmission submitJson(const char *bytes, std::size_t count,
                              std::uint32_t authenticatedPrincipal,
                              std::uint32_t connectionEpoch) noexcept override;
    // Raw drain for diagnostics/tests; does not select a current network recipient.
    bool takeReply(RoutedGameReply &reply);
    // Sole verified transport owner: at most Queue copied FIFO reads under
    // the authority lock, discarding stale/unavailable recipients. False preserves
    // destination. Selection is an observation point, not physical-send completion.
    // Recheck the bound epoch in the actual bearer before any later network send.
    bool takeCurrentReply(RoutedGameReply &reply);
    // Raw diagnostic snapshot, NOT a current verified network recipient.
    bool readStatus(ConfiguredStatus &status) const;
    // Latest copied status only if the producer's original principal+epoch
    // still match the current GM. False preserves destination. One copied
    // snapshot under authority lock; bearer must recheck before physical sending.
    bool readCurrentStatus(RoutedConfiguredStatus &status) const;

  private:
    // Access only under authorityMutex_. Fixed capacity and trivial value copies
    // bound the work; this is deliberately not a second RTOS synchronization layer.
    template <typename T> struct ValueFifo {
        T values[ControlLimits::Queue] = {};
        std::size_t head = 0, count = 0;
        bool push(const T &value) noexcept {
            if (count == ControlLimits::Queue)
                return false;
            values[(head + count) % ControlLimits::Queue] = value;
            ++count;
            return true;
        }
        bool pop(T &value) noexcept {
            if (!count)
                return false;
            value = values[head];
            head = (head + 1) % ControlLimits::Queue;
            --count;
            return true;
        }
    };
    ValueFifo<TrustedGameRequest> commands_;
    ValueFifo<RoutedGameReply> replies_;
    RoutedConfiguredStatus status_;
    bool statusAvailable_ = false;
    mutable portMUX_TYPE authorityMutex_ = portMUX_INITIALIZER_UNLOCKED;
    AuthorityState authority_;
    bool started_ = false;
    GameSubmissionOutcome admit(const TrustedGameRequest &request);
};
} // namespace companion
} // namespace larp
