#include "GameConfiguration/PackageDeclarations.h"
#include <cstdio>
#include <cstring>

namespace larp {
namespace configuration {
namespace {
const char *const rootFields[] = {"format",    "label",    "modules",  "packageId", "requires",
                                  "resources", "revision", "stations", "subjects"};
const char *const requirementFields[] = {"name", "version"};
const char *const moduleFields[] = {"digest", "moduleId", "version"};
const char *const subjectFields[] = {"id", "originStation"};
const char *const resourceFields[] = {"bytes", "digest", "id", "kind"};
const char *const stationFields[] = {"id",    "instances", "label", "ports",
                                     "rules", "timers",    "values"};
const char *const valueFields[] = {"id", "initial", "type"};
const char *const timerFields[] = {"durationMs", "id"};
const char *const portFields[] = {"capability", "direction", "id", "type"};
const char *const *const fields[] = {rootFields,    requirementFields, moduleFields,
                                     subjectFields, resourceFields,    stationFields,
                                     valueFields,   timerFields,       portFields};
const std::size_t sizes[] = {9, 2, 3, 2, 4, 7, 3, 2, 4};
} // namespace
void PackageDeclarationReader::reset() {
    result_ = {};
    depth_ = 0;
    skippedBytes_ = 0;
    skipDepth_ = 0;
    length_ = 0;
    moduleCount_ = 0;
}
void PackageDeclarationReader::path() {
    std::size_t used = 0;
    result_.path[0] = '\0';
    for (std::size_t i = 0; i < depth_; ++i) {
        const Frame &frame = frames_[i];
        const unsigned kind = static_cast<unsigned>(frame.kind);
        if (kind <= static_cast<unsigned>(Kind::Port)) {
            if (frame.field < 0)
                continue;
            // JSON-pointer escaping also handles identifiers containing slash/tilde.
            const char *key = frame.key;
            if (used + 1 < sizeof(result_.path))
                result_.path[used++] = '/';
            for (; *key; ++key) {
                const char *piece = *key == '~' ? "~0" : (*key == '/' ? "~1" : nullptr);
                const std::size_t count = piece ? 2 : 1;
                if (used + count >= sizeof(result_.path))
                    break;
                if (piece) {
                    result_.path[used++] = piece[0];
                    result_.path[used++] = piece[1];
                } else
                    result_.path[used++] = *key;
            }
        } else if (frame.kind != Kind::Skip && frame.count != 0) {
            const int written = std::snprintf(result_.path + used, sizeof(result_.path) - used,
                                              "/%lu", static_cast<unsigned long>(frame.count - 1));
            if (written < 0 || static_cast<std::size_t>(written) >= sizeof(result_.path) - used)
                break;
            used += static_cast<std::size_t>(written);
        }
    }
    result_.path[used] = '\0';
}
void PackageDeclarationReader::fail(DeclarationError error, std::size_t offset) {
    if (result_.error != DeclarationError::None)
        return;
    result_.error = error;
    result_.offset = offset;
    path();
}
bool PackageDeclarationReader::string() const {
    return scalar_ == CanonicalJsonToken::String;
}
bool PackageDeclarationReader::equals(const char *text) const {
    return length_ < sizeof(text_) && std::strlen(text) == length_ &&
           std::memcmp(text_, text, length_) == 0;
}
bool PackageDeclarationReader::identifier() const {
    if (!string() || length_ == 0 || length_ > 48)
        return false;
    for (std::size_t i = 0; i < length_; ++i)
        if (static_cast<unsigned char>(text_[i]) < 0x20 ||
            static_cast<unsigned char>(text_[i]) > 0x7f)
            return false;
    return true;
}
bool PackageDeclarationReader::integer(std::int64_t low, std::int64_t high) const {
    if (scalar_ != CanonicalJsonToken::Integer || length_ == 0 || length_ > 11)
        return false;
    std::size_t i = text_[0] == '-' ? 1 : 0;
    std::int64_t value = 0;
    for (; i < length_; ++i)
        value = value * 10 + (text_[i] - '0');
    if (text_[0] == '-')
        value = -value;
    return value >= low && value <= high;
}
bool PackageDeclarationReader::digest() const {
    if (!string() || length_ != 64)
        return false;
    for (std::size_t i = 0; i < length_; ++i)
        if (!(text_[i] >= '0' && text_[i] <= '9') && !(text_[i] >= 'a' && text_[i] <= 'f'))
            return false;
    return true;
}
bool PackageDeclarationReader::semver() const {
    if (!string() || length_ > 17)
        return false;
    std::size_t i = 0;
    for (unsigned part = 0; part < 3; ++part) {
        const std::size_t begin = i;
        unsigned value = 0;
        while (i < length_ && text_[i] >= '0' && text_[i] <= '9') {
            if (i - begin == 5)
                return false;
            value = value * 10 + static_cast<unsigned>(text_[i++] - '0');
        }
        if (begin == i || value > 65535 || (i - begin > 1 && text_[begin] == '0'))
            return false;
        if (part != 2 && (i == length_ || text_[i++] != '.'))
            return false;
    }
    return i == length_;
}
void PackageDeclarationReader::identity() {
    if (!identifier()) {
        fail(string() ? DeclarationError::Value : DeclarationError::Type, tokenOffset_);
        return;
    }
    Frame &list = frames_[depth_ - 2];
    if (list.count > 1 && std::strcmp(list.previous, text_) >= 0) {
        fail(DeclarationError::ListOrder, tokenOffset_);
        return;
    }
    std::memcpy(list.previous, text_, length_ + 1);
}
PackageDeclarationReader::Kind PackageDeclarationReader::child(bool array) {
    if (depth_ == 0) {
        if (array)
            fail(DeclarationError::Type, tokenOffset_);
        return Kind::Root;
    }
    Frame &f = frames_[depth_ - 1];
    if (f.kind == Kind::Skip)
        return Kind::Skip;
    if (static_cast<unsigned>(f.kind) >= static_cast<unsigned>(Kind::Requirements)) {
        if (array) {
            fail(DeclarationError::Type, tokenOffset_);
            return Kind::Skip;
        }
        ++f.count;
        std::size_t limit = CanonicalPackageJsonReader::DocumentBytes;
        Kind kind = Kind::Skip;
        switch (f.kind) {
        case Kind::Requirements:
            kind = Kind::Requirement;
            break;
        case Kind::Modules:
            kind = Kind::Module;
            break;
        case Kind::Subjects:
            kind = Kind::Subject;
            break;
        case Kind::Resources:
            kind = Kind::Resource;
            limit = 32;
            break;
        case Kind::Stations:
            kind = Kind::Station;
            limit = 8;
            break;
        case Kind::Values:
            kind = Kind::Value;
            limit = 32;
            break;
        case Kind::Timers:
            kind = Kind::Timer;
            limit = 16;
            break;
        case Kind::Ports:
            kind = Kind::Port;
            break;
        default:
            break;
        }
        if (f.count > limit)
            fail(DeclarationError::Capacity, tokenOffset_);
        return kind;
    }
    if (array) {
        if (f.kind == Kind::Root) {
            switch (f.field) {
            case 2:
                return Kind::Modules;
            case 4:
                return Kind::Requirements;
            case 5:
                return Kind::Resources;
            case 7:
                return Kind::Stations;
            case 8:
                return Kind::Subjects;
            default:
                break;
            }
        } else if (f.kind == Kind::Station) {
            switch (f.field) {
            case 1:
            case 4:
                return Kind::Skip;
            case 3:
                return Kind::Ports;
            case 5:
                return Kind::Timers;
            case 6:
                return Kind::Values;
            default:
                break;
            }
        }
    }
    fail(DeclarationError::Type, tokenOffset_);
    return Kind::Skip;
}
void PackageDeclarationReader::token(CanonicalJsonToken value, std::size_t offset) {
    if (result_.error != DeclarationError::None)
        return;
    tokenOffset_ = offset;
    if (value == CanonicalJsonToken::ObjectBegin || value == CanonicalJsonToken::ArrayBegin) {
        const Kind kind = child(value == CanonicalJsonToken::ArrayBegin);
        if (result_.error != DeclarationError::None)
            return;
        if (kind == Kind::Skip && skipDepth_ == 0) {
            skipBegin_ = offset;
            skipDepth_ = depth_ + 1;
        }
        if (kind == Kind::Module)
            module_ = {};
        frames_[depth_] = {};
        frames_[depth_++].kind = kind;
        return;
    }
    if (value == CanonicalJsonToken::ObjectEnd || value == CanonicalJsonToken::ArrayEnd) {
        Frame &f = frames_[depth_ - 1];
        const unsigned kind = static_cast<unsigned>(f.kind);
        if (kind <= static_cast<unsigned>(Kind::Port)) {
            const unsigned expected = (1u << sizes[kind]) - 1;
            if (f.seen != expected) {
                // Name the first missing field, rather than the last read field.
                for (std::size_t i = 0; i < sizes[kind]; ++i)
                    if (!(f.seen & (1u << i))) {
                        f.field = static_cast<int>(i);
                        std::strcpy(f.key, fields[kind][i]);
                        break;
                    }
                fail(DeclarationError::FieldSet, offset);
            }
            if (f.kind == Kind::Value && f.initialType != CanonicalJsonToken::ObjectBegin &&
                f.booleanValue != (f.initialType == CanonicalJsonToken::Boolean))
                fail(DeclarationError::Type, offset);
        }
        if (f.kind == Kind::Module && result_.error == DeclarationError::None) {
            bool known = false;
            for (std::size_t i = 0; i < moduleCount_; ++i) {
                if (std::strcmp(modules_[i].id, module_.id) == 0 &&
                    std::strcmp(modules_[i].version, module_.version) == 0) {
                    known = true;
                    if (std::strcmp(modules_[i].digest, module_.digest) != 0)
                        fail(DeclarationError::ModuleConflict, offset);
                }
            }
            if (!known) {
                if (moduleCount_ == 16)
                    fail(DeclarationError::Capacity, offset);
                else
                    modules_[moduleCount_++] = module_;
            }
        }
        if (skipDepth_ == depth_) {
            skippedBytes_ += offset - skipBegin_ + 1;
            skipDepth_ = 0;
        }
        --depth_;
        return;
    }
    scalar_ = value;
    length_ = 0;
    text_[0] = '\0';
}
void PackageDeclarationReader::scalarByte(std::uint8_t byte) {
    if (result_.error != DeclarationError::None)
        return;
    if (length_ < sizeof(text_) - 1)
        text_[length_] = static_cast<char>(byte);
    ++length_;
}
void PackageDeclarationReader::scalarEnd(std::size_t) {
    if (result_.error != DeclarationError::None)
        return;
    text_[length_ < sizeof(text_) ? length_ : sizeof(text_) - 1] = '\0';
    Frame &frame = frames_[depth_ - 1];
    if (frame.kind == Kind::Skip)
        return;
    if (scalar_ == CanonicalJsonToken::Key) {
        const unsigned kind = static_cast<unsigned>(frame.kind);
        if (kind > static_cast<unsigned>(Kind::Port)) {
            fail(DeclarationError::Type, tokenOffset_);
            return;
        }
        std::memcpy(frame.key, text_, length_ + 1);
        frame.field = -1;
        for (std::size_t i = 0; i < sizes[kind]; ++i)
            if (equals(fields[kind][i])) {
                frame.field = static_cast<int>(i);
                frame.seen |= static_cast<std::uint16_t>(1u << i);
                return;
            }
        frame.field = 0; // keep unknown key in diagnostic path
        fail(DeclarationError::FieldSet, tokenOffset_);
        return;
    }
    scalar();
}
void PackageDeclarationReader::scalar() {
    Frame &f = frames_[depth_ - 1];
    if (static_cast<unsigned>(f.kind) > static_cast<unsigned>(Kind::Port)) {
        fail(DeclarationError::Type, tokenOffset_);
        return;
    }
    const char *key = f.key;
    bool valid = false;
    bool typed = string();
    DeclarationError error = DeclarationError::Value;
    if ((f.kind == Kind::Root && f.field == 3) || std::strcmp(key, "originStation") == 0) {
        valid = identifier();
    } else if (std::strcmp(key, "id") == 0 || (f.kind == Kind::Requirement && f.field == 0)) {
        identity();
        return;
    } else if (f.kind == Kind::Module && f.field == 1) {
        valid = identifier();
        if (valid)
            std::memcpy(module_.id, text_, length_ + 1);
    } else if (std::strcmp(key, "label") == 0) {
        valid = string() && length_ >= 1 && length_ <= 96;
        error = DeclarationError::Range;
    } else if (std::strcmp(key, "format") == 0) {
        valid = string() && equals("larp.package.v1");
    } else if (std::strcmp(key, "digest") == 0) {
        valid = digest();
        if (valid && f.kind == Kind::Module)
            std::memcpy(module_.digest, text_, length_ + 1);
    } else if (f.kind == Kind::Module && f.field == 2) {
        valid = semver();
        if (valid)
            std::memcpy(module_.version, text_, length_ + 1);
    } else if (f.kind == Kind::Requirement && f.field == 1) {
        typed = scalar_ == CanonicalJsonToken::Integer;
        // Positive major has no specified upper bound. Canonical lexical rules
        // guarantee decimal digits, so no potentially overflowing conversion.
        valid = typed && length_ != 0 && text_[0] >= '1' && text_[0] <= '9';
        error = DeclarationError::Range;
    } else if (std::strcmp(key, "revision") == 0 || std::strcmp(key, "bytes") == 0) {
        typed = scalar_ == CanonicalJsonToken::Integer;
        valid = integer(1, 4294967295LL);
        error = DeclarationError::Range;
    } else if (std::strcmp(key, "durationMs") == 0) {
        typed = scalar_ == CanonicalJsonToken::Integer;
        valid = integer(1, 86400000);
        error = DeclarationError::Range;
    } else if (std::strcmp(key, "kind") == 0) {
        valid = string() && (equals("light_preset") || equals("audio"));
    } else if (std::strcmp(key, "initial") == 0) {
        typed = scalar_ == CanonicalJsonToken::Integer || scalar_ == CanonicalJsonToken::Boolean;
        valid = scalar_ == CanonicalJsonToken::Boolean || integer(-2147483647LL - 1, 2147483647);
        f.initialType = scalar_;
        error = DeclarationError::Range;
    } else if (std::strcmp(key, "type") == 0 && f.kind == Kind::Value) {
        valid = string() && (equals("bool") || equals("int32"));
        f.booleanValue = equals("bool");
    } else if (std::strcmp(key, "type") == 0 && f.kind == Kind::Port) {
        valid = string() && (equals("boolean_sample") || equals("int32_sample") ||
                             equals("light") || equals("audio"));
    } else if (std::strcmp(key, "direction") == 0) {
        valid = string() && (equals("input") || equals("output"));
    } else if (std::strcmp(key, "capability") == 0) {
        valid = string(); // opaque; no unstated grammar or capability acceptance
    } else {
        typed = false; // array-valued field supplied as a scalar
    }
    if (!valid)
        fail(typed ? error : DeclarationError::Type, tokenOffset_);
}
DeclarationResult PackageDeclarationReader::finish(CanonicalJsonResult canonical) {
    result_.canonical = canonical;
    if (!canonical) {
        result_.scope = DeclarationScope::Rejected;
        result_.error = DeclarationError::CanonicalInput;
        result_.canonical = canonical;
        result_.offset = canonical.offset;
        result_.path[0] = '\0';
    } else if (result_.error == DeclarationError::None) {
        if (canonical.offset - skippedBytes_ > MetadataBytes) {
            result_.error = DeclarationError::MetadataLimit;
            result_.offset = canonical.offset;
        } else {
            result_.scope = DeclarationScope::DeclarationsOnly;
            result_.offset = canonical.offset;
        }
        result_.canonical = canonical;
    }
    return result_;
}
DeclarationResult PackageDeclarationReader::validate(IPackageByteSource &source) {
    reset();
    return finish(canonical_.validate(source, this));
}
DeclarationResult PackageDeclarationReader::validate(const char *bytes, std::size_t count) {
    reset();
    return finish(canonical_.validate(bytes, count, this));
}
} // namespace configuration
} // namespace larp
