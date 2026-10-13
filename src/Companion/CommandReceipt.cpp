#include "Companion/CommandReceipt.h"
namespace larp {
namespace companion {
namespace {
const CommandStreamOutcome wireOutcomes[] = {CommandStreamOutcome::Receiving,
                                             CommandStreamOutcome::DuplicateFragment,
                                             CommandStreamOutcome::Queued,
                                             CommandStreamOutcome::QueueFull,
                                             CommandStreamOutcome::DecodeRejected,
                                             CommandStreamOutcome::InvalidFrame,
                                             CommandStreamOutcome::UnsupportedFrame,
                                             CommandStreamOutcome::Unauthorized,
                                             CommandStreamOutcome::Busy,
                                             CommandStreamOutcome::OutOfOrder,
                                             CommandStreamOutcome::FragmentConflict,
                                             CommandStreamOutcome::Expired,
                                             CommandStreamOutcome::Uninitialized};
std::uint8_t code(CommandStreamOutcome outcome) {
    for (std::size_t i = 0; i < sizeof(wireOutcomes) / sizeof(wireOutcomes[0]); ++i)
        if (wireOutcomes[i] == outcome)
            return static_cast<std::uint8_t>(i + 1);
    return 0;
}
std::uint16_t read16(const char *bytes) {
    return static_cast<unsigned char>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[1])) << 8;
}
void write16(char *bytes, std::uint16_t value) {
    bytes[0] = static_cast<char>(value & 0xff);
    bytes[1] = static_cast<char>(value >> 8);
}
bool valid(const CommandReceipt &value) {
    const auto &input = value.input;
    if (!code(value.outcome) || !input.message || !input.total ||
        input.total > maxGameCommandDocumentBytes || !input.count ||
        input.count > CommandFrameLimits::Payload || input.offset >= input.total ||
        input.count > input.total - input.offset)
        return false;
    if ((!value.message && (value.total || value.received)) ||
        (value.message && (!value.total || value.total > maxGameCommandDocumentBytes ||
                           value.received > value.total)))
        return false;
    const bool same = value.message == input.message && value.total == input.total;
    switch (value.outcome) {
    case CommandStreamOutcome::Receiving:
    case CommandStreamOutcome::DuplicateFragment:
        return same && value.received && value.received < value.total;
    case CommandStreamOutcome::Queued:
    case CommandStreamOutcome::QueueFull:
    case CommandStreamOutcome::DecodeRejected:
    case CommandStreamOutcome::Uninitialized:
        return same && value.received == value.total;
    case CommandStreamOutcome::Busy:
        return value.message && value.message != input.message;
    case CommandStreamOutcome::FragmentConflict:
        return value.message == input.message;
    case CommandStreamOutcome::OutOfOrder:
        return !value.message || same;
    default:
        return true;
    }
}
} // namespace
bool encodeCommandReceipt(const CommandReceipt &receipt, char *bytes, std::size_t capacity,
                          std::size_t &written) noexcept {
    written = 0;
    if (bytes && capacity)
        bytes[0] = 0;
    if (!bytes || capacity < CommandReceiptLimits::Bytes || !valid(receipt))
        return false;
    bytes[0] = CommandFrameLimits::Version;
    bytes[1] = CommandReceiptLimits::Kind;
    bytes[2] = static_cast<char>(code(receipt.outcome));
    bytes[3] = 0; // Reserved, must remain zero in v1.
    write16(bytes + 4, receipt.input.message);
    write16(bytes + 6, receipt.input.total);
    write16(bytes + 8, receipt.input.offset);
    write16(bytes + 10, receipt.input.count);
    write16(bytes + 12, receipt.message);
    write16(bytes + 14, receipt.total);
    write16(bytes + 16, receipt.received);
    written = CommandReceiptLimits::Bytes;
    return true;
}
FrameDecodeResult decodeCommandReceipt(const char *bytes, std::size_t count,
                                       CommandReceipt &destination) noexcept {
    if (!bytes || count != CommandReceiptLimits::Bytes)
        return FrameDecodeResult::Invalid;
    if (static_cast<unsigned char>(bytes[0]) != CommandFrameLimits::Version ||
        static_cast<unsigned char>(bytes[1]) != CommandReceiptLimits::Kind || bytes[3] != 0)
        return FrameDecodeResult::Unsupported;
    const auto outcome = static_cast<unsigned char>(bytes[2]);
    if (!outcome || outcome > sizeof(wireOutcomes) / sizeof(wireOutcomes[0]))
        return FrameDecodeResult::Invalid;
    CommandReceipt value;
    value.outcome = wireOutcomes[outcome - 1];
    value.input.message = read16(bytes + 4);
    value.input.total = read16(bytes + 6);
    value.input.offset = read16(bytes + 8);
    value.input.count = read16(bytes + 10);
    value.message = read16(bytes + 12);
    value.total = read16(bytes + 14);
    value.received = read16(bytes + 16);
    if (!valid(value))
        return FrameDecodeResult::Invalid;
    destination = value;
    return FrameDecodeResult::Success;
}
ReceiptCopyOutcome copyCommandReceipt(const FrameDispatchResult &dispatch,
                                      const IGameControlChannel &authority,
                                      IReplyFrameBearer &bearer) noexcept {
    if (!dispatch.received || !dispatch.input.message)
        return ReceiptCopyOutcome::NoFrame;
    TrustedAuthority current;
    if (!dispatch.principal || !dispatch.epoch || !authority.readAuthority(current) ||
        !current.gameMaster || current.principal != dispatch.principal ||
        current.epoch != dispatch.epoch)
        return ReceiptCopyOutcome::Unauthorized;
    CommandReceipt receipt;
    receipt.input = dispatch.input;
    receipt.outcome = dispatch.stream.outcome;
    receipt.message = dispatch.stream.message;
    receipt.total = dispatch.stream.total;
    receipt.received = dispatch.stream.received;
    RoutedReplyFrame packet;
    packet.principal = dispatch.principal;
    packet.epoch = dispatch.epoch;
    std::size_t written = 0;
    if (!encodeCommandReceipt(receipt, packet.bytes.data(), packet.bytes.size(), written))
        return ReceiptCopyOutcome::InvalidReceipt;
    packet.count = static_cast<std::uint16_t>(written);
    switch (bearer.copyReplyFrame(packet)) {
    case ReplyFrameCopy::Copied:
        return ReceiptCopyOutcome::Copied;
    case ReplyFrameCopy::Busy:
        return ReceiptCopyOutcome::Busy;
    default:
        return ReceiptCopyOutcome::TransportUnavailable;
    }
}
} // namespace companion
} // namespace larp
