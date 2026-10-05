#pragma once
#include "DeviceRuntime/Plan.h"
#include <type_traits>
namespace larp {
namespace devices {
enum class ControlKind : std::uint8_t { Park, Begin, Component, Commit, Resume };
enum class ControlOutcome : std::uint8_t { Applied, Rejected, Failed };
struct ControlTicket {
    std::uint32_t generation = 0, sequence = 0;
    Owner owner = Owner::Nfc;
    ControlKind kind = ControlKind::Park;
};
bool sameControlTicket(const ControlTicket &a, const ControlTicket &b);
struct Control {
    ControlTicket ticket;
    std::uint32_t profileRevision = 0;
    std::uint8_t expectedComponents = 0, componentSlot = 0, peerChannel = 0;
    std::array<I2cBus, 2> i2c = {};
    configuration::Component component; // Full settings copy, never a Profile pointer.
};
struct ControlReply {
    ControlTicket ticket;
    ControlOutcome outcome = ControlOutcome::Failed;
    std::uint32_t completedMilliseconds = 0;
    std::uint16_t detail = 0;
};
static_assert(std::is_trivially_copyable<Control>::value, "Control must be copied");
static_assert(std::is_trivially_copyable<ControlReply>::value, "Control reply must be copied");
class IControlChannel {
  public:
    virtual ~IControlChannel() = default;
    // One application producer, FIFO per owner, value copies, zero wait, no
    // callbacks. Owners reply ONLY after the requested driver work completed.
    virtual bool sendControl(const Control &control) noexcept = 0;
    virtual bool receiveControlReply(ControlReply &reply) noexcept = 0;
};
const char *controlKindName(ControlKind value);
} // namespace devices
} // namespace larp
