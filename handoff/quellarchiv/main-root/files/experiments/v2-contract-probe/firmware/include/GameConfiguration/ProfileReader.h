#pragma once
#include "GameConfiguration/Profile.h"
#include <cstddef>
#include <cstdint>

namespace larp {
namespace configuration {
struct ImportLimits {
    static const std::size_t DocumentBytes = 131072; // 128 KiB, including whitespace and escapes.
    static const std::size_t ContainerDepth = 8;
};
enum class ReadStatus : std::uint8_t { Byte, End, Failure };
class IProfileReader {
  public:
    virtual ~IProfileReader() = default;
    // Synchronous, bounded source. End is final; Failure is never interpreted as EOF.
    virtual ReadStatus readByte(std::uint8_t &byte) = 0;
};
enum class ImportError : std::uint8_t {
    None,
    InvalidArgument,
    AliasedStorage,
    InputFailure,
    UnexpectedEnd,
    UnexpectedToken,
    UnknownField,
    DuplicateField,
    MissingField,
    UnexpectedField,
    InvalidText,
    InvalidUtf8,
    InvalidUnicodeEscape,
    TextTooLong,
    InvalidEnum,
    NumberRange,
    NotInteger,
    CapacityExceeded,
    WrongArraySize,
    DepthLimit,
    DocumentLimit,
    TrailingData,
    ValidationFailed
};
struct ImportResult {
    ImportError error = ImportError::None;
    std::size_t offset = 0; // UTF-8 byte offset, zero based; EOF is the document length.
    Section section = Section::Profile;
    std::uint16_t object = 0; // 0 if id was not read before the failure.
    char field[33] = {};
    ValidationResult validation;
    explicit operator bool() const {
        return error == ImportError::None;
    }
};
const char *importErrorName(ImportError error);
// Destructive import into one exclusively owned INACTIVE draft. Failure may leave
// partial contents; only a successful result permits validation/activation/export.
// The input must not overlap draft storage. No heap or second full Profile needed.
ImportResult readDraftProfile(IProfileReader &reader, Profile &draft);
ImportResult readDraftProfile(const char *bytes, std::size_t count, Profile &draft);
// No heap allocation. Storage must be distinct and exclusively owned by the caller.
// Failure leaves destination unchanged; scratch may contain a partial document.
// Success copies the validated scratch into destination; this is NOT a task-safe
// activation protocol. Neither profile belongs on a small FreeRTOS task stack.
ImportResult readProfile(IProfileReader &reader, Profile &scratch, Profile &destination);
ImportResult readProfile(const char *bytes, std::size_t count, Profile &scratch,
                         Profile &destination);
} // namespace configuration
} // namespace larp
