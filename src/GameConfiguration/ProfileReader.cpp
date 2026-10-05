#include "GameConfiguration/ProfileReader.h"
#include "GameConfiguration/ProfileVocabulary.h"
#include <cstring>
#include <limits>

namespace larp {
namespace configuration {
namespace {
static_assert(ImportLimits::DocumentBytes > 0 &&
                  ImportLimits::DocumentBytes <=
                      static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max() / 4),
              "Document bound must also bound exact-number arithmetic");
static_assert(ImportLimits::ContainerDepth > 0, "A document requires a root container");
struct Context {
    Section section = Section::Profile;
    const std::uint16_t *object = nullptr;
    char field[33] = {};
};
class Parser {
  public:
    explicit Parser(IProfileReader &source) : source_(source) {}
    ImportResult finish(Profile &profile);
    bool read(Profile &item);
    bool read(InputPolicy &item);
    bool read(Component &item);
    bool read(Endpoint &item);
    bool read(Role &item);
    bool read(Crystal &item);
    bool read(State &item);
    bool read(Transition &item);
    bool read(Condition &item);
    bool read(Action &item);
    bool read(Effect &item);
    bool read(std::uint8_t &value) {
        return unsignedNumber(value);
    }
    bool read(std::uint16_t &value) {
        return unsignedNumber(value);
    }
    bool read(std::uint32_t &value) {
        return unsignedNumber(value);
    }
    bool read(bool &value) {
        space();
        if (peek() == 't') {
            value = true;
            return literal("true");
        }
        if (peek() == 'f') {
            value = false;
            return literal("false");
        }
        return failToken();
    }
    template <std::size_t N> bool read(Text<N> &value) {
        char buffer[N + 1] = {};
        return string(buffer, N) && (value.assign(buffer) || fail(ImportError::InvalidText));
    }
    template <typename Tag> bool read(Id<Tag> &value) {
        return read(value.value);
    }
    bool read(ExpanderMode &value) {
        std::uint8_t number = 0;
        if (!read(number))
            return false;
        if (number > 3)
            return fail(ImportError::InvalidEnum);
        value = static_cast<ExpanderMode>(number);
        return true;
    }
    bool read(Board &value);
    bool read(ComponentKind &value);
    bool read(CrystalCategory &value);
    bool read(Field &value);
    bool read(Compare &value);
    bool read(ConditionKind &value);
    bool read(ActionKind &value);
    bool read(EffectKind &value);
    template <typename T, std::size_t N> bool read(std::array<T, N> &values) {
        if (!open('['))
            return false;
        Depth guard(*this);
        std::size_t count = 0;
        space();
        if (peek() != ']') {
            while (true) {
                if (count == N)
                    return fail(ImportError::WrongArraySize);
                if (!read(values[count++]))
                    return false;
                space();
                if (peek() == ']')
                    break;
                if (!expect(','))
                    return false;
            }
        }
        if (count != N)
            return fail(ImportError::WrongArraySize);
        return expect(']');
    }
    template <typename T, std::size_t N> bool read(List<T, N> &values) {
        if (!open('['))
            return false;
        Depth guard(*this);
        values.clear();
        space();
        if (peek() != ']') {
            while (true) {
                if (values.size() == N)
                    return fail(ImportError::CapacityExceeded);
                T value;
                if (!read(value))
                    return false;
                if (!values.push(value))
                    return fail(ImportError::CapacityExceeded);
                space();
                if (peek() == ']')
                    break;
                if (!expect(','))
                    return false;
            }
        }
        return expect(']');
    }
    template <typename T, std::size_t N, typename Member>
    bool object(T &value, const char *const (&keys)[N], Section section, const std::uint16_t *id,
                Member member) {
        static_assert(N < 32, "object field mask must fit uint32");
        const Context saved = context_;
        context_.section = section;
        if (id != nullptr)
            context_.object = id;
        if (!open('{')) {
            context_ = saved;
            return false;
        }
        Depth guard(*this);
        std::uint32_t seen = 0;
        space();
        if (peek() != '}') {
            while (true) {
                char key[33] = {};
                if (!string(key, 32)) {
                    context_ = saved;
                    return false;
                }
                std::memcpy(context_.field, key, sizeof(key));
                std::size_t index = 0;
                while (index < N && std::strcmp(key, keys[index]) != 0)
                    ++index;
                if (index == N) {
                    fail(ImportError::UnknownField);
                    break;
                }
                const std::uint32_t bit = std::uint32_t(1) << index;
                if ((seen & bit) != 0) {
                    fail(ImportError::DuplicateField);
                    break;
                }
                seen |= bit;
                if (!expect(':') || !member(*this, value, index))
                    break;
                space();
                if (peek() == '}')
                    break;
                if (!expect(','))
                    break;
            }
        }
        if (result_ && expect('}')) {
            const std::uint32_t required = mask(value, (std::uint32_t(1) << N) - 1);
            const std::uint32_t missing = required & ~seen;
            const std::uint32_t extra = seen & ~required;
            if (missing || extra) {
                for (std::size_t index = 0; index < N; ++index) {
                    if (((missing ? missing : extra) & (std::uint32_t(1) << index)) != 0) {
                        std::strcpy(context_.field, keys[index]);
                        break;
                    }
                }
                fail(missing ? ImportError::MissingField : ImportError::UnexpectedField);
            }
        }
        context_ = saved;
        return static_cast<bool>(result_);
    }

