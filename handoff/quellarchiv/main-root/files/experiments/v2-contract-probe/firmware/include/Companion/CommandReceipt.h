#pragma once
#include "Companion/CommandFrameQueue.h"
#include "Companion/ReplyFrameSender.h"
namespace larp {
namespace companion {
// Fixed single-value processing receipt, not a fragment or GATT write ack.
struct CommandReceiptLimits {
    static const std::uint8_t Kind = 3;
    static const std::size_t Bytes = 18;
};
struct CommandReceipt {
    CommandStreamOutcome outcome = CommandStreamOutcome::Idle;
    CommandFramePosition input;
    std::uint16_t message = 0, total = 0, received = 0;
};
bool encodeCommandReceipt(const CommandReceipt &receipt, char *bytes, std::size_t capacity,
                          std::size_t &written) noexcept;
FrameDecodeResult decodeCommandReceipt(const char *bytes, std::size_t count,
                                       CommandReceipt &destination) noexcept;
enum class ReceiptCopyOutcome : std::uint8_t {
    NoFrame,
    Copied,
    Busy,
    Unauthorized,
    InvalidReceipt,
    TransportUnavailable
};
// Same sole transport owner as dispatch. At most one zero-wait bearer copy.
// No retained receipt, implicit retry, command execution or application tick.
// Busy/lost receipt never means that the already processed command was undone.
ReceiptCopyOutcome copyCommandReceipt(const FrameDispatchResult &dispatch,
                                      const IGameControlChannel &authority,
                                      IReplyFrameBearer &bearer) noexcept;
} // namespace companion
} // namespace larp
