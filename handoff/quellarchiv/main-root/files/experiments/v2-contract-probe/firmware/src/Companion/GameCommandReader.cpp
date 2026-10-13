#include "Companion/GameCommandReader.h"
#include <cstring>
#include <limits>
namespace larp {
namespace companion {
namespace {
const char *const fields[] = {
    "schemaVersion",         "boot", "lease", "request", "expectedConfigurationRevision",
    "expectedStateRevision", "kind", "target"};
static const std::size_t FieldCount = sizeof(fields) / sizeof(fields[0]);
static_assert(static_cast<std::size_t>(CommandField::Target) == FieldCount,
              "Field names must match the diagnostic enumeration");
static_assert(FieldCount == 8, "Required field mask and enum must match the v2 command");
int hex(int byte) {
    if (byte >= '0' && byte <= '9')
        return byte - '0';
    if (byte >= 'A' && byte <= 'F')
        return byte - 'A' + 10;
    if (byte >= 'a' && byte <= 'f')
        return byte - 'a' + 10;
    return -1;
}
bool digit(int byte) {
    return byte >= '0' && byte <= '9';
}
class Parser {
  public:
    Parser(const char *bytes, std::size_t count) : bytes_(bytes), count_(count) {}
    CommandDecodeResult decode(GameCommand &destination) {
        if (!expect('{'))
            return result_;
        std::uint16_t seen = 0;
        space();
        if (peek() != '}') {
            while (true) {
                char key[33] = {};
                result_.field = CommandField::None;
                if (!text(key))
                    return result_;
                std::size_t index = 0;
                while (index < FieldCount && std::strcmp(key, fields[index]))
                    ++index;
                if (index == FieldCount) {
                    fail(CommandDecodeError::UnknownField);
                    return result_;
                }
                result_.field = static_cast<CommandField>(index + 1);
                const std::uint16_t bit = std::uint16_t(1) << index;
                if (seen & bit) {
                    fail(CommandDecodeError::DuplicateField);
                    return result_;
                }
                if (!expect(':') || !value(index))
                    return result_;
                seen |= bit;
                space();
                if (peek() == '}')
                    break;
                if (!expect(','))
                    return result_;
            }
        }
        if (!expect('}'))
            return result_;
        space();
        if (offset_ != count_) {
            fail(CommandDecodeError::TrailingData);
            return result_;
        }
        for (std::size_t i = 0; i < FieldCount; ++i) {
            if (!(seen & (std::uint16_t(1) << i))) {
                result_.field = static_cast<CommandField>(i + 1);
                fail(CommandDecodeError::MissingField);
                return result_;
            }
        }
        result_.field = CommandField::Target;
        const bool usesTarget = command_.kind == GameCommandKind::ChangeState ||
                                command_.kind == GameCommandKind::TakeTransition;
        if (usesTarget ? command_.target == 0 : command_.target != 0) {
            fail(CommandDecodeError::InvalidTarget);
            return result_;
        }
        destination = command_;
        result_.field = CommandField::None;
        result_.offset = count_;
        return result_;
    }