  private:
    struct Depth {
        explicit Depth(Parser &parser) : parser(parser) {}
        ~Depth() {
            --parser.depth_;
        }
        Parser &parser;
    };
    IProfileReader &source_;
    int next_ = -3; // unread; -1 final EOF, -2 failure
    std::size_t offset_ = 0;
    std::size_t depth_ = 0;
    Context context_;
    ImportResult result_;
    bool fail(ImportError error) {
        if (result_) {
            result_.error = error;
            result_.offset = offset_;
            result_.section = context_.section;
            result_.object = context_.object == nullptr ? 0 : *context_.object;
            std::memcpy(result_.field, context_.field, sizeof(result_.field));
        }
        return false;
    }
    int peek() {
        if (!result_)
            return -2;
        if (next_ != -3)
            return next_;
        std::uint8_t byte = 0;
        const ReadStatus status = source_.readByte(byte);
        if (status == ReadStatus::End)
            return next_ = -1;
        if (status != ReadStatus::Byte) {
            fail(ImportError::InputFailure);
            return next_ = -2;
        }
        if (offset_ == ImportLimits::DocumentBytes) {
            fail(ImportError::DocumentLimit);
            return next_ = -2;
        }
        return next_ = byte;
    }
    int take() {
        const int byte = peek();
        if (byte >= 0) {
            ++offset_;
            next_ = -3;
        }
        return byte;
    }
    static bool whitespace(int byte) {
        return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r';
    }
    void space() {
        while (whitespace(peek()))
            take();
    }
    bool failToken() {
        return fail(peek() == -1 ? ImportError::UnexpectedEnd : ImportError::UnexpectedToken);
    }
    bool expect(char byte) {
        space();
        if (peek() != byte)
            return failToken();
        take();
        return true;
    }
    bool open(char byte) {
        if (depth_ == ImportLimits::ContainerDepth)
            return fail(ImportError::DepthLimit);
        if (!expect(byte))
            return false;
        ++depth_;
        return true;
    }
    static bool delimiter(int byte) {
        return byte == -1 || whitespace(byte) || byte == ',' || byte == ']' || byte == '}';
    }
    bool literal(const char *text) {
        for (std::size_t index = 0; text[index]; ++index) {
            if (peek() != text[index])
                return failToken();
            take();
        }
        return delimiter(peek()) || failToken();
    }
    bool hex4(std::uint32_t &value) {
        value = 0;
        for (unsigned index = 0; index < 4; ++index) {
            const int byte = take();
            if (byte < 0)
                return failToken();
            const int digit = byte >= '0' && byte <= '9'   ? byte - '0'
                              : byte >= 'a' && byte <= 'f' ? byte - 'a' + 10
                              : byte >= 'A' && byte <= 'F' ? byte - 'A' + 10
                                                           : -1;
            if (digit < 0)
                return fail(ImportError::InvalidUnicodeEscape);
            value = value * 16 + static_cast<unsigned>(digit);
        }
        return true;
    }
    bool append(char *buffer, std::size_t capacity, std::size_t &length, unsigned byte) {
        if (byte < 32)
            return fail(ImportError::InvalidText);
        if (length == capacity)
            return fail(ImportError::TextTooLong);
        buffer[length++] = static_cast<char>(byte);
        return true;
    }
    bool string(char *buffer, std::size_t capacity) {
        if (!expect('"'))
            return false;
        std::size_t length = 0;
        while (true) {
            const int byte = take();
            if (byte < 0)
                return failToken();
            if (byte == '"') {
                buffer[length] = '\0';
                return validUtf8(buffer, length) || fail(ImportError::InvalidUtf8);
            }
            if (byte != '\\') {
                if (!append(buffer, capacity, length, static_cast<unsigned>(byte)))
                    return false;
                continue;
            }
            const int escaped = take();
            if (escaped < 0)
                return failToken();
            if (escaped == '"' || escaped == '\\' || escaped == '/') {
                if (!append(buffer, capacity, length, static_cast<unsigned>(escaped)))
                    return false;
            } else if (escaped == 'u') {
                std::uint32_t codepoint = 0;
                if (!hex4(codepoint))
                    return false;
                if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                    if (take() != '\\' || take() != 'u')
                        return fail(ImportError::InvalidUnicodeEscape);
                    std::uint32_t low = 0;
                    if (!hex4(low))
                        return false;
                    if (low < 0xDC00 || low > 0xDFFF)
                        return fail(ImportError::InvalidUnicodeEscape);
                    codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + low - 0xDC00;
                } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                    return fail(ImportError::InvalidUnicodeEscape);
                }
                unsigned bytes[4] = {};
                std::size_t count = 0;
                if (codepoint < 0x80) {
                    bytes[0] = codepoint;
                    count = 1;
                } else if (codepoint < 0x800) {
                    bytes[0] = 0xC0 | (codepoint >> 6);
                    bytes[1] = 0x80 | (codepoint & 63);
                    count = 2;
                } else if (codepoint < 0x10000) {
                    bytes[0] = 0xE0 | (codepoint >> 12);
                    bytes[1] = 0x80 | ((codepoint >> 6) & 63);
                    bytes[2] = 0x80 | (codepoint & 63);
                    count = 3;
                } else {
                    bytes[0] = 0xF0 | (codepoint >> 18);
                    bytes[1] = 0x80 | ((codepoint >> 12) & 63);
                    bytes[2] = 0x80 | ((codepoint >> 6) & 63);
                    bytes[3] = 0x80 | (codepoint & 63);
                    count = 4;
                }
                for (std::size_t index = 0; index < count; ++index)
                    if (!append(buffer, capacity, length, bytes[index]))
                        return false;
            } else if (escaped == 'b' || escaped == 'f' || escaped == 'n' || escaped == 'r' ||
                       escaped == 't') {
                return fail(ImportError::InvalidText); // Valid JSON, forbidden in profile strings.
            } else
                return fail(ImportError::UnexpectedToken);
        }
    }
    template <typename T> bool unsignedNumber(T &value) {
        space();
        const bool negative = peek() == '-';
        if (negative)
            take();
        if (peek() < '0' || peek() > '9')
            return failToken();
        std::uint32_t mantissa = 0;
        std::size_t zeros = 0;
        std::size_t fraction = 0;
        bool overflow = false;
        // Keep a significant mantissa and defer trailing zeros. This accepts
        // exact integers such as 1.0 or 10000000000e-1 without floating point,
        // and rejects a fractional value rather than silently rounding it.
        const auto digit = [&](unsigned number) {
            if (number == 0) {
                if (mantissa != 0 || overflow)
                    ++zeros;
                return;
            }
            while (zeros != 0 && !overflow) {
                if (mantissa > std::numeric_limits<std::uint32_t>::max() / 10)
                    overflow = true;
                else
                    mantissa *= 10;
                --zeros;
            }
            zeros = 0;
            if (mantissa > (std::numeric_limits<std::uint32_t>::max() - number) / 10)
                overflow = true;
            if (!overflow)
                mantissa = mantissa * 10 + number;
        };
        if (peek() == '0') {
            take();
            if (peek() >= '0' && peek() <= '9')
                return failToken();
        } else {
            while (peek() >= '0' && peek() <= '9')
                digit(static_cast<unsigned>(take() - '0'));
        }
        if (peek() == '.') {
            take();
            if (peek() < '0' || peek() > '9')
                return failToken();
            while (peek() >= '0' && peek() <= '9') {
                digit(static_cast<unsigned>(take() - '0'));
                ++fraction;
            }
        }
        std::int32_t exponent = 0;
        if (peek() == 'e' || peek() == 'E') {
            take();
            const bool exponentNegative = peek() == '-';
            if (peek() == '-' || peek() == '+')
                take();
            if (peek() < '0' || peek() > '9')
                return failToken();
            const std::int32_t limit = static_cast<std::int32_t>(ImportLimits::DocumentBytes + 10);
            while (peek() >= '0' && peek() <= '9') {
                const int number = take() - '0';
                exponent = exponent > (limit - number) / 10 ? limit : exponent * 10 + number;
            }
            if (exponentNegative)
                exponent = -exponent;
        }
        if (!delimiter(peek()))
            return failToken();
        if (!result_)
            return false;
        if (overflow || (negative && mantissa != 0))
            return fail(ImportError::NumberRange);
        if (mantissa == 0) {
            value = 0;
            return true;
        }
        std::int32_t scale =
            exponent + static_cast<std::int32_t>(zeros) - static_cast<std::int32_t>(fraction);
        if (scale < 0)
            return fail(ImportError::NotInteger);
        while (scale-- > 0) {
            if (mantissa > std::numeric_limits<std::uint32_t>::max() / 10)
                return fail(ImportError::NumberRange);
            mantissa *= 10;
        }
        if (mantissa > std::numeric_limits<T>::max())
            return fail(ImportError::NumberRange);
        value = static_cast<T>(mantissa);
        return true;
    }
    template <typename T, std::size_t N> bool enumeration(T &value, const char *const (&names)[N]) {
        char name[33] = {};
        if (!string(name, 32))
            return false;
        for (std::size_t index = 0; index < N; ++index) {
            if (std::strcmp(name, names[index]) == 0) {
                value = static_cast<T>(index);
                return true;
            }
        }
        return fail(ImportError::InvalidEnum);
    }
    template <typename T> std::uint32_t mask(const T &, std::uint32_t all) const {
        return all;
    }
    std::uint32_t mask(const Condition &item, std::uint32_t) const {
        // Keys: id, kind, field, component, comparison, operand, first, second.
        return item.kind == ConditionKind::Wait        ? 0x23
               : item.kind == ConditionKind::Predicate ? 0x3F
               : item.kind == ConditionKind::Not       ? 0x43
                                                       : 0xC3;
    }
};
bool Parser::read(Board &value) {
    return enumeration(value, vocabulary::BoardNames);
}
bool Parser::read(ComponentKind &value) {
    return enumeration(value, vocabulary::ComponentKindNames);
}
bool Parser::read(CrystalCategory &value) {
    return enumeration(value, vocabulary::CrystalCategoryNames);
}
bool Parser::read(Field &value) {
    return enumeration(value, vocabulary::FieldNames);
}
bool Parser::read(Compare &value) {
    return enumeration(value, vocabulary::CompareNames);
}
bool Parser::read(ConditionKind &value) {
    return enumeration(value, vocabulary::ConditionKindNames);
}
bool Parser::read(ActionKind &value) {
    return enumeration(value, vocabulary::ActionKindNames);
}
bool Parser::read(EffectKind &value) {
    return enumeration(value, vocabulary::EffectKindNames);
}
bool Parser::read(Profile &item) {
    static const char *const keys[] = {"schemaVersion", "revision",   "name",        "board",
                                       "initialState",  "errorState", "inputs",      "components",
                                       "roles",         "states",     "transitions", "conditions",
                                       "actions",       "effects"};
    return object(item, keys, Section::Profile, nullptr,
                  [](Parser &parser, Profile &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.schemaVersion);
                      case 1:
                          return parser.read(value.revision);
                      case 2:
                          return parser.read(value.name);
                      case 3:
                          return parser.read(value.board);
                      case 4:
                          return parser.read(value.initialState);
                      case 5:
                          return parser.read(value.errorState);
                      case 6:
                          return parser.read(value.inputs);
                      case 7:
                          return parser.read(value.components);
                      case 8:
                          return parser.read(value.roles);
                      case 9:
                          return parser.read(value.states);
                      case 10:
                          return parser.read(value.transitions);
                      case 11:
                          return parser.read(value.conditions);
                      case 12:
                          return parser.read(value.actions);
                      case 13:
                          return parser.read(value.effects);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(InputPolicy &item) {
    static const char *const keys[] = {"cardTimeoutMilliseconds",  "emptyReadsBeforeClear",
                                       "keystoneFailureThreshold", "consecutiveEmptyReads",
                                       "wakeRippleOrigins",        "wakeRippleFrames"};
    return object(item, keys, Section::Profile, nullptr,
                  [](Parser &parser, InputPolicy &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.cardTimeoutMilliseconds);
                      case 1:
                          return parser.read(value.emptyReadsBeforeClear);
                      case 2:
                          return parser.read(value.keystoneFailureThreshold);
                      case 3:
                          return parser.read(value.consecutiveEmptyReads);
                      case 4:
                          return parser.read(value.wakeRippleOrigins);
                      case 5:
                          return parser.read(value.wakeRippleFrames);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(Component &item) {
    static const char *const keys[] = {"id",
                                       "name",
                                       "kind",
                                       "enabled",
                                       "pins",
                                       "bus",
                                       "address",
                                       "ledCount",
                                       "powerLimitMilliamperes",
                                       "volume",
                                       "baud",
                                       "initialBrightness",
                                       "initialTwinkleDensity",
                                       "initialTwinkleSpeed",
                                       "startupModes",
                                       "peer",
                                       "channel",
                                       "rows",
                                       "columns",
                                       "wakeInputs"};
    return object(item, keys, Section::Components, &item.id.value,
                  [](Parser &parser, Component &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.id);
                      case 1:
                          return parser.read(value.name);
                      case 2:
                          return parser.read(value.kind);
                      case 3:
                          return parser.read(value.enabled);
                      case 4:
                          return parser.read(value.pins);
                      case 5:
                          return parser.read(value.bus);
                      case 6:
                          return parser.read(value.address);
                      case 7:
                          return parser.read(value.ledCount);
                      case 8:
                          return parser.read(value.powerLimitMilliamperes);
                      case 9:
                          return parser.read(value.volume);
                      case 10:
                          return parser.read(value.baud);
                      case 11:
                          return parser.read(value.initialBrightness);
                      case 12:
                          return parser.read(value.initialTwinkleDensity);
                      case 13:
                          return parser.read(value.initialTwinkleSpeed);
                      case 14:
                          return parser.read(value.startupModes);
                      case 15:
                          return parser.read(value.peer);
                      case 16:
                          return parser.read(value.channel);
                      case 17:
                          return parser.read(value.rows);
                      case 18:
                          return parser.read(value.columns);
                      case 19:
                          return parser.read(value.wakeInputs);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(Endpoint &item) {
    static const char *const keys[] = {"expander", "pin"};
    return object(item, keys, Section::Components, nullptr,
                  [](Parser &parser, Endpoint &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.expander);
                      case 1:
                          return parser.read(value.pin);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(Role &item) {
    static const char *const keys[] = {"id", "name", "uid", "audioTrack", "crystals"};
    return object(item, keys, Section::Roles, &item.id.value,
                  [](Parser &parser, Role &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.id);
                      case 1:
                          return parser.read(value.name);
                      case 2:
                          return parser.read(value.uid);
                      case 3:
                          return parser.read(value.audioTrack);
                      case 4:
                          return parser.read(value.crystals);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(Crystal &item) {
    static const char *const keys[] = {
        "name", "row", "column", "audioTrack", "category", "changesPalette", "palette"};
    return object(item, keys, Section::Roles, nullptr,
                  [](Parser &parser, Crystal &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.name);
                      case 1:
                          return parser.read(value.row);
                      case 2:
                          return parser.read(value.column);
                      case 3:
                          return parser.read(value.audioTrack);
                      case 4:
                          return parser.read(value.category);
                      case 5:
                          return parser.read(value.changesPalette);
                      case 6:
                          return parser.read(value.palette);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(State &item) {
    static const char *const keys[] = {"id", "name", "manualAllowed", "entry", "tick", "exit"};
    return object(item, keys, Section::States, &item.id.value,
                  [](Parser &parser, State &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.id);
                      case 1:
                          return parser.read(value.name);
                      case 2:
                          return parser.read(value.manualAllowed);
                      case 3:
                          return parser.read(value.entry);
                      case 4:
                          return parser.read(value.tick);
                      case 5:
                          return parser.read(value.exit);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(Transition &item) {
    static const char *const keys[] = {"id",       "source",        "target", "condition",
                                       "priority", "manualAllowed", "actions"};
    return object(item, keys, Section::Transitions, &item.id.value,
                  [](Parser &parser, Transition &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.id);
                      case 1:
                          return parser.read(value.source);
                      case 2:
                          return parser.read(value.target);
                      case 3:
                          return parser.read(value.condition);
                      case 4:
                          return parser.read(value.priority);
                      case 5:
                          return parser.read(value.manualAllowed);
                      case 6:
                          return parser.read(value.actions);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(Condition &item) {
    static const char *const keys[] = {"id",         "kind",    "field", "component",
                                       "comparison", "operand", "first", "second"};
    return object(item, keys, Section::Conditions, &item.id.value,
                  [](Parser &parser, Condition &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.id);
                      case 1:
                          return parser.read(value.kind);
                      case 2:
                          return parser.read(value.field);
                      case 3:
                          return parser.read(value.component);
                      case 4:
                          return parser.read(value.comparison);
                      case 5:
                          return parser.read(value.operand);
                      case 6:
                          return parser.read(value.first);
                      case 7:
                          return parser.read(value.second);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(Action &item) {
    static const char *const keys[] = {"id", "kind", "component", "source", "effect", "value"};
    return object(item, keys, Section::Actions, &item.id.value,
                  [](Parser &parser, Action &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.id);
                      case 1:
                          return parser.read(value.kind);
                      case 2:
                          return parser.read(value.component);
                      case 3:
                          return parser.read(value.source);
                      case 4:
                          return parser.read(value.effect);
                      case 5:
                          return parser.read(value.value);
                      default:
                          return false;
                      }
                  });
}
bool Parser::read(Effect &item) {
    static const char *const keys[] = {"id",          "name",       "component",
                                       "kind",        "density",    "changesSpeed",
                                       "speed",       "brightness", "intervalMilliseconds",
                                       "frames",      "colored",    "colorRgb",
                                       "rippleIndex", "origin",     "palette"};
    return object(item, keys, Section::Effects, &item.id.value,
                  [](Parser &parser, Effect &value, std::size_t index) -> bool {
                      switch (index) {
                      case 0:
                          return parser.read(value.id);
                      case 1:
                          return parser.read(value.name);
                      case 2:
                          return parser.read(value.component);
                      case 3:
                          return parser.read(value.kind);
                      case 4:
                          return parser.read(value.density);
                      case 5:
                          return parser.read(value.changesSpeed);
                      case 6:
                          return parser.read(value.speed);
                      case 7:
                          return parser.read(value.brightness);
                      case 8:
                          return parser.read(value.intervalMilliseconds);
                      case 9:
                          return parser.read(value.frames);
                      case 10:
                          return parser.read(value.colored);
                      case 11:
                          return parser.read(value.colorRgb);
                      case 12:
                          return parser.read(value.rippleIndex);
                      case 13:
                          return parser.read(value.origin);
                      case 14:
                          return parser.read(value.palette);
                      default:
                          return false;
                      }
                  });
}

ImportResult Parser::finish(Profile &profile) {
    if (read(profile)) {
        space();
        if (peek() != -1)
            fail(ImportError::TrailingData);
        if (result_) {
            result_.offset = offset_;
            result_.validation = validateProfile(profile);
            if (!result_.validation) {
                fail(ImportError::ValidationFailed);
                result_.section = result_.validation.section;
                result_.object = result_.validation.object;
            }
        }
    }
    return result_;
}
class MemoryReader : public IProfileReader {
  public:
    MemoryReader(const char *bytes, std::size_t count) : bytes_(bytes), count_(count) {}
    ReadStatus readByte(std::uint8_t &byte) override {
        if (index_ == count_)
            return ReadStatus::End;
        byte = static_cast<std::uint8_t>(bytes_[index_++]);
        return ReadStatus::Byte;
    }

  private:
    const char *bytes_;
    std::size_t count_;
    std::size_t index_ = 0;
};
} // namespace
const char *importErrorName(ImportError error) {
    static const char *const names[] = {"none",
                                        "invalid_argument",
                                        "aliased_storage",
                                        "input_failure",
                                        "unexpected_end",
                                        "unexpected_token",
                                        "unknown_field",
                                        "duplicate_field",
                                        "missing_field",
                                        "unexpected_field",
                                        "invalid_text",
                                        "invalid_utf8",
                                        "invalid_unicode_escape",
                                        "text_too_long",
                                        "invalid_enum",
                                        "number_range",
                                        "not_integer",
                                        "capacity_exceeded",
                                        "wrong_array_size",
                                        "depth_limit",
                                        "document_limit",
                                        "trailing_data",
                                        "validation_failed"};
    const unsigned index = static_cast<unsigned>(error);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown_error";
}
ImportResult readDraftProfile(IProfileReader &reader, Profile &draft) {
    Parser parser(reader);
    return parser.finish(draft);
}
ImportResult readDraftProfile(const char *bytes, std::size_t count, Profile &draft) {
    if ((bytes == nullptr && count != 0) || count > ImportLimits::DocumentBytes) {
        ImportResult result;
        result.error = count > ImportLimits::DocumentBytes ? ImportError::DocumentLimit
                                                           : ImportError::InvalidArgument;
        return result;
    }
    MemoryReader reader(bytes, count);
    return readDraftProfile(reader, draft);
}
ImportResult readProfile(IProfileReader &reader, Profile &scratch, Profile &destination) {
    if (&scratch == &destination) {
        ImportResult result;
        result.error = ImportError::AliasedStorage;
        return result;
    }
    const ImportResult result = readDraftProfile(reader, scratch);
    if (result)
        destination = scratch;
    return result;
}
ImportResult readProfile(const char *bytes, std::size_t count, Profile &scratch,
                         Profile &destination) {
    if ((bytes == nullptr && count != 0) || count > ImportLimits::DocumentBytes) {
        ImportResult result;
        result.error = count > ImportLimits::DocumentBytes ? ImportError::DocumentLimit
                                                           : ImportError::InvalidArgument;
        return result;
    }
    MemoryReader reader(bytes, count);
    return readProfile(reader, scratch, destination);
}
} // namespace configuration
} // namespace larp
