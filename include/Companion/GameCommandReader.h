#pragma once
#include "Companion/ConfiguredProtocol.h"
namespace larp {
namespace companion {
// Maximum complete message bytes, excluding a caller's optional NUL. Decoder
// never reads that terminator; count is authoritative. No fragments/stream I/O.
constexpr std::size_t maxGameCommandDocumentBytes = 1024;
enum class CommandField : std::uint8_t {
    None,
    SchemaVersion,
    Boot,
    Lease,
    Request,
    ExpectedConfigurationRevision,
    ExpectedStateRevision,
    Kind,
    Target
};
enum class CommandDecodeError : std::uint8_t {
    None,
    InvalidArgument,
    MessageTooLarge,
    UnexpectedEnd,
    UnexpectedToken,
    UnknownField,
    DuplicateField,
    MissingField,
    InvalidText,
    TextTooLong,
    InvalidBoot,
    InvalidKind,
    InvalidInteger,
    IntegerRange,
    UnsupportedVersion,
    InvalidLease,
    InvalidRequest,
    InvalidTarget,
    TrailingData
};
struct CommandDecodeResult {
    CommandDecodeError error = CommandDecodeError::None;
    CommandField field = CommandField::None;
    // Zero-based byte at rejection; EOF=count. Semantic checks may report the
    // position AFTER a parsed value. No input text is copied into diagnostics.
    std::size_t offset = 0;
    explicit operator bool() const {
        return error == CommandDecodeError::None;
    }
};
const char *commandFieldName(CommandField field);
const char *commandDecodeErrorName(CommandDecodeError error);
// Flat exact v2 object, all eight fields required, arbitrary order. ASCII wire
// names/values accept equivalent JSON escapes (including ASCII \uXXXX). No
// nested values, client principal/role, duplicate or unknown fields. Numbers are
// canonical unsigned decimal digit tokens: no signs, fractions or exponents.
// No heap/recursion, bounded by count. Caller keeps bytes stable for this call.
// Failure preserves destination. Success is decoding ONLY, not authorization,
// queue admission, revision validation or execution.
CommandDecodeResult decodeGameCommand(const char *bytes, std::size_t count,
                                      GameCommand &destination);
} // namespace companion
} // namespace larp