  private:
    const char *bytes_;
    std::size_t count_, offset_ = 0;
    CommandDecodeResult result_;
    GameCommand command_;
    int peek() const {
        return offset_ < count_ ? static_cast<unsigned char>(bytes_[offset_]) : -1;
    }
    int take() {
        const int byte = peek();
        if (byte >= 0)
            ++offset_;
        return byte;
    }
    bool fail(CommandDecodeError error) {
        if (result_.error == CommandDecodeError::None) {
            result_.error = error;
            result_.offset = offset_;
        }
        return false;
    }
    bool tokenError() {
        return fail(peek() < 0 ? CommandDecodeError::UnexpectedEnd
                               : CommandDecodeError::UnexpectedToken);
    }
    void space() {
        while (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')
            ++offset_;
    }
    bool expect(char token) {
        space();
        if (peek() != token)
            return tokenError();
        ++offset_;
        return true;
    }
    bool text(char (&value)[33]) {
        if (!expect('"'))
            return false;
        std::size_t used = 0;
        while (true) {
            int byte = peek();
            if (byte < 0)
                return fail(CommandDecodeError::UnexpectedEnd);
            if (byte == '"') {
                ++offset_;
                value[used] = '\0';
                return true;
            }
            if (byte == '\\') {
                ++offset_;
                byte = take();
                if (byte < 0)
                    return fail(CommandDecodeError::UnexpectedEnd);
                if (byte == 'u') {
                    byte = 0;
                    for (std::size_t i = 0; i < 4; ++i) {
                        if (peek() < 0)
                            return fail(CommandDecodeError::UnexpectedEnd);
                        const int nibble = hex(peek());
                        if (nibble < 0)
                            return fail(CommandDecodeError::InvalidText);
                        byte = byte * 16 + nibble;
                        ++offset_;
                    }
                } else if (byte != '"' && byte != '\\' && byte != '/')
                    return fail(CommandDecodeError::InvalidText);
            } else
                ++offset_;
            // All supported keys, kinds and boot hex are printable ASCII. No
            // UTF-8/Unicode normalization can introduce a second field spelling.
            if (byte < 32 || byte > 126)
                return fail(CommandDecodeError::InvalidText);
            if (used == 32)
                return fail(CommandDecodeError::TextTooLong);
            value[used++] = static_cast<char>(byte);
        }
    }
    bool number(std::uint32_t &value,
                std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max()) {
        space();
        if (peek() < 0)
            return fail(CommandDecodeError::UnexpectedEnd);
        if (!digit(peek()))
            return fail(CommandDecodeError::InvalidInteger);
        const bool zero = peek() == '0';
        value = 0;
        do {
            const auto next = static_cast<std::uint32_t>(peek() - '0');
            if (next > maximum || value > (maximum - next) / 10)
                return fail(CommandDecodeError::IntegerRange);
            value = value * 10 + next;
            ++offset_;
            if (zero && digit(peek()))
                return fail(CommandDecodeError::InvalidInteger);
        } while (digit(peek()));
        if (peek() == '.' || peek() == 'e' || peek() == 'E')
            return fail(CommandDecodeError::InvalidInteger);
        return true;
    }
    bool value(std::size_t index) {
        std::uint32_t parsed = 0;
        char string[33] = {};
        switch (index) {
        case 0:
            return number(parsed) && (parsed == 2 || fail(CommandDecodeError::UnsupportedVersion));
        case 1: {
            if (!text(string))
                return false;
            if (std::strlen(string) != 16)
                return fail(CommandDecodeError::InvalidBoot);
            for (std::size_t i = 0; i < 16; ++i) {
                const int byte = static_cast<unsigned char>(string[i]);
                if (!digit(byte) && !(byte >= 'A' && byte <= 'F'))
                    return fail(CommandDecodeError::InvalidBoot);
                auto &part = i < 8 ? command_.boot.high : command_.boot.low;
                part = (part << 4) | static_cast<std::uint32_t>(hex(byte));
            }
            return command_.boot.valid() || fail(CommandDecodeError::InvalidBoot);
        }
        case 2:
            return number(command_.lease) &&
                   (command_.lease != 0 || fail(CommandDecodeError::InvalidLease));
        case 3:
            return number(command_.request) &&
                   (command_.request != 0 || fail(CommandDecodeError::InvalidRequest));
        case 4:
            return number(command_.expectedConfigurationRevision);
        case 5:
            return number(command_.expectedStateRevision);
        case 6: {
            if (!text(string))
                return false;
            struct Kind {
                const char *name;
                GameCommandKind value;
            };
            static const Kind kinds[] = {{"start", GameCommandKind::Start},
                                         {"stop", GameCommandKind::Stop},
                                         {"pause", GameCommandKind::Pause},
                                         {"resume", GameCommandKind::Resume},
                                         {"change_state", GameCommandKind::ChangeState},
                                         {"take_transition", GameCommandKind::TakeTransition}};
            for (const auto &kind : kinds) {
                if (!std::strcmp(string, kind.name)) {
                    command_.kind = kind.value;
                    return true;
                }
            }
            return fail(CommandDecodeError::InvalidKind);
        }
        case 7:
            if (!number(parsed, 65535))
                return false;
            command_.target = static_cast<std::uint16_t>(parsed);
            return true;
        }
        return fail(CommandDecodeError::UnknownField);
    }
};
} // namespace
const char *commandFieldName(CommandField field) {
    const auto index = static_cast<std::size_t>(field);
    return index == 0 ? "none" : index <= FieldCount ? fields[index - 1] : "unknown";
}
const char *commandDecodeErrorName(CommandDecodeError error) {
    switch (error) {
    case CommandDecodeError::None:
        return "none";
    case CommandDecodeError::InvalidArgument:
        return "invalid_argument";
    case CommandDecodeError::MessageTooLarge:
        return "message_too_large";
    case CommandDecodeError::UnexpectedEnd:
        return "unexpected_end";
    case CommandDecodeError::UnexpectedToken:
        return "unexpected_token";
    case CommandDecodeError::UnknownField:
        return "unknown_field";
    case CommandDecodeError::DuplicateField:
        return "duplicate_field";
    case CommandDecodeError::MissingField:
        return "missing_field";
    case CommandDecodeError::InvalidText:
        return "invalid_text";
    case CommandDecodeError::TextTooLong:
        return "text_too_long";
    case CommandDecodeError::InvalidBoot:
        return "invalid_boot";
    case CommandDecodeError::InvalidKind:
        return "invalid_kind";
    case CommandDecodeError::InvalidInteger:
        return "invalid_integer";
    case CommandDecodeError::IntegerRange:
        return "integer_range";
    case CommandDecodeError::UnsupportedVersion:
        return "unsupported_version";
    case CommandDecodeError::InvalidLease:
        return "invalid_lease";
    case CommandDecodeError::InvalidRequest:
        return "invalid_request";
    case CommandDecodeError::InvalidTarget:
        return "invalid_target";
    case CommandDecodeError::TrailingData:
        return "trailing_data";
    }
    return "unknown";
}
CommandDecodeResult decodeGameCommand(const char *bytes, std::size_t count,
                                      GameCommand &destination) {
    CommandDecodeResult result;
    if (!bytes || !count) {
        result.error = CommandDecodeError::InvalidArgument;
        return result;
    }
    if (count > maxGameCommandDocumentBytes) {
        result.error = CommandDecodeError::MessageTooLarge;
        result.offset = maxGameCommandDocumentBytes;
        return result;
    }
    return Parser(bytes, count).decode(destination);
}
} // namespace companion
} // namespace larp
